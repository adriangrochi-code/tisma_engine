// The choice of the compute device (compute.hpp), apart from Vulkan so that
// it is tested on any machine. Own design.

#include "gpu/compute.hpp"

#include <algorithm>
#include <cctype>

namespace tisma::gpu {

namespace {

bool software_name(std::string name) {
    std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) { return char(std::tolower(c)); });
    for (const char* s : {"llvmpipe", "lavapipe", "swiftshader", "basic render"})
        if (name.find(s) != std::string::npos)
            return true;
    return false;
}

int rank(const DeviceCandidate& d) {
    if (d.kind == DeviceCandidate::Kind::Cpu || software_name(d.name))
        return 1;
    switch (d.kind) {
    case DeviceCandidate::Kind::Discrete: return 4;
    case DeviceCandidate::Kind::Integrated: return 3;
    case DeviceCandidate::Kind::Virtual: return 2;
    default: return 0;
    }
}

} // namespace

int choose_device(const std::vector<DeviceCandidate>& devices, bool allow_cpu, std::string* why) {
    int best = -1, best_rank = 0;
    std::string seen;
    for (std::size_t i = 0; i < devices.size(); ++i) {
        const DeviceCandidate& d = devices[i];
        const int r = rank(d);
        seen += (seen.empty() ? "" : ", ") + d.name;
        if (!d.vulkan_1_1 || !d.compute || r == 0 || (r == 1 && !allow_cpu))
            continue;
        if (r > best_rank || (r == best_rank && d.local_memory > devices[std::size_t(best)].local_memory))
            best = int(i), best_rank = r;
    }
    if (best < 0 && why) {
        if (devices.empty())
            *why = "the Vulkan driver lists no device (update the graphics driver)";
        else
            *why = "no Vulkan device fit for computing (seen: " + seen +
                   "; software devices are slower than the CPU path, and a GPU needs Vulkan 1.1 with a compute queue)";
    }
    return best;
}

} // namespace tisma::gpu
