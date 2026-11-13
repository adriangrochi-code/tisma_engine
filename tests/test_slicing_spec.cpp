// The cases of docs/spec/features/slicing.md (C1 to C16) on the engine's
// slicing: layer heights, the outline's edges, fill modes and offsets.

#include "config/settings.hpp"
#include "geom/polygons.hpp"
#include "mesh/indexed_mesh.hpp"
#include "mesh/mesh.hpp"
#include "slice/pipeline.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <algorithm>
#include <cmath>
#include <utility>
#include <limits>

using namespace tisma;
using Catch::Matchers::WithinAbs;

namespace {

void add_box(TriangleMesh& m, float x0, float y0, float z0, float x1, float y1, float z1, bool inverted = false) {
    const Vec3f v[8] = {{x0, y0, z0}, {x1, y0, z0}, {x1, y1, z0}, {x0, y1, z0},
                        {x0, y0, z1}, {x1, y0, z1}, {x1, y1, z1}, {x0, y1, z1}};
    const int faces[6][4] = {{0, 3, 2, 1}, {4, 5, 6, 7}, {0, 1, 5, 4}, {1, 2, 6, 5}, {2, 3, 7, 6}, {3, 0, 4, 7}};
    for (const auto& f : faces) {
        if (inverted) {
            m.add_triangle(v[f[0]], v[f[2]], v[f[1]]);
            m.add_triangle(v[f[0]], v[f[3]], v[f[2]]);
        } else {
            m.add_triangle(v[f[0]], v[f[1]], v[f[2]]);
            m.add_triangle(v[f[0]], v[f[2]], v[f[3]]);
        }
    }
}

// A square frame: side `outer`, a centred hole of side `inner`, made of four boxes.
void add_frame(TriangleMesh& m, float outer, float inner, float height) {
    const float a = outer / 2.0f, b = inner / 2.0f;
    add_box(m, -a, -a, 0, -b, a, height);
    add_box(m, b, -a, 0, a, a, height);
    add_box(m, -b, -a, 0, b, -b, height);
    add_box(m, -b, b, 0, b, a, height);
}

struct Box2 {
    double x0 = std::numeric_limits<double>::max(), y0 = x0, x1 = -x0, y1 = -x0;
};

Box2 bounds(const Polygons& region) {
    Box2 b;
    for (const Polygon& p : region)
        for (const auto& q : p) {
            b.x0 = std::min(b.x0, to_mm(q.x));
            b.x1 = std::max(b.x1, to_mm(q.x));
            b.y0 = std::min(b.y0, to_mm(q.y));
            b.y1 = std::max(b.y1, to_mm(q.y));
        }
    return b;
}

std::size_t islands(const Polygons& region) {
    return std::size_t(std::count_if(region.begin(), region.end(), [](const Polygon& p) { return Clipper2Lib::IsPositive(p); }));
}

SlicedModel slice(const TriangleMesh& m, const Settings& s) { return slice_model(weld_exact(m), s); }

TriangleMesh cube(float height = 10.0f, bool inverted = false) {
    TriangleMesh m;
    add_box(m, -10, -10, 0, 10, 10, height, inverted);
    return m;
}

void check_edges(const Polygons& region, double half, double margin = 1e-3) {
    const Box2 b = bounds(region);
    CHECK_THAT(b.x0, WithinAbs(-half, margin));
    CHECK_THAT(b.x1, WithinAbs(half, margin));
    CHECK_THAT(b.y0, WithinAbs(-half, margin));
    CHECK_THAT(b.y1, WithinAbs(half, margin));
}

} // namespace

TEST_CASE("slicing.md C1-C4: layer count and heights of the cube and the prisms", "[slicing-spec]") {
    Settings s;
    const SlicedModel c1 = slice(cube(), s);
    REQUIRE(c1.layers.size() == 50);
    CHECK_THAT(c1.layers.front().top, WithinAbs(0.2, 1e-9));
    CHECK_THAT(c1.layers.back().top, WithinAbs(10.0, 1e-9));
    for (const Polygons& r : c1.regions)
        check_edges(r, 10.0);
    // C2: 10.1 mm tall: 51 layers, the last at Z 10.2, 0.2 high.
    const SlicedModel c2 = slice(cube(10.1f), s);
    REQUIRE(c2.layers.size() == 51);
    CHECK_THAT(c2.layers.back().top, WithinAbs(10.2, 1e-6));
    CHECK_THAT(c2.layers.back().top - c2.layers.back().bottom, WithinAbs(0.2, 1e-6));
    // C3: 10.09 mm: 50 layers, the last at Z 10.0.
    const SlicedModel c3 = slice(cube(10.09f), s);
    REQUIRE(c3.layers.size() == 50);
    CHECK_THAT(c3.layers.back().top, WithinAbs(10.0, 1e-6));
    // C4: a first layer of 0.3: 50 layers at 0.3, 0.5 ... 10.1.
    s.first_layer_height = 0.3;
    const SlicedModel c4 = slice(cube(), s);
    REQUIRE(c4.layers.size() == 50);
    CHECK_THAT(c4.layers[0].top, WithinAbs(0.3, 1e-9));
    CHECK_THAT(c4.layers[1].top, WithinAbs(0.5, 1e-9));
    CHECK_THAT(c4.layers.back().top, WithinAbs(10.1, 1e-6));
}

TEST_CASE("slicing.md C9-C11: inverted normals, overlapping prisms, even-odd", "[slicing-spec]") {
    Settings s;
    // C9: every normal inverted: the same as C1.
    const SlicedModel c9 = slice(cube(10.0f, true), s);
    REQUIRE(c9.layers.size() == 50);
    for (const Polygons& r : c9.regions) {
        CHECK(islands(r) == 1);
        CHECK_THAT(area_mm2(r), WithinAbs(400.0, 1e-3));
        check_edges(r, 10.0);
    }
    // C10: two 15 x 20 prisms overlapping 10 mm in X: one island, as C1.
    TriangleMesh two;
    add_box(two, -10, -10, 0, 5, 10, 10);
    add_box(two, -5, -10, 0, 10, 10, 10);
    const SlicedModel c10 = slice(two, s);
    REQUIRE(c10.layers.size() == 50);
    for (const Polygons& r : c10.regions) {
        CHECK(islands(r) == 1);
        CHECK_THAT(area_mm2(r), WithinAbs(400.0, 1e-3));
    }
    // C11: even-odd: the zone covered twice is a hole; two 5 x 20 islands.
    s.fill_mode = FillMode::EvenOdd;
    const SlicedModel c11 = slice(two, s);
    for (const Polygons& r : c11.regions) {
        CHECK(islands(r) == 2);
        CHECK_THAT(area_mm2(r), WithinAbs(200.0, 1e-3));
    }
}

TEST_CASE("slicing.md C12, C13, C16: frames, holes and fill modes", "[slicing-spec]") {
    Settings s;
    // C12: a 30 mm frame with a 20 mm hole and a 10 mm cube in it: three outlines per layer.
    TriangleMesh c12;
    add_frame(c12, 30, 20, 5);
    add_box(c12, -5, -5, 0, 5, 5, 5);
    for (const Polygons& r : slice(c12, s).regions) {
        CHECK(r.size() == 3);
        CHECK(islands(r) == 2);
        CHECK_THAT(area_mm2(r), WithinAbs(900.0 - 400.0 + 100.0, 1e-3));
    }
    // C13: a 20 mm frame with a 10 mm hole and xy_offset -0.2: the edge at
    // +-9.8, the hole 0.2 wider each side.
    TriangleMesh frame;
    add_frame(frame, 20, 10, 5);
    s.xy_offset = -0.2;
    for (const Polygons& r : slice(frame, s).regions) {
        check_edges(r, 9.8);
        CHECK_THAT(area_mm2(r), WithinAbs(19.6 * 19.6 - 10.4 * 10.4, 1e-2));
    }
    // C16: fill_holes: the frame as C1, without its hole.
    s.xy_offset = 0.0;
    s.fill_mode = FillMode::FillHoles;
    for (const Polygons& r : slice(frame, s).regions) {
        CHECK(r.size() == 1);
        CHECK_THAT(area_mm2(r), WithinAbs(400.0, 1e-3));
    }
}

TEST_CASE("slicing.md C14, C15: xy_offset and first_layer_shrink", "[slicing-spec]") {
    Settings s;
    s.xy_offset = 0.2;
    for (const Polygons& r : slice(cube(), s).regions)
        check_edges(r, 10.2);
    s.xy_offset = 0.0;
    s.first_layer_shrink = 0.2;
    const SlicedModel c15 = slice(cube(), s);
    check_edges(c15.regions.front(), 9.8);
    for (std::size_t l = 1; l < c15.regions.size(); ++l)
        check_edges(c15.regions[l], 10.0);
}

TEST_CASE("slicing.md C5: the cone's edge is the cone's radius at the middle of the layer", "[slicing-spec]") {
    // Radius 10, 10 high, a base of 128 sides; 0.3 mm layers.
    TriangleMesh cone;
    const int n = 128;
    const Vec3f apex{0, 0, 10}, centre{0, 0, 0};
    for (int k = 0; k < n; ++k) {
        const double a0 = 2.0 * 3.14159265358979323846 * k / n, a1 = 2.0 * 3.14159265358979323846 * (k + 1) / n;
        const Vec3f p0{float(10 * std::cos(a0)), float(10 * std::sin(a0)), 0}, p1{float(10 * std::cos(a1)), float(10 * std::sin(a1)), 0};
        cone.add_triangle(p0, p1, apex);
        cone.add_triangle(centre, p1, p0);
    }
    Settings s;
    s.layer_height = s.first_layer_height = 0.3;
    const SlicedModel sliced = slice(cone, s);
    // The table's layers 1, 11, 21 and 31 (counted from 1): 9.85, 6.85, 3.85, 0.85.
    for (const auto& [layer, radius] : {std::pair{1, 9.85}, {11, 6.85}, {21, 3.85}, {31, 0.85}}) {
        INFO("layer " << layer);
        REQUIRE(std::size_t(layer) <= sliced.regions.size());
        double far = 0.0;
        for (const Polygon& p : sliced.regions[std::size_t(layer - 1)])
            for (const auto& q : p)
                far = std::max(far, std::hypot(to_mm(q.x), to_mm(q.y)));
        CHECK_THAT(far, WithinAbs(radius, 0.01));
    }
}

namespace {

// The cube of C1 with its right face moved `shift` mm out, on its own: the
// other faces end at x = 10, so every section has two cracks `shift` wide.
TriangleMesh cracked_cube(float shift) {
    TriangleMesh m;
    const float a = 10.0f, h = 10.0f, r = a + shift;
    const Vec3f v[8] = {{-a, -a, 0}, {a, -a, 0}, {a, a, 0}, {-a, a, 0}, {-a, -a, h}, {a, -a, h}, {a, a, h}, {-a, a, h}};
    const int faces[5][4] = {{0, 3, 2, 1}, {4, 5, 6, 7}, {0, 1, 5, 4}, {2, 3, 7, 6}, {3, 0, 4, 7}};
    for (const auto& f : faces) {
        m.add_triangle(v[f[0]], v[f[1]], v[f[2]]);
        m.add_triangle(v[f[0]], v[f[2]], v[f[3]]);
    }
    const Vec3f q[4] = {{r, -a, 0}, {r, a, 0}, {r, a, h}, {r, -a, h}};
    m.add_triangle(q[0], q[1], q[2]);
    m.add_triangle(q[0], q[2], q[3]);
    return m;
}

} // namespace

TEST_CASE("slicing.md C6-C8: cracks of 0.05 and 5 mm close; one of 12 mm drops the section", "[slicing-spec]") {
    Settings s;
    for (const auto& [shift, width] : {std::pair{0.05f, 20.05}, {5.0f, 25.0}}) {
        INFO("crack " << shift);
        const SlicedModel sliced = slice(cracked_cube(shift), s);
        REQUIRE(sliced.layers.size() == 50);
        CHECK(sliced.open_contours == 0);
        for (const Polygons& r : sliced.regions) {
            REQUIRE(islands(r) == 1);
            const Box2 b = bounds(r);
            // The spec's own program gave 24.94 for 5 mm; the engine closes to the model's 25.
            CHECK_THAT(b.x1 - b.x0, WithinAbs(width, 0.07));
        }
    }
    // 12 mm: not closed. Every section is dropped (never printed open) and counted.
    const SlicedModel wide = slice(cracked_cube(12.0f), s);
    CHECK(wide.open_contours > 0);
    for (const Polygons& r : wide.regions)
        CHECK(area_mm2(r) < 1.0);
}

TEST_CASE("slicing.md C17: simplify_tolerance takes points off the cylinder's outline", "[slicing-spec]") {
    TriangleMesh cyl;
    const int n = 128;
    for (int k = 0; k < n; ++k) {
        const double a0 = 2.0 * 3.14159265358979323846 * k / n, a1 = 2.0 * 3.14159265358979323846 * (k + 1) / n;
        const Vec3f b0{float(10 * std::cos(a0)), float(10 * std::sin(a0)), 0}, b1{float(10 * std::cos(a1)), float(10 * std::sin(a1)), 0};
        const Vec3f t0{b0.x, b0.y, 5}, t1{b1.x, b1.y, 5};
        cyl.add_triangle({0, 0, 0}, b1, b0);
        cyl.add_triangle({0, 0, 5}, t0, t1);
        cyl.add_triangle(b0, b1, t1);
        cyl.add_triangle(b0, t1, t0);
    }
    Settings s;
    s.simplify_tolerance = 0.0;
    const SlicedModel exact = slice(cyl, s);
    s.simplify_tolerance = 0.5;
    const SlicedModel coarse = slice(cyl, s);
    REQUIRE(exact.regions.size() > 11);
    REQUIRE(coarse.regions.size() > 11);
    // Layer 11 (from 1): every one of the 128 sides with 0 (and the points
    // where the plane crosses each side's diagonal); far fewer with 0.5 (the
    // spec's program: 65 and 16, with its G-code's own tolerance).
    CHECK(exact.regions[10].front().size() >= 128);
    const std::size_t few = coarse.regions[10].front().size();
    CHECK(few >= 12);
    CHECK(few <= 24);
}

TEST_CASE("slicing.md C18, C19: layers thicker than the nozzle are refused", "[slicing-spec]") {
    CHECK_THROWS_WITH(check_settings(parse_settings(R"({"layers": {"first_height": 0.5}})")),
                      Catch::Matchers::ContainsSubstring("thicker than 0.4 mm"));
    CHECK_THROWS_WITH(check_settings(parse_settings(R"({"layers": {"height": 0.5}})")),
                      Catch::Matchers::ContainsSubstring("thicker than 0.4 mm"));
}
