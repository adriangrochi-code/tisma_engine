#include "config/settings.hpp"
#include "geom/polygons.hpp"
#include "io/stl.hpp"
#include "mesh/indexed_mesh.hpp"
#include "slice/pipeline.hpp"

#include <catch2/catch_approx.hpp>
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

TEST_CASE("speeds come only from the printer's profile, a stock Ender-3's when it has none", "[settings]") {
    // No speed of the engine's own (owner's decision, 2026-10-26).
    const Settings none = parse_settings("{}");
    CHECK(missing_speeds(none) ==
          std::vector<std::string>{"machine.corner_speed", "speed.walls", "speed.outer_wall", "speed.infill",
                                   "speed.solid_infill", "speed.top_solid_infill", "speed.support", "speed.travel",
                                   "speed.first_layer", "retraction.speed", "cooling.min_speed"});
    CHECK_THROWS_WITH(require_speeds(none), ContainsSubstring("no speed for machine.corner_speed, speed.walls"));
    CHECK(none.max_speed == 0.0); // no cap
    CHECK(none.max_feed_x == 0.0); // not known
    // Not set is null in the JSON, and comes back not set.
    CHECK_THAT(settings_json(none), ContainsSubstring("\"walls\": null"));
    CHECK(missing_speeds(parse_settings(settings_json(none))) == missing_speeds(none));
    CHECK(parse_settings(R"({"speed": {"walls": null, "outer_wall": null}, "retraction": {"speed": null},
                             "cooling": {"min_speed": null}, "machine": {"corner_speed": null, "max_feed_x": null}})")
              .wall_speed == 0.0);

    // The fallback fills only what is missing, with Creality's numbers.
    Settings some = parse_settings(R"({"speed": {"walls": 120, "outer_wall": "50%"}})");
    const std::vector<std::string> filled = fill_missing_speeds(some);
    CHECK(filled.size() == 9);
    CHECK(some.wall_speed == 120.0);           // the printer's stays
    CHECK(some.outer_wall_speed.of(120.0) == 60.0);
    CHECK(some.infill_speed == 60.0);
    CHECK(some.solid_infill_speed == 60.0);
    CHECK(some.top_solid_infill_speed == 40.0);
    CHECK(some.support_speed == 40.0);
    CHECK(some.travel_speed == 180.0);
    CHECK(some.first_layer_speed == 25.0);
    CHECK(some.retract_speed == 25.0);
    CHECK(some.min_print_speed == 10.0);
    CHECK(some.corner_speed == 10.0);
    CHECK(some.max_speed == 0.0); // limits are not filled
    CHECK(some.max_feed_x == 0.0);
    CHECK(missing_speeds(some).empty());
    CHECK_NOTHROW(require_speeds(some));
    CHECK(fill_missing_speeds(some).empty());
    Settings outer;
    fill_missing_speeds(outer);
    CHECK_FALSE(outer.outer_wall_speed.percent);
    CHECK(outer.outer_wall_speed.value == 30.0);
}

TEST_CASE("settings are read by group", "[settings]") {
    const Settings s = parse_settings(R"({
        "layers": {"height": 0.3, "first_height": 0.35},
        "slicing": {"fill_mode": "even_odd", "gap_closing_radius": 0.1, "xy_offset": -0.05, "first_layer_shrink": 0.2, "simplify_tolerance": 0},
        "compute": {"gpu": "off"},
        "walls": {"count": 2, "width": 0.45},
        "surfaces": {"top_layers": 4, "bottom_layers": 0},
        "infill": {"density": 35, "angle": 30, "pattern": "rectilinear"},
        "machine": {"nozzle_diameter": 0.6, "filament_diameter": 2.85, "relative_extrusion": true, "flavor": "klipper",
                    "start_gcode": "M104 S[temp]", "end_gcode": "M84", "variables": {"temp": 215, "names": ["a", "b"]}},
        "speed": {"walls": 50, "outer_wall": "40%", "travel": 150, "max": 100},
        "material": {"density": 1.24}
    })");
    CHECK(s.layer_height == 0.3);
    CHECK(s.first_layer_height == 0.35);
    CHECK(s.fill_mode == FillMode::EvenOdd);
    CHECK(s.gap_closing_radius == 0.1);
    CHECK(s.xy_offset == -0.05);
    CHECK(s.first_layer_shrink == 0.2);
    CHECK(s.simplify_tolerance == 0.0);
    CHECK(s.gpu == GpuMode::Off);
    CHECK(s.wall_count == 2);
    CHECK(s.wall_width == 0.45);
    CHECK(s.top_layers == 4);
    CHECK(s.bottom_layers == 0);
    CHECK(s.infill_density == 35.0);
    CHECK(s.infill_angle == 30.0);
    CHECK(s.infill_pattern == InfillPattern::Rectilinear);
    CHECK(s.nozzle_diameter == 0.6);
    CHECK(s.filament_diameter == 2.85);
    CHECK(s.relative_extrusion);
    CHECK(s.flavor == GcodeFlavor::Klipper);
    CHECK(s.start_gcode == "M104 S[temp]");
    CHECK(s.end_gcode == "M84");
    CHECK(s.variables.at("temp") == std::vector<std::string>{"215"});
    CHECK(s.variables.at("names") == std::vector<std::string>{"a", "b"});
    CHECK(s.wall_speed == 50.0);
    CHECK(s.outer_wall_speed.percent);
    CHECK(s.outer_wall_speed.of(s.wall_speed) == 20.0);
    CHECK(s.travel_speed == 150.0);
    CHECK(s.max_speed == 100.0);
    CHECK(s.infill_speed == 0.0); // not set: the printer's profile gives it
    CHECK(s.ignored_groups.empty());
    CHECK(s.filament_density == 1.24); // material.density is extrusion.density
}

TEST_CASE("settings: the specs' material group goes to the engine's keys", "[settings]") {
    const Settings s = parse_settings(R"({
        "material": {"nozzle_temp": 215, "first_nozzle_temp": 220, "bed_temp": 60, "first_bed_temp": 65,
                     "flow_factor": 0.95, "type": "PLA", "cost": 20, "variables": {"pa": "0.04"}}
    })");
    CHECK(s.variables.at("temperature") == std::vector<std::string>{"215"});
    CHECK(s.variables.at("first_layer_temperature") == std::vector<std::string>{"220"});
    CHECK(s.variables.at("bed_temperature") == std::vector<std::string>{"60"});
    CHECK(s.variables.at("first_layer_bed_temperature") == std::vector<std::string>{"65"});
    CHECK(s.variables.at("pa") == std::vector<std::string>{"0.04"});
    CHECK(s.flow == Catch::Approx(95.0));
    CHECK(s.filament_type == "PLA");
    CHECK(s.filament_cost == 20.0);
    // Written back under the engine's names, and read again the same.
    const Settings back = parse_settings(settings_json(s));
    CHECK(back.variables == s.variables);
    CHECK(back.filament_type == "PLA");
    // An unknown key, the group not an object, or a key given twice: errors.
    CHECK_THROWS_WITH(parse_settings(R"({"material": {"zz_not_a_key": 1}})"),
                      Catch::Matchers::ContainsSubstring("material.zz_not_a_key"));
    CHECK_THROWS_AS(parse_settings(R"({"material": 3})"), SettingsError);
    CHECK_THROWS_WITH(parse_settings(R"({"material": {"nozzle_temp": 215}, "machine": {"variables": {"temperature": 210}}})"),
                      Catch::Matchers::ContainsSubstring("also given"));
    CHECK_THROWS_AS(parse_settings(R"({"material": {"cost": 1}, "extrusion": {"cost": 2}})"), SettingsError);
    // The keys as the assistant names them.
    CHECK(engine_setting_key("material.nozzle_temp") == "machine.variables.temperature");
    CHECK(engine_setting_key("material.density") == "extrusion.density");
    CHECK(engine_setting_key("walls.count") == "walls.count");
    CHECK_FALSE(engine_setting_key("material.flow_factor"));
    CHECK_FALSE(engine_setting_key("material.zz_not_a_key"));
}

TEST_CASE("settings: the specs' names for settings the engine has under another (2026-10-28-ajustes.md)", "[settings]") {
    const Settings s = parse_settings(R"({"support": {"angle_limit": 40}, "arrange": {"distance": 9},
                                          "filament": {"pressure_advance": 0.04}, "machine": {"e_mode": "volume"}})");
    CHECK(s.support_angle == 40.0);
    CHECK(s.placement_spacing == 9.0);
    CHECK(s.pressure_advance == 0.04);
    CHECK(s.volumetric_e);
    CHECK_FALSE(parse_settings(R"({"machine": {"e_mode": "length"}})").volumetric_e);
    // Given under both names, or a value the spec does not have: errors.
    CHECK_THROWS_WITH(parse_settings(R"({"support": {"angle_limit": 40, "threshold_angle": 30}})"),
                      Catch::Matchers::ContainsSubstring("also given as support.threshold_angle"));
    CHECK_THROWS_WITH(parse_settings(R"({"machine": {"e_mode": "pellets"}})"),
                      Catch::Matchers::ContainsSubstring("machine.e_mode"));
    // A spec group with a key the engine does not have yet stays an error.
    CHECK_THROWS_AS(parse_settings(R"({"filament": {"transmission_distance": 5}})"), SettingsError);
    CHECK(engine_setting_key("support.angle_limit") == "support.threshold_angle");
    CHECK_FALSE(engine_setting_key("machine.e_mode"));
}

TEST_CASE("bad settings name the key", "[settings]") {
    CHECK_THROWS_WITH(parse_settings("{"), ContainsSubstring("not valid JSON"));
    CHECK_THROWS_WITH(parse_settings("[]"), ContainsSubstring("top level"));
    CHECK_THROWS_WITH(parse_settings(R"({"layer": {}})"), ContainsSubstring("layer: unknown group"));
    CHECK_THROWS_WITH(parse_settings(R"({"layers": {"hieght": 0.2}})"), ContainsSubstring("layers.hieght: unknown setting"));
    CHECK_THROWS_WITH(parse_settings(R"({"layers": {"height": "0.2"}})"), ContainsSubstring("layers.height: must be a number"));
    CHECK_THROWS_WITH(parse_settings(R"({"layers": {"first_height": 0}})"), ContainsSubstring("from 0.01 to 10 mm"));
    CHECK_THROWS_WITH(parse_settings(R"({"slicing": {"first_layer_shrink": -1}})"), ContainsSubstring("not be negative"));
    CHECK_THROWS_WITH(parse_settings(R"({"slicing": {"fill_mode": "xor"}})"), ContainsSubstring("must be one of"));
    CHECK_THROWS_WITH(parse_settings(R"({"layers": 3})"), ContainsSubstring("must be an object"));
    CHECK_THROWS_WITH(parse_settings(R"({"walls": {"count": 2.5}})"), ContainsSubstring("walls.count: must be a whole number"));
    CHECK_THROWS_WITH(parse_settings(R"({"walls": {"count": -1}})"), ContainsSubstring("walls.count: must be a whole number"));
    CHECK_THROWS_WITH(parse_settings(R"({"walls": {"width": 0}})"), ContainsSubstring("walls.width: must be from 0.05 to 40 mm"));
    CHECK_THROWS_WITH(parse_settings(R"({"surfaces": {"top_layers": 1.5}})"), ContainsSubstring("surfaces.top_layers: must be a whole number"));
    CHECK_THROWS_WITH(parse_settings(R"({"surfaces": {"bottom": 3}})"), ContainsSubstring("surfaces.bottom: unknown setting"));
    CHECK(parse_settings(R"({"infill": {"pattern": "grid"}})").infill_pattern == InfillPattern::Grid);
    CHECK(Settings().detect_overhangs);
    const Settings thin = parse_settings(R"({"walls": {"min_line_width": 0.3, "min_detail": "20%"}})");
    CHECK(thin.wall_min_line_width == Speed{0.3, false});
    CHECK(parse_settings(R"({"walls": {"generator": "fixed"}})").wall_generator == WallGenerator::Fixed);
    CHECK_THROWS_WITH(parse_settings(R"({"walls": {"generator": "arachne"}})"), ContainsSubstring("walls.generator: must be one of"));
    CHECK_FALSE(parse_settings(R"({"walls": {"gap_fill": false}})").wall_gap_fill);
    CHECK(thin.wall_min_detail == Speed{20.0, true});
    CHECK(Settings().wall_min_line_width == Speed{85.0, true});
    CHECK(parse_settings(R"({"support": {"width": "150%"}})").support_width == Speed{150.0, true});
    CHECK(parse_settings(R"({"support": {"width": 0.6}})").support_width == Speed{0.6, false});
    CHECK_THROWS_WITH(parse_settings(R"({"speed": {"over_bridges": "fast"}})"), ContainsSubstring("speed.over_bridges: must be"));
    CHECK_THROWS_WITH(parse_settings(R"({"speed": {"travel_z": 0}})"), ContainsSubstring("speed.travel_z: must be greater than 0"));
    CHECK_THROWS_WITH(parse_settings(R"({"extrusion": {"bridge_flow_ratio": 3}})"),
                      ContainsSubstring("extrusion.bridge_flow_ratio: must be from 0.1 to 2"));
    CHECK_THROWS_WITH(parse_settings(R"({"extrusion": {"infill_max_speed": -1}})"),
                      ContainsSubstring("extrusion.infill_max_speed: must be from 0 to 1000"));
    CHECK(parse_settings(R"({"speed": {"first_layer_infill": null}})").first_layer_infill_speed == Speed{});
    CHECK_THROWS_WITH(parse_settings(R"({"acceleration": {"walls": -1}})"),
                      ContainsSubstring("acceleration.walls: must be from 0 to 100000"));
    CHECK_THROWS_WITH(parse_settings(R"({"acceleration": {"wall": 1}})"), ContainsSubstring("acceleration.wall: unknown"));
    CHECK_THROWS_WITH(parse_settings(R"({"support": {"width": "0%"}})"), ContainsSubstring("percentage of the layer"));
    CHECK_THROWS_WITH(parse_settings(R"({"walls": {"min_detail": "x"}})"), ContainsSubstring("percentage of the nozzle"));
    const Settings arcs = parse_settings(R"({"arcs": {"enabled": true, "overlap": 20, "speed": 0, "max_distance": 8}})");
    CHECK(arcs.arcs);
    CHECK(arcs.arcs_overlap == 20.0);
    CHECK(arcs.arcs_speed == 0.0);
    CHECK(arcs.arcs_max_distance == 8.0);
    CHECK_FALSE(Settings().arcs);
    CHECK_THROWS_WITH(parse_settings(R"({"arcs": {"overlap": 95}})"), ContainsSubstring("arcs.overlap: must be from 0 to 90"));
    CHECK_FALSE(parse_settings(R"({"walls": {"detect_overhangs": false}})").detect_overhangs);
    CHECK_THROWS_WITH(parse_settings(R"({"walls": {"detect_overhangs": 1}})"), ContainsSubstring("must be true or false"));
    CHECK(parse_settings(R"({"speed": {"outer_wall": 35}})").outer_wall_speed.of(60.0) == 35.0);
    const Settings r = parse_settings(R"({"retraction": {"length": 0.8, "speed": 35, "lift": 0.2, "min_travel": 1.5},
                                          "walls": {"seam": "rear"}})");
    CHECK(r.retract_length == 0.8);
    CHECK(r.retract_speed == 35.0);
    CHECK(r.retract_lift == 0.2);
    CHECK(r.retract_min_travel == 1.5);
    CHECK(r.seam == SeamMode::Rear);
    CHECK(Settings{}.seam == SeamMode::Aligned);
    const Settings c = parse_settings(R"({"cooling": {"min_layer_time": 8, "min_speed": 15, "fan_min": 20, "fan_max": 90,
                                                      "fan_off_layers": 3}})");
    CHECK(c.min_layer_time == 8.0);
    CHECK(c.min_print_speed == 15.0);
    CHECK(c.fan_min == 20.0);
    CHECK(c.fan_max == 90.0);
    CHECK(c.fan_off_layers == 3);
    const Settings m = parse_settings(R"({"machine": {"max_feed_x": 300, "max_feed_z": 8, "acceleration": 2000,
                                                      "travel_acceleration": 3000, "corner_speed": 0}})");
    CHECK(m.max_feed_x == 300.0);
    CHECK(m.max_feed_y == 0.0); // not known
    CHECK(m.max_feed_z == 8.0);
    CHECK(m.acceleration == 2000.0);
    CHECK(m.travel_acceleration == 3000.0);
    CHECK(m.corner_speed == 0.0);
    CHECK(m.bed.empty()); // not checked unless given
    CHECK(parse_settings(R"({"machine": {"bed": []}})").bed.empty());
    const Settings b = parse_settings(R"({"machine": {"bed": [[0, 0], [200, 0], [100, 180]], "max_height": 150}})");
    REQUIRE(b.bed.size() == 3);
    CHECK(b.bed[2][0] == 100.0);
    CHECK(b.max_height == 150.0);
    CHECK_THROWS_WITH(parse_settings(R"({"machine": {"bed": [[0, 0], [1, 1]]}})"), ContainsSubstring("machine.bed: must be empty or a list of at least 3"));
    CHECK_THROWS_WITH(parse_settings(R"({"machine": {"bed": [[0, 0], [1, 1], [2]]}})"), ContainsSubstring("machine.bed: must be empty or a list of at least 3"));
    CHECK_THROWS_WITH(parse_settings(R"({"machine": {"max_height": -1}})"), ContainsSubstring("machine.max_height: must not be negative"));
    CHECK_THROWS_WITH(parse_settings(R"({"machine": {"acceleration": 0}})"), ContainsSubstring("machine.acceleration: must be greater than 0"));
    CHECK_THROWS_WITH(parse_settings(R"({"machine": {"corner_speed": -1}})"), ContainsSubstring("machine.corner_speed: must not be negative"));
    const Settings a = parse_settings(R"({"adhesion": {"skirt_loops": 2, "skirt_distance": 5, "skirt_layers": 3, "brim_width": 4}})");
    CHECK(a.skirt_loops == 2);
    CHECK(a.skirt_distance == 5.0);
    CHECK(a.skirt_layers == 3);
    CHECK(a.brim_width == 4.0);
    CHECK(Settings{}.skirt_loops == 1);
    CHECK(Settings{}.brim_width == 0.0);
    CHECK_THROWS_WITH(parse_settings(R"({"adhesion": {"skirt_loops": 1.5}})"), ContainsSubstring("adhesion.skirt_loops: must be a whole number"));
    CHECK_THROWS_WITH(parse_settings(R"({"adhesion": {"brim_width": -1}})"), ContainsSubstring("adhesion.brim_width: must not be negative"));
    const Settings sup = parse_settings(R"({"support": {"enabled": true, "threshold_angle": 50, "z_gap": 0.3, "xy_gap": 1}})");
    CHECK(sup.support);
    CHECK(sup.support_angle == 50.0);
    CHECK(sup.support_z_gap == 0.3);
    CHECK(sup.support_xy_gap == Speed{1.0, false});
    CHECK_FALSE(Settings{}.support);
    // The XY gap is half the wall width by default (supports_skirt.md, rule 2).
    CHECK(Settings{}.support_xy_gap == Speed{50.0, true});
    CHECK(parse_settings(R"({"support": {"xy_gap": "75%"}})").support_xy_gap == Speed{75.0, true});
    CHECK_THROWS_WITH(parse_settings(R"({"support": {"xy_gap": "75"}})"), ContainsSubstring("support.xy_gap: must be"));
    CHECK(parse_settings(R"({"support": {"line_angle": 45}})").support_line_angle == 45.0);
    const Settings sl = parse_settings(R"({"support": {"spacing": 3, "interface_layers": 2}, "speed": {"support": 60}})");
    CHECK(sl.support_spacing == 3.0);
    CHECK(sl.support_interface_layers == 2);
    CHECK(sl.support_speed == 60.0);
    CHECK_THROWS_WITH(parse_settings(R"({"support": {"spacing": 0}})"), ContainsSubstring("support.spacing: must be greater than 0"));
    CHECK_THROWS_WITH(parse_settings(R"({"support": {"threshold_angle": 90}})"), ContainsSubstring("support.threshold_angle: must be from 0"));
    CHECK_THROWS_WITH(parse_settings(R"({"support": {"enabled": 1}})"), ContainsSubstring("support.enabled: must be true or false"));
    CHECK_THROWS_WITH(parse_settings(R"({"cooling": {"fan_max": 120}})"), ContainsSubstring("cooling.fan_max: must be a percentage"));
    CHECK_THROWS_WITH(parse_settings(R"({"cooling": {"min_speed": 0}})"), ContainsSubstring("cooling.min_speed: must be greater than 0"));
    CHECK_THROWS_WITH(parse_settings(R"({"cooling": {"fan_off_layers": 1.5}})"), ContainsSubstring("cooling.fan_off_layers: must be a whole number"));
    CHECK_THROWS_WITH(parse_settings(R"({"retraction": {"speed": 0}})"), ContainsSubstring("retraction.speed: must be greater than 0"));
    CHECK_THROWS_WITH(parse_settings(R"({"retraction": {"length": -1}})"), ContainsSubstring("retraction.length: must not be negative"));
    CHECK_THROWS_WITH(parse_settings(R"({"walls": {"seam": "back"}})"), ContainsSubstring("walls.seam: must be one of"));
    CHECK_THROWS_WITH(parse_settings(R"({"speed": {"outer_wall": "fast"}})"), ContainsSubstring("speed.outer_wall: must be a number of mm/s or a percentage"));
    CHECK_THROWS_WITH(parse_settings(R"({"speed": {"travel": 0}})"), ContainsSubstring("speed.travel: must be greater than 0"));
    CHECK_THROWS_WITH(parse_settings(R"({"machine": {"flavor": "x3g"}})"), ContainsSubstring("machine.flavor: must be one of"));
    // Every flavor by its name, and back (gcode_extras.md, rule 6).
    for (const char* name : {"marlin", "klipper", "reprap", "repetier", "smoothie", "teacup", "makerware", "sailfish",
                             "mach3", "machinekit", "no_extrusion"}) {
        const Settings f = parse_settings(std::string(R"({"machine": {"flavor": ")") + name + "\"}}");
        CHECK(std::string(flavor_name(f.flavor)) == name);
        CHECK(parse_settings(settings_json(f)).flavor == f.flavor);
    }
    CHECK_THROWS_WITH(parse_settings(R"({"machine": {"relative_extrusion": 1}})"), ContainsSubstring("machine.relative_extrusion: must be true or false"));
    CHECK_THROWS_WITH(parse_settings(R"({"machine": {"variables": {"x": {"y": 1}}}})"), ContainsSubstring("machine.variables.x: values must be"));
    CHECK(parse_settings(R"({"infill": {"pattern": "gyroid"}})").infill_pattern == InfillPattern::Gyroid);
    CHECK_THROWS_WITH(parse_settings(R"({"infill": {"density": 120}})"), ContainsSubstring("infill.density: must be from 0 to 100"));
    CHECK_THROWS_WITH(parse_settings(R"({"infill": {"pattern": "voronoi"}})"), ContainsSubstring("infill.pattern: must be one of"));
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

TEST_CASE("slice_model builds the walls of every layer and leaves the middle for infill", "[settings]") {
    const IndexedMesh m = weld_exact(read_stl(std::filesystem::path(TISMA_TEST_DATA_DIR) / "cube_ascii.stl").mesh);
    Settings s; // 3 walls of 0.4 mm
    const SlicedModel sliced = slice_model(m, s);
    REQUIRE(sliced.walls.size() == sliced.layers.size());
    for (const LayerWalls& w : sliced.walls) {
        REQUIRE(w.toolpaths.size() == 3);
        for (const Toolpath& t : w.toolpaths) {
            CHECK(t.closed);
            for (const ToolpathPoint& q : t.points)
                CHECK_THAT(q.width, WithinAbs(0.4, 1e-9));
        }
        CHECK_THAT(area_mm2(w.infill), WithinAbs(7.6 * 7.6, 1e-6)); // 10 mm less 3 walls on each side
    }
    s.wall_count = 0;
    const SlicedModel bare = slice_model(m, s);
    for (std::size_t l = 0; l < bare.layers.size(); ++l) {
        CHECK(bare.walls[l].toolpaths.empty());
        CHECK_THAT(area_mm2(bare.walls[l].infill), WithinAbs(area_mm2(bare.regions[l]), 1e-9));
    }
}

TEST_CASE("settings written as JSON read back the same", "[settings]") {
    // Every setting away from its default, so a key written under the wrong
    // name or forgotten shows as a difference.
    const Settings s = parse_settings(R"({
        "layers": {"height": 0.15, "first_height": 0.25},
        "slicing": {"fill_mode": "fill_holes", "gap_closing_radius": 0.07, "xy_offset": -0.03, "first_layer_shrink": 0.1,
                    "simplify_tolerance": 0.02},
        "walls": {"count": 4, "width": 0.42, "seam": "rear", "generator": "fixed", "gap_fill": false},
        "surfaces": {"top_layers": 5, "bottom_layers": 4, "top_pattern": "rectilinear"},
        "infill": {"density": 15.5, "angle": 30, "pattern": "gyroid", "wall_overlap": 12},
        "machine": {"nozzle_diameter": 0.6, "filament_diameter": 2.85, "relative_extrusion": true, "flavor": "marlin",
                    "start_gcode": "G28\nM104 S[temp] ; \"quoted\"", "end_gcode": "M84",
                    "variables": {"temp": 215, "name": "PLA", "list": [1, "b", true]},
                    "max_feed_x": 300, "max_feed_y": 290, "max_feed_z": 15, "max_feed_e": 100, "acceleration": 2000,
                    "travel_acceleration": 3000, "corner_speed": 8, "bed": [[0, 0], [220, 0], [220, 230.5], [0, 230.5]],
                    "max_height": 250, "max_flow": 24, "max_nozzle_temp": 300, "max_bed_temp": 110,
                    "max_chamber_temp": 60, "enclosed": true, "hardened_nozzle": true, "z_offset": -0.15},
        "speed": {"walls": 55, "outer_wall": 35, "infill": 90, "solid_infill": 70, "top_solid_infill": 45, "support": 60,
                  "travel": 150, "first_layer": 25, "max": 120, "bridges": 30, "gap_fill": 15, "over_bridges": "60%",
                  "first_layer_infill": 35, "travel_z": 8, "dynamic_overhangs": true, "overhang_0": "40%",
                  "overhang_1": 12, "overhang_2": 18, "overhang_3": "90%", "flow_slope_up": 1.5, "flow_slope_down": 2.5},
        "acceleration": {"default": 2000, "walls": 1000, "outer_wall": 800, "infill": 3000, "solid_infill": 2500,
                         "top_solid_infill": 1500, "bridges": 500, "first_layer": 700, "travel": 4000,
                         "short_travel": 600},
        "retraction": {"length": 0.8, "speed": 35, "lift": 0.2, "min_travel": 1.5},
        "adhesion": {"skirt_loops": 2, "skirt_distance": 4, "skirt_layers": 2, "brim_width": 5, "raft_layers": 3,
                     "skirt_min_filament": 250, "draft_shield": "limited", "brim_type": "outer_and_inner", "brim_gap": 0.5,
                     "raft_gap": 0.15, "raft_expansion": 2, "raft_first_layer_density": 80, "raft_first_layer_expansion": 4},
        "support": {"enabled": true, "threshold_angle": 50, "z_gap": 0.15, "xy_gap": 0.8, "spacing": 2, "interface_layers": 2},
        "cooling": {"min_layer_time": 8, "min_speed": 12, "fan_min": 20, "fan_max": 90, "fan_off_layers": 2},
        "compute": {"gpu": "off"},
        "ironing": {"enabled": true, "surfaces": "topmost", "flow": 20, "spacing": 0.12, "speed": 18},
        "compensation": {"scale_x": 99.6, "scale_y": 100.4, "scale_z": 99.9, "hole_offset": 0.08},
        "extrusion": {"flow": 97, "pressure_advance": 0.04, "max_flow": 18, "chamber_temperature": 45, "abrasive": true,
                      "needs_enclosure": true, "infill_max_speed": 50, "infill_max_crossing_speed": 35,
                      "bridge_flow_ratio": 0.8, "chamber_min_temperature": 35, "density": 1.24, "cost": 25,
                      "spool_weight": 200, "shrinkage_xy": 0.6, "shrinkage_z": 1.5},
        "gcode": {"color_change": "M600 B1", "pause": "M601 ; [x]", "before_layer": ";B {layer_num}", "after_layer": ";A",
                  "volumetric_e": true, "verbose_comments": true,
                  "arcs": true},
        "changes": [{"z": 10, "temperature": 215, "gcode": "M117 hi", "color_change": true, "pause": true},
                    {"z": 5, "fan": 50, "retraction_length": 1.2,
                     "retraction_speed": 30, "flow": 95, "speed": 80, "bed_temperature": 65, "volumetric_flow": 12,
                     "acceleration": 3000}]
    })");
    REQUIRE(s.changes.size() == 2);
    CHECK(s.over_bridges_speed == Speed{60.0, true});
    CHECK(s.first_layer_infill_speed == Speed{35.0, false});
    CHECK(s.bridge_flow_ratio == 0.8);
    CHECK(s.shrinkage_xy == 0.6);
    CHECK(s.shrinkage_z == 1.5);
    CHECK(s.accel_short_travel == 600.0);
    CHECK(s.overhang_speeds[0] == Speed{40.0, true});
    CHECK(s.overhang_speeds[3] == Speed{90.0, true});
    CHECK(s.flow_slope_down == 2.5);
    CHECK(s.changes[0].z == 5.0); // sorted by height
    CHECK(s.changes[1].gcode == "M117 hi");
    CHECK(!s.changes[1].fan.has_value());
    const Settings back = parse_settings(settings_json(s));
    CHECK(describe(back) == describe(s));
    CHECK(back.variables == s.variables);
    CHECK(back.bed == s.bed);

    // A percentage speed stays a percentage, and the defaults come back as defaults.
    Settings percent;
    percent.outer_wall_speed = {40.0, true};
    CHECK(describe(parse_settings(settings_json(percent))) == describe(percent));
    CHECK(describe(parse_settings(settings_json(Settings{}))) == describe(Settings{}));
}

TEST_CASE("compensation, flow and changes are read with their ranges", "[settings]") {
    const Settings d = parse_settings("{}");
    CHECK(d.scale_x == 100.0);
    CHECK(d.hole_offset == 0.0);
    CHECK(d.flow == 100.0);
    CHECK(d.changes.empty());
    CHECK_THROWS_WITH(parse_settings(R"({"compensation": {"scale_x": 20}})"),
                      ContainsSubstring("compensation.scale_x: must be a percentage"));
    CHECK_THROWS_WITH(parse_settings(R"({"compensation": {"hole_offset": 9}})"),
                      ContainsSubstring("compensation.hole_offset"));
    CHECK_THROWS_WITH(parse_settings(R"({"extrusion": {"flow": 0}})"), ContainsSubstring("extrusion.flow"));
    CHECK_THROWS_WITH(parse_settings(R"({"extrusion": {"max_flow": -1}})"), ContainsSubstring("extrusion.max_flow"));
    CHECK_THROWS_WITH(parse_settings(R"({"extrusion": {"shrinkage_xy": 25}})"),
                      ContainsSubstring("extrusion.shrinkage_xy: must be a percentage from 0 to 20"));
    CHECK_THROWS_WITH(parse_settings(R"({"extrusion": {"chamber_temperature": 300}})"),
                      ContainsSubstring("extrusion.chamber_temperature"));
    CHECK_THROWS_WITH(parse_settings(R"({"extrusion": {"abrasive": 1}})"),
                      ContainsSubstring("extrusion.abrasive: must be true or false"));
    CHECK_THROWS_WITH(parse_settings(R"({"machine": {"max_flow": 2000}})"), ContainsSubstring("machine.max_flow"));
    CHECK_THROWS_WITH(parse_settings(R"({"machine": {"max_nozzle_temp": 900}})"),
                      ContainsSubstring("machine.max_nozzle_temp"));
    CHECK_THROWS_WITH(parse_settings(R"({"machine": {"enclosed": "yes"}})"),
                      ContainsSubstring("machine.enclosed: must be true or false"));
    CHECK_THROWS_WITH(parse_settings(R"({"changes": {"z": 1}})"), ContainsSubstring("changes: must be a list"));
    CHECK_THROWS_WITH(parse_settings(R"({"changes": [{"temperature": 200}]})"),
                      ContainsSubstring("changes[0].z: is missing"));
    CHECK_THROWS_WITH(parse_settings(R"({"changes": [{"z": 1, "temprature": 200}]})"),
                      ContainsSubstring("changes[0].temprature: unknown setting"));
    CHECK_THROWS_WITH(parse_settings(R"({"changes": [{"z": 1}, {"z": 2, "flow": 1}]})"),
                      ContainsSubstring("changes[1].flow"));
    CHECK_THROWS_WITH(parse_settings(R"({"changes": [{"z": 1, "acceleration": 10}]})"),
                      ContainsSubstring("changes[0].acceleration"));
    CHECK_THROWS_WITH(parse_settings(R"({"changes": [{"z": 1, "volumetric_flow": 0}]})"),
                      ContainsSubstring("changes[0].volumetric_flow"));
}

TEST_CASE("pellet printers: thick layers from a wide nozzle, and never thicker than it", "[settings]") {
    // An 8 mm nozzle laying 4 mm layers 9 mm wide (pellets.md: the profile pack's pellet printers).
    const Settings s = parse_settings(
        R"({"machine": {"nozzle_diameter": 8}, "layers": {"height": 4, "first_height": 4}, "walls": {"width": 9, "count": 2}})");
    CHECK(s.layer_height == 4.0);
    CHECK(s.wall_width == 9.0);
    CHECK_NOTHROW(check_settings(s));
    // A 100 x 100 x 40 mm box: ten layers, each with its walls.
    TriangleMesh m;
    const Vec3f v[8] = {{0, 0, 0}, {100, 0, 0}, {100, 100, 0}, {0, 100, 0}, {0, 0, 40}, {100, 0, 40}, {100, 100, 40}, {0, 100, 40}};
    const auto quad = [&](int a, int b, int c, int d) {
        m.add_triangle(v[a], v[b], v[c]);
        m.add_triangle(v[a], v[c], v[d]);
    };
    quad(0, 3, 2, 1);
    quad(4, 5, 6, 7);
    for (int i = 0; i < 4; ++i)
        quad(i, (i + 1) % 4, 4 + (i + 1) % 4, 4 + i);
    Settings cpu = s;
    cpu.gpu = GpuMode::Off;
    const SlicedModel sliced = slice_model(weld_exact(m), cpu);
    REQUIRE(sliced.layers.size() == 10);
    for (const LayerWalls& w : sliced.walls)
        CHECK_FALSE(w.toolpaths.empty());
    // Thicker than the nozzle is refused when the whole set is checked.
    CHECK_THROWS_WITH(
        check_settings(parse_settings(R"({"machine": {"nozzle_diameter": 8}, "layers": {"height": 9}, "walls": {"width": 9}})")),
        ContainsSubstring("thicker than 8 mm"));
}
