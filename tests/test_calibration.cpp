#include "calibration/calibration.hpp"
#include "config/settings.hpp"
#include "gcode/check.hpp"
#include "gcode/writer.hpp"
#include "geom/polygons.hpp"
#include "mesh/indexed_mesh.hpp"
#include "slice/pipeline.hpp"
#include "slice/printable.hpp"
#include "test_printer.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <array>
#include <cmath>
#include <regex>
#include <set>
#include <numbers>
#include <sstream>
#include <string>

using namespace tisma;
using Catch::Matchers::ContainsSubstring;
using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

namespace {

Settings plain() {
    Settings s = test_printer();
    s.gpu = GpuMode::Off;
    s.skirt_loops = 0;
    return s;
}

std::string gcode_of(const SlicedModel& model, const Settings& s) {
    std::ostringstream out;
    write_gcode(out, model, s);
    return out.str();
}

// The check, with its first problem in the test's output when there is one.
bool checks(const std::string& g, const GcodeLimits& limits) {
    const GcodeCheck c = check_gcode(g, limits);
    if (!c.ok())
        UNSCOPED_INFO("first problem, line " << c.problems.front().line << ": " << c.problems.front().message);
    return c.ok();
}

std::size_t count(const std::string& text, const std::string& what) {
    std::size_t n = 0;
    for (std::size_t at = text.find(what); at != std::string::npos; at = text.find(what, at + 1))
        ++n;
    return n;
}

// The area of the layer region at that height.
double area_at(const SlicedModel& m, double z) {
    for (std::size_t l = 0; l < m.layers.size(); ++l)
        if (m.layers[l].bottom <= z && z < m.layers[l].top)
            return area_mm2(m.regions[l]);
    return 0.0;
}

} // namespace

TEST_CASE("calibration: the dimensional test is the block with its hole", "[calibration]") {
    const DimensionTest t;
    const CalibrationPrint p = dimension_test(t);
    CHECK(p.kind == "dimensions");
    const BoundingBox box = p.mesh.bounding_box();
    CHECK_THAT(box.max.x - box.min.x, WithinAbs(30.0, 1e-4));
    CHECK_THAT(box.max.y - box.min.y, WithinAbs(20.0, 1e-4));
    CHECK_THAT(box.max.z - box.min.z, WithinAbs(15.0, 1e-4));
    const double solid = 30.0 * 20.0 * 15.0 - std::numbers::pi * 25.0 * 15.0;
    CHECK_THAT(p.mesh.signed_volume(), WithinRel(solid, 2e-4));

    const SlicedModel m = slice_model(weld_exact(p.mesh), plain());
    CHECK_THAT(area_at(m, 7.5), WithinRel(30.0 * 20.0 - std::numbers::pi * 25.0, 2e-3));
    const Settings s = plain();
    CHECK(check_gcode(gcode_of(m, s), gcode_limits(m, s)).ok());
}

TEST_CASE("calibration: compensation scales the part and grows its holes", "[calibration]") {
    const IndexedMesh block = weld_exact(dimension_test().mesh);
    Settings s = plain();
    s.scale_x = 110.0;
    s.scale_z = 90.0;
    const IndexedMesh scaled = compensated(block, s);
    float x0 = 1e9f, x1 = -1e9f, z1 = -1e9f;
    for (const Vec3f& v : scaled.vertices)
        x0 = std::min(x0, v.x), x1 = std::max(x1, v.x), z1 = std::max(z1, v.z);
    CHECK_THAT(x1 - x0, WithinAbs(33.0, 1e-4));
    CHECK_THAT(x0 + x1, WithinAbs(0.0, 1e-4)); // around the footprint's centre
    CHECK_THAT(z1, WithinAbs(13.5, 1e-4));     // from the bottom
    const SlicedModel m = slice_model(block, s);
    CHECK_THAT(m.layers.back().top, WithinAbs(13.5, 0.2));

    // The hole grows by the offset on its radius; the outline does not move.
    Settings holes = plain();
    holes.hole_offset = 0.25;
    const double before = area_at(slice_model(block, plain()), 7.5);
    const double after = area_at(slice_model(block, holes), 7.5);
    CHECK_THAT(before - after, WithinRel(std::numbers::pi * (5.25 * 5.25 - 25.0), 0.02));
    holes.hole_offset = -0.25;
    CHECK_THAT(area_at(slice_model(block, holes), 7.5) - before, WithinRel(std::numbers::pi * (25.0 - 4.75 * 4.75), 0.02));

    // The printability checks see the part as it will be printed.
    Settings bed = plain();
    bed.bed = {{-20, -20}, {20, -20}, {20, 20}, {-20, 20}};
    CHECK(unprintable_reason(block, bed).empty());
    bed.scale_x = 150.0;
    CHECK_THAT(unprintable_reason(block, bed), ContainsSubstring("outside the bed"));
}

TEST_CASE("calibration: compensation from what was measured", "[calibration]") {
    const DimensionTest t;
    Settings s = test_printer();
    // As designed: nothing changes.
    Compensation c = compensation_from(t, {30.0, 20.0, 15.0, 10.0}, s);
    CHECK(c.scale_x == 100.0);
    CHECK(c.hole_offset == 0.0);
    // Oversized in X, short in Z, a small hole; Y not measured.
    c = compensation_from(t, {30.3, 0.0, 14.85, 9.7}, s);
    CHECK_THAT(c.scale_x, WithinAbs(100.0 * 30.0 / 30.3, 1e-9));
    CHECK(c.scale_y == 100.0);
    CHECK_THAT(c.scale_z, WithinAbs(100.0 * 15.0 / 14.85, 1e-9));
    const double grow = (30.0 / 30.3 + 1.0) / 2.0;
    CHECK_THAT(c.hole_offset, WithinAbs((10.0 - 9.7 * grow) / 2.0, 1e-9));
    // From an already compensated printer, the scales multiply.
    s.scale_x = 99.0;
    s.hole_offset = 0.1;
    c = compensation_from(t, {30.06, 20.0, 15.0, 10.0}, s);
    CHECK_THAT(c.scale_x, WithinAbs(99.0 * 30.0 / 30.06, 1e-9));
    CHECK(c.hole_offset > 0.1);
    // Clamped to the settings' ranges.
    CHECK(compensation_from(t, {3.0, 20.0, 15.0, 10.0}, s).scale_x == 200.0);
}

TEST_CASE("calibration: towers write each segment's value", "[calibration]") {
    CHECK(tower_values(230, 190, 5) == std::vector<double>{230, 225, 220, 215, 210, 205, 200, 195, 190});
    CHECK(tower_values(0.4, 2.0, 0.4).size() == 5);
    CHECK(tower_values(1, 1, 0).size() == 1);
    CHECK(tower_values(0, 1000, 1).size() == 40);

    SECTION("temperature") {
        const CalibrationPrint p = temperature_tower(230, 200, 10);
        REQUIRE(p.segments.size() == 4);
        CHECK(p.mesh.signed_volume() > 0.0);
        const BoundingBox box = p.mesh.bounding_box();
        CHECK_THAT(box.max.z, WithinAbs(40.0, 1e-5));
        CHECK_THAT(box.min.x, WithinAbs(-12.0 - 4.0 * std::tan(72.0 * std::numbers::pi / 180.0), 1e-4)); // the ridge
        CHECK_THAT(box.max.x, WithinAbs(20.0, 1e-5));  // the shelf
        CHECK_THAT(box.max.y, WithinAbs(10.0, 1e-5));  // the bracket and its pin
        CHECK_THAT(box.min.y, WithinAbs(-6.6, 1e-5));  // the relief
        Settings s = plain();
        s.changes = p.changes;
        const SlicedModel m = slice_model(weld_exact(p.mesh), s);
        const std::string g = gcode_of(m, s);
        CHECK(count(g, "M104 S") == 4);
        CHECK(g.find("M104 S230") < g.find("M104 S220"));
        CHECK(g.find("M104 S210") < g.find("M104 S200"));
        // The 200 C segment starts after Z 30: its M104 comes with the first layer above it.
        const std::size_t at = g.find("M104 S200");
        CHECK(g.rfind(";Z:30.2", at) != std::string::npos);
        CHECK(checks(g, gcode_limits(m, s)));

        // What each zone adds to the layers: the pin stands free (a small
        // square away from the body), and the top of the second segment holds
        // the ridge's widest point and the shelf.
        const auto box_at = [&](double z) {
            double lo[2] = {1e9, 1e9}, hi[2] = {-1e9, -1e9};
            for (std::size_t l = 0; l < m.layers.size(); ++l)
                if (m.layers[l].bottom <= z && z < m.layers[l].top)
                    for (const auto& poly : m.regions[l])
                        for (const auto& q : poly) {
                            lo[0] = std::min(lo[0], to_mm(q.x)), hi[0] = std::max(hi[0], to_mm(q.x));
                            lo[1] = std::min(lo[1], to_mm(q.y)), hi[1] = std::max(hi[1], to_mm(q.y));
                        }
            return std::array<double, 4>{lo[0], lo[1], hi[0], hi[1]};
        };
        CHECK_THAT(box_at(17.0)[3], WithinAbs(10.0, 0.05));   // the pin of the second segment
        CHECK_THAT(box_at(18.9)[0], WithinAbs(-24.3, 0.3));   // the ridge, at its widest
        CHECK_THAT(box_at(18.9)[2], WithinAbs(20.0, 0.05));   // the shelf
    }
    SECTION("retraction, checked by height") {
        const CalibrationPrint p = retraction_tower(0.5, 2.0, 0.5);
        Settings s = plain();
        s.changes = p.changes;
        const SlicedModel m = slice_model(weld_exact(p.mesh), s);
        const std::string g = gcode_of(m, s);
        CHECK(checks(g, gcode_limits(m, s)));
        // Without the changes, the checker sees the wrong retraction lengths.
        Settings without = plain();
        const GcodeCheck wrong = check_gcode(g, gcode_limits(m, without));
        CHECK_FALSE(wrong.ok());
        CHECK_THAT(wrong.problems.front().message, ContainsSubstring("retracts"));
    }
    SECTION("flow, checked by height") {
        const CalibrationPrint p = flow_tower(90, 110, 10);
        Settings s = plain();
        s.changes = p.changes;
        const SlicedModel m = slice_model(weld_exact(p.mesh), s);
        const std::string g = gcode_of(m, s);
        CHECK(checks(g, gcode_limits(m, s)));
        Settings without = plain();
        CHECK_FALSE(check_gcode(g, gcode_limits(m, without)).ok());
    }
    SECTION("speed") {
        const CalibrationPrint p = speed_tower(100, 200, 50);
        Settings s = plain();
        s.changes = p.changes;
        const SlicedModel m = slice_model(weld_exact(p.mesh), s);
        const std::string g = gcode_of(m, s);
        CHECK(checks(g, gcode_limits(m, s)));
        Settings without = plain();
        CHECK_FALSE(check_gcode(g, gcode_limits(m, without)).ok()); // feeds above the usual maximum
    }
}

TEST_CASE("calibration: a global flow and the changes' G-code", "[calibration]") {
    const CalibrationPrint p = flow_tower(100, 100, 1);
    Settings s = plain();
    s.flow = 95.0;
    HeightChange note;
    note.z = 2.0;
    note.gcode = "M117 [layer_note]";
    note.fan = 60.0;
    s.variables["layer_note"] = {"hello"};
    s.changes = {note};
    const SlicedModel m = slice_model(weld_exact(p.mesh), s);
    const std::string g = gcode_of(m, s);
    CHECK(count(g, "M117 hello") == 1);
    CHECK(g.find("M106 S153") != std::string::npos); // 60 %
    CHECK(checks(g, gcode_limits(m, s)));
    Settings full = s;
    full.flow = 100.0;
    CHECK_FALSE(check_gcode(g, gcode_limits(m, full)).ok());
}

TEST_CASE("calibration: labels, automatic ranges and the chosen value", "[calibration]") {
    CHECK(label_strokes("230", 3, 5, 1).size() == 5 + 5 + 6);
    CHECK(label_strokes("1.2", 3, 5, 1).size() == 2 + 1 + 5);
    CHECK(label_strokes("8", 3, 5, 1).size() == 7);
    const std::vector<Stroke> one = label_strokes("1", 3, 5, 1);
    for (const Stroke& st : one)
        CHECK((st.x0 == 3.0 && st.x1 == 3.0)); // the right side only
    const std::vector<Stroke> two = label_strokes("11", 3, 5, 1);
    CHECK(two.back().x0 == 7.0); // the second cell starts at width + gap

    Settings s = test_printer();
    s.variables["temperature"] = {"215"};
    CalibrationPrint t = automatic_calibration(CalibrationKind::Temperature, s);
    REQUIRE(!t.segments.empty());
    CHECK(t.segments.front().value == 235.0);
    CHECK(t.segments.back().value == 195.0);
    s.retract_length = 0.8;
    CHECK(automatic_calibration(CalibrationKind::Retraction, s).segments.front().value == 0.2);
    s.retract_length = 4.0;
    CHECK(automatic_calibration(CalibrationKind::Retraction, s).segments.front().value == 1.0);
    s.flow = 98.0;
    const CalibrationPrint f = automatic_calibration(CalibrationKind::Flow, s);
    CHECK(f.segments.front().value == 88.0);
    CHECK(f.segments.back().value == 108.0);
    CHECK(automatic_calibration(CalibrationKind::Dimensions, s).kind == "dimensions");

    s.changes = t.changes;
    REQUIRE(apply_calibration(CalibrationKind::Temperature, 220.0, s));
    CHECK(s.variables["temperature"] == std::vector<std::string>{"220"});
    CHECK(s.changes.empty());
    const double walls = s.wall_speed, top = s.max_speed;
    REQUIRE(apply_calibration(CalibrationKind::Speed, 150.0, s));
    CHECK(s.wall_speed == walls * 1.5);
    CHECK(s.max_speed == top * 1.5);
    REQUIRE(apply_calibration(CalibrationKind::Flow, 96.5, s));
    CHECK(s.flow == 96.5);
    CHECK_FALSE(apply_calibration(CalibrationKind::Dimensions, 1.0, s));

    // Every automatic print slices into G-code that passes its check.
    for (int i = 0; i < kCalibrationKinds; ++i) {
        const CalibrationKind k = CalibrationKind(i);
        Settings run = plain();
        const CalibrationPrint p = automatic_calibration(k, run);
        run.changes = p.changes;
        const SlicedModel m = slice_model(weld_exact(p.mesh), run);
        INFO(p.kind);
        CHECK(checks(gcode_of(m, run), gcode_limits(m, run)));
    }
}

TEST_CASE("calibration: kinds by name, which are towers, what each applies", "[calibration]") {
    for (int i = 0; i < kCalibrationKinds; ++i) {
        const auto k = CalibrationKind(i);
        CHECK(calibration_kind_from_name(calibration_kind_name(k)) == k);
        const CalibrationPrint p = automatic_calibration(k, test_printer());
        CHECK(p.kind == calibration_kind_name(k));
        CHECK(!p.mesh.empty());
        CHECK(is_tower(k) == !p.changes.empty());
        if (is_tower(k)) {
            const CalibrationPrint again = calibration_tower(k, p.segments.front().value, p.segments.back().value,
                                                             p.segments[1].value - p.segments[0].value,
                                                             p.segments[0].z1 - p.segments[0].z0);
            CHECK(again.segments.size() == p.segments.size());
        } else {
            CHECK(calibration_tower(k, 1, 2, 1, 5).mesh.empty());
        }
    }
    CHECK_FALSE(calibration_kind_from_name("pressure"));

    Settings s = test_printer();
    s.changes.push_back({});
    REQUIRE(apply_calibration(CalibrationKind::RetractionSpeed, 35.0, s));
    CHECK(s.retract_speed == 35.0);
    CHECK(s.changes.empty());
    REQUIRE(apply_calibration(CalibrationKind::PressureAdvance, 0.045, s));
    CHECK(s.pressure_advance == 0.045);
    const Settings before = s;
    CHECK_FALSE(apply_calibration(CalibrationKind::FirstLayer, 1.0, s));
    CHECK_FALSE(apply_calibration(CalibrationKind::Fit, 0.2, s));
    CHECK(settings_json(s) == settings_json(before));

    // Automatic ranges for the new towers.
    s.retract_length = 0.8;
    const CalibrationPrint direct = automatic_calibration(CalibrationKind::PressureAdvance, s);
    CHECK(direct.segments.front().value == 0.0);
    CHECK_THAT(direct.segments.back().value, WithinAbs(0.08, 1e-9));
    s.retract_length = 5.0;
    CHECK_THAT(automatic_calibration(CalibrationKind::PressureAdvance, s).segments.back().value, WithinAbs(0.8, 1e-9));
    const CalibrationPrint speeds = automatic_calibration(CalibrationKind::RetractionSpeed, s);
    CHECK(speeds.segments.front().value == 20.0);
    CHECK(speeds.segments.back().value == 60.0);
}

TEST_CASE("calibration: the retraction speed tower retracts at each segment's speed", "[calibration]") {
    Settings s = plain();
    const CalibrationPrint p = retraction_speed_tower(25.0, 55.0, 10.0);
    REQUIRE(p.segments.size() == 4);
    CHECK(p.unit == "mm/s");
    s.changes = p.changes;
    const SlicedModel m = slice_model(weld_exact(p.mesh), s);
    const std::string g = gcode_of(m, s);
    CHECK(checks(g, gcode_limits(m, s)));
    // Retractions and their returns: lines with E and F only.
    std::set<int> feeds;
    const std::regex retraction(R"(^G1 E-?[0-9.]+ F([0-9]+)$)");
    std::istringstream in(g);
    for (std::string line; std::getline(in, line);)
        if (std::smatch match; std::regex_match(line, match, retraction))
            feeds.insert(std::stoi(match[1]));
    CHECK(feeds == std::set<int>{1500, 2100, 2700, 3300});
}

TEST_CASE("calibration: pressure advance, a setting and a tower in each flavor's command", "[calibration]") {
    CHECK(pressure_advance_gcode(GcodeFlavor::Marlin, 0.05) == "M900 K0.0500 ; pressure advance");
    CHECK(pressure_advance_gcode(GcodeFlavor::Klipper, 0.05) == "SET_PRESSURE_ADVANCE ADVANCE=0.0500");
    CHECK(pressure_advance_gcode(GcodeFlavor::RepRap, 0.05) == "M572 D0 S0.0500 ; pressure advance");

    // The setting: read, written, its range, and written once at the start.
    Settings s = parse_settings(R"({"extrusion": {"pressure_advance": 0.035},
                                    "changes": [{"z": 2, "pressure_advance": 0.0}]})");
    CHECK(s.pressure_advance == 0.035);
    REQUIRE(s.changes.size() == 1);
    CHECK(s.changes[0].pressure_advance == 0.0);
    CHECK(settings_json(parse_settings(settings_json(s))) == settings_json(s));
    CHECK_THROWS_WITH(parse_settings(R"({"extrusion": {"pressure_advance": 3}})"),
                      ContainsSubstring("extrusion.pressure_advance"));
    s = plain();
    s.flavor = GcodeFlavor::Marlin;
    s.pressure_advance = 0.035;
    const CalibrationPrint cube = flow_tower(100, 100, 1);
    SlicedModel m = slice_model(weld_exact(cube.mesh), s);
    std::string g = gcode_of(m, s);
    CHECK(count(g, "M900 K0.0350") == 1);
    CHECK(g.find("M900 K0.0350") < g.find("G90\n"));
    s.pressure_advance = 0.0;
    CHECK(count(gcode_of(m, s), "M900") == 0);

    // The tower: the hollow square, its groove, and each segment's command.
    const CalibrationPrint p = pressure_advance_tower(0.0, 0.04, 0.01);
    REQUIRE(p.segments.size() == 5);
    CHECK(p.unit.empty());
    const BoundingBox box = p.mesh.bounding_box();
    CHECK_THAT(box.max.x - box.min.x, WithinAbs(36.0, 1e-4));
    CHECK_THAT(box.max.z, WithinAbs(25.0, 1e-4));
    for (const GcodeFlavor flavor : {GcodeFlavor::Marlin, GcodeFlavor::Klipper, GcodeFlavor::RepRap}) {
        Settings run = plain();
        run.flavor = flavor;
        run.changes = p.changes;
        m = slice_model(weld_exact(p.mesh), run);
        g = gcode_of(m, run);
        CHECK(checks(g, gcode_limits(m, run)));
        std::size_t at = 0;
        for (const CalibrationSegment& seg : p.segments) {
            const std::size_t found = g.find(pressure_advance_gcode(flavor, seg.value), at);
            REQUIRE(found != std::string::npos);
            CHECK(found > at); // in order, up the tower
            at = found;
        }
    }
    // The walls: 36 mm squared less the hole, and narrower at the groove.
    const SlicedModel tower = slice_model(weld_exact(p.mesh), plain());
    CHECK_THAT(area_at(tower, 2.5), WithinRel(36.0 * 36.0 - 32.8 * 32.8, 1e-2));
    CHECK_THAT(area_at(tower, 4.7), WithinRel(35.2 * 35.2 - 32.8 * 32.8, 3e-2));
}

TEST_CASE("calibration: first layer squares, fit test, volumetric flow and extruder steps", "[calibration]") {
    Settings s = plain();
    s.bed = {{0, 0}, {256, 0}, {256, 256}, {0, 256}};
    CalibrationPrint p = first_layer_test(s);
    CHECK(p.segments.empty());
    BoundingBox box = p.mesh.bounding_box();
    CHECK_THAT(box.max.x - box.min.x, WithinAbs(256.0 - 60.0, 1e-3));
    CHECK_THAT(box.max.z, WithinAbs(s.first_layer_height, 1e-6));
    // Centred on the bed, as the interface puts it: one layer of five squares.
    IndexedMesh placed = weld_exact(p.mesh);
    for (Vec3f& v : placed.vertices)
        v.x += 128.0f, v.y += 128.0f;
    SlicedModel m = slice_model(placed, s);
    REQUIRE(m.layers.size() == 1);
    CHECK_THAT(area_mm2(m.regions[0]), WithinRel(5 * 900.0, 1e-3));
    CHECK(checks(gcode_of(m, s), gcode_limits(m, s)));
    // A small bed keeps only the middle one.
    s.bed = {{0, 0}, {100, 0}, {100, 100}, {0, 100}};
    CHECK_THAT(first_layer_test(s).mesh.signed_volume(), WithinRel(900.0 * s.first_layer_height, 1e-4));

    // Fit: seven blocks with a pin in each hole, the clearance on the radius.
    p = fit_test();
    REQUIRE(p.segments.size() == 7);
    box = p.mesh.bounding_box();
    CHECK_THAT(box.max.x - box.min.x, WithinAbs(98.0, 1e-3));
    double holes = 0.0;
    for (const CalibrationSegment& seg : p.segments)
        holes += std::numbers::pi * (4.0 + seg.value) * (4.0 + seg.value);
    const double pins = 7.0 * std::numbers::pi * 16.0;
    m = slice_model(weld_exact(p.mesh), plain());
    // Above the labels (1 to 4 mm) and below the block's top; then the pins alone.
    CHECK_THAT(area_at(m, 4.5), WithinRel(98.0 * 16.0 - holes + pins, 5e-3));
    CHECK_THAT(area_at(m, 6.5), WithinRel(pins, 5e-3));
    // Every gap stays open, the 0.1 mm one too: the plate's outline, its seven
    // holes and the seven pins.
    for (std::size_t l = 0; l < m.layers.size(); ++l)
        if (m.layers[l].bottom <= 4.5 && 4.5 < m.layers[l].top)
            CHECK(m.regions[l].size() == 15);
    const Settings plain_settings = plain();
    CHECK(checks(gcode_of(m, plain_settings), gcode_limits(m, plain_settings)));

    // Volumetric flow of the fastest extrusion: 80 mm/s x 0.4 x 0.2 mm.
    const Settings d = test_printer();
    CHECK_THAT(volumetric_flow(d, 100.0), WithinAbs(6.4, 1e-9));
    CHECK_THAT(volumetric_flow(d, 150.0), WithinAbs(9.6, 1e-9));

    // Extruder steps: asked for 100 mm, 95 came out.
    CHECK_THAT(extruder_steps_from(GcodeFlavor::Marlin, 93.0, 100.0, 95.0), WithinAbs(93.0 * 100.0 / 95.0, 1e-9));
    CHECK_THAT(extruder_steps_from(GcodeFlavor::Klipper, 7.7, 100.0, 95.0), WithinAbs(7.7 * 0.95, 1e-9));
    CHECK(extruder_steps_from(GcodeFlavor::Marlin, 93.0, 100.0, 0.0) == 93.0);
    CHECK(extruder_steps_text(GcodeFlavor::Marlin, 97.894) == "M92 E97.89\nM500");
    CHECK(extruder_steps_text(GcodeFlavor::Klipper, 7.315) == "rotation_distance: 7.3150");
    Settings hot = test_printer();
    hot.variables["temperature"] = {"235"};
    const std::string test = extruder_test_gcode(hot);
    CHECK_THAT(test, ContainsSubstring("M109 S235"));
    CHECK_THAT(test, ContainsSubstring("G1 E100.0 F60"));
    CHECK_THAT(test, ContainsSubstring("120 mm above"));
}

TEST_CASE("calibration: flow limit and acceleration towers, the elephant's foot", "[calibration]") {
    // The flow tower: every extrusion of a segment runs at its flow, past the
    // speed cap and never slowed down for the minimum layer time.
    const CalibrationPrint flow = max_flow_tower(8.0, 16.0, 4.0);
    REQUIRE(flow.segments.size() == 3);
    CHECK(flow.unit == "mm³/s");
    Settings s = plain();
    s.max_speed = 20.0;
    s.min_layer_time = 60.0;
    s.max_feed_x = s.max_feed_y = 1000.0;
    s.changes = flow.changes;
    const SlicedModel m = slice_model(weld_exact(flow.mesh), s);
    const std::string g = gcode_of(m, s);
    CHECK(checks(g, gcode_limits(m, s)));
    // A 0.4 x 0.2 mm line at 16 mm3/s runs at 200 mm/s; at 8 mm3/s, 100 mm/s.
    const std::size_t top = g.find(";Z:10.2");
    REQUIRE(top != std::string::npos);
    CHECK(g.find("F12000", top) != std::string::npos);
    CHECK(g.find("F6000") < top);
    CHECK(g.find("F12000") > top);
    // What the machine cannot move is not asked.
    s.max_feed_x = s.max_feed_y = 150.0;
    const std::string slow = gcode_of(slice_model(weld_exact(flow.mesh), s), s);
    CHECK(slow.find("F12000") == std::string::npos);
    CHECK(slow.find("F9000", slow.find(";Z:10.2")) != std::string::npos);

    // The acceleration tower: the start writes the acceleration in use, each
    // segment its own, in the flavor's command.
    CHECK(acceleration_gcode(GcodeFlavor::Marlin, 1500.0, 2000.0) == "M204 P1500 T2000 ; acceleration");
    CHECK(acceleration_gcode(GcodeFlavor::RepRap, 1500.0, 2000.0) == "M204 P1500 T2000 ; acceleration");
    CHECK(acceleration_gcode(GcodeFlavor::Klipper, 1500.0, 2000.0) == "SET_VELOCITY_LIMIT ACCEL=1500");
    const CalibrationPrint acc = acceleration_tower(2000.0, 4000.0, 1000.0);
    CHECK(acc.unit == "mm/s²");
    for (GcodeFlavor flavor : {GcodeFlavor::Marlin, GcodeFlavor::Klipper, GcodeFlavor::RepRap}) {
        Settings a = plain();
        a.flavor = flavor;
        a.changes = acc.changes;
        const SlicedModel am = slice_model(weld_exact(acc.mesh), a);
        const std::string ag = gcode_of(am, a);
        INFO(int(flavor));
        CHECK(checks(ag, gcode_limits(am, a)));
        CHECK(ag.find(acceleration_gcode(flavor, a.acceleration, a.travel_acceleration)) < ag.find("G1 "));
        std::size_t at = 0;
        for (const CalibrationSegment& seg : acc.segments) {
            const std::size_t found = ag.find(acceleration_gcode(flavor, seg.value, a.travel_acceleration), at);
            REQUIRE(found != std::string::npos);
            at = found + 1;
        }
    }

    // The elephant's foot: a block, and the shrink from the bottom's excess.
    const CalibrationPrint foot = elephant_foot_test();
    CHECK(foot.kind == "elephant_foot");
    const BoundingBox box = foot.mesh.bounding_box();
    CHECK_THAT(box.max.x - box.min.x, WithinAbs(20.0, 1e-9));
    CHECK_THAT(box.max.z, WithinAbs(6.0, 1e-9));
    CHECK_FALSE(is_tower(CalibrationKind::ElephantFoot));
    Settings e = test_printer();
    CHECK_THAT(elephant_foot_from(20.4, 20.0, e), WithinAbs(0.2, 1e-9));
    e.first_layer_shrink = 0.1;
    CHECK_THAT(elephant_foot_from(20.3, 20.0, e), WithinAbs(0.25, 1e-9));
    CHECK(elephant_foot_from(0.0, 20.0, e) == 0.1); // not measured
    CHECK(elephant_foot_from(19.0, 20.0, e) == 0.0); // the bottom is narrower: nothing to shrink
    CHECK_FALSE(apply_calibration(CalibrationKind::ElephantFoot, 0.2, e));

    // Using the towers' values, and the automatic ranges.
    Settings t = test_printer();
    REQUIRE(apply_calibration(CalibrationKind::MaxFlow, 14.0, t));
    CHECK(t.machine_max_flow == 14.0);
    REQUIRE(apply_calibration(CalibrationKind::Acceleration, 3000.0, t));
    CHECK(t.acceleration == 3000.0);
    CHECK(t.travel_acceleration == 3000.0); // raised to it
    CHECK(automatic_calibration(CalibrationKind::MaxFlow, test_printer()).segments.front().value == 4.0);
    t.machine_max_flow = 20.0;
    const CalibrationPrint around = automatic_calibration(CalibrationKind::MaxFlow, t);
    CHECK(around.segments.front().value == 10.0);
    CHECK(around.segments.back().value == 30.0);
    CHECK(automatic_calibration(CalibrationKind::Acceleration, t).segments.back().value == 6000.0);
    CHECK(automatic_calibration(CalibrationKind::Acceleration, test_printer()).segments.back().value == 5000.0);
}


TEST_CASE("calibration: a forced flow on a wedge that thins to an edge checks", "[calibration]") {
    // A wedge that thins to an edge: the walls narrow to their thinnest beads.
    TriangleMesh wedge;
    const auto tri = [&](Vec3f a, Vec3f b, Vec3f c) { wedge.add_triangle(a, b, c); };
    const Vec3f a0{0, 0, 0}, b0{30, 0, 0}, c0{0, 6, 0}, a1{0, 0, 3}, b1{30, 0, 3}, c1{0, 6, 3};
    tri(a0, c0, b0);
    tri(a1, b1, c1);
    tri(a0, b0, b1), tri(a0, b1, a1);
    tri(b0, c0, c1), tri(b0, c1, b1);
    tri(c0, a0, a1), tri(c0, a1, c1);
    for (const double flow : {2.0, 4.0}) {
        Settings s = plain();
        s.wall_width = 0.8;
        s.layer_height = s.first_layer_height = 0.1;
        HeightChange c;
        c.volumetric_flow = flow;
        s.changes = {c};
        const SlicedModel m = slice_model(weld_exact(wedge), s);
        INFO(flow);
        const std::string g = gcode_of(m, s);
        const GcodeLimits limits = gcode_limits(m, s);
        CHECK(checks(g, limits));
        // The forced flow is applied (a full-width bead at that flow, at least) and stays within the limit.
        double fastest = 0.0, feed = 0.0;
        std::istringstream in(g);
        for (std::string line; std::getline(in, line);) {
            if (line.rfind("G1 ", 0) != 0)
                continue;
            if (const std::size_t f = line.find(" F"); f != std::string::npos)
                feed = std::stod(line.substr(f + 2)) / 60.0; // modal
            if (line.find(" E") != std::string::npos && (line.find(" X") != std::string::npos || line.find(" Y") != std::string::npos))
                fastest = std::max(fastest, feed);
        }
        CHECK(fastest >= flow / (s.wall_width * s.layer_height) - 1.0 / 60.0);
        CHECK(fastest <= limits.max_feed + 1.0 / 60.0);
    }
}
