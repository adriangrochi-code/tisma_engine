// The window drawn with Vulkan, written from the Vulkan specification
// (Khronos). Own design; see PROVENANCE.md.

#include "vk_window.hpp"

#ifdef TISMA_VULKAN_WINDOW

#include <volk.h>

#include <GLFW/glfw3.h>
#include <imgui_impl_vulkan.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <functional>
#include <stdexcept>

namespace tisma::ui {
namespace {

constexpr int kFrames = 2; // frames in flight

void check(VkResult r, const char* what) {
    if (r != VK_SUCCESS)
        throw std::runtime_error(std::string("vulkan: ") + what + " failed (" + std::to_string(int(r)) + ")");
}

int device_rank(VkPhysicalDeviceType type) {
    switch (type) {
    case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU: return 4;
    case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: return 3;
    case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU: return 2;
    case VK_PHYSICAL_DEVICE_TYPE_CPU: return 1;
    default: return 0;
    }
}

// A layout change of a whole colour image, waiting for what came before.
void barrier(VkCommandBuffer c, VkImage image, VkImageLayout from, VkImageLayout to, VkAccessFlags src_access,
             VkAccessFlags dst_access, VkPipelineStageFlags src_stage, VkPipelineStageFlags dst_stage) {
    VkImageMemoryBarrier b{};
    b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    b.srcAccessMask = src_access;
    b.dstAccessMask = dst_access;
    b.oldLayout = from;
    b.newLayout = to;
    b.srcQueueFamilyIndex = b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    b.image = image;
    b.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    vkCmdPipelineBarrier(c, src_stage, dst_stage, 0, 0, nullptr, 0, nullptr, 1, &b);
}

} // namespace

struct VulkanWindow::Impl {
    GLFWwindow* window = nullptr;
    bool vsync = true;
    VkInstance instance = VK_NULL_HANDLE;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkPhysicalDevice physical = VK_NULL_HANDLE;
    VkPhysicalDeviceMemoryProperties memory{};
    std::string name;
    VkDevice device = VK_NULL_HANDLE;
    std::uint32_t family = 0;
    VkQueue queue = VK_NULL_HANDLE;
    VkCommandPool pool = VK_NULL_HANDLE;
    bool imgui = false;

    // The swapchain and what hangs from it.
    VkSurfaceFormatKHR format{};
    VkRenderPass pass = VK_NULL_HANDLE;
    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    VkExtent2D extent{};
    std::uint32_t min_images = 2;
    bool readable = false; // the swapchain's images can be copied out
    std::vector<VkImage> images;
    std::vector<VkImageView> views;
    std::vector<VkFramebuffer> framebuffers;
    std::vector<VkSemaphore> rendered; // per image: drawn, it can be shown

    struct Frame {
        VkCommandBuffer commands = VK_NULL_HANDLE;
        VkFence done = VK_NULL_HANDLE;
        VkSemaphore acquired = VK_NULL_HANDLE;
    };
    std::array<Frame, kFrames> frames{};
    int current = 0;

    // The frame kept for read_back: the GPU copies the image into this buffer.
    VkBuffer kept = VK_NULL_HANDLE;
    VkDeviceMemory kept_memory = VK_NULL_HANDLE;
    VkDeviceSize kept_size = 0;
    VkExtent2D kept_extent{};
    int kept_frame = -1; // the frame whose fence the copy waits on; -1: none

    struct Texture {
        VkImage image = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        VkImageView view = VK_NULL_HANDLE;
        VkDescriptorSet set = VK_NULL_HANDLE;
    };
    std::vector<Texture> textures;

    std::uint32_t memory_type(std::uint32_t bits, VkMemoryPropertyFlags flags) const {
        for (std::uint32_t i = 0; i < memory.memoryTypeCount; ++i)
            if ((bits & (1u << i)) && (memory.memoryTypes[i].propertyFlags & flags) == flags)
                return i;
        throw std::runtime_error("vulkan: no memory type for the window");
    }

    VkDeviceMemory allocate(VkMemoryRequirements need, VkMemoryPropertyFlags flags) {
        VkMemoryAllocateInfo mai{};
        mai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        mai.allocationSize = need.size;
        mai.memoryTypeIndex = memory_type(need.memoryTypeBits, flags);
        VkDeviceMemory m = VK_NULL_HANDLE;
        check(vkAllocateMemory(device, &mai, nullptr, &m), "vkAllocateMemory");
        return m;
    }

    // A host-visible buffer for copies.
    void make_buffer(VkDeviceSize size, VkBufferUsageFlags usage, VkBuffer& buffer, VkDeviceMemory& mem) {
        VkBufferCreateInfo bci{};
        bci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bci.size = size;
        bci.usage = usage;
        bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        check(vkCreateBuffer(device, &bci, nullptr, &buffer), "vkCreateBuffer");
        VkMemoryRequirements need{};
        vkGetBufferMemoryRequirements(device, buffer, &need);
        mem = allocate(need, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        check(vkBindBufferMemory(device, buffer, mem, 0), "vkBindBufferMemory");
    }

    // Records commands and runs them now, waiting for the end.
    void one_time(const std::function<void(VkCommandBuffer)>& record) {
        VkCommandBufferAllocateInfo cai{};
        cai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        cai.commandPool = pool;
        cai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        cai.commandBufferCount = 1;
        VkCommandBuffer c = VK_NULL_HANDLE;
        check(vkAllocateCommandBuffers(device, &cai, &c), "vkAllocateCommandBuffers");
        VkCommandBufferBeginInfo begin{};
        begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(c, &begin);
        record(c);
        vkEndCommandBuffer(c);
        VkSubmitInfo si{};
        si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        si.commandBufferCount = 1;
        si.pCommandBuffers = &c;
        check(vkQueueSubmit(queue, 1, &si, VK_NULL_HANDLE), "vkQueueSubmit");
        vkQueueWaitIdle(queue);
        vkFreeCommandBuffers(device, pool, 1, &c);
    }

    void free_swapchain() {
        for (VkFramebuffer f : framebuffers)
            vkDestroyFramebuffer(device, f, nullptr);
        for (VkImageView v : views)
            vkDestroyImageView(device, v, nullptr);
        for (VkSemaphore s : rendered)
            vkDestroySemaphore(device, s, nullptr);
        framebuffers.clear(), views.clear(), rendered.clear(), images.clear();
        if (swapchain)
            vkDestroySwapchainKHR(device, swapchain, nullptr);
        swapchain = VK_NULL_HANDLE;
    }

    // (Re)makes the swapchain at the window's size; false while the window
    // has no area (minimized).
    bool make_swapchain() {
        vkDeviceWaitIdle(device);
        free_swapchain();
        VkSurfaceCapabilitiesKHR caps{};
        check(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical, surface, &caps),
              "vkGetPhysicalDeviceSurfaceCapabilitiesKHR");
        int w = 0, h = 0;
        glfwGetFramebufferSize(window, &w, &h);
        extent = caps.currentExtent;
        if (extent.width == 0xFFFFFFFFu) {
            extent.width = std::clamp(std::uint32_t(std::max(w, 0)), caps.minImageExtent.width, caps.maxImageExtent.width);
            extent.height =
                std::clamp(std::uint32_t(std::max(h, 0)), caps.minImageExtent.height, caps.maxImageExtent.height);
        }
        if (extent.width == 0 || extent.height == 0)
            return false;
        // FIFO waits for the screen and is always there; without vsync, the
        // fastest the surface offers.
        VkPresentModeKHR mode = VK_PRESENT_MODE_FIFO_KHR;
        if (!vsync) {
            std::uint32_t count = 0;
            vkGetPhysicalDeviceSurfacePresentModesKHR(physical, surface, &count, nullptr);
            std::vector<VkPresentModeKHR> modes(count);
            vkGetPhysicalDeviceSurfacePresentModesKHR(physical, surface, &count, modes.data());
            for (VkPresentModeKHR wanted : {VK_PRESENT_MODE_MAILBOX_KHR, VK_PRESENT_MODE_IMMEDIATE_KHR})
                if (std::find(modes.begin(), modes.end(), wanted) != modes.end()) {
                    mode = wanted;
                    break;
                }
        }
        min_images = std::max(caps.minImageCount, 2u);
        std::uint32_t count = min_images + 1;
        if (caps.maxImageCount > 0)
            count = std::min(count, caps.maxImageCount);
        readable = (caps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) != 0;
        VkSwapchainCreateInfoKHR sci{};
        sci.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        sci.surface = surface;
        sci.minImageCount = count;
        sci.imageFormat = format.format;
        sci.imageColorSpace = format.colorSpace;
        sci.imageExtent = extent;
        sci.imageArrayLayers = 1;
        sci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | (readable ? VK_IMAGE_USAGE_TRANSFER_SRC_BIT : 0);
        sci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        sci.preTransform = caps.currentTransform;
        sci.compositeAlpha = (caps.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR)
                                 ? VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR
                                 : VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;
        sci.presentMode = mode;
        sci.clipped = VK_TRUE;
        check(vkCreateSwapchainKHR(device, &sci, nullptr, &swapchain), "vkCreateSwapchainKHR");
        vkGetSwapchainImagesKHR(device, swapchain, &count, nullptr);
        images.resize(count);
        vkGetSwapchainImagesKHR(device, swapchain, &count, images.data());
        for (VkImage image : images) {
            VkImageViewCreateInfo vci{};
            vci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            vci.image = image;
            vci.viewType = VK_IMAGE_VIEW_TYPE_2D;
            vci.format = format.format;
            vci.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
            VkImageView view = VK_NULL_HANDLE;
            check(vkCreateImageView(device, &vci, nullptr, &view), "vkCreateImageView");
            views.push_back(view);
            VkFramebufferCreateInfo fci{};
            fci.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            fci.renderPass = pass;
            fci.attachmentCount = 1;
            fci.pAttachments = &view;
            fci.width = extent.width;
            fci.height = extent.height;
            fci.layers = 1;
            VkFramebuffer fb = VK_NULL_HANDLE;
            check(vkCreateFramebuffer(device, &fci, nullptr, &fb), "vkCreateFramebuffer");
            framebuffers.push_back(fb);
            VkSemaphoreCreateInfo sem{};
            sem.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
            VkSemaphore s = VK_NULL_HANDLE;
            check(vkCreateSemaphore(device, &sem, nullptr, &s), "vkCreateSemaphore");
            rendered.push_back(s);
        }
        if (imgui)
            ImGui_ImplVulkan_SetMinImageCount(min_images);
        return true;
    }

    void init(GLFWwindow* w, bool sync) {
        window = w;
        vsync = sync;
        if (volkInitialize() != VK_SUCCESS)
            throw std::runtime_error("no Vulkan loader on this machine");
        if (!glfwVulkanSupported())
            throw std::runtime_error("GLFW finds no Vulkan for windows here");
        std::uint32_t extension_count = 0;
        const char** extensions = glfwGetRequiredInstanceExtensions(&extension_count);
        if (!extensions)
            throw std::runtime_error("Vulkan cannot show windows here");
        VkApplicationInfo app{};
        app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        app.pApplicationName = "Tisma";
        app.pEngineName = "Tisma engine";
        app.apiVersion = VK_API_VERSION_1_1;
        VkInstanceCreateInfo ici{};
        ici.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        ici.pApplicationInfo = &app;
        ici.enabledExtensionCount = extension_count;
        ici.ppEnabledExtensionNames = extensions;
        check(vkCreateInstance(&ici, nullptr, &instance), "vkCreateInstance");
        volkLoadInstance(instance);
        check(glfwCreateWindowSurface(instance, window, nullptr, &surface), "glfwCreateWindowSurface");

        // The best device that has Vulkan 1.1, the swapchain, and a queue
        // family that draws and shows on this surface.
        std::uint32_t count = 0;
        vkEnumeratePhysicalDevices(instance, &count, nullptr);
        std::vector<VkPhysicalDevice> devices(count);
        vkEnumeratePhysicalDevices(instance, &count, devices.data());
        int best = -1;
        for (VkPhysicalDevice d : devices) {
            VkPhysicalDeviceProperties props{};
            vkGetPhysicalDeviceProperties(d, &props);
            if (props.apiVersion < VK_API_VERSION_1_1)
                continue;
            std::uint32_t n = 0;
            vkEnumerateDeviceExtensionProperties(d, nullptr, &n, nullptr);
            std::vector<VkExtensionProperties> exts(n);
            vkEnumerateDeviceExtensionProperties(d, nullptr, &n, exts.data());
            const bool swaps = std::any_of(exts.begin(), exts.end(), [](const VkExtensionProperties& e) {
                return std::strcmp(e.extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0;
            });
            if (!swaps)
                continue;
            std::uint32_t families = 0;
            vkGetPhysicalDeviceQueueFamilyProperties(d, &families, nullptr);
            std::vector<VkQueueFamilyProperties> fams(families);
            vkGetPhysicalDeviceQueueFamilyProperties(d, &families, fams.data());
            for (std::uint32_t f = 0; f < families; ++f) {
                VkBool32 present = VK_FALSE;
                vkGetPhysicalDeviceSurfaceSupportKHR(d, f, surface, &present);
                if (!(fams[f].queueFlags & VK_QUEUE_GRAPHICS_BIT) || !present)
                    continue;
                if (device_rank(props.deviceType) > best) {
                    best = device_rank(props.deviceType);
                    physical = d;
                    family = f;
                    name = props.deviceName;
                }
                break;
            }
        }
        if (!physical)
            throw std::runtime_error("no Vulkan 1.1 device can show this window");
        vkGetPhysicalDeviceMemoryProperties(physical, &memory);

        const float priority = 1.0f;
        VkDeviceQueueCreateInfo qci{};
        qci.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        qci.queueFamilyIndex = family;
        qci.queueCount = 1;
        qci.pQueuePriorities = &priority;
        const char* device_extensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
        VkDeviceCreateInfo dci{};
        dci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        dci.queueCreateInfoCount = 1;
        dci.pQueueCreateInfos = &qci;
        dci.enabledExtensionCount = 1;
        dci.ppEnabledExtensionNames = device_extensions;
        check(vkCreateDevice(physical, &dci, nullptr, &device), "vkCreateDevice");
        volkLoadDevice(device);
        vkGetDeviceQueue(device, family, 0, &queue);

        VkCommandPoolCreateInfo pci{};

        pci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        pci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        pci.queueFamilyIndex = family;
        check(vkCreateCommandPool(device, &pci, nullptr, &pool), "vkCreateCommandPool");

        // The surface's format: 8 bits a channel, not sRGB, as OpenGL's
        // window (the interface's colours are written as they are).
        std::uint32_t formats = 0;
        vkGetPhysicalDeviceSurfaceFormatsKHR(physical, surface, &formats, nullptr);
        std::vector<VkSurfaceFormatKHR> offered(formats);
        vkGetPhysicalDeviceSurfaceFormatsKHR(physical, surface, &formats, offered.data());
        if (offered.empty())
            throw std::runtime_error("the window offers no Vulkan format");
        format = offered.front();
        for (VkFormat wanted : {VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM})
            if (auto it = std::find_if(offered.begin(), offered.end(),
                                       [&](const VkSurfaceFormatKHR& f) {
                                           return f.format == wanted &&
                                                  f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
                                       });
                it != offered.end()) {
                format = *it;
                break;
            }

        // One pass: clear, draw, ready to show.
        VkAttachmentDescription colour{};
        colour.format = format.format;
        colour.samples = VK_SAMPLE_COUNT_1_BIT;
        colour.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colour.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colour.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colour.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colour.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colour.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        VkAttachmentReference ref{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
        VkSubpassDescription sub{};
        sub.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        sub.colorAttachmentCount = 1;
        sub.pColorAttachments = &ref;
        // The image comes from the acquire semaphore, waited at this stage.
        VkSubpassDependency dep{};
        dep.srcSubpass = VK_SUBPASS_EXTERNAL;
        dep.dstSubpass = 0;
        dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        VkRenderPassCreateInfo rpi{};
        rpi.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        rpi.attachmentCount = 1;
        rpi.pAttachments = &colour;
        rpi.subpassCount = 1;
        rpi.pSubpasses = &sub;
        rpi.dependencyCount = 1;
        rpi.pDependencies = &dep;
        check(vkCreateRenderPass(device, &rpi, nullptr, &pass), "vkCreateRenderPass");

        for (Frame& f : frames) {
            VkCommandBufferAllocateInfo cai{};
            cai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            cai.commandPool = pool;
            cai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            cai.commandBufferCount = 1;
            check(vkAllocateCommandBuffers(device, &cai, &f.commands), "vkAllocateCommandBuffers");
            VkFenceCreateInfo fci{};
            fci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
            fci.flags = VK_FENCE_CREATE_SIGNALED_BIT;
            check(vkCreateFence(device, &fci, nullptr, &f.done), "vkCreateFence");
            VkSemaphoreCreateInfo sci{};
            sci.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
            check(vkCreateSemaphore(device, &sci, nullptr, &f.acquired), "vkCreateSemaphore");
        }
        make_swapchain();
    }

    void shutdown() {
        if (device) {
            vkDeviceWaitIdle(device);
            for (Texture& t : textures) {
                if (imgui && t.set)
                    ImGui_ImplVulkan_RemoveTexture(t.set);
                vkDestroyImageView(device, t.view, nullptr);
                vkDestroyImage(device, t.image, nullptr);
                vkFreeMemory(device, t.memory, nullptr);
            }
            textures.clear();
            if (imgui)
                ImGui_ImplVulkan_Shutdown();
            imgui = false;
            if (kept)
                vkDestroyBuffer(device, kept, nullptr), vkFreeMemory(device, kept_memory, nullptr);
            kept = VK_NULL_HANDLE;
            free_swapchain();
            for (Frame& f : frames) {
                if (f.done)
                    vkDestroyFence(device, f.done, nullptr);
                if (f.acquired)
                    vkDestroySemaphore(device, f.acquired, nullptr);
                f = {};
            }
            if (pass)
                vkDestroyRenderPass(device, pass, nullptr);
            if (pool)
                vkDestroyCommandPool(device, pool, nullptr);
            vkDestroyDevice(device, nullptr);
            device = VK_NULL_HANDLE;
        }
        if (surface)
            vkDestroySurfaceKHR(instance, surface, nullptr);
        surface = VK_NULL_HANDLE;
        if (instance)
            vkDestroyInstance(instance, nullptr);
        instance = VK_NULL_HANDLE;
    }
};

VulkanWindow::VulkanWindow() : impl_(std::make_unique<Impl>()) {}
VulkanWindow::~VulkanWindow() { shutdown(); }

bool VulkanWindow::built() { return true; }

bool VulkanWindow::init(GLFWwindow* window, bool vsync, std::string& error) {
    try {
        impl_->init(window, vsync);
        return true;
    } catch (const std::exception& e) {
        error = e.what();
        impl_->shutdown();
        return false;
    }
}

bool VulkanWindow::init_imgui(std::string& error) {
    Impl& v = *impl_;
    ImGui_ImplVulkan_InitInfo info{};
    info.ApiVersion = VK_API_VERSION_1_1;
    info.Instance = v.instance;
    info.PhysicalDevice = v.physical;
    info.Device = v.device;
    info.QueueFamily = v.family;
    info.Queue = v.queue;
    info.DescriptorPoolSize = 256; // the backend's own pool: the font and the icons
    info.MinImageCount = v.min_images;
    info.ImageCount = std::max<std::uint32_t>(std::uint32_t(v.images.size()), v.min_images);
    info.PipelineInfoMain.RenderPass = v.pass;
    info.PipelineInfoMain.Subpass = 0;
    info.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    if (!ImGui_ImplVulkan_Init(&info)) {
        error = "Dear ImGui's Vulkan backend did not start";
        return false;
    }
    v.imgui = true;
    return true;
}

void VulkanWindow::shutdown() {
    if (impl_)
        impl_->shutdown();
}

ImTextureID VulkanWindow::add_texture(const std::uint8_t* rgba, int width, int height) {
    Impl& v = *impl_;
    if (!v.imgui || width <= 0 || height <= 0)
        return ImTextureID(0);
    try {
        Impl::Texture t;
        VkImageCreateInfo ici{};
        ici.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        ici.imageType = VK_IMAGE_TYPE_2D;
        ici.format = VK_FORMAT_R8G8B8A8_UNORM;
        ici.extent = {std::uint32_t(width), std::uint32_t(height), 1};
        ici.mipLevels = 1;
        ici.arrayLayers = 1;
        ici.samples = VK_SAMPLE_COUNT_1_BIT;
        ici.tiling = VK_IMAGE_TILING_OPTIMAL;
        ici.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        ici.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        ici.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        check(vkCreateImage(v.device, &ici, nullptr, &t.image), "vkCreateImage");
        VkMemoryRequirements need{};
        vkGetImageMemoryRequirements(v.device, t.image, &need);
        t.memory = v.allocate(need, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        check(vkBindImageMemory(v.device, t.image, t.memory, 0), "vkBindImageMemory");

        const VkDeviceSize bytes = VkDeviceSize(width) * VkDeviceSize(height) * 4;
        VkBuffer staging = VK_NULL_HANDLE;
        VkDeviceMemory staging_memory = VK_NULL_HANDLE;
        v.make_buffer(bytes, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, staging, staging_memory);
        void* mapped = nullptr;
        check(vkMapMemory(v.device, staging_memory, 0, bytes, 0, &mapped), "vkMapMemory");
        std::memcpy(mapped, rgba, std::size_t(bytes));
        vkUnmapMemory(v.device, staging_memory);
        v.one_time([&](VkCommandBuffer c) {
            barrier(c, t.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0,
                    VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
            VkBufferImageCopy copy{};
            copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
            copy.imageExtent = ici.extent;
            vkCmdCopyBufferToImage(c, staging, t.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
            barrier(c, t.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
        });
        vkDestroyBuffer(v.device, staging, nullptr);
        vkFreeMemory(v.device, staging_memory, nullptr);

        VkImageViewCreateInfo vci{};

        vci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        vci.image = t.image;
        vci.viewType = VK_IMAGE_VIEW_TYPE_2D;
        vci.format = VK_FORMAT_R8G8B8A8_UNORM;
        vci.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        check(vkCreateImageView(v.device, &vci, nullptr, &t.view), "vkCreateImageView");
        t.set = ImGui_ImplVulkan_AddTexture(t.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        v.textures.push_back(t);
        return ImTextureID(t.set);
    } catch (const std::exception&) {
        return ImTextureID(0);
    }
}

void VulkanWindow::frame(ImDrawData* draw, const float clear[3], bool keep) {
    Impl& v = *impl_;
    int w = 0, h = 0;
    glfwGetFramebufferSize(v.window, &w, &h);
    if (w <= 0 || h <= 0)
        return; // minimized: nothing to show
    if (!v.swapchain || std::uint32_t(w) != v.extent.width || std::uint32_t(h) != v.extent.height)
        if (!v.make_swapchain())
            return;
    Impl::Frame& f = v.frames[std::size_t(v.current)];
    vkWaitForFences(v.device, 1, &f.done, VK_TRUE, UINT64_MAX);
    std::uint32_t index = 0;
    VkResult r = vkAcquireNextImageKHR(v.device, v.swapchain, UINT64_MAX, f.acquired, VK_NULL_HANDLE, &index);
    if (r == VK_ERROR_OUT_OF_DATE_KHR) {
        v.make_swapchain();
        return;
    }
    if (r != VK_SUCCESS && r != VK_SUBOPTIMAL_KHR)
        return;
    vkResetFences(v.device, 1, &f.done);
    VkCommandBuffer c = f.commands;
    vkResetCommandBuffer(c, 0);
    VkCommandBufferBeginInfo begin{};
    begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(c, &begin);
    VkClearValue colour{};
    colour.color.float32[0] = clear[0], colour.color.float32[1] = clear[1], colour.color.float32[2] = clear[2];
    colour.color.float32[3] = 1.0f;
    VkRenderPassBeginInfo rbi{};
    rbi.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rbi.renderPass = v.pass;
    rbi.framebuffer = v.framebuffers[index];
    rbi.renderArea.extent = v.extent;
    rbi.clearValueCount = 1;
    rbi.pClearValues = &colour;
    vkCmdBeginRenderPass(c, &rbi, VK_SUBPASS_CONTENTS_INLINE);
    if (draw && v.imgui)
        ImGui_ImplVulkan_RenderDrawData(draw, c);
    vkCmdEndRenderPass(c);
    if (keep && v.readable) {
        const VkDeviceSize bytes = VkDeviceSize(v.extent.width) * VkDeviceSize(v.extent.height) * 4;
        if (bytes > v.kept_size) {
            vkDeviceWaitIdle(v.device); // the old buffer may still be copied into
            if (v.kept)
                vkDestroyBuffer(v.device, v.kept, nullptr), vkFreeMemory(v.device, v.kept_memory, nullptr);
            v.make_buffer(bytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT, v.kept, v.kept_memory);
            v.kept_size = bytes;
        }
        VkImage image = v.images[index];
        barrier(c, image, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
        VkBufferImageCopy copy{};
        copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        copy.imageExtent = {v.extent.width, v.extent.height, 1};
        vkCmdCopyImageToBuffer(c, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, v.kept, 1, &copy);
        barrier(c, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                VK_ACCESS_TRANSFER_READ_BIT, 0, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
        v.kept_extent = v.extent;
        v.kept_frame = v.current;
    }
    vkEndCommandBuffer(c);
    const VkPipelineStageFlags wait = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo si{};
    si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    si.waitSemaphoreCount = 1;
    si.pWaitSemaphores = &f.acquired;
    si.pWaitDstStageMask = &wait;
    si.commandBufferCount = 1;
    si.pCommandBuffers = &c;
    si.signalSemaphoreCount = 1;
    si.pSignalSemaphores = &v.rendered[index];
    if (vkQueueSubmit(v.queue, 1, &si, f.done) != VK_SUCCESS)
        return;
    VkPresentInfoKHR pi{};
    pi.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    pi.waitSemaphoreCount = 1;
    pi.pWaitSemaphores = &v.rendered[index];
    pi.swapchainCount = 1;
    pi.pSwapchains = &v.swapchain;
    pi.pImageIndices = &index;
    r = vkQueuePresentKHR(v.queue, &pi);
    v.current = (v.current + 1) % kFrames;
    if (r == VK_ERROR_OUT_OF_DATE_KHR || r == VK_SUBOPTIMAL_KHR)
        v.make_swapchain();
}

std::vector<std::uint8_t> VulkanWindow::read_back(int& width, int& height) {
    Impl& v = *impl_;
    width = height = 0;
    if (v.kept_frame < 0 || !v.kept)
        return {};
    vkWaitForFences(v.device, 1, &v.frames[std::size_t(v.kept_frame)].done, VK_TRUE, UINT64_MAX);
    width = int(v.kept_extent.width), height = int(v.kept_extent.height);
    const std::size_t row = std::size_t(width) * 4;
    std::vector<std::uint8_t> out(row * std::size_t(height));
    void* mapped = nullptr;
    if (vkMapMemory(v.device, v.kept_memory, 0, VkDeviceSize(out.size()), 0, &mapped) != VK_SUCCESS)
        return {};
    const auto* in = static_cast<const std::uint8_t*>(mapped);
    const bool bgra = v.format.format == VK_FORMAT_B8G8R8A8_UNORM;
    // Rows from the bottom, as OpenGL reads them; RGBA.
    for (int y = 0; y < height; ++y) {
        const std::uint8_t* src = in + std::size_t(height - 1 - y) * row;
        std::uint8_t* dst = out.data() + std::size_t(y) * row;
        for (int x = 0; x < width; ++x) {
            dst[4 * x + 0] = src[4 * x + (bgra ? 2 : 0)];
            dst[4 * x + 1] = src[4 * x + 1];
            dst[4 * x + 2] = src[4 * x + (bgra ? 0 : 2)];
            dst[4 * x + 3] = src[4 * x + 3];
        }
    }
    vkUnmapMemory(v.device, v.kept_memory);
    return out;
}

std::string VulkanWindow::device_name() const { return impl_->name; }

} // namespace tisma::ui

#else // no Vulkan in this build: OpenGL only

namespace tisma::ui {

struct VulkanWindow::Impl {};
VulkanWindow::VulkanWindow() = default;
VulkanWindow::~VulkanWindow() = default;
bool VulkanWindow::built() { return false; }
bool VulkanWindow::init(GLFWwindow*, bool, std::string& error) {
    error = "this build has no Vulkan";
    return false;
}
bool VulkanWindow::init_imgui(std::string& error) {
    error = "this build has no Vulkan";
    return false;
}
void VulkanWindow::shutdown() {}
ImTextureID VulkanWindow::add_texture(const std::uint8_t*, int, int) { return ImTextureID(0); }
void VulkanWindow::frame(ImDrawData*, const float[3], bool) {}
std::vector<std::uint8_t> VulkanWindow::read_back(int& width, int& height) {
    width = height = 0;
    return {};
}
std::string VulkanWindow::device_name() const { return {}; }

} // namespace tisma::ui

#endif
