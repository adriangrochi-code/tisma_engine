// Vulkan compute backend, written from the Vulkan specification (Khronos).
// Vulkan is loaded at run time through volk, so the engine starts on machines
// without it. Own design; see PROVENANCE.md.

#include "gpu/compute.hpp"

#include "gpu/shaders/generated/level_ranges.spv.h"
#include "gpu/shaders/generated/level_segments.spv.h"
#include "gpu/shaders/generated/support_column.spv.h"

// The device functions only through the context's own table (Functions):
// volk's global ones belong to the interface's window.
#define VOLK_NO_DEVICE_PROTOTYPES
#include <volk.h>

#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>

namespace tisma::gpu {
namespace {

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

static_assert(sizeof(std::array<std::uint32_t, 3>) == 12, "triangles are copied to the GPU as packed uint32 triples");

// The Vulkan functions the context calls, loaded for its own instance and
// device. volk's global pointers are not used: the interface's window
// (apps/tisma/vk_window.*) loads them for its instance and device, maybe on
// another GPU, and draws with them while a slice runs here on another thread.
#define TISMA_VK_INSTANCE_FUNCTIONS(X)                                                                                 \
    X(vkDestroyInstance)                                                                                               \
    X(vkEnumeratePhysicalDevices)                                                                                      \
    X(vkGetPhysicalDeviceProperties)                                                                                   \
    X(vkGetPhysicalDeviceQueueFamilyProperties)                                                                        \
    X(vkGetPhysicalDeviceMemoryProperties)                                                                             \
    X(vkCreateDevice)                                                                                                  \
    X(vkGetDeviceProcAddr)
#define TISMA_VK_DEVICE_FUNCTIONS(X)                                                                                   \
    X(vkDestroyDevice)                                                                                                 \
    X(vkGetDeviceQueue)                                                                                                \
    X(vkDeviceWaitIdle)                                                                                                \
    X(vkCreateCommandPool)                                                                                             \
    X(vkDestroyCommandPool)                                                                                            \
    X(vkAllocateCommandBuffers)                                                                                        \
    X(vkFreeCommandBuffers)                                                                                            \
    X(vkBeginCommandBuffer)                                                                                            \
    X(vkEndCommandBuffer)                                                                                              \
    X(vkCreateBuffer)                                                                                                  \
    X(vkDestroyBuffer)                                                                                                 \
    X(vkGetBufferMemoryRequirements)                                                                                   \
    X(vkAllocateMemory)                                                                                                \
    X(vkFreeMemory)                                                                                                    \
    X(vkBindBufferMemory)                                                                                              \
    X(vkMapMemory)                                                                                                     \
    X(vkCreateDescriptorSetLayout)                                                                                     \
    X(vkDestroyDescriptorSetLayout)                                                                                    \
    X(vkCreatePipelineLayout)                                                                                          \
    X(vkDestroyPipelineLayout)                                                                                         \
    X(vkCreateShaderModule)                                                                                            \
    X(vkDestroyShaderModule)                                                                                           \
    X(vkCreateComputePipelines)                                                                                        \
    X(vkDestroyPipeline)                                                                                               \
    X(vkCreateDescriptorPool)                                                                                          \
    X(vkDestroyDescriptorPool)                                                                                         \
    X(vkAllocateDescriptorSets)                                                                                        \
    X(vkUpdateDescriptorSets)                                                                                          \
    X(vkCmdBindPipeline)                                                                                               \
    X(vkCmdBindDescriptorSets)                                                                                         \
    X(vkCmdPushConstants)                                                                                              \
    X(vkCmdDispatch)                                                                                                   \
    X(vkCmdPipelineBarrier)                                                                                            \
    X(vkCreateFence)                                                                                                   \
    X(vkDestroyFence)                                                                                                  \
    X(vkQueueSubmit)                                                                                                   \
    X(vkWaitForFences)

struct Functions {
#define TISMA_VK_DECLARE(name) PFN_##name name = nullptr;
    TISMA_VK_INSTANCE_FUNCTIONS(TISMA_VK_DECLARE)
    TISMA_VK_DEVICE_FUNCTIONS(TISMA_VK_DECLARE)
#undef TISMA_VK_DECLARE

    // False if the instance lacks one.
    bool load(VkInstance instance) {
        bool all = true;
#define TISMA_VK_LOAD(name)                                                                                            \
    name = reinterpret_cast<PFN_##name>(vkGetInstanceProcAddr(instance, #name));                                      \
    all = all && name;
        TISMA_VK_INSTANCE_FUNCTIONS(TISMA_VK_LOAD)
#undef TISMA_VK_LOAD
        return all;
    }

    // False if the device lacks one.
    bool load(VkDevice device) {
        bool all = true;
#define TISMA_VK_LOAD(name)                                                                                            \
    name = reinterpret_cast<PFN_##name>(vkGetDeviceProcAddr(device, #name));                                          \
    all = all && name;
        TISMA_VK_DEVICE_FUNCTIONS(TISMA_VK_LOAD)
#undef TISMA_VK_LOAD
        return all;
    }
};

} // namespace

bool load_vulkan() {
    // Once for the program, from whichever thread comes first (C++11 static
    // initialisation is thread-safe). volkInitialize sets only the loader's
    // global functions (vkGetInstanceProcAddr, vkCreateInstance and the
    // enumerations), which nothing changes afterwards.
    static const bool loaded = volkInitialize() == VK_SUCCESS;
    return loaded;
}

struct Context::Impl {
    Functions vk;
    VkInstance instance = VK_NULL_HANDLE;
    VkPhysicalDevice physical = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkQueue queue = VK_NULL_HANDLE;
    std::uint32_t queue_family = 0;
    VkCommandPool command_pool = VK_NULL_HANDLE;
    VkPhysicalDeviceMemoryProperties memory{};
    std::string name;

    // Host-visible storage buffer, mapped for its whole life. Simple and enough
    // for the first kernels; device-local buffers with staging come when the
    // measurements ask for them.
    struct Buffer {
        Impl* owner = nullptr;
        VkBuffer buffer = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        void* mapped = nullptr;
        VkDeviceSize size = 0;

        Buffer() = default;
        Buffer(const Buffer&) = delete;
        Buffer& operator=(const Buffer&) = delete;
        ~Buffer() {
            if (!owner)
                return;
            if (memory)
                owner->vk.vkFreeMemory(owner->device, memory, nullptr);
            if (buffer)
                owner->vk.vkDestroyBuffer(owner->device, buffer, nullptr);
        }
    };

    ~Impl() {
        if (device && vk.vkDestroyDevice) {
            if (vk.vkDeviceWaitIdle)
                vk.vkDeviceWaitIdle(device);
            if (command_pool && vk.vkDestroyCommandPool)
                vk.vkDestroyCommandPool(device, command_pool, nullptr);
            vk.vkDestroyDevice(device, nullptr);
        }
        if (instance && vk.vkDestroyInstance)
            vk.vkDestroyInstance(instance, nullptr);
    }

    void make_buffer(Buffer& b, VkDeviceSize size) {
        b.owner = this;
        b.size = std::max<VkDeviceSize>(size, 4); // zero-sized buffers are not allowed
        VkBufferCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        info.size = b.size;
        info.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        check(vk.vkCreateBuffer(device, &info, nullptr, &b.buffer), "vkCreateBuffer");
        VkMemoryRequirements req;
        vk.vkGetBufferMemoryRequirements(device, b.buffer, &req);
        const VkMemoryPropertyFlags want = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        std::uint32_t type = UINT32_MAX;
        for (std::uint32_t i = 0; i < memory.memoryTypeCount; ++i)
            if ((req.memoryTypeBits & (1u << i)) && (memory.memoryTypes[i].propertyFlags & want) == want) {
                type = i;
                break;
            }
        if (type == UINT32_MAX)
            throw std::runtime_error("vulkan: no host-visible memory type");
        VkMemoryAllocateInfo alloc{};
        alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        alloc.allocationSize = req.size;
        alloc.memoryTypeIndex = type;
        check(vk.vkAllocateMemory(device, &alloc, nullptr, &b.memory), "vkAllocateMemory");
        check(vk.vkBindBufferMemory(device, b.buffer, b.memory, 0), "vkBindBufferMemory");
        check(vk.vkMapMemory(device, b.memory, 0, VK_WHOLE_SIZE, 0, &b.mapped), "vkMapMemory");
    }

    // Runs one compute shader over `groups` workgroups with the given storage
    // buffers bound at bindings 0..n-1 and the push constants.
    void run(std::span<const std::uint32_t> spirv, std::span<Buffer* const> buffers, const void* push,
             std::uint32_t push_size, std::uint32_t groups) {
        run_steps(spirv, buffers, push, push_size, 1, groups);
    }

    // The same shader `steps` times in one submission, each with its own push
    // constants (push_size bytes each, one after another in `pushes`), every
    // step seeing what the one before wrote.
    void run_steps(std::span<const std::uint32_t> spirv, std::span<Buffer* const> buffers, const void* pushes,
                   std::uint32_t push_size, std::uint32_t steps, std::uint32_t groups) {
        std::vector<VkDescriptorSetLayoutBinding> bindings(buffers.size());
        for (std::uint32_t i = 0; i < bindings.size(); ++i)
            bindings[i] = {i, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr};

        VkDescriptorSetLayout set_layout = VK_NULL_HANDLE;
        VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
        VkShaderModule module = VK_NULL_HANDLE;
        VkPipeline pipeline = VK_NULL_HANDLE;
        VkDescriptorPool pool = VK_NULL_HANDLE;
        VkFence fence = VK_NULL_HANDLE;
        VkCommandBuffer cmd = VK_NULL_HANDLE;
        auto cleanup = [&] {
            if (cmd)
                vk.vkFreeCommandBuffers(device, command_pool, 1, &cmd);
            if (fence)
                vk.vkDestroyFence(device, fence, nullptr);
            if (pool)
                vk.vkDestroyDescriptorPool(device, pool, nullptr);
            if (pipeline)
                vk.vkDestroyPipeline(device, pipeline, nullptr);
            if (module)
                vk.vkDestroyShaderModule(device, module, nullptr);
            if (pipeline_layout)
                vk.vkDestroyPipelineLayout(device, pipeline_layout, nullptr);
            if (set_layout)
                vk.vkDestroyDescriptorSetLayout(device, set_layout, nullptr);
        };
        try {
            VkDescriptorSetLayoutCreateInfo dsl{};
            dsl.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            dsl.bindingCount = static_cast<std::uint32_t>(bindings.size());
            dsl.pBindings = bindings.data();
            check(vk.vkCreateDescriptorSetLayout(device, &dsl, nullptr, &set_layout), "vkCreateDescriptorSetLayout");

            VkPushConstantRange range{VK_SHADER_STAGE_COMPUTE_BIT, 0, push_size};
            VkPipelineLayoutCreateInfo pl{};
            pl.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
            pl.setLayoutCount = 1;
            pl.pSetLayouts = &set_layout;
            pl.pushConstantRangeCount = push_size ? 1 : 0;
            pl.pPushConstantRanges = &range;
            check(vk.vkCreatePipelineLayout(device, &pl, nullptr, &pipeline_layout), "vkCreatePipelineLayout");

            VkShaderModuleCreateInfo sm{};

            sm.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
            sm.codeSize = spirv.size_bytes();
            sm.pCode = spirv.data();
            check(vk.vkCreateShaderModule(device, &sm, nullptr, &module), "vkCreateShaderModule");

            VkComputePipelineCreateInfo cp{};

            cp.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
            cp.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            cp.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
            cp.stage.module = module;
            cp.stage.pName = "main";
            cp.layout = pipeline_layout;
            check(vk.vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &cp, nullptr, &pipeline),
                  "vkCreateComputePipelines");

            VkDescriptorPoolSize pool_size{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, static_cast<std::uint32_t>(buffers.size())};
            VkDescriptorPoolCreateInfo dp{};
            dp.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
            dp.maxSets = 1;
            dp.poolSizeCount = 1;
            dp.pPoolSizes = &pool_size;
            check(vk.vkCreateDescriptorPool(device, &dp, nullptr, &pool), "vkCreateDescriptorPool");

            VkDescriptorSet set = VK_NULL_HANDLE;
            VkDescriptorSetAllocateInfo ds{};
            ds.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
            ds.descriptorPool = pool;
            ds.descriptorSetCount = 1;
            ds.pSetLayouts = &set_layout;
            check(vk.vkAllocateDescriptorSets(device, &ds, &set), "vkAllocateDescriptorSets");

            std::vector<VkDescriptorBufferInfo> infos(buffers.size());
            std::vector<VkWriteDescriptorSet> writes(buffers.size());
            for (std::size_t i = 0; i < buffers.size(); ++i) {
                infos[i] = {buffers[i]->buffer, 0, VK_WHOLE_SIZE};
                writes[i] = {};
                writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                writes[i].dstSet = set;
                writes[i].dstBinding = static_cast<std::uint32_t>(i);
                writes[i].descriptorCount = 1;
                writes[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                writes[i].pBufferInfo = &infos[i];
            }
            vk.vkUpdateDescriptorSets(device, static_cast<std::uint32_t>(writes.size()), writes.data(), 0, nullptr);

            VkCommandBufferAllocateInfo cba{};

            cba.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            cba.commandPool = command_pool;
            cba.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            cba.commandBufferCount = 1;
            check(vk.vkAllocateCommandBuffers(device, &cba, &cmd), "vkAllocateCommandBuffers");

            VkCommandBufferBeginInfo begin{};

            begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
            check(vk.vkBeginCommandBuffer(cmd, &begin), "vkBeginCommandBuffer");
            vk.vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
            vk.vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_layout, 0, 1, &set, 0, nullptr);
            for (std::uint32_t step = 0; step < steps; ++step) {
                if (step > 0) {
                    VkMemoryBarrier between{};
                    between.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
                    between.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
                    between.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
                    vk.vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &between, 0, nullptr, 0,
                                            nullptr);
                }
                if (push_size)
                    vk.vkCmdPushConstants(cmd, pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, push_size,
                                          static_cast<const std::byte*>(pushes) + std::size_t(step) * push_size);
                vk.vkCmdDispatch(cmd, groups, 1, 1);
            }
            // Make the shader writes visible to the host before it reads the mapped memory.
            VkMemoryBarrier barrier{};
            barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
            barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
            vk.vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 1, &barrier, 0,
                                 nullptr, 0, nullptr);
            check(vk.vkEndCommandBuffer(cmd), "vkEndCommandBuffer");

            VkFenceCreateInfo fi{};

            fi.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
            check(vk.vkCreateFence(device, &fi, nullptr, &fence), "vkCreateFence");
            VkSubmitInfo submit{};
            submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            submit.commandBufferCount = 1;
            submit.pCommandBuffers = &cmd;
            check(vk.vkQueueSubmit(queue, 1, &submit, fence), "vkQueueSubmit");
            check(vk.vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX), "vkWaitForFences");
        } catch (...) {
            cleanup();
            throw;
        }
        cleanup();
    }
};

std::unique_ptr<Context> Context::create(const ContextOptions& options) {
    if (const char* env = std::getenv("TISMA_GPU"); env && std::strcmp(env, "0") == 0)
        return nullptr;
    if (!load_vulkan())
        return nullptr; // no Vulkan loader on this machine
    auto impl = std::make_unique<Impl>();
    try {
        VkApplicationInfo app{};
        app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        app.pApplicationName = "tisma-engine";
        app.apiVersion = VK_API_VERSION_1_1;
        VkInstanceCreateInfo ici{};
        ici.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        ici.pApplicationInfo = &app;
        if (vkCreateInstance(&ici, nullptr, &impl->instance) != VK_SUCCESS)
            return nullptr;
        if (!impl->vk.load(impl->instance))
            return nullptr;

        std::uint32_t count = 0;
        impl->vk.vkEnumeratePhysicalDevices(impl->instance, &count, nullptr);
        std::vector<VkPhysicalDevice> devices(count);
        impl->vk.vkEnumeratePhysicalDevices(impl->instance, &count, devices.data());
        int best_rank = 0;
        for (VkPhysicalDevice d : devices) {
            VkPhysicalDeviceProperties props;
            impl->vk.vkGetPhysicalDeviceProperties(d, &props);
            if (props.apiVersion < VK_API_VERSION_1_1)
                continue;
            const int rank = device_rank(props.deviceType);
            if (rank == 0 || (rank == 1 && !options.allow_cpu_device) || rank <= best_rank)
                continue;
            std::uint32_t families = 0;
            impl->vk.vkGetPhysicalDeviceQueueFamilyProperties(d, &families, nullptr);
            std::vector<VkQueueFamilyProperties> fam(families);
            impl->vk.vkGetPhysicalDeviceQueueFamilyProperties(d, &families, fam.data());
            for (std::uint32_t f = 0; f < families; ++f)
                if (fam[f].queueFlags & VK_QUEUE_COMPUTE_BIT) {
                    best_rank = rank;
                    impl->physical = d;
                    impl->queue_family = f;
                    impl->name = props.deviceName;
                    break;
                }
        }
        if (!impl->physical)
            return nullptr;
        impl->vk.vkGetPhysicalDeviceMemoryProperties(impl->physical, &impl->memory);

        const float priority = 1.0f;
        VkDeviceQueueCreateInfo qci{};
        qci.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        qci.queueFamilyIndex = impl->queue_family;
        qci.queueCount = 1;
        qci.pQueuePriorities = &priority;
        VkDeviceCreateInfo dci{};
        dci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        dci.queueCreateInfoCount = 1;
        dci.pQueueCreateInfos = &qci;
        if (impl->vk.vkCreateDevice(impl->physical, &dci, nullptr, &impl->device) != VK_SUCCESS)
            return nullptr;
        if (!impl->vk.load(impl->device))
            return nullptr;
        impl->vk.vkGetDeviceQueue(impl->device, impl->queue_family, 0, &impl->queue);

        VkCommandPoolCreateInfo cpi{};

        cpi.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        cpi.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        cpi.queueFamilyIndex = impl->queue_family;
        if (impl->vk.vkCreateCommandPool(impl->device, &cpi, nullptr, &impl->command_pool) != VK_SUCCESS)
            return nullptr;
    } catch (...) {
        return nullptr;
    }
    return std::unique_ptr<Context>(new Context(std::move(impl)));
}

Context::Context(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}
Context::~Context() = default;

std::string Context::device_name() const { return impl_->name; }

void Context::triangle_level_ranges(const IndexedMesh& mesh, std::span<const float> values, std::span<const float> isos,
                                    std::vector<std::uint32_t>& first, std::vector<std::uint32_t>& end) {
    const std::size_t n = mesh.triangles.size();
    first.assign(n, 0);
    end.assign(n, 0);
    if (n == 0 || isos.empty())
        return; // every range is empty

    Impl::Buffer b_values, b_tris, b_isos, b_first, b_end;
    impl_->make_buffer(b_values, values.size_bytes());
    impl_->make_buffer(b_tris, n * 3 * sizeof(std::uint32_t));
    impl_->make_buffer(b_isos, isos.size_bytes());
    impl_->make_buffer(b_first, n * sizeof(std::uint32_t));
    impl_->make_buffer(b_end, n * sizeof(std::uint32_t));
    std::memcpy(b_values.mapped, values.data(), values.size_bytes());
    std::memcpy(b_tris.mapped, mesh.triangles.data(), n * 3 * sizeof(std::uint32_t)); // std::array is contiguous
    std::memcpy(b_isos.mapped, isos.data(), isos.size_bytes());

    const std::array<std::uint32_t, 2> push = {static_cast<std::uint32_t>(n), static_cast<std::uint32_t>(isos.size())};
    const std::array<Impl::Buffer*, 5> bound = {&b_values, &b_tris, &b_isos, &b_first, &b_end};
    const auto groups = static_cast<std::uint32_t>((n + 255) / 256);
    impl_->run(kLevelRangesSpirv, bound, push.data(), sizeof(push), groups);

    std::memcpy(first.data(), b_first.mapped, n * sizeof(std::uint32_t));
    std::memcpy(end.data(), b_end.mapped, n * sizeof(std::uint32_t));
}

// Layout of Segment in level_segments.comp (std430, 4-byte members).
struct GpuSegment {
    float from_x, from_y, from_z;
    float to_x, to_y, to_z;
    std::uint32_t edge_from_hi, edge_from_lo;
    std::uint32_t edge_to_hi, edge_to_lo;
    std::uint32_t triangle;
};
static_assert(sizeof(GpuSegment) == 44, "must match the std430 layout of the shader");

void Context::level_segments_by_triangle(const IndexedMesh& mesh, std::span<const float> values,
                                         std::span<const float> isos, std::span<const std::uint32_t> first,
                                         std::span<const std::uint32_t> end, std::vector<std::uint32_t>& offsets,
                                         std::vector<LevelSegment>& out) {
    const std::size_t n = mesh.triangles.size();
    // Exclusive prefix sum of the segment counts: where each triangle writes.
    offsets.assign(n + 1, 0);
    for (std::size_t t = 0; t < n; ++t)
        offsets[t + 1] = offsets[t] + (end[t] - first[t]);
    const std::size_t total = offsets[n];
    out.clear();
    if (total == 0)
        return;

    static_assert(sizeof(Vec3f) == 12, "vertices are copied to the GPU as packed float triples");
    Impl::Buffer b_vertices, b_values, b_tris, b_isos, b_first, b_end, b_offsets, b_out;
    impl_->make_buffer(b_vertices, mesh.vertices.size() * sizeof(Vec3f));
    impl_->make_buffer(b_values, values.size_bytes());
    impl_->make_buffer(b_tris, n * 3 * sizeof(std::uint32_t));
    impl_->make_buffer(b_isos, isos.size_bytes());
    impl_->make_buffer(b_first, first.size_bytes());
    impl_->make_buffer(b_end, end.size_bytes());
    impl_->make_buffer(b_offsets, offsets.size() * sizeof(std::uint32_t));
    impl_->make_buffer(b_out, total * sizeof(GpuSegment));
    std::memcpy(b_vertices.mapped, mesh.vertices.data(), mesh.vertices.size() * sizeof(Vec3f));
    std::memcpy(b_values.mapped, values.data(), values.size_bytes());
    std::memcpy(b_tris.mapped, mesh.triangles.data(), n * 3 * sizeof(std::uint32_t));
    std::memcpy(b_isos.mapped, isos.data(), isos.size_bytes());
    std::memcpy(b_first.mapped, first.data(), first.size_bytes());
    std::memcpy(b_end.mapped, end.data(), end.size_bytes());
    std::memcpy(b_offsets.mapped, offsets.data(), offsets.size() * sizeof(std::uint32_t));

    const std::uint32_t push = static_cast<std::uint32_t>(n);
    const std::array<Impl::Buffer*, 8> bound = {&b_vertices, &b_values, &b_tris,    &b_isos,
                                                &b_first,    &b_end,    &b_offsets, &b_out};
    impl_->run(kLevelSegmentsSpirv, bound, &push, sizeof(push), static_cast<std::uint32_t>((n + 255) / 256));

    const auto* gpu = static_cast<const GpuSegment*>(b_out.mapped);
    out.resize(total);
    for (std::size_t i = 0; i < total; ++i) {
        const GpuSegment& g = gpu[i];
        out[i] = {{g.from_x, g.from_y, g.from_z},
                  {g.to_x, g.to_y, g.to_z},
                  (EdgeKey(g.edge_from_hi) << 32) | g.edge_from_lo,
                  (EdgeKey(g.edge_to_hi) << 32) | g.edge_to_lo,
                  g.triangle};
    }
}

void Context::support_column(BitGrid& column, std::span<const BitGrid> parts, std::span<const BitGrid> adds,
                             std::span<BitGrid> kept, int radius) {
    const std::size_t count = parts.size();
    if (count == 0 || column.width() == 0 || column.height() == 0)
        return;
    if (radius < 0 || radius > 31)
        throw std::runtime_error("support_column: the radius must be 0 to 31 pixels");
    const std::size_t grid = column.words().size(); // 64-bit words
    const std::size_t bytes = grid * sizeof(std::uint64_t);
    Impl::Buffer b_parts, b_adds, b_column, b_temp, b_kept;
    impl_->make_buffer(b_parts, count * bytes);
    impl_->make_buffer(b_adds, count * bytes);
    impl_->make_buffer(b_column, bytes);
    impl_->make_buffer(b_temp, bytes);
    impl_->make_buffer(b_kept, count * bytes);
    std::memset(b_adds.mapped, 0, count * bytes);
    for (std::size_t k = 0; k < count; ++k) {
        std::memcpy(static_cast<std::byte*>(b_parts.mapped) + k * bytes, parts[k].words().data(), bytes);
        if (adds[k].width() > 0)
            std::memcpy(static_cast<std::byte*>(b_adds.mapped) + k * bytes, adds[k].words().data(), bytes);
    }
    std::memcpy(b_column.mapped, column.words().data(), bytes);

    // Push constants of support_column.comp, 32-bit words on the GPU.
    struct Push {
        std::uint32_t width, height, stride, radius, mode, layer;
    };
    const auto stride = static_cast<std::uint32_t>(2 * column.words_per_row());
    const auto words = static_cast<std::uint32_t>(2 * grid);
    std::vector<Push> steps;
    for (std::size_t k = count; k-- > 0;) {
        const Push base{static_cast<std::uint32_t>(column.width()), static_cast<std::uint32_t>(column.height()),
                        stride, static_cast<std::uint32_t>(radius), 2u, static_cast<std::uint32_t>(k) * words};
        if (adds[k].width() > 0) {
            Push dilate = base, erode = base;
            dilate.mode = 0u, erode.mode = 1u;
            steps.push_back(dilate);
            steps.push_back(erode);
        } else {
            steps.push_back(base);
        }
    }
    const std::array<Impl::Buffer*, 5> bound = {&b_parts, &b_adds, &b_column, &b_temp, &b_kept};
    impl_->run_steps(kSupportColumnSpirv, bound, steps.data(), sizeof(Push), static_cast<std::uint32_t>(steps.size()),
                     (words + 255u) / 256u);

    std::memcpy(column.words().data(), b_column.mapped, bytes);
    for (std::size_t k = 0; k < count; ++k) {
        kept[k] = BitGrid(column.x0(), column.y0(), column.pixel(), column.width(), column.height());
        std::memcpy(kept[k].words().data(), static_cast<const std::byte*>(b_kept.mapped) + k * bytes, bytes);
    }
}

} // namespace tisma::gpu
