#include "gpu/compute.hpp"
#include "io/stl.hpp"
#include "slice/layers.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <numbers>

using namespace tisma;

namespace {

// The tests run the kernels on any Vulkan device, software ones included. Set
// TISMA_REQUIRE_GPU=1 (as the Linux CI does, with lavapipe) to fail instead of
// skipping when there is none.
gpu::Context* test_context() {
    static std::unique_ptr<gpu::Context> ctx = gpu::Context::create({.allow_cpu_device = true});
    return ctx.get();
}

gpu::Context& require_context() {
    gpu::Context* ctx = test_context();
    if (!ctx) {
        const char* require = std::getenv("TISMA_REQUIRE_GPU");
        if (require && std::string(require) == "1")
            FAIL("TISMA_REQUIRE_GPU=1 but no Vulkan device was found");
        SKIP("no Vulkan device");
    }
    return *ctx;
}

// Closed UV sphere with exact poles and seam, like bench/gen_sphere.py.
IndexedMesh sphere(int n, float radius) {
    TriangleMesh m;
    auto point = [&](int i, int j) -> Vec3f {
        if (i == 0)
            return {0, 0, radius};
        if (i == n)
            return {0, 0, -radius};
        const double theta = std::numbers::pi * i / n;
        const double phi = 2.0 * std::numbers::pi * (j % n) / n;
        return {float(radius * std::sin(theta) * std::cos(phi)), float(radius * std::sin(theta) * std::sin(phi)),
                float(radius * std::cos(theta))};
    };
    for (int i = 0; i < n; ++i)
        for (int j = 0; j < n; ++j) {
            const Vec3f a = point(i, j), b = point(i + 1, j), c = point(i + 1, j + 1), d = point(i, j + 1);
            if (i != 0)
                m.add_triangle(a, b, d);
            if (i != n - 1)
                m.add_triangle(b, c, d);
        }
    return weld_exact(m);
}

void compare_with_cpu(gpu::Context& ctx, const IndexedMesh& mesh, const std::vector<float>& isos) {
    const std::vector<float> z = height_field(mesh);
    std::vector<std::uint32_t> cpu_first, cpu_end, gpu_first, gpu_end;
    triangle_level_ranges(mesh, z, isos, cpu_first, cpu_end);
    ctx.triangle_level_ranges(mesh, z, isos, gpu_first, gpu_end);
    REQUIRE(gpu_first.size() == cpu_first.size());
    CHECK(gpu_first == cpu_first);
    CHECK(gpu_end == cpu_end);
}

} // namespace

TEST_CASE("GPU level ranges match the CPU on a sphere", "[gpu]") {
    gpu::Context& ctx = require_context();
    INFO("device: " << ctx.device_name());
    const IndexedMesh m = sphere(200, 50.0f);
    std::vector<float> isos;
    for (const Layer& l : plan_layers(-50.0, 50.0, {0.3, 0.2}))
        isos.push_back(float(l.slice_z()));
    compare_with_cpu(ctx, m, isos);
}

TEST_CASE("GPU level ranges match the CPU when levels pass through vertices", "[gpu]") {
    gpu::Context& ctx = require_context();
    const IndexedMesh m = weld_exact(read_stl(std::filesystem::path(TISMA_TEST_DATA_DIR) / "cube_ascii.stl").mesh);
    compare_with_cpu(ctx, m, {0.0f, 0.1f, 5.0f, 10.0f, 10.5f});
    compare_with_cpu(ctx, m, {5.0f});
}

TEST_CASE("GPU level ranges with no levels or no triangles", "[gpu]") {
    gpu::Context& ctx = require_context();
    const IndexedMesh m = sphere(8, 1.0f);
    compare_with_cpu(ctx, m, {});
    compare_with_cpu(ctx, IndexedMesh{}, {0.5f});
}
