#pragma once

#include <volk.h>

#include <cstdint>

namespace tisma::ui {

// The Vulkan device a window draws with (vk_window.*), for the view drawn on
// it (vk_view.*).
struct VulkanDevice {
    VkInstance instance = VK_NULL_HANDLE;
    VkPhysicalDevice physical = VK_NULL_HANDLE;
    VkPhysicalDeviceMemoryProperties memory{};
    VkPhysicalDeviceLimits limits{};
    VkDevice device = VK_NULL_HANDLE;
    VkQueue queue = VK_NULL_HANDLE;
    std::uint32_t family = 0;
    VkCommandPool pool = VK_NULL_HANDLE;
    int frames = 2; // frames in flight
};

} // namespace tisma::ui
