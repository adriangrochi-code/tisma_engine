// Untrusted input and settings at their limits, from the deep review of
// 2026-10-25: each case here once crashed, hung or wrote dangerous G-code.

#include "calibration/calibration.hpp"
#include "config/analysis_case.hpp"
#include "config/profiles.hpp"
#include "config/settings.hpp"
#include "geom/medial_axis.hpp"
#include "mesh/indexed_mesh.hpp"
#include "gcode/check.hpp"
#include "gcode/writer.hpp"
#include "perimeters/walls.hpp"
#include "project/project.hpp"
#include "slice/pipeline.hpp"
#include "slice/printable.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <sstream>
#include <string>

using namespace tisma;

namespace {

IndexedMesh cube(float side) {
    IndexedMesh m;
    m.vertices = {{0, 0, 0}, {side, 0, 0}, {side, side, 0}, {0, side, 0},
                  {0, 0, side}, {side, 0, side}, {side, side, side}, {0, side, side}};
    auto quad = [&](std::uint32_t a, std::uint32_t b, std::uint32_t c, std::uint32_t d) {
        m.triangles.push_back({a, b, c});
        m.triangles.push_back({a, c, d});
    };
    quad(0, 3, 2, 1);
    quad(4, 5, 6, 7);
    for (std::uint32_t i = 0; i < 4; ++i)
        quad(i, (i + 1) % 4, 4 + (i + 1) % 4, 4 + i);
    return m;
}

std::string nested(std::size_t depth) { return std::string(depth, '[') + std::string(depth, ']'); }

} // namespace

TEST_CASE("hostile input: JSON nested too deeply is refused, not a stack overflow", "[hostile]") {
    CHECK(json_depth_ok("{\"a\":[[1,2]],\"b\":\"[[[[[[\"}", 3));
    CHECK(!json_depth_ok(nested(65)));
    CHECK(json_depth_ok(nested(64)));
    // Brackets inside strings, escaped quotes included, do not count.
    CHECK(json_depth_ok("{\"x\":\"\\\"[[[[[[[[\"}", 2));
    const std::string deep = nested(100000);
    CHECK_THROWS_AS(parse_settings("{\"layers\":" + deep + "}"), SettingsError);
    CHECK_THROWS_AS(
        parse_profile("{\"format\":\"tisma-profile\",\"version\":1,\"kind\":\"print\",\"name\":\"x\",\"settings\":{\"layers\":" +
                      deep + "}}"),
        SettingsError);
    CHECK_THROWS_AS(parse_analysis_case("{\"loads\":" + deep + "}"), SettingsError);
}

TEST_CASE("hostile input: JSON errors of any kind come back as the file's own error", "[hostile]") {
    // A number out of range, a format or kind of the wrong type.
    CHECK_THROWS_AS(parse_settings("{\"layers\":{\"height\":1e400}}"), SettingsError);
    CHECK_THROWS_AS(parse_profile("{\"format\":5}"), SettingsError);
    CHECK_THROWS_AS(parse_profile("{\"format\":\"tisma-profile\",\"version\":1,\"kind\":5,\"name\":\"x\",\"settings\":{}}"),
                    SettingsError);
}

TEST_CASE("hostile input: a start G-code variable cannot add G-code after the print", "[hostile]") {
    Settings s = parse_settings("{\"machine\":{\"variables\":{\"note\":\"PLA\\nG1 X0 Y0\"}}}");
    s.gpu = GpuMode::Off;
    const SlicedModel m = slice_model(cube(5.0f), s);
    std::ostringstream g;
    write_gcode(g, m, s);
    const std::string text = g.str();
    // The value stays in its comment, its line break written out.
    CHECK(text.find("\nG1 X0 Y0") == std::string::npos);
    CHECK(text.find("; machine.variables.note = PLA\\nG1 X0 Y0\n") != std::string::npos);
    // Names with a line break or brackets are refused.
    CHECK_THROWS_AS(parse_settings("{\"machine\":{\"variables\":{\"x\\nG1 Z-5\":\"1\"}}}"), SettingsError);
    CHECK_THROWS_AS(parse_settings("{\"machine\":{\"variables\":{\"a]b\":\"1\"}}}"), SettingsError);
}

TEST_CASE("hostile input: a percentage of infinity is refused", "[hostile]") {
    CHECK_THROWS_AS(parse_settings("{\"speed\":{\"outer_wall\":\"inf%\"}}"), SettingsError);
    CHECK_THROWS_AS(parse_settings("{\"speed\":{\"outer_wall\":\"1e999%\"}}"), SettingsError);
    CHECK_THROWS_AS(parse_settings("{\"speed\":{\"outer_wall\":\"nan%\"}}"), SettingsError);
    CHECK(parse_settings("{\"speed\":{\"outer_wall\":\"50%\"}}").outer_wall_speed.value == 50.0);
}

TEST_CASE("hostile input: settings out of reach of the printer are refused or warned", "[hostile]") {
    // Bounds of the file itself: offsets that overflowed the polygons, layers
    // so thin they ran out of memory, a lift off the machine, a retraction
    // that pulls the filament out.
    for (const char* bad : {R"({"slicing":{"xy_offset":-1e15}})", R"({"slicing":{"first_layer_shrink":6}})",
                            R"({"layers":{"height":1e-4}})", R"({"layers":{"first_height":9.5e-6}})",
                            R"({"walls":{"width":1e-4}})", R"({"retraction":{"lift":100000}})",
                            R"({"retraction":{"length":100000}})"}) {
        INFO(bad);
        CHECK_THROWS_AS(parse_settings(bad), SettingsError);
    }
    // The whole set against the nozzle: layers thicker than it, or than the
    // printer's own limit, and lines it cannot lay.
    Settings s;
    CHECK_NOTHROW(check_settings(s));
    s.layer_height = 0.6;
    CHECK_THROWS_AS(check_settings(s), SettingsError);
    s = Settings{};
    s.first_layer_height = 1.0;
    CHECK_THROWS_AS(check_settings(s), SettingsError);
    s = Settings{};
    s.max_layer_height = 0.3;
    s.layer_height = 0.32;
    CHECK_THROWS_AS(check_settings(s), SettingsError);
    s = Settings{};
    s.min_layer_height = 0.1;
    s.layer_height = 0.08;
    CHECK_THROWS_AS(check_settings(s), SettingsError);
    s = Settings{};
    s.wall_width = 0.05;
    CHECK_THROWS_AS(check_settings(s), SettingsError);
    s.wall_width = 1.7;
    CHECK_THROWS_AS(check_settings(s), SettingsError);
    // A 0.8 mm nozzle lays 0.6 mm layers and 1.2 mm lines.
    s = Settings{};
    s.nozzle_diameter = 0.8;
    s.layer_height = 0.6;
    s.wall_width = 1.2;
    CHECK_NOTHROW(check_settings(s));
    // Wide but possible lines, and a tower hotter than the printer: warnings.
    s = Settings{};
    s.wall_width = 1.2;
    s.max_nozzle_temp = 260.0;
    HeightChange hot;
    hot.z = 2.0;
    hot.temperature = 450.0;
    s.changes.push_back(hot);
    std::vector<std::string> kinds;
    for (const Warning& w : capability_warnings(s))
        kinds.push_back(w.kind);
    CHECK(kinds == std::vector<std::string>{"nozzle_temperature", "line_width"});
}

TEST_CASE("hostile input: calibrations stay inside what the settings can read back", "[hostile]") {
    const auto values = [](const CalibrationPrint& c) {
        std::vector<double> out;
        for (const CalibrationSegment& seg : c.segments)
            out.push_back(seg.value);
        return out;
    };
    const auto within = [&](const CalibrationPrint& c, double lo, double hi) {
        for (const double v : values(c))
            if (v < lo - 1e-9 || v > hi + 1e-9)
                return false;
        return !c.segments.empty();
    };
    Settings s;
    s.flow = 15.0;
    CHECK(within(automatic_calibration(CalibrationKind::Flow, s), 10.0, 300.0));
    s = Settings{};
    s.acceleration = 60000.0;
    CHECK(within(automatic_calibration(CalibrationKind::Acceleration, s), 100.0, 100000.0));
    s = Settings{};
    s.machine_max_flow = 800.0;
    CHECK(within(automatic_calibration(CalibrationKind::MaxFlow, s), 0.5, 1000.0));
    s.machine_max_flow = 0.3;
    CHECK(within(automatic_calibration(CalibrationKind::MaxFlow, s), 0.5, 1000.0));
    // A temperature tower never goes past what the printer reaches.
    s = Settings{};
    s.variables["temperature"] = {"250"};
    s.max_nozzle_temp = 260.0;
    CHECK(within(automatic_calibration(CalibrationKind::Temperature, s), 100.0, 260.0));
    // The chosen value too, and the settings read back.
    s = Settings{};
    REQUIRE(apply_calibration(CalibrationKind::Flow, 5.0, s));
    CHECK(s.flow == 10.0);
    REQUIRE(apply_calibration(CalibrationKind::Acceleration, 108500.0, s));
    CHECK(s.acceleration == 100000.0);
    CHECK_NOTHROW(parse_settings(settings_json(s)));
}

TEST_CASE("hostile input: regions the own Voronoi diagram got wrong still get walls", "[hostile]") {
    // Found by the review's random regions: the own diagram put a node off
    // its bisector (one parabolic edge then became 164 000 pieces and the
    // walls hung or overflowed the stack), and two polygons touching at two
    // corners crashed the walls. medial_axis now notices and takes the
    // reference diagram.
    const Polygons off_bisector = {
        {{3290213, 3539190}, {8250862, 3243227}, {5996529, 1230406}, {11648797, 1230406}, {11648797, 13866004}, {11672961, 14046970}, {11702879, 14722687}, {11672961, 15398405}, {11648797, 15579370}, {11648797, 23533852}, {20495632, 12042150}, {20659433, 12168252}, {11648797, 23872726}, {11648797, 30979530}, {1428896, 30979530}, {1428896, 21895828}, {1249063, 21833858}, {631566, 21557838}, {40880, 21228325}, {-518372, 20847897}, {-1041813, 20419527}, {-1525347, 19946575}, {-1965189, 19432739}, {-2357897, 18882040}, {-2700397, 18298788}, {-2990009, 17687548}, {-3224466, 17053104}, {-3401934, 16400421}, {-3521023, 15734609}, {-3580802, 15060877}, {-3580802, 14384497}, {-3521023, 13710765}, {-3401934, 13044952}, {-3224466, 12392270}, {-2990009, 11757826}, {-2700397, 11146586}, {-2357897, 10563334}, {-1965189, 10012635}, {-1525347, 9498799}, {-1041813, 9025846}, {-518372, 8597479}, {40880, 8217050}, {631566, 7887536}, {1249063, 7611517}, {1428896, 7549546}, {1428896, 1230406}, {1585518, 1230406}},
        {{274945, -544591}, {-2360634, 479791}, {-1860256, -3436439}},
        {{6098938, -5477847}, {12967462, -5477847}, {2876201, -1555632}, {-50116, -4168447}, {-251353, -4918732}, {-1967869, -5010653}, {4285936, -9964129}, {6098938, -10668796}}};
    const Polygons touching = {
        {{985204,4164502},{740356,5809873},{-905014,6516118},{-2639176,5898664},{-2907224,4611173},{-2800154,4850010},{0,5929168},{953844,4080468}},
        {{1872403,-3243097},{3896845,-2249845},{5644353,0},{4167152,2405906},{1615486,2798104},{953844,4080468},{471515,2787973},{-905014,1999228},{-2517615,2551902},{-3000219,4164502},{-2907224,4611173},{-3951629,2281474},{-5448742,0},{-4352657,-2513008},{-2614256,-4528025},{0,-6056607}}};
    for (const Polygons* region : {&off_bisector, &touching}) {
        const std::size_t own = medial_axis(*region).size();
        const std::size_t reference = medial_axis_reference(*region).size();
        CHECK(own < 4 * reference + 100);
        BeadingParams bp;
        bp.preferred_width = 0.4;
        const LayerWalls walls = layer_walls(*region, bp);
        CHECK(!walls.toolpaths.empty());
    }
}

TEST_CASE("hostile input: one facet wound the wrong way does not drop the part", "[hostile]") {
    // Every loop through the turned facet split into two open chains whose
    // ends were too far apart to close: every layer came out empty.
    IndexedMesh m = cube(10.0f);
    std::swap(m.triangles[6][1], m.triangles[6][2]); // a side facet, turned
    Settings s;
    s.gpu = GpuMode::Off;
    const SlicedModel sliced = slice_model(m, s);
    REQUIRE(!sliced.regions.empty());
    for (const Polygons& region : sliced.regions)
        CHECK(!region.empty());
    // The repair itself: one triangle turned back; a sound mesh untouched.
    IndexedMesh sound = cube(10.0f);
    CHECK(orient_consistently(sound) == 0);
    CHECK(orient_consistently(m) == 1);
}

TEST_CASE("hostile input: after a change's own G-code the writer takes nothing for granted", "[hostile]") {
    // A pause that lifts the head and does not come back, and one that parks
    // it and comes back: the engine went on from where it thought the head
    // was, without F or a retraction, or the check refused the export.
    for (const char* gcode : {"G91\nG1 Z5 F600\nG90\nM0", "G91\nG1 Z5 F300\nG90\nG1 X-60 Y-60 F3000\nM0\nG91\nG1 Z-5 F300\nG90"}) {
        INFO(gcode);
        Settings s;
        s.gpu = GpuMode::Off;
        HeightChange pause;
        pause.z = 2.0;
        pause.gcode = gcode;
        s.changes.push_back(pause);
        const SlicedModel m = slice_model(cube(10.0f), s);
        std::ostringstream g;
        write_gcode(g, m, s);
        const std::string text = g.str();
        const GcodeCheck check = check_gcode(text, gcode_limits(m, s));
        INFO((check.problems.empty() ? std::string() : check.problems.front().message));
        CHECK(check.ok());
        // The modes and E set again, a retraction, and a move that writes Z
        // and then X and Y.
        const std::size_t at = text.find("M0\n");
        REQUIRE(at != std::string::npos);
        const std::string after = text.substr(at, 400);
        CHECK(after.find("G90\nM82\nG92 E") != std::string::npos);
        const std::size_t z = after.find("\nG1 Z");
        const std::size_t xy = after.find("\nG1 X", z);
        CHECK(z != std::string::npos);
        CHECK(xy != std::string::npos);
        CHECK(after.find(" Y", xy) != std::string::npos);
    }
}

TEST_CASE("hostile input: a whisper of fan is never S1, a full fan on RepRapFirmware", "[hostile]") {
    Settings s;
    s.gpu = GpuMode::Off;
    s.fan_min = 0.4;
    s.fan_max = 0.4;
    const SlicedModel m = slice_model(cube(5.0f), s);
    std::ostringstream g;
    write_gcode(g, m, s);
    CHECK(g.str().find("M106 S1\n") == std::string::npos);
    CHECK(g.str().find("M106 S2\n") != std::string::npos);
}

TEST_CASE("hostile input: a project cannot name one mesh for many models", "[hostile]") {
    Project p;
    TriangleMesh mesh;
    const IndexedMesh c = cube(5.0f);
    for (const auto& t : c.triangles)
        mesh.add_triangle(c.vertices[t[0]], c.vertices[t[1]], c.vertices[t[2]]);
    p.models.push_back({"a", mesh, {0.0, 0.0, 0.0}});
    p.models.push_back({"b", mesh, {10.0, 0.0, 0.0}});
    std::string bytes = write_project(p);
    CHECK(read_project(bytes).models.size() == 2);
    // The second model made to name the first one's chunk (same length).
    const std::size_t at = bytes.find("\"mesh\": 1");
    REQUIRE(at != std::string::npos);
    bytes[at + 8] = '0';
    CHECK_THROWS_AS(read_project(bytes), ProjectError);
}

TEST_CASE("hostile input: an empty slice says why, and ignored wavy infill is told", "[hostile]") {
    Settings s;
    s.gpu = GpuMode::Off;
    s.xy_offset = -6.0; // a 10 mm cube shrunk away
    const SlicedModel gone = slice_model(cube(10.0f), s);
    CHECK(unprintable_reason(gone, s).find("slicing.xy_offset") != std::string::npos);
    // Wavy infill with an infill that moves from layer to layer.
    Settings wavy;
    wavy.infill_nonplanar = true;
    wavy.infill_pattern = InfillPattern::Rectilinear;
    bool told = false;
    for (const Warning& w : capability_warnings(wavy))
        told = told || w.kind == "nonplanar_ignored";
    CHECK(told);
    wavy.infill_pattern = InfillPattern::Grid;
    for (const Warning& w : capability_warnings(wavy))
        CHECK(w.kind != "nonplanar_ignored");
}
