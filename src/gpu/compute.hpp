#pragma once

#include "geom/bitgrid.hpp"
#include "mesh/indexed_mesh.hpp"
#include "slice/level_set.hpp"

#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace tisma::gpu {

// Finds the Vulkan loader, once for the whole program and from any thread;
// false if this machine has none (or the build has no GPU backend). The
// compute context and the interface's window both start from it, then each
// loads its own functions, so neither changes the other's.
bool load_vulkan();

struct ContextOptions {
    // Software devices (such as Mesa's lavapipe) run on the CPU and are slower
    // than the engine's own CPU path, so they are skipped unless allowed. The
    // tests allow them so that CI without a GPU still runs the kernels.
    bool allow_cpu_device = false;
};

// One Vulkan device as the driver describes it, for choose_device.
struct DeviceCandidate {
    enum class Kind { Discrete, Integrated, Virtual, Cpu, Other };
    std::string name;
    Kind kind = Kind::Other;
    std::uint64_t local_memory = 0; // bytes in its device-local heaps
    bool vulkan_1_1 = true;
    bool compute = true; // a queue family with compute
};

// The device the engine computes on: a discrete GPU first, then an
// integrated one, then a virtual one; among equals the one with the most
// device-local memory (on a laptop with two GPUs, the discrete one). A
// software device (Mesa's llvmpipe or lavapipe, SwiftShader, Microsoft's
// Basic Render Driver), by its type or its name, only when allow_cpu.
// Returns the index, or -1 with the reason in why when there is none.
int choose_device(const std::vector<DeviceCandidate>& devices, bool allow_cpu, std::string* why = nullptr);

// A Vulkan compute device. GPU use is optional (design rule 4 in PLAN.md):
// every kernel has a CPU version that is the reference in the tests.
class Context {
public:
    // Returns null when the engine was built without the GPU backend, when no
    // Vulkan loader or suitable device is present, or when the environment
    // variable TISMA_GPU is set to 0.
    // With why, the reason when it returns null, in plain words, for the
    // warning that says why the work stayed on the CPU.
    static std::unique_ptr<Context> create(const ContextOptions& options = {}, std::string* why = nullptr);

    ~Context();
    Context(const Context&) = delete;
    Context& operator=(const Context&) = delete;

    std::string device_name() const;

    // GPU version of triangle_level_ranges (src/slice/layers.hpp), with the
    // same results. Throws std::runtime_error on a Vulkan failure.
    void triangle_level_ranges(const IndexedMesh& mesh, std::span<const float> values, std::span<const float> isos,
                               std::vector<std::uint32_t>& first, std::vector<std::uint32_t>& end);

    // GPU version of level_segments_by_triangle (src/slice/level_set.hpp):
    // the same segments in the same order, with points that agree to a few
    // ULP. Throws std::runtime_error on a Vulkan failure.
    void level_segments_by_triangle(const IndexedMesh& mesh, std::span<const float> values,
                                    std::span<const float> isos, std::span<const std::uint32_t> first,
                                    std::span<const std::uint32_t> end, std::vector<std::uint32_t>& offsets,
                                    std::vector<LevelSegment>& out);

    // GPU version of the support column's steps over a chunk of layers
    // (support_areas, src/support/support.cpp), bit for bit the CPU's: from
    // the chunk's last layer down, the column takes adds[k] and is closed by
    // `radius` pixels (at most 31) when adds[k] has a grid (an overhang
    // above), then loses parts[k]; kept[k] becomes the column after it. Every
    // grid has the column's size. Throws std::runtime_error on a Vulkan
    // failure.
    void support_column(BitGrid& column, std::span<const BitGrid> parts, std::span<const BitGrid> adds,
                        std::span<BitGrid> kept, int radius);

    // GPU version of squared_distance_inside (src/geom/distance.hpp), the
    // same numbers. Throws std::runtime_error on a Vulkan failure.
    void distance_inside(const BitGrid& grid, std::vector<float>& out);
    // The same for many grids in one run (every layer's at once): out[k] is
    // grids[k]'s.
    void distance_inside(std::span<const BitGrid> grids, std::vector<std::vector<float>>& out);

    struct Impl;

private:
    explicit Context(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> impl_;
};

} // namespace tisma::gpu
