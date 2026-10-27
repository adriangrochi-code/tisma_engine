// Speeds of their own, the sparse infill's caps and the bridges' flow
// (docs/spec/features/speeds_accels.md, tests V2 to V5 and V10; the round
// bridge bead of gcode_motion.md, M2).

#include "config/settings.hpp"
#include "gcode/writer.hpp"
#include "mesh/indexed_mesh.hpp"
#include "slice/pipeline.hpp"
#include "test_printer.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <array>
#include <cmath>
#include <cstdint>
#include <set>
#include <tuple>
#include <utility>
#include <sstream>
#include <string>
#include <vector>

using namespace tisma;
using Catch::Matchers::WithinAbs;

namespace {

// Adds a box from (x0, y0, bottom) to (x1, y1, h), facing out.
void add_box(IndexedMesh& m, float x0, float y0, float x1, float y1, float h, float bottom = 0.0f) {
    const auto base = std::uint32_t(m.vertices.size());
    for (float z : {bottom, h})
        for (const auto& p : {std::array<float, 2>{x0, y0}, {x1, y0}, {x1, y1}, {x0, y1}})
            m.vertices.push_back({p[0], p[1], z});
    auto quad = [&](std::uint32_t a, std::uint32_t b, std::uint32_t c, std::uint32_t d) {
        m.triangles.push_back({base + a, base + b, base + c});
        m.triangles.push_back({base + a, base + c, base + d});
    };
    quad(0, 3, 2, 1);
    quad(4, 5, 6, 7);
    for (std::uint32_t i = 0; i < 4; ++i)
        quad(i, (i + 1) % 4, 4 + (i + 1) % 4, 4 + i);
}

// A plate 20 x 10 mm on two pillars 5 mm high (from Z 5 up to `top`): its
// first layer bridges the 10 mm between them.
IndexedMesh table(float top = 7.0f) {
    IndexedMesh m;
    add_box(m, 0, 0, 5, 10, 5);
    add_box(m, 15, 0, 20, 10, 5);
    add_box(m, 0, 0, 20, 10, top, 5);
    return m;
}

IndexedMesh cube(float side, float height) {
    IndexedMesh m;
    add_box(m, 0, 0, side, side, height);
    return m;
}

// Every G1 of the G-code with its layer's Z and type, the feed it runs at
// and what it extrudes (E relative).
struct Move {
    std::string z, type, height;
    double x0, y0, x1, y1, e, feed;
    bool moves_z;
    double length() const { return std::hypot(x1 - x0, y1 - y0); }
};
std::vector<Move> moves_of(const std::string& gcode) {
    std::vector<Move> out;
    std::string z, type, height;
    double x = 0.0, y = 0.0, feed = 0.0;
    std::istringstream in(gcode);
    for (std::string line; std::getline(in, line);) {
        if (line.rfind(";Z:", 0) == 0)
            z = line.substr(3);
        else if (line.rfind(";TYPE:", 0) == 0)
            type = line.substr(6);
        else if (line.rfind(";HEIGHT:", 0) == 0)
            height = line.substr(8);
        else if (line.rfind("G1 ", 0) == 0) {
            double nx = x, ny = y, e = 0.0;
            bool moves_z = false;
            std::istringstream words(line.substr(3));
            for (std::string w; words >> w;) {
                if (w[0] == ';')
                    break;
                const double v = std::stod(w.substr(1));
                if (w[0] == 'X')
                    nx = v;
                else if (w[0] == 'Y')
                    ny = v;
                else if (w[0] == 'Z')
                    moves_z = true;
                else if (w[0] == 'E')
                    e = v;
                else if (w[0] == 'F')
                    feed = v;
            }
            out.push_back({z, type, height, x, y, nx, ny, e, feed, moves_z});
            x = nx, y = ny;
        }
    }
    return out;
}

std::string gcode_of(const IndexedMesh& m, Settings s) {
    s.relative_extrusion = true;
    s.skirt_loops = 0;
    s.min_layer_time = 0.0; // as measured: no layer slowed down for its time
    std::ostringstream out;
    write_gcode(out, slice_model(m, s), s);
    return out.str();
}

bool extrudes(const Move& m) { return m.e > 0.0 && m.length() > 0.0; }

} // namespace

TEST_CASE("speeds: bridges and overhang perimeters at speed.bridges, with a round bead (gcode_motion.md, M2; "
          "speeds_accels.md, V10)",
          "[gcode][speeds]") {
    Settings s = test_printer();
    s.bridge_speed = 25.0;
    double bridge = 0.0, bridge_e = 0.0, overhang = 0.0, overhang_e = 0.0;
    for (const Move& m : moves_of(gcode_of(table(), s))) {
        if (!extrudes(m) || (m.type != "Bridge infill" && m.type != "Overhang perimeter"))
            continue;
        INFO(m.z << " " << m.type);
        CHECK(m.z == "5.200");
        CHECK(m.feed == 25.0 * 60.0);
        // Round, 0.4 mm across: pi/4 of 0.4 thick.
        CHECK(m.height == "0.314");
        (m.type == "Bridge infill" ? bridge : overhang) += m.length();
        (m.type == "Bridge infill" ? bridge_e : overhang_e) += m.e;
    }
    REQUIRE(bridge > 50.0);
    REQUIRE(overhang > 10.0);
    // pi/4 0.4^2 / (pi 0.875^2): M2's 0.052735, W2's 0.05224.
    CHECK_THAT(bridge_e / bridge, WithinAbs(0.052245, 0.0002));
    CHECK_THAT(overhang_e / overhang, WithinAbs(0.052245, 0.0002));

    // V10: 0.8 of the section, the lines closer by the root.
    s.bridge_flow_ratio = 0.8;
    double thin = 0.0, thin_e = 0.0, thin_overhang = 0.0, thin_overhang_e = 0.0;
    for (const Move& m : moves_of(gcode_of(table(), s))) {
        if (!extrudes(m))
            continue;
        if (m.type == "Bridge infill")
            thin += m.length(), thin_e += m.e;
        else if (m.type == "Overhang perimeter")
            thin_overhang += m.length(), thin_overhang_e += m.e;
    }
    CHECK_THAT(thin_e / thin, WithinAbs(0.8 * 0.052245, 0.0002));
    CHECK_THAT(thin_overhang_e / thin_overhang, WithinAbs(0.8 * 0.052245, 0.0002));
    // V10: 477.2 mm then 523.8 (x 1.10); the root of 1 / 0.8 is 1.118.
    CHECK(thin / bridge > 1.05);
    CHECK(thin / bridge < 1.15);

    // Without a speed of its own: the solid infill's.
    s.bridge_speed = 0.0;
    s.solid_infill_speed = 35.0;
    for (const Move& m : moves_of(gcode_of(table(), s)))
        if (extrudes(m) && (m.type == "Bridge infill" || m.type == "Overhang perimeter"))
            CHECK(m.feed == 35.0 * 60.0);
}

TEST_CASE("speeds: the solid infill right over a bridge at speed.over_bridges (speeds_accels.md, rule 2; V2)",
          "[gcode][speeds]") {
    Settings s = test_printer();
    s.solid_infill_speed = 40.0;
    const auto feeds = [&](const Settings& settings) {
        // Layer 26 (Z 5.4): over the bridge (X 5 to 15) and over the pillars,
        // where a plate 0.8 mm thick is solid too (under its top).
        std::set<double> over, beside;
        std::set<double> above; // layer 27
        for (const Move& m : moves_of(gcode_of(table(5.8f), settings))) {
            if (!extrudes(m) || m.type != "Solid infill")
                continue;
            const double x = (m.x0 + m.x1) / 2.0;
            if (m.z == "5.400" && m.length() > 0.3) {
                if (x > 5.6 && x < 14.4)
                    over.insert(m.feed / 60.0);
                else if (x < 4.4 || x > 15.6)
                    beside.insert(m.feed / 60.0);
            } else if (m.z == "5.600") {
                above.insert(m.feed / 60.0);
            }
        }
        return std::tuple{over, beside, above};
    };
    // Not set: the solid infill's.
    auto [over, beside, above] = feeds(s);
    CHECK(over == std::set<double>{40.0});
    CHECK(beside == std::set<double>{40.0});
    // V2: 50 % of the solid infill's, or 10 mm/s; only right over the bridge.
    s.over_bridges_speed = {50.0, true};
    std::tie(over, beside, above) = feeds(s);
    CHECK(over == std::set<double>{20.0});
    CHECK(beside == std::set<double>{40.0});
    CHECK(above == std::set<double>{40.0});
    s.over_bridges_speed = {10.0, false};
    std::tie(over, beside, above) = feeds(s);
    CHECK(over == std::set<double>{10.0});
    CHECK(beside == std::set<double>{40.0});
}

TEST_CASE("speeds: the first layer's solid infill at speed.first_layer_infill, past speed.first_layer "
          "(speeds_accels.md, rule 3; V3)",
          "[gcode][speeds]") {
    Settings s = test_printer();
    s.first_layer_speed = 30.0;
    s.solid_infill_speed = 40.0;
    const auto first_layer = [&](const Settings& settings) {
        std::set<double> solid, walls;
        for (const Move& m : moves_of(gcode_of(cube(20.0f, 2.0f), settings))) {
            if (!extrudes(m) || m.z != "0.200")
                continue;
            if (m.type == "Solid infill")
                solid.insert(m.feed / 60.0);
            else if (m.type == "Perimeter" || m.type == "External perimeter")
                walls.insert(m.feed / 60.0);
        }
        return std::pair{solid, walls};
    };
    // V3: 50 % of the solid infill's, 0, 50 mm/s; the walls at 30 all along.
    s.first_layer_infill_speed = {50.0, true};
    CHECK(first_layer(s) == std::pair{std::set<double>{20.0}, std::set<double>{30.0}});
    s.first_layer_infill_speed = {};
    CHECK(first_layer(s) == std::pair{std::set<double>{30.0}, std::set<double>{30.0}});
    s.first_layer_infill_speed = {50.0, false};
    CHECK(first_layer(s) == std::pair{std::set<double>{50.0}, std::set<double>{30.0}});
}

TEST_CASE("speeds: moves in Z at speed.travel_z (speeds_accels.md, rule 4; V4)", "[gcode][speeds]") {
    Settings s = test_printer();
    s.retract_lift = 0.4;
    const auto z_feeds = [&](const Settings& settings) {
        std::set<double> feeds;
        int lifts = 0;
        for (const Move& m : moves_of(gcode_of(cube(20.0f, 2.0f), settings)))
            if (m.moves_z && m.length() == 0.0 && m.e == 0.0) {
                feeds.insert(m.feed / 60.0);
                ++lifts;
            }
        CHECK(lifts > 10);
        return feeds;
    };
    // V4: at the travels' 130 mm/s (F7800), or at 10 (F600).
    CHECK(z_feeds(s) == std::set<double>{130.0});
    s.travel_z_speed = 10.0;
    CHECK(z_feeds(s) == std::set<double>{10.0});
}

TEST_CASE("speeds: the sparse infill within the filament's cap for its pattern (speeds_accels.md, rule 5; V5)",
          "[gcode][speeds]") {
    Settings s = test_printer();
    s.infill_speed = 80.0;
    s.infill_max_speed = 50.0;
    s.infill_max_crossing_speed = 35.0;
    // V5: lines that do not cross at 50, lines that cross at 35.
    for (const auto& [pattern, cap] : {std::pair{InfillPattern::Rectilinear, 50.0},
                                       {InfillPattern::Gyroid, 50.0},
                                       {InfillPattern::Honeycomb, 50.0},
                                       {InfillPattern::Grid, 35.0},
                                       {InfillPattern::Stars, 35.0}}) {
        s.infill_pattern = pattern;
        std::set<double> feeds;
        for (const Move& m : moves_of(gcode_of(cube(20.0f, 4.0f), s)))
            if (extrudes(m) && m.type == "Internal infill")
                feeds.insert(m.feed / 60.0);
        INFO(infill_pattern_name(pattern));
        CHECK(feeds == std::set<double>{cap});
    }
    // No cap: the infill's own speed.
    s.infill_max_speed = s.infill_max_crossing_speed = 0.0;
    s.infill_pattern = InfillPattern::Grid;
    std::set<double> feeds;
    for (const Move& m : moves_of(gcode_of(cube(20.0f, 4.0f), s)))
        if (extrudes(m) && m.type == "Internal infill")
            feeds.insert(m.feed / 60.0);
    CHECK(feeds == std::set<double>{80.0});
}
