// What the machine is: processor, memory and power, for the Video page and the
// automatic view quality (view_auto_quality.md, rule 1). The graphics card and
// the screen come from the window's graphics and from GLFW, in the app. Own
// design from the operating systems' documented interfaces (Linux /proc and
// /sys, Windows' registry and system calls, macOS sysctl); see PROVENANCE.md.

#pragma once

#include <cstdint>
#include <string>

namespace tisma::ui {

enum class PowerSource { Unknown, Mains, Battery };

struct SystemInfo {
    std::string cpu;             // the processor's model name; empty if unknown
    int logical_cores = 0;       // threads the processor runs at once
    int physical_cores = 0;      // 0 if unknown
    std::uint64_t ram_total = 0; // bytes
    std::uint64_t ram_free = 0;  // bytes available to programs
    PowerSource power = PowerSource::Unknown;
    int battery_percent = -1; // -1: no battery or unknown
};

// Read now (a few milliseconds).
SystemInfo system_info();

// Only the memory and the power, cheap enough to read every few seconds.
void refresh_system_info(SystemInfo& info);

// Bytes as «15,6 GB» in the interface's language.
std::string bytes_text(std::uint64_t bytes);

} // namespace tisma::ui
