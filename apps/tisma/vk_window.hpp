#pragma once

#include <imgui.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

struct GLFWwindow;

namespace tisma::ui {

struct VulkanDevice;

// The window drawn with Vulkan (PLAN.md, «Vista con Vulkan», V1): the
// instance and the device, the swapchain, one render pass that clears the
// window and draws Dear ImGui, two frames in flight, the textures ImGui shows
// and the last frame read back for screenshots. Vulkan is loaded at run time
// through volk, as the engine's compute does: on a machine without it, init
// fails with the reason and the interface stays with OpenGL. Written from the
// Vulkan specification (Khronos); own design, see PROVENANCE.md.
class VulkanWindow {
public:
    VulkanWindow();
    ~VulkanWindow();
    VulkanWindow(const VulkanWindow&) = delete;
    VulkanWindow& operator=(const VulkanWindow&) = delete;

    // Whether this build has the Vulkan window at all (TISMA_GPU).
    static bool built();

    // On a window made with GLFW_CLIENT_API = GLFW_NO_API. False with the
    // reason when there is no Vulkan 1.1 device that can show the window.
    bool init(GLFWwindow* window, bool vsync, std::string& error);
    // Dear ImGui's Vulkan backend on this device; after ImGui::CreateContext
    // and ImGui_ImplGlfw_InitForVulkan.
    bool init_imgui(std::string& error);
    void shutdown();

    // A texture for ImGui::Image, from RGBA rows from the top; 0 if it failed.
    ImTextureID add_texture(const std::uint8_t* rgba, int width, int height);

    // Draws a frame: first `before` records into the frame's command buffer
    // (a VkCommandBuffer) what must come before the window's pass, with the
    // frame's index in flight (the 3D view, vk_view.*); then the window cleared
    // to `clear` (RGB 0 to 1) and ImGui's draw data; and shows it. With keep,
    // the frame is also copied to memory for read_back.
    void frame(ImDrawData* draw, const float clear[3], bool keep,
               const std::function<void(void* command_buffer, int frame)>& before = {});

    // The device it draws with, for the view; nullptr before init.
    const VulkanDevice* device() const;

    // The last kept frame, RGBA rows from the bottom (as glReadPixels gives
    // them); empty if none.
    std::vector<std::uint8_t> read_back(int& width, int& height);

    // The device's name, for the status bar.
    std::string device_name() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace tisma::ui
