// The 3D view drawn with Vulkan, written from the Vulkan specification
// (Khronos). Own design; see PROVENANCE.md.

#include "vk_view.hpp"

#ifdef TISMA_VULKAN_WINDOW

#include "vk_device.hpp"

#include "shaders/generated/background.frag.spv.h"
#include "shaders/generated/composite.frag.spv.h"
#include "shaders/generated/grid.frag.spv.h"
#include "shaders/generated/line.frag.spv.h"
#include "shaders/generated/line.vert.spv.h"
#include "shaders/generated/mesh.frag.spv.h"
#include "shaders/generated/mesh.vert.spv.h"
#include "shaders/generated/plate.frag.spv.h"
#include "shaders/generated/plate.vert.spv.h"
#include "shaders/generated/preview.frag.spv.h"
#include "shaders/generated/preview.vert.spv.h"
#include "shaders/generated/screen.vert.spv.h"

#include <imgui_impl_vulkan.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <functional>
#include <stdexcept>

namespace tisma::ui {
namespace {

void check(VkResult r, const char* what) {
    if (r != VK_SUCCESS)
        throw std::runtime_error(std::string("vulkan view: ") + what + " failed (" + std::to_string(int(r)) + ")");
}

// The frame's values, as view_common.glsl's Frame block (std140).
struct FrameBlock {
    float view_projection[16];
    float view[16];
    float eye[4], key[4], fill[4];
    float sky[4], ground[4];
    float target[4];
    std::int32_t flags[4];
    float grid[4];
    float colours[16][4];
};

// Each draw's values, as view_common.glsl's Draw push constants.
struct DrawBlock {
    float colour[4] = {};
    float extra[4] = {};
    std::uint32_t preview[4] = {};
    float range[4] = {};
};

struct CompositeBlock {
    float size[2];
    std::int32_t fxaa;
    std::int32_t pad;
};

struct Buffer {
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkDeviceSize size = 0;
    void* mapped = nullptr; // a uniform buffer stays mapped
};

struct Image {
    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
};

// The lines from..to of a line list, cut into pieces of at most `piece` mm.
std::vector<LineVertex> split_lines(const LineVertex* v, std::size_t count, float piece) {
    std::vector<LineVertex> out;
    for (std::size_t i = 0; i + 1 < count; i += 2) {
        const LineVertex a = v[i], b = v[i + 1];
        const float dx = b.x - a.x, dy = b.y - a.y, dz = b.z - a.z;
        const int n = std::max(1, int(std::ceil(std::sqrt(dx * dx + dy * dy + dz * dz) / piece)));
        for (int k = 0; k < n; ++k) {
            const float t0 = float(k) / float(n), t1 = float(k + 1) / float(n);
            out.push_back({a.x + dx * t0, a.y + dy * t0, a.z + dz * t0, a.argb});
            out.push_back({a.x + dx * t1, a.y + dy * t1, a.z + dz * t1, b.argb});
        }
    }
    return out;
}

void copy(float* to, const Mat4& m) { std::memcpy(to, &m[0], 16 * sizeof(float)); }
void copy(float* to, Vec3 v, float w = 0.0f) { to[0] = v.x, to[1] = v.y, to[2] = v.z, to[3] = w; }

} // namespace

struct VulkanView::Impl {
    VulkanDevice d;
    bool ready = false;

    VkDescriptorSetLayout frame_layout = VK_NULL_HANDLE, composite_layout = VK_NULL_HANDLE;
    VkPipelineLayout scene_layout = VK_NULL_HANDLE, composite_layout_pipe = VK_NULL_HANDLE;
    VkDescriptorPool pool = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> frame_sets; // one per frame in flight
    VkDescriptorSet composite_set = VK_NULL_HANDLE;
    std::vector<Buffer> uniforms;             // one per frame in flight
    VkSampler sampler = VK_NULL_HANDLE;
    Image white; // the visibility without occlusion: all the light arrives

    std::array<VkShaderModule, 12> modules{};

    // The targets, at the view's size and samples.
    int width = 0, height = 0;
    VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT;
    Image colour_ms, depth, resolved, picture;
    VkRenderPass scene_pass = VK_NULL_HANDLE, composite_pass = VK_NULL_HANDLE;
    VkFramebuffer scene_fb = VK_NULL_HANDLE, composite_fb = VK_NULL_HANDLE;
    VkDescriptorSet picture_set = VK_NULL_HANDLE; // ImGui's texture, the same through resizes

    enum Pipe { Background, Plate, Grid, Outline, Mesh, Paint, Preview, Travels, Composite, kPipes };
    std::array<VkPipeline, kPipes> pipes{};

    // What is drawn.
    Buffer plate_v, grid_v, model_v, model_coarse_v, paint_v, extrusions_v, travels_v;
    std::uint32_t plate_count = 0, grid_count = 0, grid_outline = 0, model_count = 0, model_coarse_count = 0, paint_count = 0,
                  travel_count = 0;
    float grid_step = 10.0f;
    int grid_major_every = 5;
    std::vector<Renderer::PaintGroup> paint_groups;
    PreviewDrawOrder order; // its instances on the GPU
    std::vector<std::size_t> layer_extrusions, layer_travels;
    float min_feed = 0, max_feed = 0, min_width = 0, max_width = 0, min_height = 0, max_height = 0;
    std::size_t drawn = 0;

    // The view's time: two timestamps per frame in flight.
    VkQueryPool timer = VK_NULL_HANDLE;
    std::uint64_t timer_mask = 0;
    std::vector<char> timer_out; // a frame's pair is written, not read yet
    double time_ms = 0.0;
    bool time_fresh = false;

    std::uint32_t memory_type(std::uint32_t bits, VkMemoryPropertyFlags flags) const {
        for (std::uint32_t i = 0; i < d.memory.memoryTypeCount; ++i)
            if ((bits & (1u << i)) && (d.memory.memoryTypes[i].propertyFlags & flags) == flags)
                return i;
        throw std::runtime_error("vulkan view: no memory type");
    }

    VkDeviceMemory allocate(VkMemoryRequirements need, VkMemoryPropertyFlags flags) {
        VkMemoryAllocateInfo mai{};
        mai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        mai.allocationSize = need.size;
        mai.memoryTypeIndex = memory_type(need.memoryTypeBits, flags);
        VkDeviceMemory m = VK_NULL_HANDLE;
        check(vkAllocateMemory(d.device, &mai, nullptr, &m), "vkAllocateMemory");
        return m;
    }

    Buffer make_buffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags flags) {
        Buffer b;
        b.size = size;
        VkBufferCreateInfo bci{};
        bci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bci.size = std::max<VkDeviceSize>(size, 16);
        bci.usage = usage;
        bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        check(vkCreateBuffer(d.device, &bci, nullptr, &b.buffer), "vkCreateBuffer");
        VkMemoryRequirements need{};
        vkGetBufferMemoryRequirements(d.device, b.buffer, &need);
        b.memory = allocate(need, flags);
        check(vkBindBufferMemory(d.device, b.buffer, b.memory, 0), "vkBindBufferMemory");
        return b;
    }

    // Runs `work`; false if it failed, told on stderr.
    static bool guard(const std::function<void()>& work) {
        try {
            work();
            return true;
        } catch (const std::exception& e) {
            std::fprintf(stderr, "%s\n", e.what());
            return false;
        }
    }

    void free_buffer(Buffer& b) {
        if (b.mapped)
            vkUnmapMemory(d.device, b.memory);
        if (b.buffer)
            vkDestroyBuffer(d.device, b.buffer, nullptr);
        if (b.memory)
            vkFreeMemory(d.device, b.memory, nullptr);
        b = {};
    }

    void one_time(const std::function<void(VkCommandBuffer)>& record) {
        VkCommandBufferAllocateInfo cai{};
        cai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        cai.commandPool = d.pool;
        cai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        cai.commandBufferCount = 1;
        VkCommandBuffer c = VK_NULL_HANDLE;
        check(vkAllocateCommandBuffers(d.device, &cai, &c), "vkAllocateCommandBuffers");
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
        check(vkQueueSubmit(d.queue, 1, &si, VK_NULL_HANDLE), "vkQueueSubmit");
        vkQueueWaitIdle(d.queue);
        vkFreeCommandBuffers(d.device, d.pool, 1, &c);
    }

    // Vertex or instance data into device memory, through a staging buffer.
    // The old buffer may still be read by frames in flight: wait for them.
    void upload(Buffer& to, const void* data, std::size_t bytes) {
        vkDeviceWaitIdle(d.device);
        free_buffer(to);
        if (bytes == 0)
            return;
        Buffer staging = make_buffer(bytes, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        void* mapped = nullptr;
        check(vkMapMemory(d.device, staging.memory, 0, bytes, 0, &mapped), "vkMapMemory");
        std::memcpy(mapped, data, bytes);
        vkUnmapMemory(d.device, staging.memory);
        to = make_buffer(bytes, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        one_time([&](VkCommandBuffer c) {
            VkBufferCopy region{0, 0, bytes};
            vkCmdCopyBuffer(c, staging.buffer, to.buffer, 1, &region);
        });
        free_buffer(staging);
    }

    Image make_image(int w, int h, VkFormat format, VkImageUsageFlags usage, VkSampleCountFlagBits count,
                     VkImageAspectFlags aspect) {
        Image img;
        VkImageCreateInfo ici{};
        ici.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        ici.imageType = VK_IMAGE_TYPE_2D;
        ici.format = format;
        ici.extent = {std::uint32_t(w), std::uint32_t(h), 1};
        ici.mipLevels = 1;
        ici.arrayLayers = 1;
        ici.samples = count;
        ici.tiling = VK_IMAGE_TILING_OPTIMAL;
        ici.usage = usage;
        ici.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        ici.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        check(vkCreateImage(d.device, &ici, nullptr, &img.image), "vkCreateImage");
        VkMemoryRequirements need{};
        vkGetImageMemoryRequirements(d.device, img.image, &need);
        img.memory = allocate(need, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        check(vkBindImageMemory(d.device, img.image, img.memory, 0), "vkBindImageMemory");
        VkImageViewCreateInfo vci{};
        vci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        vci.image = img.image;
        vci.viewType = VK_IMAGE_VIEW_TYPE_2D;
        vci.format = format;
        vci.subresourceRange = {aspect, 0, 1, 0, 1};
        check(vkCreateImageView(d.device, &vci, nullptr, &img.view), "vkCreateImageView");
        return img;
    }

    void free_image(Image& img) {
        if (img.view)
            vkDestroyImageView(d.device, img.view, nullptr);
        if (img.image)
            vkDestroyImage(d.device, img.image, nullptr);
        if (img.memory)
            vkFreeMemory(d.device, img.memory, nullptr);
        img = {};
    }

    VkShaderModule module(const std::uint32_t* code, std::size_t bytes) {
        VkShaderModuleCreateInfo smi{};
        smi.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        smi.codeSize = bytes;
        smi.pCode = code;
        VkShaderModule m = VK_NULL_HANDLE;
        check(vkCreateShaderModule(d.device, &smi, nullptr, &m), "vkCreateShaderModule");
        return m;
    }

    void init() {
        // The shaders, in the order of the Pipe stages below.
        const std::array<std::pair<const std::uint32_t*, std::size_t>, 12> code = {{
            {kScreenVertSpirv, sizeof kScreenVertSpirv},       {kBackgroundFragSpirv, sizeof kBackgroundFragSpirv},
            {kPlateVertSpirv, sizeof kPlateVertSpirv},         {kPlateFragSpirv, sizeof kPlateFragSpirv},
            {kLineVertSpirv, sizeof kLineVertSpirv},           {kGridFragSpirv, sizeof kGridFragSpirv},
            {kLineFragSpirv, sizeof kLineFragSpirv},           {kMeshVertSpirv, sizeof kMeshVertSpirv},
            {kMeshFragSpirv, sizeof kMeshFragSpirv},           {kPreviewVertSpirv, sizeof kPreviewVertSpirv},
            {kPreviewFragSpirv, sizeof kPreviewFragSpirv},     {kCompositeFragSpirv, sizeof kCompositeFragSpirv},
        }};
        for (std::size_t i = 0; i < code.size(); ++i)
            modules[i] = module(code[i].first, code[i].second);

        // Set 0: the frame's block and the visibility.
        VkDescriptorSetLayoutBinding frame_bindings[2] = {};
        frame_bindings[0].binding = 0;
        frame_bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        frame_bindings[0].descriptorCount = 1;
        frame_bindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        frame_bindings[1].binding = 1;
        frame_bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        frame_bindings[1].descriptorCount = 1;
        frame_bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        VkDescriptorSetLayoutCreateInfo dli{};
        dli.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        dli.bindingCount = 2;
        dli.pBindings = frame_bindings;
        check(vkCreateDescriptorSetLayout(d.device, &dli, nullptr, &frame_layout), "vkCreateDescriptorSetLayout");
        VkDescriptorSetLayoutBinding source{};
        source.binding = 0;
        source.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        source.descriptorCount = 1;
        source.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        dli.bindingCount = 1;
        dli.pBindings = &source;
        check(vkCreateDescriptorSetLayout(d.device, &dli, nullptr, &composite_layout), "vkCreateDescriptorSetLayout");

        VkPushConstantRange draw_range{VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(DrawBlock)};
        VkPipelineLayoutCreateInfo pli{};
        pli.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pli.setLayoutCount = 1;
        pli.pSetLayouts = &frame_layout;
        pli.pushConstantRangeCount = 1;
        pli.pPushConstantRanges = &draw_range;
        check(vkCreatePipelineLayout(d.device, &pli, nullptr, &scene_layout), "vkCreatePipelineLayout");
        VkPushConstantRange composite_range{VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(CompositeBlock)};
        pli.pSetLayouts = &composite_layout;
        pli.pPushConstantRanges = &composite_range;
        check(vkCreatePipelineLayout(d.device, &pli, nullptr, &composite_layout_pipe), "vkCreatePipelineLayout");

        VkSamplerCreateInfo sci{};
        sci.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        sci.magFilter = sci.minFilter = VK_FILTER_LINEAR;
        sci.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        sci.addressModeU = sci.addressModeV = sci.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sci.maxLod = 0.0f;
        check(vkCreateSampler(d.device, &sci, nullptr, &sampler), "vkCreateSampler");

        // White: all the light arrives.
        white = make_image(1, 1, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                           VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
        one_time([&](VkCommandBuffer c) {
            VkImageMemoryBarrier b{};
            b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            b.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            b.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            b.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            b.srcQueueFamilyIndex = b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            b.image = white.image;
            b.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
            vkCmdPipelineBarrier(c, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr,
                                 0, nullptr, 1, &b);
            VkClearColorValue one{};
            one.float32[0] = one.float32[1] = one.float32[2] = one.float32[3] = 1.0f;
            vkCmdClearColorImage(c, white.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &one, 1, &b.subresourceRange);
            b.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            b.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            b.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            b.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            vkCmdPipelineBarrier(c, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0,
                                 nullptr, 0, nullptr, 1, &b);
        });

        // The frames' uniform blocks, mapped, and their sets.
        const std::uint32_t frames = std::uint32_t(std::max(d.frames, 1));
        VkDescriptorPoolSize sizes[2] = {{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, frames},
                                         {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, frames + 1}};
        VkDescriptorPoolCreateInfo dpi{};
        dpi.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        dpi.maxSets = frames + 1;
        dpi.poolSizeCount = 2;
        dpi.pPoolSizes = sizes;
        check(vkCreateDescriptorPool(d.device, &dpi, nullptr, &pool), "vkCreateDescriptorPool");
        std::vector<VkDescriptorSetLayout> layouts(frames, frame_layout);
        VkDescriptorSetAllocateInfo dai{};
        dai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        dai.descriptorPool = pool;
        dai.descriptorSetCount = frames;
        dai.pSetLayouts = layouts.data();
        frame_sets.resize(frames);
        check(vkAllocateDescriptorSets(d.device, &dai, frame_sets.data()), "vkAllocateDescriptorSets");
        dai.descriptorSetCount = 1;
        dai.pSetLayouts = &composite_layout;
        check(vkAllocateDescriptorSets(d.device, &dai, &composite_set), "vkAllocateDescriptorSets");
        for (std::uint32_t i = 0; i < frames; ++i) {
            Buffer u = make_buffer(sizeof(FrameBlock), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
            check(vkMapMemory(d.device, u.memory, 0, sizeof(FrameBlock), 0, &u.mapped), "vkMapMemory");
            uniforms.push_back(u);
            VkDescriptorBufferInfo bi{u.buffer, 0, sizeof(FrameBlock)};
            VkDescriptorImageInfo ii{sampler, white.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
            VkWriteDescriptorSet w[2] = {};
            w[0].sType = w[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            w[0].dstSet = w[1].dstSet = frame_sets[i];
            w[0].dstBinding = 0;
            w[0].descriptorCount = 1;
            w[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            w[0].pBufferInfo = &bi;
            w[1].dstBinding = 1;
            w[1].descriptorCount = 1;
            w[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            w[1].pImageInfo = &ii;
            vkUpdateDescriptorSets(d.device, 2, w, 0, nullptr);
        }

        // The timer, where the queue writes timestamps.
        std::uint32_t families = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(d.physical, &families, nullptr);
        std::vector<VkQueueFamilyProperties> family(families);
        vkGetPhysicalDeviceQueueFamilyProperties(d.physical, &families, family.data());
        const std::uint32_t bits = d.family < families ? family[d.family].timestampValidBits : 0;
        if (bits > 0 && d.limits.timestampPeriod > 0.0f) {
            VkQueryPoolCreateInfo qpi{};
            qpi.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
            qpi.queryType = VK_QUERY_TYPE_TIMESTAMP;
            qpi.queryCount = 2 * frames;
            check(vkCreateQueryPool(d.device, &qpi, nullptr, &timer), "vkCreateQueryPool");
            timer_mask = bits >= 64 ? ~std::uint64_t(0) : (std::uint64_t(1) << bits) - 1;
            timer_out.assign(frames, 0);
        }
        ready = true;
    }

    void free_targets() {
        for (VkPipeline& p : pipes)
            if (p) {
                vkDestroyPipeline(d.device, p, nullptr);
                p = VK_NULL_HANDLE;
            }
        for (VkFramebuffer* f : {&scene_fb, &composite_fb})
            if (*f) {
                vkDestroyFramebuffer(d.device, *f, nullptr);
                *f = VK_NULL_HANDLE;
            }
        for (VkRenderPass* r : {&scene_pass, &composite_pass})
            if (*r) {
                vkDestroyRenderPass(d.device, *r, nullptr);
                *r = VK_NULL_HANDLE;
            }
        for (Image* img : {&colour_ms, &depth, &resolved, &picture})
            free_image(*img);
        width = height = 0;
    }

    VkPipeline make_pipeline(Pipe which) {
        VkPipelineShaderStageCreateInfo stages[2] = {};
        for (auto& s : stages) {
            s.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            s.pName = "main";
        }
        stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
        stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        // modules: 0 screen.vert, 1 background.frag, 2 plate.vert, 3 plate.frag, 4 line.vert, 5 grid.frag,
        // 6 line.frag, 7 mesh.vert, 8 mesh.frag, 9 preview.vert, 10 preview.frag, 11 composite.frag
        const std::array<std::array<int, 2>, kPipes> shader = {{
            {0, 1}, {2, 3}, {4, 5}, {4, 6}, {7, 8}, {7, 8}, {9, 10}, {4, 6}, {0, 11},
        }};
        stages[0].module = modules[std::size_t(shader[which][0])];
        stages[1].module = modules[std::size_t(shader[which][1])];

        std::vector<VkVertexInputBindingDescription> bindings;
        std::vector<VkVertexInputAttributeDescription> attributes;
        VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        switch (which) {
        case Plate:
            bindings.push_back({0, 3 * sizeof(float), VK_VERTEX_INPUT_RATE_VERTEX});
            attributes.push_back({0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0});
            break;
        case Grid:
        case Outline:
        case Travels:
            bindings.push_back({0, sizeof(LineVertex), VK_VERTEX_INPUT_RATE_VERTEX});
            attributes.push_back({0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0});
            attributes.push_back({1, 0, VK_FORMAT_R8G8B8A8_UNORM, offsetof(LineVertex, argb)});
            topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
            break;
        case Mesh:
        case Paint:
            bindings.push_back({0, 6 * sizeof(float), VK_VERTEX_INPUT_RATE_VERTEX});
            attributes.push_back({0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0});
            attributes.push_back({1, 0, VK_FORMAT_R32G32B32_SFLOAT, 3 * sizeof(float)});
            break;
        case Preview:
            bindings.push_back({0, sizeof(PreviewInstance), VK_VERTEX_INPUT_RATE_INSTANCE});
            attributes.push_back({0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(PreviewInstance, x0)});
            attributes.push_back({1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(PreviewInstance, x1)});
            attributes.push_back({2, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(PreviewInstance, width)});
            attributes.push_back({3, 0, VK_FORMAT_R32_SFLOAT, offsetof(PreviewInstance, feed)});
            attributes.push_back({4, 0, VK_FORMAT_R32_UINT, offsetof(PreviewInstance, kind_layer)});
            attributes.push_back({5, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(PreviewInstance, join0_x)});
            break;
        default:
            break;
        }
        VkPipelineVertexInputStateCreateInfo input{};
        input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        input.vertexBindingDescriptionCount = std::uint32_t(bindings.size());
        input.pVertexBindingDescriptions = bindings.data();
        input.vertexAttributeDescriptionCount = std::uint32_t(attributes.size());
        input.pVertexAttributeDescriptions = attributes.data();
        VkPipelineInputAssemblyStateCreateInfo assembly{};
        assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        assembly.topology = topology;
        VkPipelineViewportStateCreateInfo viewport{};
        viewport.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewport.viewportCount = viewport.scissorCount = 1;
        VkPipelineRasterizationStateCreateInfo raster{};
        raster.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        raster.polygonMode = VK_POLYGON_MODE_FILL;
        raster.cullMode = VK_CULL_MODE_NONE;
        raster.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        raster.lineWidth = 1.0f;
        if (which == Plate) { // pushed back, so the grid and the first layer draw over it
            raster.depthBiasEnable = VK_TRUE;
            raster.depthBiasConstantFactor = 1.0f;
            raster.depthBiasSlopeFactor = 1.0f;
        } else if (which == Paint) { // pulled forward over its own faces
            raster.depthBiasEnable = VK_TRUE;
            raster.depthBiasConstantFactor = -4.0f;
            raster.depthBiasSlopeFactor = -1.0f;
        }
        VkPipelineMultisampleStateCreateInfo multisample{};
        multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisample.rasterizationSamples = which == Composite ? VK_SAMPLE_COUNT_1_BIT : samples;
        VkPipelineDepthStencilStateCreateInfo depth_state{};
        depth_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        const bool scene = which != Background && which != Composite;
        depth_state.depthTestEnable = scene ? VK_TRUE : VK_FALSE;
        depth_state.depthWriteEnable = scene && which != Grid && which != Outline ? VK_TRUE : VK_FALSE;
        depth_state.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
        VkPipelineColorBlendAttachmentState blend{};
        blend.colorWriteMask = 0xF;
        if (which == Grid || which == Outline) {
            blend.blendEnable = VK_TRUE;
            blend.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
            blend.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            blend.colorBlendOp = VK_BLEND_OP_ADD;
            blend.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
            blend.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            blend.alphaBlendOp = VK_BLEND_OP_ADD;
        }
        VkPipelineColorBlendStateCreateInfo blending{};
        blending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        blending.attachmentCount = 1;
        blending.pAttachments = &blend;
        const VkDynamicState dynamic_states[2] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
        VkPipelineDynamicStateCreateInfo dynamic{};
        dynamic.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamic.dynamicStateCount = 2;
        dynamic.pDynamicStates = dynamic_states;
        VkGraphicsPipelineCreateInfo gpi{};
        gpi.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        gpi.stageCount = 2;
        gpi.pStages = stages;
        gpi.pVertexInputState = &input;
        gpi.pInputAssemblyState = &assembly;
        gpi.pViewportState = &viewport;
        gpi.pRasterizationState = &raster;
        gpi.pMultisampleState = &multisample;
        gpi.pDepthStencilState = &depth_state;
        gpi.pColorBlendState = &blending;
        gpi.pDynamicState = &dynamic;
        gpi.layout = which == Composite ? composite_layout_pipe : scene_layout;
        gpi.renderPass = which == Composite ? composite_pass : scene_pass;
        VkPipeline p = VK_NULL_HANDLE;
        check(vkCreateGraphicsPipelines(d.device, VK_NULL_HANDLE, 1, &gpi, nullptr, &p), "vkCreateGraphicsPipelines");
        return p;
    }

    // The targets at w x h with `count` samples, the passes and the
    // pipelines for them; ImGui's texture then shows the new picture.
    void make_targets(int w, int h, VkSampleCountFlagBits count) {
        vkDeviceWaitIdle(d.device);
        free_targets();
        width = w, height = h, samples = count;
        const VkFormat colour_format = VK_FORMAT_R8G8B8A8_UNORM, depth_format = VK_FORMAT_D32_SFLOAT;
        const bool ms = count != VK_SAMPLE_COUNT_1_BIT;
        if (ms)
            colour_ms = make_image(w, h, colour_format, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, count,
                                   VK_IMAGE_ASPECT_COLOR_BIT);
        depth = make_image(w, h, depth_format, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, count,
                           VK_IMAGE_ASPECT_DEPTH_BIT);
        resolved = make_image(w, h, colour_format, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                              VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
        picture = make_image(w, h, colour_format, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                             VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_ASPECT_COLOR_BIT);

        // The scene: cleared, drawn multisampled, resolved for the composite.
        std::vector<VkAttachmentDescription> attachments;
        VkAttachmentDescription a{};
        a.format = colour_format;
        a.samples = count;
        a.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        a.storeOp = ms ? VK_ATTACHMENT_STORE_OP_DONT_CARE : VK_ATTACHMENT_STORE_OP_STORE;
        a.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        a.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        a.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        a.finalLayout = ms ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        attachments.push_back(a);
        a.format = depth_format;
        a.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        a.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        attachments.push_back(a);
        if (ms) {
            a.format = colour_format;
            a.samples = VK_SAMPLE_COUNT_1_BIT;
            a.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            a.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            a.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            attachments.push_back(a);
        }
        VkAttachmentReference colour_ref{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
        VkAttachmentReference depth_ref{1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
        VkAttachmentReference resolve_ref{2, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
        VkSubpassDescription sub{};
        sub.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        sub.colorAttachmentCount = 1;
        sub.pColorAttachments = &colour_ref;
        sub.pDepthStencilAttachment = &depth_ref;
        sub.pResolveAttachments = ms ? &resolve_ref : nullptr;
        // In: the last frame wrote these images and its composite read the
        // resolved picture. Out: this frame's composite reads it.
        const VkPipelineStageFlags attachment_stages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                                                       VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                                                       VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        const VkAccessFlags attachment_writes =
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        VkSubpassDependency deps[2] = {};
        deps[0].srcSubpass = VK_SUBPASS_EXTERNAL;
        deps[0].dstSubpass = 0;
        deps[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | attachment_stages;
        deps[0].dstStageMask = attachment_stages;
        deps[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT | attachment_writes;
        deps[0].dstAccessMask = attachment_writes | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
        deps[1].srcSubpass = 0;
        deps[1].dstSubpass = VK_SUBPASS_EXTERNAL;
        deps[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        deps[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        deps[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        deps[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        VkRenderPassCreateInfo rpi{};
        rpi.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        rpi.attachmentCount = std::uint32_t(attachments.size());
        rpi.pAttachments = attachments.data();
        rpi.subpassCount = 1;
        rpi.pSubpasses = &sub;
        rpi.dependencyCount = 2;
        rpi.pDependencies = deps;
        check(vkCreateRenderPass(d.device, &rpi, nullptr, &scene_pass), "vkCreateRenderPass");
        std::vector<VkImageView> views = {ms ? colour_ms.view : resolved.view, depth.view};
        if (ms)
            views.push_back(resolved.view);
        VkFramebufferCreateInfo fci{};
        fci.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fci.renderPass = scene_pass;
        fci.attachmentCount = std::uint32_t(views.size());
        fci.pAttachments = views.data();
        fci.width = std::uint32_t(w);
        fci.height = std::uint32_t(h);
        fci.layers = 1;
        check(vkCreateFramebuffer(d.device, &fci, nullptr, &scene_fb), "vkCreateFramebuffer");

        // The composite: the picture ImGui samples.
        VkAttachmentDescription p{};
        p.format = colour_format;
        p.samples = VK_SAMPLE_COUNT_1_BIT;
        p.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        p.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        p.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        p.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        p.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        p.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        VkSubpassDescription csub{};
        csub.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        csub.colorAttachmentCount = 1;
        csub.pColorAttachments = &colour_ref;
        VkSubpassDependency cdeps[2] = {deps[0], deps[1]};
        cdeps[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        cdeps[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        cdeps[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        cdeps[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        rpi.attachmentCount = 1;
        rpi.pAttachments = &p;
        rpi.pSubpasses = &csub;
        rpi.pDependencies = cdeps;
        check(vkCreateRenderPass(d.device, &rpi, nullptr, &composite_pass), "vkCreateRenderPass");
        fci.renderPass = composite_pass;
        fci.attachmentCount = 1;
        fci.pAttachments = &picture.view;
        check(vkCreateFramebuffer(d.device, &fci, nullptr, &composite_fb), "vkCreateFramebuffer");

        for (int i = 0; i < kPipes; ++i)
            pipes[std::size_t(i)] = make_pipeline(Pipe(i));

        // The composite samples the resolved scene.
        VkDescriptorImageInfo ii{sampler, resolved.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        VkWriteDescriptorSet wr{};
        wr.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        wr.dstSet = composite_set;
        wr.dstBinding = 0;
        wr.descriptorCount = 1;
        wr.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        wr.pImageInfo = &ii;
        vkUpdateDescriptorSets(d.device, 1, &wr, 0, nullptr);
        // ImGui's texture: made once, then pointed at each new picture.
        if (!picture_set) {
            picture_set = ImGui_ImplVulkan_AddTexture(picture.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        } else {
            VkDescriptorImageInfo pi{VK_NULL_HANDLE, picture.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
            VkWriteDescriptorSet pw{};
            pw.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            pw.dstSet = picture_set;
            pw.dstBinding = 0;
            pw.descriptorCount = 1;
            pw.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
            pw.pImageInfo = &pi;
            vkUpdateDescriptorSets(d.device, 1, &pw, 0, nullptr);
        }
    }

    VkSampleCountFlagBits sample_count(int wanted) const {
        const VkSampleCountFlags can = d.limits.framebufferColorSampleCounts & d.limits.framebufferDepthSampleCounts;
        for (int n : {8, 4, 2})
            if (wanted >= n && (can & VkSampleCountFlags(n)))
                return VkSampleCountFlagBits(n);
        return VK_SAMPLE_COUNT_1_BIT;
    }

    // The targets for f's size and samples.
    void record_ready(const ViewFrame& f) {
        const VkSampleCountFlagBits count = sample_count(f.samples);
        if (f.width >= 1 && f.height >= 1 && (f.width != width || f.height != height || count != samples))
            make_targets(f.width, f.height, count);
    }

    // With `time`, between two timestamps of the frame in flight's pair; its
    // last pair is read first: the window waited for that frame.
    void record(VkCommandBuffer c, int frame, const ViewFrame& f, bool time) {
        if (!ready || f.width < 1 || f.height < 1)
            return;
        record_ready(f);
        const std::size_t slot = std::size_t(std::clamp(frame, 0, int(uniforms.size()) - 1));
        time = time && timer;
        const std::uint32_t query = 2 * std::uint32_t(slot);
        if (time) {
            if (timer_out[slot]) {
                std::uint64_t t[2] = {};
                if (vkGetQueryPoolResults(d.device, timer, query, 2, sizeof t, t, sizeof t[0],
                                          VK_QUERY_RESULT_64_BIT) == VK_SUCCESS) {
                    time_ms = double((t[1] - t[0]) & timer_mask) * double(d.limits.timestampPeriod) / 1e6;
                    time_fresh = true;
                }
                timer_out[slot] = 0;
            }
            vkCmdResetQueryPool(c, timer, query, 2);
            vkCmdWriteTimestamp(c, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, timer, query);
        }

        // The frame's block.
        FrameBlock fb{};
        copy(fb.view_projection, f.projection * f.view);
        copy(fb.view, f.view);
        copy(fb.eye, f.eye);
        copy(fb.key, f.key);
        copy(fb.fill, f.fill);
        copy(fb.sky, f.sky);
        copy(fb.ground, f.ground);
        fb.target[0] = float(width), fb.target[1] = float(height);
        fb.target[2] = 0.0f; // no occlusion yet (V3)
        fb.flags[0] = 0;
        fb.flags[1] = f.debug_visibility ? 1 : 0;
        fb.flags[2] = f.moire_filter ? 1 : 0;
        fb.grid[0] = grid_step, fb.grid[1] = float(grid_major_every);
        for (std::size_t k = 0; k < kLineKinds && k < 16; ++k)
            copy(fb.colours[k], f.kind_colours[k], 1.0f);
        std::memcpy(uniforms[slot].mapped, &fb, sizeof fb);

        VkClearValue clears[3] = {};
        clears[1].depthStencil = {1.0f, 0};
        VkRenderPassBeginInfo rbi{};
        rbi.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        rbi.renderPass = scene_pass;
        rbi.framebuffer = scene_fb;
        rbi.renderArea.extent = {std::uint32_t(width), std::uint32_t(height)};
        rbi.clearValueCount = samples != VK_SAMPLE_COUNT_1_BIT ? 3 : 2;
        rbi.pClearValues = clears;
        vkCmdBeginRenderPass(c, &rbi, VK_SUBPASS_CONTENTS_INLINE);
        // The shaders turn OpenGL's clip space into Vulkan's (view_common.glsl).
        const VkViewport upright{0.0f, 0.0f, float(width), float(height), 0.0f, 1.0f};
        const VkRect2D all{{0, 0}, {std::uint32_t(width), std::uint32_t(height)}};
        vkCmdSetViewport(c, 0, 1, &upright);
        vkCmdSetScissor(c, 0, 1, &all);
        vkCmdBindDescriptorSets(c, VK_PIPELINE_BIND_POINT_GRAPHICS, scene_layout, 0, 1, &frame_sets[slot], 0, nullptr);
        const auto push = [&](const DrawBlock& b) {
            vkCmdPushConstants(c, scene_layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                               sizeof b, &b);
        };
        const auto bind = [&](Pipe p, const Buffer* vertices) {
            vkCmdBindPipeline(c, VK_PIPELINE_BIND_POINT_GRAPHICS, pipes[p]);
            if (vertices && vertices->buffer) {
                const VkDeviceSize zero = 0;
                vkCmdBindVertexBuffers(c, 0, 1, &vertices->buffer, &zero);
            }
        };

        // The background.
        DrawBlock draw;
        copy(draw.colour, f.background_top);
        copy(draw.extra, f.background_bottom);
        bind(Background, nullptr);
        push(draw);
        vkCmdDraw(c, 3, 1, 0, 0);

        if (plate_count > 0) {
            draw = {};
            copy(draw.colour, f.plate);
            bind(Plate, &plate_v);
            push(draw);
            vkCmdDraw(c, plate_count, 1, 0, 0);
        }
        if (grid_count > 0) {
            const std::uint32_t lines = grid_count - grid_outline;
            draw = {};
            if (lines > 0) {
                bind(Grid, &grid_v);
                push(draw);
                vkCmdDraw(c, lines, 1, 0, 0);
            }
            if (grid_outline > 0) {
                bind(Outline, &grid_v);
                push(draw);
                vkCmdDraw(c, grid_outline, 1, lines, 0);
            }
        }
        if (f.show_model && model_count > 0) {
            // While the view moves, the coarse copy if there is one (V6).
            const bool coarse = f.coarse && model_coarse_count > 0;
            const std::uint32_t count = coarse ? model_coarse_count : model_count;
            // The chosen model in its own colour, the rest before and after it.
            const std::uint32_t first =
                std::uint32_t(std::min<std::size_t>(coarse ? f.coarse_selected_first : f.selected_first, count));
            const std::uint32_t chosen = std::uint32_t(
                std::min<std::size_t>(coarse ? f.coarse_selected_count : f.selected_count, count - first));
            bind(Mesh, coarse ? &model_coarse_v : &model_v);
            draw = {};
            copy(draw.colour, f.model);
            push(draw);
            if (first > 0)
                vkCmdDraw(c, first, 1, 0, 0);
            if (first + chosen < count)
                vkCmdDraw(c, count - first - chosen, 1, first + chosen, 0);
            if (chosen > 0) {
                copy(draw.colour, f.model_selected);
                copy(draw.extra, f.selected_offset);
                push(draw);
                vkCmdDraw(c, chosen, 1, first, 0);
            }
            if (f.show_paint && paint_count > 0) {
                bind(Paint, &paint_v);
                for (const Renderer::PaintGroup& g : paint_groups) {
                    if (g.first + g.count > paint_count)
                        continue;
                    draw = {};
                    copy(draw.colour, g.colour);
                    push(draw);
                    vkCmdDraw(c, std::uint32_t(g.count), 1, std::uint32_t(g.first), 0);
                }
            }
        }
        drawn = 0;
        if (f.show_preview && layer_extrusions.size() >= 2 && extrusions_v.buffer) {
            const std::size_t layers = layer_extrusions.size() - 1;
            const std::size_t lo = std::min(f.first_layer, layers - 1), hi = std::clamp(f.last_layer, lo, layers - 1);
            draw = {};
            draw.preview[0] = f.kind_mask;
            draw.preview[1] = std::uint32_t(f.color_mode);
            const float* range = f.color_mode == ColorMode::Speed    ? &min_feed
                                 : f.color_mode == ColorMode::Height ? &min_height
                                                                     : &min_width;
            draw.range[0] = range[0], draw.range[1] = range[1];
            draw.range[2] = f.mode == RenderMode::RayTraced ? 0.0f : 1.0f;
            draw.preview[2] = std::uint32_t(lo), draw.preview[3] = std::uint32_t(hi);
            bind(Preview, &extrusions_v);
            push(draw);
            for (const PreviewRange& r : preview_ranges(order, lo, hi, f.kind_mask, f.top_shown, f.top_hidden))
                if (r.count > 0) {
                    vkCmdDraw(c, 36, std::uint32_t(r.count), 0, std::uint32_t(r.first));
                    drawn += r.count;
                }
            if (f.show_travels && travel_count > 0 && layer_travels.size() > hi + 1) {
                const std::size_t t0 = layer_travels[lo], tn = layer_travels[hi + 1] - t0;
                if (tn > 0) {
                    bind(Travels, &travels_v);
                    push(DrawBlock{});
                    vkCmdDraw(c, std::uint32_t(tn), 1, std::uint32_t(t0), 0);
                }
            }
        }
        vkCmdEndRenderPass(c);

        // Into the picture, smoothed with FXAA when asked.
        rbi.renderPass = composite_pass;
        rbi.framebuffer = composite_fb;
        rbi.clearValueCount = 0;
        vkCmdBeginRenderPass(c, &rbi, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdSetViewport(c, 0, 1, &upright);
        vkCmdSetScissor(c, 0, 1, &all);
        vkCmdBindPipeline(c, VK_PIPELINE_BIND_POINT_GRAPHICS, pipes[Composite]);
        vkCmdBindDescriptorSets(c, VK_PIPELINE_BIND_POINT_GRAPHICS, composite_layout_pipe, 0, 1, &composite_set, 0,
                                nullptr);
        const CompositeBlock cb{{float(width), float(height)}, f.fxaa ? 1 : 0, 0};
        vkCmdPushConstants(c, composite_layout_pipe, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof cb, &cb);
        vkCmdDraw(c, 3, 1, 0, 0);
        vkCmdEndRenderPass(c);
        if (time) {
            vkCmdWriteTimestamp(c, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, timer, query + 1);
            timer_out[slot] = 1;
        }
    }

    void shutdown() {
        if (!d.device)
            return;
        vkDeviceWaitIdle(d.device);
        free_targets();
        if (picture_set)
            ImGui_ImplVulkan_RemoveTexture(picture_set);
        picture_set = VK_NULL_HANDLE;
        for (Buffer* b : {&plate_v, &grid_v, &model_v, &model_coarse_v, &paint_v, &extrusions_v, &travels_v})
            free_buffer(*b);
        for (Buffer& u : uniforms)
            free_buffer(u);
        uniforms.clear();
        if (timer)
            vkDestroyQueryPool(d.device, timer, nullptr);
        free_image(white);
        if (sampler)
            vkDestroySampler(d.device, sampler, nullptr);
        if (pool)
            vkDestroyDescriptorPool(d.device, pool, nullptr);
        for (VkPipelineLayout l : {scene_layout, composite_layout_pipe})
            if (l)
                vkDestroyPipelineLayout(d.device, l, nullptr);
        for (VkDescriptorSetLayout l : {frame_layout, composite_layout})
            if (l)
                vkDestroyDescriptorSetLayout(d.device, l, nullptr);
        for (VkShaderModule m : modules)
            if (m)
                vkDestroyShaderModule(d.device, m, nullptr);
        *this = Impl{};
    }
};

VulkanView::VulkanView() : impl_(std::make_unique<Impl>()) {}
VulkanView::~VulkanView() { shutdown(); }

bool VulkanView::init(const VulkanDevice& device, std::string& error) {
    try {
        impl_->d = device;
        impl_->init();
        return true;
    } catch (const std::exception& e) {
        error = e.what();
        impl_->shutdown();
        return false;
    }
}

void VulkanView::shutdown() {
    if (impl_)
        impl_->shutdown();
}

void VulkanView::set_bed(const BedPolygon& bed, const BedLook& look) {
    Impl& v = *impl_;
    if (!v.ready)
        return;
    v.plate_count = v.grid_count = v.grid_outline = 0;
    v.guard([&] {
        const std::vector<float> plate = bed_triangles(bed);
        v.upload(v.plate_v, plate.data(), plate.size() * sizeof(float));
        v.plate_count = std::uint32_t(plate.size() / 3);
        const std::vector<LineVertex> all = bed_lines(bed, look);
        const std::size_t outline = all.empty() ? 0 : 2 * bed.size(); // bed_lines puts it last
        std::vector<LineVertex> lines = split_lines(all.data(), all.size() - outline, 5.0f);
        const std::vector<LineVertex> edge = split_lines(all.data() + (all.size() - outline), outline, 5.0f);
        lines.insert(lines.end(), edge.begin(), edge.end());
        v.upload(v.grid_v, lines.data(), lines.size() * sizeof(LineVertex));
        v.grid_count = std::uint32_t(lines.size());
        v.grid_outline = std::uint32_t(edge.size());
        v.grid_step = float(look.step);
        v.grid_major_every = std::max(look.major_every, 1);
    });
}

void VulkanView::set_model(const std::vector<float>& vertices) {
    Impl& v = *impl_;
    if (!v.ready)
        return;
    v.model_count = 0;
    v.guard([&] {
        v.upload(v.model_v, vertices.data(), vertices.size() * sizeof(float));
        v.model_count = std::uint32_t(vertices.size() / 6);
    });
}

void VulkanView::set_model_coarse(const std::vector<float>& vertices) {
    Impl& v = *impl_;
    if (!v.ready)
        return;
    v.model_coarse_count = 0;
    v.guard([&] {
        v.upload(v.model_coarse_v, vertices.data(), vertices.size() * sizeof(float));
        v.model_coarse_count = std::uint32_t(vertices.size() / 6);
    });
}

void VulkanView::set_paint(const std::vector<float>& vertices, const std::vector<Renderer::PaintGroup>& groups) {
    Impl& v = *impl_;
    if (!v.ready)
        return;
    v.paint_count = 0;
    v.guard([&] {
        v.upload(v.paint_v, vertices.data(), vertices.size() * sizeof(float));
        v.paint_count = std::uint32_t(vertices.size() / 6);
        v.paint_groups = groups;
    });
}

void VulkanView::set_preview(const PreviewBuffers& p) {
    Impl& v = *impl_;
    if (!v.ready)
        return;
    clear_preview();
    v.guard([&] {
        PreviewDrawOrder order = preview_draw_order(p);
        v.upload(v.extrusions_v, order.instances.data(), order.instances.size() * sizeof(PreviewInstance));
        order.instances = {}; // on the GPU now
        v.upload(v.travels_v, p.travels.data(), p.travels.size() * sizeof(LineVertex));
        v.order = std::move(order);
        v.travel_count = std::uint32_t(p.travels.size());
        v.layer_extrusions = p.layer_extrusions;
        v.layer_travels = p.layer_travels;
        v.min_feed = p.min_feed, v.max_feed = p.max_feed;
        v.min_width = p.min_width, v.max_width = p.max_width;
        v.min_height = p.min_height, v.max_height = p.max_height;
    });
}

void VulkanView::clear_preview() {
    Impl& v = *impl_;
    v.order = {};
    v.layer_extrusions.clear();
    v.layer_travels.clear();
    v.travel_count = 0;
}

void VulkanView::record(void* command_buffer, int frame, const ViewFrame& f) {
    // Only the targets can fail, before anything is recorded: then the view
    // stops, and the window goes on.
    Impl& v = *impl_;
    if (!v.guard([&] { v.record(static_cast<VkCommandBuffer>(command_buffer), frame, f, true); }))
        v.ready = false;
}

bool VulkanView::take_time(double& ms) {
    if (!impl_->time_fresh)
        return false;
    impl_->time_fresh = false;
    ms = impl_->time_ms;
    return true;
}

double VulkanView::bench(const ViewFrame& f, int n) {
    Impl& v = *impl_;
    if (!v.ready || n < 1)
        return 0.0;
    double ms = 0.0;
    v.guard([&] {
        v.record_ready(f); // the targets first, outside the timed part
        vkDeviceWaitIdle(v.d.device);
        const auto start = std::chrono::steady_clock::now();
        v.one_time([&](VkCommandBuffer c) {
            for (int i = 0; i < n; ++i)
                v.record(c, 0, f, false);
        });
        ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count() / n;
    });
    return ms;
}

ImTextureID VulkanView::texture() const { return ImTextureID(impl_->picture_set); }

std::size_t VulkanView::drawn_extrusions() const { return impl_->drawn; }

} // namespace tisma::ui

#else // no Vulkan in this build

namespace tisma::ui {

struct VulkanView::Impl {};
VulkanView::VulkanView() = default;
VulkanView::~VulkanView() = default;
bool VulkanView::init(const VulkanDevice&, std::string& error) {
    error = "this build has no Vulkan";
    return false;
}
void VulkanView::shutdown() {}
void VulkanView::set_bed(const BedPolygon&, const BedLook&) {}
void VulkanView::set_model(const std::vector<float>&) {}
void VulkanView::set_model_coarse(const std::vector<float>&) {}
void VulkanView::set_paint(const std::vector<float>&, const std::vector<Renderer::PaintGroup>&) {}
void VulkanView::set_preview(const PreviewBuffers&) {}
void VulkanView::clear_preview() {}
void VulkanView::record(void*, int, const ViewFrame&) {}
ImTextureID VulkanView::texture() const { return ImTextureID(0); }
std::size_t VulkanView::drawn_extrusions() const { return 0; }
bool VulkanView::take_time(double&) { return false; }
double VulkanView::bench(const ViewFrame&, int) { return 0.0; }

} // namespace tisma::ui

#endif
