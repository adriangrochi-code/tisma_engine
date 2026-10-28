#pragma once

#include "renderer.hpp"

#include "ui/scene.hpp"

#include <imgui.h>

#include <memory>
#include <string>
#include <vector>

namespace tisma::ui {

struct VulkanDevice;

// The 3D view drawn with Vulkan (PLAN.md, «Vista con Vulkan», V2): the bed,
// the model with its paint and the G-code preview, lit as the OpenGL view
// (renderer.*) and with the same shaders' formulas (apps/tisma/shaders/),
// multisampled, then smoothed with FXAA into a picture ImGui shows under the
// panels. The light without occlusion for now (the Fast mode): the ambient
// occlusion and the ray tracing come in V3. Written from the Vulkan
// specification; own design, see PROVENANCE.md.
class VulkanView {
public:
    VulkanView();
    ~VulkanView();
    VulkanView(const VulkanView&) = delete;
    VulkanView& operator=(const VulkanView&) = delete;

    // On the window's device, after its ImGui backend started (the picture
    // is an ImGui texture). False with the reason if the device refuses.
    bool init(const VulkanDevice& device, std::string& error);
    void shutdown();

    // As Renderer's.
    void set_bed(const BedPolygon& bed, const BedLook& look);
    void set_model(const std::vector<float>& vertices);
    void set_model_coarse(const std::vector<float>& vertices);
    void set_paint(const std::vector<float>& vertices, const std::vector<Renderer::PaintGroup>& groups);
    void set_preview(const PreviewBuffers& preview);
    void clear_preview();

    // Records the view of `f` into a command buffer (a VkCommandBuffer),
    // before the window's pass, for the frame in flight `frame`. Its picture
    // is then ready to sample, f.width x f.height.
    void record(void* command_buffer, int frame, const ViewFrame& f);

    // The view's picture for ImGui::Image: the same texture all along, 0
    // until the first record.
    ImTextureID texture() const;

    // The preview's extrusions the last record drew.
    std::size_t drawn_extrusions() const;

    // The view's time on the GPU, from timestamps written around its passes
    // and read when its frame in flight comes back: true once per new
    // measurement, in milliseconds. False always on a device without
    // timestamps.
    bool take_time(double& ms);

    // --bench-frames: the view of `f` recorded `n` times into one command
    // buffer, submitted and waited for; milliseconds per view.
    double bench(const ViewFrame& f, int n);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace tisma::ui
