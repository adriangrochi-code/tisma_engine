// What the machine is. Own design; see PROVENANCE.md.

#include "ui/system_info.hpp"

#include "ui/text.hpp"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>
#include <thread>
#include <utility>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#elif defined(__APPLE__)
#include <sys/sysctl.h>
#include <sys/types.h>
#include <mach/mach.h>
#endif

namespace tisma::ui {
namespace {

#if !defined(_WIN32) && !defined(__APPLE__)
std::string read_first_line(const std::filesystem::path& path) {
    std::ifstream in(path);
    std::string line;
    std::getline(in, line);
    return line;
}

std::string trimmed(std::string s) {
    const auto first = s.find_first_not_of(" \t");
    const auto last = s.find_last_not_of(" \t\r\n");
    return first == std::string::npos ? std::string() : s.substr(first, last - first + 1);
}
#endif

void read_memory_and_power(SystemInfo& info) {
#if defined(_WIN32)
    MEMORYSTATUSEX m{};
    m.dwLength = sizeof m;
    if (GlobalMemoryStatusEx(&m)) {
        info.ram_total = m.ullTotalPhys;
        info.ram_free = m.ullAvailPhys;
    }
    SYSTEM_POWER_STATUS p{};
    if (GetSystemPowerStatus(&p)) {
        info.power = p.ACLineStatus == 1 ? PowerSource::Mains : p.ACLineStatus == 0 ? PowerSource::Battery : PowerSource::Unknown;
        info.battery_percent = p.BatteryLifePercent <= 100 && !(p.BatteryFlag & 128) ? int(p.BatteryLifePercent) : -1;
    }
#elif defined(__APPLE__)
    std::uint64_t total = 0;
    std::size_t size = sizeof total;
    if (sysctlbyname("hw.memsize", &total, &size, nullptr, 0) == 0)
        info.ram_total = total;
    vm_statistics64_data_t vm{};
    mach_msg_type_number_t count = HOST_VM_INFO64_COUNT;
    vm_size_t page = 0;
    if (host_page_size(mach_host_self(), &page) == KERN_SUCCESS &&
        host_statistics64(mach_host_self(), HOST_VM_INFO64, reinterpret_cast<host_info64_t>(&vm), &count) == KERN_SUCCESS)
        info.ram_free = std::uint64_t(vm.free_count + vm.inactive_count) * page;
#else
    std::ifstream meminfo("/proc/meminfo");
    std::string key;
    std::uint64_t kb = 0;
    std::string unit;
    while (meminfo >> key >> kb) {
        std::getline(meminfo, unit);
        if (key == "MemTotal:")
            info.ram_total = kb * 1024;
        else if (key == "MemAvailable:")
            info.ram_free = kb * 1024;
    }
    // Power: a mains supply online, or a battery and its charge.
    info.power = PowerSource::Unknown;
    info.battery_percent = -1;
    bool mains = false, battery = false, mains_online = false;
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator("/sys/class/power_supply", ec)) {
        const std::string type = trimmed(read_first_line(entry.path() / "type"));
        if (type == "Mains") {
            mains = true;
            mains_online = mains_online || trimmed(read_first_line(entry.path() / "online")) == "1";
        } else if (type == "Battery") {
            battery = true;
            const std::string capacity = trimmed(read_first_line(entry.path() / "capacity"));
            if (!capacity.empty())
                info.battery_percent = std::clamp(std::atoi(capacity.c_str()), 0, 100);
        }
    }
    if (mains)
        info.power = mains_online || !battery ? PowerSource::Mains : PowerSource::Battery;
    else if (battery)
        info.power = PowerSource::Battery;
#endif
}

} // namespace

SystemInfo system_info() {
    SystemInfo info;
    info.logical_cores = int(std::thread::hardware_concurrency());
#if defined(_WIN32)
    char name[256] = {};
    DWORD size = sizeof name;
    if (RegGetValueA(HKEY_LOCAL_MACHINE, "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", "ProcessorNameString",
                     RRF_RT_REG_SZ, nullptr, name, &size) == ERROR_SUCCESS)
        info.cpu = name;
    DWORD bytes = 0;
    GetLogicalProcessorInformation(nullptr, &bytes);
    if (bytes > 0) {
        std::string buffer(bytes, '\0');
        auto* items = reinterpret_cast<SYSTEM_LOGICAL_PROCESSOR_INFORMATION*>(buffer.data());
        if (GetLogicalProcessorInformation(items, &bytes))
            for (DWORD i = 0; i < bytes / sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION); ++i)
                if (items[i].Relationship == RelationProcessorCore)
                    ++info.physical_cores;
    }
#elif defined(__APPLE__)
    char name[256] = {};
    std::size_t size = sizeof name;
    if (sysctlbyname("machdep.cpu.brand_string", name, &size, nullptr, 0) == 0)
        info.cpu = name;
    int cores = 0;
    size = sizeof cores;
    if (sysctlbyname("hw.physicalcpu", &cores, &size, nullptr, 0) == 0)
        info.physical_cores = cores;
#else
    // The model name, and the cores as distinct (package, core) pairs.
    std::ifstream cpuinfo("/proc/cpuinfo");
    std::string line;
    std::set<std::pair<std::string, std::string>> cores;
    std::string package, core;
    while (std::getline(cpuinfo, line)) {
        const auto colon = line.find(':');
        if (colon == std::string::npos) {
            if (!core.empty())
                cores.insert({package, core});
            package.clear(), core.clear();
            continue;
        }
        const std::string key = trimmed(line.substr(0, colon)), value = trimmed(line.substr(colon + 1));
        if (key == "model name" && info.cpu.empty())
            info.cpu = value;
        else if (key == "physical id")
            package = value;
        else if (key == "core id")
            core = value;
    }
    if (!core.empty())
        cores.insert({package, core});
    info.physical_cores = int(cores.size());
#endif
    read_memory_and_power(info);
    return info;
}

void refresh_system_info(SystemInfo& info) {
    read_memory_and_power(info);
}

std::string bytes_text(std::uint64_t bytes) {
    const double gb = double(bytes) / (1024.0 * 1024.0 * 1024.0);
    return gb >= 1.0 ? format("%.1f GB", gb) : format("%.0f MB", double(bytes) / (1024.0 * 1024.0));
}

} // namespace tisma::ui
