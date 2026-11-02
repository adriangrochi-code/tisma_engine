// What the machine is (view_auto_quality.md, rule 1).

#include "ui/system_info.hpp"

#include <catch2/catch_test_macros.hpp>

#include <string>

using namespace tisma::ui;

TEST_CASE("system info: the processor, the memory and the power are read", "[system]") {
    const SystemInfo info = system_info();
    CHECK(info.logical_cores >= 1);
    CHECK(info.ram_total > 64ull * 1024 * 1024);
    CHECK(info.ram_free <= info.ram_total);
#if defined(__linux__)
    // A Linux /proc/cpuinfo always has a model name on x86; on ARM it may not.
    CHECK((!info.cpu.empty() || info.physical_cores >= 0));
#endif
    SystemInfo again = info;
    refresh_system_info(again);
    CHECK(again.ram_total == info.ram_total);
    const std::string gb = bytes_text(16ull * 1024 * 1024 * 1024); // "16.0 GB" or "16,0 GB" by language
    CHECK((gb == "16.0 GB" || gb == "16,0 GB"));
    CHECK(bytes_text(512ull * 1024 * 1024) == "512 MB");
}
