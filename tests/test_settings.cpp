#include "config/settings.hpp"
#include "geom/polygons.hpp"
#include "io/stl.hpp"
#include "slice/pipeline.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <algorithm>
#include <filesystem>

using namespace tisma;
using Catch::Matchers::ContainsSubstring;
using Catch::Matchers::WithinAbs;

TEST_CASE("empty settings give the defaults", "[settings]") {
    const Settings s = parse_settings("{}");
    CHECK(s.layer_height == 0.2);
    CHECK(s.first_layer_height == 0.2);
    CHECK(s.fill_mode == FillMode::Union);
    CHECK(s.gap_closing_radius == 0.049);
    CHECK(s.xy_offset == 0.0);
    CHECK(s.first_layer_shrink == 0.0);
    CHECK(s.simplify_tolerance == 0.01);
    CHECK(s.gpu == GpuMode::Auto);
}

TEST_CASE("settings are read by group", "[settings]") {
    const Settings s = parse_settings(R"({
        "layers": {"height": 0.3, "first_height": 0.35},
        "slicing": {"fill_mode": "even_odd", "gap_closing_radius": 0.1, "xy_offset": -0.05, "first_layer_shrink": 0.2, "simplify_tolerance": 0},
        "compute": {"gpu": "off"},
        "walls": {"anything": 1}
    })");
    CHECK(s.layer_height == 0.3);
    CHECK(s.first_layer_height == 0.35);
    CHECK(s.fill_mode == FillMode::EvenOdd);
    CHECK(s.gap_closing_radius == 0.1);
    CHECK(s.xy_offset == -0.05);
    CHECK(s.first_layer_shrink == 0.2);
    CHECK(s.simplify_tolerance == 0.0);
    CHECK(s.gpu == GpuMode::Off);
    CHECK(s.ignored_groups == std::vector<std::string>{"walls"});
}

TEST_CASE("bad settings name the key", "[settings]") {
    CHECK_THROWS_WITH(parse_settings("{"), ContainsSubstring("not valid JSON"));
    CHECK_THROWS_WITH(parse_settings("[]"), ContainsSubstring("top level"));
    CHECK_THROWS_WITH(parse_settings(R"({"layer": {}})"), ContainsSubstring("layer: unknown group"));
    CHECK_THROWS_WITH(parse_settings(R"({"layers": {"hieght": 0.2}})"), ContainsSubstring("layers.hieght: unknown setting"));
    CHECK_THROWS_WITH(parse_settings(R"({"layers": {"height": "0.2"}})"), ContainsSubstring("layers.height: must be a number"));
    CHECK_THROWS_WITH(parse_settings(R"({"layers": {"first_height": 0}})"), ContainsSubstring("greater than 0"));
    CHECK_THROWS_WITH(parse_settings(R"({"slicing": {"first_layer_shrink": -1}})"), ContainsSubstring("not be negative"));
    CHECK_THROWS_WITH(parse_settings(R"({"slicing": {"fill_mode": "xor"}})"), ContainsSubstring("must be one of"));
    CHECK_THROWS_WITH(parse_settings(R"({"layers": 3})"), ContainsSubstring("must be an object"));
}

TEST_CASE("fill modes on overlapping and nested squares", "[settings]") {
    auto sq = [](double x, double y, double side) {
        return Polygon{{to_units(x), to_units(y)}, {to_units(x + side), to_units(y)},
                       {to_units(x + side), to_units(y + side)}, {to_units(x), to_units(y + side)}};
    };
    Polygon hole = sq(3, 3, 4);
    std::reverse(hole.begin(), hole.end());
    // Outer square with a hole, plus an island inside the hole.
    const Polygons ring_island = {sq(0, 0, 10), hole, sq(4, 4, 2)};
    CHECK_THAT(area_mm2(layer_region(ring_island, FillMode::Union)), WithinAbs(84.0 + 4.0, 1e-9));
    CHECK_THAT(area_mm2(layer_region(ring_island, FillMode::FillHoles)), WithinAbs(100.0, 1e-9));
    // Two nested squares wound the same way: union keeps both as one, even-odd makes a hole.
    const Polygons nested = {sq(0, 0, 10), sq(3, 3, 4)};
    CHECK_THAT(area_mm2(layer_region(nested, FillMode::Union)), WithinAbs(100.0, 1e-9));
    CHECK_THAT(area_mm2(layer_region(nested, FillMode::EvenOdd)), WithinAbs(84.0, 1e-9));
}

TEST_CASE("slice_model applies XY offset and the first-layer shrink", "[settings]") {
    const IndexedMesh m = weld_exact(read_stl(std::filesystem::path(TISMA_TEST_DATA_DIR) / "cube_ascii.stl").mesh);
    Settings s;
    s.xy_offset = -0.1;
    s.first_layer_shrink = 0.2;
    const SlicedModel sliced = slice_model(m, s);
    REQUIRE(sliced.layers.size() == 50);
    CHECK_THAT(area_mm2(sliced.regions[0]), WithinAbs(9.4 * 9.4, 1e-6)); // -0.1 - 0.2 on each side
    for (std::size_t l = 1; l < sliced.regions.size(); ++l)
        CHECK_THAT(area_mm2(sliced.regions[l]), WithinAbs(9.8 * 9.8, 1e-6));
    CHECK(sliced.open_contours == 0);
}
