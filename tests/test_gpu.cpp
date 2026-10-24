#include "gpu/compute.hpp"
#include "io/stl.hpp"
#include "slice/layers.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
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

TEST_CASE("GPU segments match the CPU: same edges and order, points within a few ULP", "[gpu]") {
    gpu::Context& ctx = require_context();
    const IndexedMesh m = sphere(200, 50.0f);
    const std::vector<float> z = height_field(m);
    std::vector<float> isos;
    for (const Layer& l : plan_layers(-50.0, 50.0, {0.3, 0.2}))
        isos.push_back(float(l.slice_z()));
    std::vector<std::uint32_t> first, end, cpu_offsets, gpu_offsets;
    triangle_level_ranges(m, z, isos, first, end);
    std::vector<LevelSegment> cpu, gpu_segs;
    level_segments_by_triangle(m, z, isos, first, end, cpu_offsets, cpu);
    ctx.level_segments_by_triangle(m, z, isos, first, end, gpu_offsets, gpu_segs);
    CHECK(gpu_offsets == cpu_offsets);
    REQUIRE(gpu_segs.size() == cpu.size());
    REQUIRE_FALSE(cpu.empty());
    double worst = 0.0;
    for (std::size_t i = 0; i < cpu.size(); ++i) {
        CHECK(gpu_segs[i].edge_from == cpu[i].edge_from);
        CHECK(gpu_segs[i].edge_to == cpu[i].edge_to);
        CHECK(gpu_segs[i].triangle == cpu[i].triangle);
        const float d[] = {gpu_segs[i].from.x - cpu[i].from.x, gpu_segs[i].from.y - cpu[i].from.y,
                           gpu_segs[i].from.z - cpu[i].from.z, gpu_segs[i].to.x - cpu[i].to.x,
                           gpu_segs[i].to.y - cpu[i].to.y,     gpu_segs[i].to.z - cpu[i].to.z};
        for (float v : d)
            worst = std::max(worst, double(std::fabs(v)));
    }
    WARN("largest GPU-CPU difference: " << worst << " mm"); // reported in every run
    CHECK(worst <= 1e-4); // 0.1 um; float ULP at 50 mm is about 4e-6 mm
}

TEST_CASE("slicing on the GPU gives the same contours as on the CPU", "[gpu]") {
    gpu::Context& ctx = require_context();
    const IndexedMesh m = sphere(120, 30.0f);
    const std::vector<float> z = height_field(m);
    std::vector<float> isos;
    for (const Layer& l : plan_layers(-30.0, 30.0, {0.2, 0.2}))
        isos.push_back(float(l.slice_z()));
    const auto cpu = slice_levels(m, z, isos);
    const auto gpu_layers = slice_levels(m, z, isos, 0.0, &ctx);
    REQUIRE(gpu_layers.size() == cpu.size());
    for (std::size_t l = 0; l < cpu.size(); ++l) {
        REQUIRE(gpu_layers[l].size() == cpu[l].size());
        for (std::size_t c = 0; c < cpu[l].size(); ++c) {
            REQUIRE(gpu_layers[l][c].points.size() == cpu[l][c].points.size());
            CHECK(gpu_layers[l][c].closed == cpu[l][c].closed);
            for (std::size_t p = 0; p < cpu[l][c].points.size(); ++p) {
                CHECK(std::fabs(gpu_layers[l][c].points[p].x - cpu[l][c].points[p].x) <= 1e-4f);
                CHECK(std::fabs(gpu_layers[l][c].points[p].y - cpu[l][c].points[p].y) <= 1e-4f);
            }
        }
    }
}
