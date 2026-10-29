#include "slice/belt.hpp"

#include "config/settings.hpp"
#include "gcode/check.hpp"
#include "gcode/preview.hpp"
#include "gcode/writer.hpp"
#include "slice/pipeline.hpp"
#include "test_printer.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <sstream>
#include <string>
#include <vector>

using namespace tisma;
using Catch::Matchers::WithinAbs;

namespace {

// The cube of belt_printer.md: 20 by 20 mm on the belt, 10 mm tall, at X 90
// to 110 (and Y 90 to 110, which the belt does not use).
IndexedMesh belt_cube() {
    IndexedMesh m;
    m.vertices = {{90, 90, 0}, {110, 90, 0}, {110, 110, 0}, {90, 110, 0},
                  {90, 90, 10}, {110, 90, 10}, {110, 110, 10}, {90, 110, 10}};
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

Settings belt(double angle, bool belt_is_z) {
    Settings s = test_printer();
    s.gpu = GpuMode::Off;
    s.nozzle_diameter = 0.4;
    s.wall_width = 0.45;
    s.layer_height = 0.2;
    s.first_layer_height = 0.2;
    s.min_layer_time = 0.0;
    s.bed = {{0, 0}, {200, 0}, {200, 200}, {0, 200}};
    s.belt_enabled = true;
    s.belt_angle = angle;
    s.belt_is_z = belt_is_z;
    return s;
}

// What the G-code's layers do on the machine's axes: per layer (from 1), its
// ;Z:, the belt's and the gantry's positions where it extrudes, and X.
struct BeltLayer {
    double z_mark = 0.0;
    double belt_lo = 1e9, belt_hi = -1e9;
    double gantry_lo = 1e9, gantry_hi = -1e9;
    double x_lo = 1e9, x_hi = -1e9;
};

std::vector<BeltLayer> read_layers(const std::string& gcode, bool belt_is_z) {
    std::vector<BeltLayer> layers;
    std::istringstream in(gcode);
    double pos[3] = {0, 0, 0};
    bool started = false;
    for (std::string line; std::getline(in, line);) {
        if (line == ";LAYER_CHANGE") {
            layers.emplace_back();
            started = true;
            continue;
        }
        if (line.rfind(";Z:", 0) == 0 && !layers.empty())
            layers.back().z_mark = std::atof(line.c_str() + 3);
        if (line.size() < 3 || line[0] != 'G' || (line[1] != '0' && line[1] != '1') || line[2] != ' ')
            continue;
        std::istringstream words(line.substr(3, line.find(';') == std::string::npos ? std::string::npos : line.find(';') - 3));
        bool extrudes = false;
        for (std::string w; words >> w;) {
            const double v = std::atof(w.c_str() + 1);
            if (w[0] == 'X')
                pos[0] = v;
            else if (w[0] == 'Y')
                pos[1] = v;
            else if (w[0] == 'Z')
                pos[2] = v;
            else if (w[0] == 'E' && v > 0.0)
                extrudes = true;
        }
        if (!started || !extrudes || line[1] != '1')
            continue;
        BeltLayer& l = layers.back();
        const double b = belt_is_z ? pos[2] : pos[1], g = belt_is_z ? pos[1] : pos[2];
        l.belt_lo = std::min(l.belt_lo, b), l.belt_hi = std::max(l.belt_hi, b);
        l.gantry_lo = std::min(l.gantry_lo, g), l.gantry_hi = std::max(l.gantry_hi, g);
        l.x_lo = std::min(l.x_lo, pos[0]), l.x_hi = std::max(l.x_hi, pos[0]);
    }
    return layers;
}

std::string belt_gcode_of(const Settings& s, const SlicedModel& model) {
    std::ostringstream g;
    write_gcode(g, model, s);
    return g.str();
}

double max_gantry(const std::vector<BeltLayer>& layers) {
    double hi = -1e9;
    for (const BeltLayer& l : layers)
        hi = std::max(hi, l.gantry_hi);
    return hi;
}

void check_passes(const std::string& gcode, const SlicedModel& model, const Settings& s) {
    const GcodeCheck check = check_gcode(gcode, gcode_limits(model, s));
    INFO((check.problems.empty() ? std::string() : check.problems.front().message));
    CHECK(check.ok());
}

} // namespace

TEST_CASE("belt printer B1: the cube on a belt at 45 degrees, the belt in Z", "[belt]") {
    const Settings s = belt(45.0, true);
    REQUIRE_NOTHROW(check_settings(s));
    const SlicedModel model = slice_model(belt_cube(), s);
    const std::string gcode = belt_gcode_of(s, model);
    const std::vector<BeltLayer> layers = read_layers(gcode, true);
    // (20 + 10) · sin 45° / 0.2 layers.
    REQUIRE(layers.size() == 106);
    // Layer 11 (;Z: 2.2, the slicing plane): the belt at 2.2 / sin 45°, the
    // gantry from the belt (0, less the outer wall's half width, less the
    // tenth of a layer between its middle and its top) up the cube's front.
    CHECK_THAT(layers[10].z_mark, WithinAbs(2.2, 1e-9));
    CHECK_THAT(layers[10].belt_lo, WithinAbs(3.111, 1e-9));
    CHECK_THAT(layers[10].belt_hi, WithinAbs(3.111, 1e-9));
    CHECK_THAT(layers[10].gantry_lo, WithinAbs(0.325, 1e-9));
    CHECK_THAT(layers[10].gantry_hi, WithinAbs(4.075, 1e-9));
    CHECK_THAT(layers[52].z_mark, WithinAbs(10.6, 1e-9));
    CHECK_THAT(layers[52].belt_lo, WithinAbs(14.991, 1e-9));
    // The gantry never past the cube's top, 10 / sin 45° from the belt.
    CHECK_THAT(max_gantry(layers), WithinAbs(14.129, 1e-9));
    // X as on a bed.
    for (const BeltLayer& l : layers) {
        CHECK(l.x_lo >= 90.1);
        CHECK(l.x_hi <= 109.9 + 1e-9);
    }
    CHECK_THAT(layers[10].x_lo, WithinAbs(90.225, 1e-9));
    check_passes(gcode, model, s);
}

TEST_CASE("belt printer: the preview shows the part on the belt", "[belt]") {
    const Settings s = belt(45.0, true);
    const std::string gcode = belt_gcode_of(s, slice_model(belt_cube(), s));
    GcodePreview preview = read_gcode_preview(gcode, s.filament_diameter);
    REQUIRE(preview.layers.size() == 106);
    belt_preview(preview, s, 90.0);
    // The cube where it was, within its outer wall's half width.
    CHECK(preview.min[0] >= 90.0f);
    CHECK(preview.max[0] <= 110.0f);
    CHECK(preview.min[1] >= 90.0f - 1e-3f);
    CHECK(preview.max[1] <= 110.0f + 1e-3f);
    CHECK(preview.min[2] >= -1e-3f);
    CHECK_THAT(preview.max[2], WithinAbs(10.0, 0.02));
    // Layer 11's extrusions on its plane, (y − 90) · sin 45° + z · cos 45° = 2.2.
    const PreviewLayer& l = preview.layers[10];
    REQUIRE(l.extrusion_count > 0);
    for (std::size_t k = l.first_extrusion; k < l.first_extrusion + l.extrusion_count; ++k) {
        const PreviewMove& m = preview.moves[preview.extrusions[k]];
        CHECK_THAT((m.y1 - 90.0) * std::sqrt(0.5) + m.z1 * std::sqrt(0.5), WithinAbs(2.2, 1e-3));
    }
}

TEST_CASE("belt printer B2: the belt in Y, the gantry in Z", "[belt]") {
    const Settings s = belt(45.0, false);
    const SlicedModel model = slice_model(belt_cube(), s);
    const std::string gcode = belt_gcode_of(s, model);
    const std::vector<BeltLayer> layers = read_layers(gcode, false);
    REQUIRE(layers.size() == 106);
    CHECK_THAT(layers[10].belt_lo, WithinAbs(3.111, 1e-9));
    CHECK_THAT(layers[10].gantry_lo, WithinAbs(0.325, 1e-9));
    CHECK_THAT(layers[10].gantry_hi, WithinAbs(4.075, 1e-9));
    // The same G-code as B1's with Y and Z swapped.
    const Settings z = belt(45.0, true);
    const std::vector<BeltLayer> other = read_layers(belt_gcode_of(z, slice_model(belt_cube(), z)), true);
    REQUIRE(other.size() == layers.size());
    for (std::size_t i = 0; i < layers.size(); ++i) {
        CHECK(layers[i].belt_lo == other[i].belt_lo);
        CHECK(layers[i].gantry_hi == other[i].gantry_hi);
    }
    check_passes(gcode, model, s);
}

TEST_CASE("belt printer B3: the gantry at 30 degrees", "[belt]") {
    const Settings s = belt(30.0, true);
    const SlicedModel model = slice_model(belt_cube(), s);
    const std::string gcode = belt_gcode_of(s, model);
    const std::vector<BeltLayer> layers = read_layers(gcode, true);
    // (20 + 10 · cot 30°) · sin 30° / 0.2 layers.
    REQUIRE(layers.size() == 93);
    CHECK_THAT(layers[10].z_mark, WithinAbs(2.2, 1e-9));
    CHECK_THAT(layers[10].belt_lo, WithinAbs(4.4, 1e-9));
    // The cube's top, 10 / sin 30° = 20 up the gantry.
    CHECK_THAT(max_gantry(layers), WithinAbs(19.988, 0.02));
    CHECK(max_gantry(layers) < 20.0);
    check_passes(gcode, model, s);
}

TEST_CASE("belt printer: the part turned, the axes, the settings", "[belt]") {
    // The axes (rule 2): the belt at s / sin α, the gantry at z / sin α.
    Settings s = belt(30.0, true);
    const std::array<double, 3> m = belt_axes(s, 5.0, 1.0, 2.0);
    CHECK(m[0] == 5.0);
    CHECK_THAT(m[1], WithinAbs(4.0, 1e-12));
    CHECK_THAT(m[2], WithinAbs(1.0 + 2.0 * std::sqrt(3.0), 1e-12));
    // The cube turned: its lowest point on the first plane, its far top
    // corner at y · sin α + z · cos α.
    const IndexedMesh turned = belt_mesh(belt_cube(), s);
    float lo = 1e9f, hi = -1e9f;
    for (const Vec3f& v : turned.vertices)
        lo = std::min(lo, v.z), hi = std::max(hi, v.z);
    CHECK_THAT(lo, WithinAbs(0.0, 1e-6));
    CHECK_THAT(hi, WithinAbs(20.0 * 0.5 + 10.0 * std::sqrt(3.0) / 2.0, 1e-4));
    // What a belt cannot print with (rule 4); the skirt is left out.
    CHECK_NOTHROW(check_settings(s));
    for (const auto& change : std::vector<void (*)(Settings&)>{
             [](Settings& t) { t.support = true; },
             [](Settings& t) { t.raft_layers = 2; },
             [](Settings& t) { t.brim_width = 5.0; },
             [](Settings& t) { t.sequential = true; },
             [](Settings& t) { t.vase = true; },
             [](Settings& t) { t.gcode_arcs = true; },
             [](Settings& t) { t.infill_waves = true; },
             [](Settings& t) { t.curved_mode = CurvedMode::Wave; }}) {
        Settings t = s;
        change(t);
        CHECK_THROWS_AS(check_settings(t), SettingsError);
    }
    const std::string gcode = belt_gcode_of(s, slice_model(belt_cube(), s));
    CHECK(gcode.find(";TYPE:Skirt") == std::string::npos);
    // Read and written.
    const Settings back = parse_settings(R"({"belt": {"enabled": true, "angle": 35, "belt_is_z": false}})");
    CHECK(back.belt_enabled);
    CHECK(back.belt_angle == 35.0);
    CHECK_FALSE(back.belt_is_z);
    const Settings again = parse_settings(settings_json(back));
    CHECK(again.belt_enabled);
    CHECK(again.belt_angle == 35.0);
    CHECK_FALSE(again.belt_is_z);
    CHECK_THROWS_AS(parse_settings(R"({"belt": {"angle": 5}})"), SettingsError);
    CHECK_THROWS_AS(parse_settings(R"({"belt": {"enabled": 1}})"), SettingsError);
}
