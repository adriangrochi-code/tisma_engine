#include "gcode/preview.hpp"
#include "io/stl.hpp"
#include "ui/raytrace.hpp"
#include "ui/scene.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <filesystem>
#include <random>

using namespace tisma;
using namespace tisma::ui;
using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

namespace {

// The answer without the hierarchy: every primitive, the nearest hit.
float brute_closest(const std::vector<RayPrimitive>& prims, const Ray& ray, const RayFilter& filter = {}) {
    float best = -1.0f;
    for (const RayPrimitive& p : prims) {
        if (p.layer < filter.first_layer || p.layer > filter.last_layer)
            continue;
        if (p.kind != kAlwaysVisible && !((filter.kind_mask >> p.kind) & 1u))
            continue;
        const float t = p.box ? intersect_box(ray, p.p0, p.p1, p.p2, p.p3)
                              : intersect_triangle(ray, p.p0, p.p1 - p.p0, p.p2 - p.p0);
        if (t >= 0.0f && t < ray.tmax && (best < 0.0f || t < best))
            best = t;
    }
    return best;
}

Vec3 random_unit(std::mt19937& rng) {
    std::normal_distribution<float> n;
    return normalize(Vec3{n(rng), n(rng), n(rng)});
}

} // namespace

TEST_CASE("raytrace: a ray meets a triangle where it should", "[ui][raytrace]") {
    const Vec3 a{0, 0, 0}, e1{10, 0, 0}, e2{0, 10, 0};
    CHECK_THAT(intersect_triangle({{2, 3, 5}, {0, 0, -1}}, a, e1, e2), WithinAbs(5.0, 1e-6));
    CHECK_THAT(intersect_triangle({{2, 3, -4}, {0, 0, 2}}, a, e1, e2), WithinAbs(2.0, 1e-6)); // t in units of dir
    CHECK(intersect_triangle({{8, 8, 5}, {0, 0, -1}}, a, e1, e2) < 0.0f);  // past the hypotenuse
    CHECK(intersect_triangle({{2, 3, 5}, {0, 0, 1}}, a, e1, e2) < 0.0f);   // going away
    CHECK(intersect_triangle({{2, 3, 5}, {1, 0, 0}}, a, e1, e2) < 0.0f);   // parallel
}

TEST_CASE("raytrace: a ray enters a box from outside only", "[ui][raytrace]") {
    // A box 4 x 2 x 1 turned 45 degrees around Z.
    const float s = std::sqrt(0.5f);
    const Vec3 c{10, 10, 1}, hx = Vec3{s, s, 0} * 2.0f, hy = Vec3{-s, s, 0} * 1.0f, hz{0, 0, 0.5f};
    CHECK_THAT(intersect_box({{10, 10, 5}, {0, 0, -1}}, c, hx, hy, hz), WithinAbs(3.5, 1e-5));
    CHECK_THAT(intersect_box({{0, 0, 1}, {s, s, 0}}, c, hx, hy, hz), WithinAbs(std::sqrt(200.0f) - 2.0f, 1e-4));
    CHECK(intersect_box({{0, 0, 1}, {1, 0, 0}}, c, hx, hy, hz) < 0.0f);
    CHECK(intersect_box({{10, 10, 1}, {0, 0, 1}}, c, hx, hy, hz) < 0.0f); // starts inside
    CHECK(intersect_box({{10, 10, 5}, {0, 0, 1}}, c, hx, hy, hz) < 0.0f); // going away
    CHECK(intersect_box({{12, 8, 5}, {0, 0, -1}}, c, hx, hy, hz) < 0.0f); // beside it
}

TEST_CASE("raytrace: the hierarchy finds what every primitive finds", "[ui][raytrace]") {
    std::mt19937 rng(11);
    std::uniform_real_distribution<float> u(-50.0f, 50.0f), small(0.2f, 4.0f);
    std::vector<RayPrimitive> prims;
    for (int i = 0; i < 3000; ++i) {
        RayPrimitive p;
        const Vec3 c{u(rng), u(rng), u(rng) * 0.5f};
        if (i % 3 == 0) {
            p.box = true;
            const Vec3 x = random_unit(rng);
            const Vec3 y = normalize(cross(x, random_unit(rng)));
            p.p0 = c;
            p.p1 = x * small(rng);
            p.p2 = y * small(rng);
            p.p3 = cross(x, y) * small(rng);
        } else {
            p.p0 = c;
            p.p1 = c + random_unit(rng) * small(rng) * 2.0f;
            p.p2 = c + random_unit(rng) * small(rng) * 2.0f;
        }
        p.layer = std::uint32_t(i % 40);
        p.kind = std::uint32_t(i % 7);
        prims.push_back(p);
    }
    RayScene scene;
    scene.build(prims);
    REQUIRE(scene.primitive_count() == prims.size());
    CHECK(scene.depth() < 40);

    const RayFilter filters[] = {{}, {0, 19, 0xFFFFFFFFu}, {5, 30, 0b1010101u}};
    int hits = 0;
    for (int i = 0; i < 4000; ++i) {
        Ray ray{{u(rng), u(rng), u(rng) * 0.5f}, random_unit(rng) * (i % 2 ? 1.0f : 3.0f)};
        ray.tmax = i % 4 == 0 ? 10.0f : std::numeric_limits<float>::infinity();
        const RayFilter& f = filters[i % 3];
        const float expected = brute_closest(prims, ray, f);
        const float got = scene.closest(ray, f);
        INFO("ray " << i);
        if (expected < 0.0f) {
            CHECK(got < 0.0f);
            CHECK_FALSE(scene.occluded(ray, f));
        } else {
            ++hits;
            CHECK_THAT(got, WithinRel(expected, 1e-5f));
            CHECK(scene.occluded(ray, f));
        }
    }
    CHECK(hits > 500);
}

TEST_CASE("raytrace: the GPU's copy holds the hierarchy", "[ui][raytrace]") {
    std::vector<RayPrimitive> prims;
    for (int i = 0; i < 9; ++i) {
        RayPrimitive p;
        p.p0 = {float(i), 0, 0};
        p.p1 = {float(i) + 1, 0, 0};
        p.p2 = {float(i), 1, 0};
        p.layer = std::uint32_t(i);
        p.kind = 3;
        prims.push_back(p);
    }
    RayScene scene;
    scene.build(prims);
    const std::vector<float> nodes = scene.node_texels();
    const std::vector<float> texels = scene.primitive_texels();
    CHECK(nodes.size() % (std::size_t(RayScene::kTextureWidth) * 4) == 0);
    CHECK(texels.size() % (std::size_t(RayScene::kTextureWidth) * 4) == 0);
    // The root: the box of everything, an inner node with its children next.
    CHECK(nodes[0] == 0.0f);
    CHECK(nodes[4] == 9.0f);  // max x
    CHECK(nodes[3] == 1.0f);  // first child
    CHECK(nodes[7] == 0.0f);  // inner node
    CHECK(nodes[8] == 0.0f);  // first layer
    CHECK(nodes[9] == 8.0f);  // last layer
    // Every leaf's primitives, read back from the texels, are the scene's.
    std::size_t leaf_primitives = 0;
    for (std::size_t n = 0; n < scene.node_count(); ++n) {
        const float count = nodes[n * 12 + 7];
        if (count == 0.0f)
            continue;
        leaf_primitives += std::size_t(count);
        for (std::size_t i = 0; i < std::size_t(count); ++i) {
            const std::size_t at = (std::size_t(nodes[n * 12 + 3]) + i) * 16;
            CHECK(texels[at + 5] == 0.0f);             // e1.y of a triangle along x
            CHECK(texels[at + 4] == 1.0f);             // e1.x
            CHECK(texels[at + 7] == 0.0f);             // a triangle
            CHECK(texels[at + 11] == 3.0f);            // kind
            CHECK(texels[at + 3] == texels[at + 0]);   // layer = x of a
        }
    }
    CHECK(leaf_primitives == 9);
}

TEST_CASE("raytrace: scenes from the model and the preview", "[ui][raytrace]") {
    const TriangleMesh cube = read_stl(std::filesystem::path(TISMA_TEST_DATA_DIR) / "cube_ascii.stl").mesh;
    const BoundingBox box = cube.bounding_box();
    std::vector<RayPrimitive> prims = model_primitives(cube, {100, 100, -box.min.z});
    REQUIRE(prims.size() == cube.triangle_count());
    const std::vector<RayPrimitive> plate = plate_primitives({{0, 0}, {200, 0}, {200, 200}, {0, 200}});
    REQUIRE(plate.size() == 2);
    prims.insert(prims.end(), plate.begin(), plate.end());
    RayScene scene;
    scene.build(prims);
    const Vec3 top{100 + (box.min.x + box.max.x) / 2, 100 + (box.min.y + box.max.y) / 2, box.max.z - box.min.z};
    // Straight down from above the cube: its top; beside it: the plate.
    CHECK_THAT(scene.closest({top + Vec3{0, 0, 10}, {0, 0, -1}}), WithinAbs(10.0, 1e-4));
    CHECK_THAT(scene.closest({{5, 5, 10}, {0, 0, -1}}), WithinAbs(10.0, 1e-4));
    CHECK_FALSE(scene.occluded({{5, 5, 10}, {0, 0, 1}}));
    // The plate next to the cube is in its shadow from a low light on the other side.
    const Vec3 beside{100 + box.max.x + 1.0f, top.y, 0.01f};
    CHECK(scene.occluded({beside, normalize(Vec3{-1, 0, 0.3f})}));
    CHECK_FALSE(scene.occluded({beside, normalize(Vec3{1, 0, 0.3f})}));

    // The preview: beads hang under the path, as tall as the layer.
    const GcodePreview p = read_gcode_preview(";LAYER_CHANGE\n;Z:0.2\n;HEIGHT:0.2\nG1 Z0.2\n;TYPE:Perimeter\n"
                                              ";WIDTH:0.4\nG1 X10 E1\n;LAYER_CHANGE\n;Z:0.4\n;HEIGHT:0.2\nG1 Z0.4\n"
                                              ";TYPE:Internal infill\nG1 X0 E2\n");
    const std::vector<RayPrimitive> lines = preview_primitives(preview_buffers(p));
    REQUIRE(lines.size() == 2);
    CHECK(lines[0].bead);
    CHECK_THAT(lines[0].p0.z, WithinAbs(0.2, 1e-6));
    CHECK_THAT(lines[0].p1.x, WithinAbs(10.0, 1e-6));
    CHECK(lines[0].p2.z == kFreeEnd);
    CHECK(lines[1].layer == 1);
    CHECK(lines[1].kind == std::uint32_t(LineKind::SparseInfill));
    // A wall of three moves: straight on, then a right angle. The beads are
    // continuous: no gap at the joints, the corner closed by the mitre, and
    // the free start grown by half a width.
    {
        RayScene wall;
        wall.build(preview_primitives(preview_buffers(read_gcode_preview(
            ";LAYER_CHANGE\n;Z:0.2\n;HEIGHT:0.2\nG1 Z0.2\n;TYPE:Perimeter\n;WIDTH:0.4\nG1 X10 E1\nG1 X20 E2\nG1 Y10 E3\n"))));
        const auto down = [&](float x, float y) { return wall.closest({{x, y, 5}, {0, 0, -1}}); };
        for (const float x : {0.0f, 5.0f, 9.999f, 10.0f, 10.001f, 15.0f, 19.9f})
            CHECK_THAT(down(x, 0.0f), WithinAbs(4.8, 1e-4));
        CHECK(down(20.15f, -0.15f) > 0.0f); // the corner's outer square, inside the mitred beads
        CHECK(down(19.9f, 5.0f) > 0.0f);    // up the second side
        CHECK(down(-0.15f, 0.0f) > 0.0f);   // the free start
        CHECK(down(-0.25f, 0.0f) < 0.0f);
        CHECK(down(5.0f, 0.25f) < 0.0f);    // beside the bead
        // The same hits as one long bead: the joints add nothing.
        CHECK_THAT(down(10.0f, 0.15f), WithinAbs(down(5.0f, 0.15f), 1e-4));
        // A ray grazing along the top of the wall meets nothing: no joint sticks up.
        CHECK(wall.closest({{1, 0, 0.201f}, {1, 0, 0}}) < 0.0f);
    }
    RayScene preview;
    preview.build(lines);
    CHECK_THAT(preview.closest({{5, 0, 5}, {0, 0, -1}}), WithinAbs(4.6, 1e-5));
    // Showing only the first layer, or hiding infill, leaves the lower line.
    CHECK_THAT(preview.closest({{5, 0, 5}, {0, 0, -1}}, {0, 0, 0xFFFFFFFFu}), WithinAbs(4.8, 1e-5));
    CHECK_THAT(preview.closest({{5, 0, 5}, {0, 0, -1}}, {0, 9, ~(1u << unsigned(LineKind::SparseInfill))}),
               WithinAbs(4.8, 1e-5));
}
