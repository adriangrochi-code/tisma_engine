#include "geom/polygons.hpp"
#include "io/stl.hpp"
#include "mesh/indexed_mesh.hpp"
#include "slice/layers.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <algorithm>
#include <cmath>
#include <filesystem>

using namespace tisma;
using Catch::Matchers::WithinAbs;

namespace {

// Axis-aligned square in mm; counter-clockwise unless clockwise is set.
Polygon square(double x0, double y0, double side, bool clockwise = false) {
    Polygon p = {{to_units(x0), to_units(y0)},
                 {to_units(x0 + side), to_units(y0)},
                 {to_units(x0 + side), to_units(y0 + side)},
                 {to_units(x0), to_units(y0 + side)}};
    if (clockwise)
        std::reverse(p.begin(), p.end());
    return p;
}

} // namespace

TEST_CASE("unit conversion is exact for float millimetres", "[geom]") {
    CHECK(to_units(1.0) == 1000000);
    CHECK(to_units(-0.0000005) == -1); // rounds half away from zero
    CHECK(to_mm(to_units(123.456)) == 123.456);
}

TEST_CASE("layer region merges overlaps and keeps holes", "[geom]") {
    CHECK_THAT(area_mm2(layer_region({square(0, 0, 10), square(5, 5, 10)})), WithinAbs(175.0, 1e-9));
    CHECK_THAT(area_mm2(layer_region({square(0, 0, 10), square(3, 3, 4, true)})), WithinAbs(84.0, 1e-9));
    // A mesh wound the wrong way gives clockwise outlines; the region is the same.
    CHECK_THAT(area_mm2(layer_region({square(0, 0, 10, true)})), WithinAbs(100.0, 1e-9));
    // Two nested outer shells with the same orientation count once.
    CHECK_THAT(area_mm2(layer_region({square(0, 0, 10), square(3, 3, 4)})), WithinAbs(100.0, 1e-9));
}

TEST_CASE("offset grows outer contours and shrinks holes", "[geom]") {
    const Polygons ring = layer_region({square(0, 0, 10), square(3, 3, 4, true)});
    // Mitered corners: outer 12 x 12, hole 2 x 2.
    CHECK_THAT(area_mm2(offset(ring, 1.0)), WithinAbs(144.0 - 4.0, 1e-6));
    // Shrinking: outer 9 x 9, hole 5 x 5.
    CHECK_THAT(area_mm2(offset(ring, -0.5)), WithinAbs(81.0 - 25.0, 1e-6));
    // Shrinking a 2 mm square by 1.5 mm leaves nothing.
    CHECK(offset(layer_region({square(0, 0, 2)}), -1.5).empty());
    CHECK_THAT(area_mm2(offset(ring, 0.0)), WithinAbs(84.0, 1e-9));
}

TEST_CASE("sliced cube layers become 100 mm2 regions", "[geom]") {
    const IndexedMesh m = weld_exact(read_stl(std::filesystem::path(TISMA_TEST_DATA_DIR) / "cube_ascii.stl").mesh);
    const std::vector<float> z = {0.1f, 5.0f, 9.9f};
    const auto layers = slice_levels(m, height_field(m), z);
    const std::vector<Polygons> regions = layer_regions(layers);
    REQUIRE(regions.size() == layers.size());
    for (const Polygons& region : regions) {
        REQUIRE(region.size() == 1);
        CHECK_THAT(area_mm2(region), WithinAbs(100.0, 1e-9));
        // XY compensation of -0.1 mm: a 9.8 mm square.
        CHECK_THAT(area_mm2(offset(region, -0.1)), WithinAbs(9.8 * 9.8, 1e-6));
    }
}

TEST_CASE("open contours do not become polygons", "[geom]") {
    Contour open;
    open.points = {{0, 0, 0}, {1, 0, 0}, {1, 1, 0}};
    CHECK(to_polygons({open}).empty());
}

TEST_CASE("simplify removes nearly collinear vertices and keeps the area", "[geom]") {
    // A circle of radius 50 mm with 708 vertices deviates 0.0005 mm from the true
    // circle; simplifying at 0.01 mm leaves far fewer vertices and almost the same area.
    Polygon circle;
    for (int i = 0; i < 708; ++i) {
        const double a = 2.0 * 3.14159265358979323846 * i / 708;
        circle.emplace_back(to_units(50 * std::cos(a)), to_units(50 * std::sin(a)));
    }
    const Polygons simple = simplify({circle}, 0.01);
    REQUIRE(simple.size() == 1);
    CHECK(simple[0].size() < 300);
    CHECK_THAT(area_mm2(simple), WithinAbs(area_mm2({circle}), 0.001 * area_mm2({circle})));
    CHECK(simplify({circle}, 0.0)[0].size() == 708);
    // A square has nothing to simplify.
    CHECK(simplify({square(0, 0, 10)}, 0.01)[0].size() == 4);
}
