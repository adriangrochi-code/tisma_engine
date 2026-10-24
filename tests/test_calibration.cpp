#include "calibration/calibration.hpp"
#include "config/settings.hpp"
#include "gcode/check.hpp"
#include "gcode/writer.hpp"
#include "geom/polygons.hpp"
#include "mesh/indexed_mesh.hpp"
#include "slice/pipeline.hpp"
#include "slice/printable.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <array>
#include <cmath>
#include <numbers>
#include <sstream>
#include <string>

using namespace tisma;
using Catch::Matchers::ContainsSubstring;
using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

namespace {

Settings plain() {
    Settings s;
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
    Settings s;
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

    Settings s;
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
    for (CalibrationKind k : {CalibrationKind::Dimensions, CalibrationKind::Temperature, CalibrationKind::Retraction,
                              CalibrationKind::Flow, CalibrationKind::Speed}) {
        Settings run = plain();
        const CalibrationPrint p = automatic_calibration(k, run);
        run.changes = p.changes;
        const SlicedModel m = slice_model(weld_exact(p.mesh), run);
        INFO(p.kind);
        CHECK(checks(gcode_of(m, run), gcode_limits(m, run)));
    }
}
