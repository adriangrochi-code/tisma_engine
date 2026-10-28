// Speeds of their own, the sparse infill's caps and the bridges' flow
// (docs/spec/features/speeds_accels.md, tests V2 to V5 and V10; the round
// bridge bead of gcode_motion.md, M2).

#include "config/settings.hpp"
#include "gcode/check.hpp"
#include "gcode/writer.hpp"
#include "mesh/indexed_mesh.hpp"
#include "slice/pipeline.hpp"
#include "test_printer.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <map>
#include <numbers>
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
        // Round, 0.4 mm across and as tall.
        CHECK(m.height == "0.400");
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

namespace {

// The accelerations each kind of extrusion and the travels run at: M204 S
// for both, or Marlin's P for printing and T for travels. By layer (its Z).
struct Accelerations {
    std::map<std::string, std::set<long>> by_type;  // "<z> <type>"
    std::set<long> travel, short_travel;            // travels longer and shorter than 1 mm
    int commands = 0;
};
Accelerations accelerations_of(const std::string& gcode) {
    Accelerations out;
    std::string z, type;
    long print = -1, travel = -1;
    double x = 0.0, y = 0.0;
    std::istringstream in(gcode);
    for (std::string line; std::getline(in, line);) {
        if (line.rfind(";Z:", 0) == 0)
            z = line.substr(3);
        else if (line.rfind(";TYPE:", 0) == 0)
            type = line.substr(6);
        else if (line.rfind("M204 ", 0) == 0) {
            ++out.commands;
            const long v = std::stol(line.substr(6));
            if (line[5] == 'S')
                print = travel = v;
            else if (line[5] == 'P')
                print = v;
            else if (line[5] == 'T')
                travel = v;
        } else if (line.rfind("G1 ", 0) == 0) {
            double nx = x, ny = y;
            bool e = false;
            std::istringstream words(line.substr(3));
            for (std::string w; words >> w;) {
                if (w[0] == 'X')
                    nx = std::stod(w.substr(1));
                else if (w[0] == 'Y')
                    ny = std::stod(w.substr(1));
                else if (w[0] == 'E')
                    e = true;
            }
            const double length = std::hypot(nx - x, ny - y);
            if (length > 0.0 && e)
                out.by_type[z + " " + type].insert(print);
            else if (length > 0.0)
                (length < 1.0 ? out.short_travel : out.travel).insert(travel);
            x = nx, y = ny;
        }
    }
    return out;
}

Settings accelerated() {
    Settings s = test_printer();
    s.flavor = GcodeFlavor::RepRap;
    s.accel_outer_wall = 800.0;
    s.accel_walls = 1000.0;
    s.accel_infill = 3000.0;
    s.accel_solid_infill = 2500.0;
    s.accel_top_solid_infill = 1500.0;
    s.accel_bridges = 500.0;
    s.accel_first_layer = 700.0;
    s.accel_travel = 4000.0;
    s.accel_default = 2000.0;
    return s;
}

} // namespace

TEST_CASE("accelerations: each kind's before its extrusions, the travels' before them (speeds_accels.md, rules 10 to "
          "12; V8)",
          "[gcode][speeds]") {
    Settings s = accelerated();
    const Accelerations a = accelerations_of(gcode_of(cube(20.0f, 10.0f), s));
    const auto at = [&](const char* key) {
        const auto it = a.by_type.find(key);
        return it == a.by_type.end() ? std::set<long>{} : it->second;
    };
    // Layer 0: all at 700.
    for (const auto& [key, values] : a.by_type)
        if (key.rfind("0.200 ", 0) == 0) {
            INFO(key);
            CHECK(values == std::set<long>{700});
        }
    CHECK(at("0.600 Perimeter") == std::set<long>{1000});
    CHECK(at("0.600 External perimeter") == std::set<long>{800});
    CHECK(at("0.600 Solid infill") == std::set<long>{2500});
    CHECK(at("1.200 Internal infill") == std::set<long>{3000});
    CHECK(at("10.000 Top solid infill") == std::set<long>{1500});
    CHECK(a.travel == std::set<long>{4000});
    // A short travel at its own (V8: 0.6 mm at 600).
    s.accel_short_travel = 600.0;
    s.retract_min_travel = 1.0;
    const Accelerations shorter = accelerations_of(gcode_of(cube(20.0f, 10.0f), s));
    CHECK(shorter.travel == std::set<long>{4000});
    REQUIRE_FALSE(shorter.short_travel.empty());
    CHECK(shorter.short_travel == std::set<long>{600});

    // Only the inner walls and the infill (and the default): the outer wall
    // at the inner walls', the solid and top at the infill's.
    Settings few = test_printer();
    few.flavor = GcodeFlavor::RepRap;
    few.accel_walls = 1000.0;
    few.accel_infill = 3000.0;
    few.accel_default = 2000.0;
    const Accelerations b = accelerations_of(gcode_of(cube(20.0f, 10.0f), few));
    CHECK(b.by_type.at("0.600 External perimeter") == std::set<long>{1000});
    CHECK(b.by_type.at("0.600 Solid infill") == std::set<long>{3000});
    CHECK(b.by_type.at("10.000 Top solid infill") == std::set<long>{3000});
    CHECK(b.by_type.at("0.200 Solid infill") == std::set<long>{3000}); // no first layer's

    // Without acceleration.default: only the travels', which the extrusions keep.
    s.accel_default = 0.0;
    s.accel_short_travel = 0.0;
    const Accelerations c = accelerations_of(gcode_of(cube(20.0f, 10.0f), s));
    for (const auto& [key, values] : c.by_type) {
        INFO(key);
        CHECK(values == std::set<long>{4000});
    }

    // The table's overhang perimeters at the bridges'.
    const Accelerations t = accelerations_of(gcode_of(table(), accelerated()));
    CHECK(t.by_type.at("5.200 Overhang perimeter") == std::set<long>{500});
    CHECK(t.by_type.at("5.200 Bridge infill") == std::set<long>{500});

    // None set: no command at all.
    CHECK(accelerations_of(gcode_of(cube(20.0f, 4.0f), test_printer())).commands == 1); // the start's
}

TEST_CASE("accelerations: Marlin prints with M204 P and sets the travels' once with M204 T (speeds_accels.md, rule "
          "13; V9)",
          "[gcode][speeds]") {
    Settings s = test_printer();
    s.flavor = GcodeFlavor::Marlin;
    s.accel_walls = 1000.0;
    s.accel_infill = 3000.0;
    s.accel_travel = 4000.0;
    s.accel_default = 2000.0;
    s.acceleration = 0.0; // nothing written at the start
    const std::string gcode = gcode_of(cube(20.0f, 4.0f), s);
    std::set<std::string> commands;
    int travels = 0;
    std::istringstream in(gcode);
    for (std::string line; std::getline(in, line);)
        if (line.rfind("M204 ", 0) == 0) {
            commands.insert(line);
            travels += line == "M204 T4000";
        }
    CHECK(commands == std::set<std::string>{"M204 P1000", "M204 P2000", "M204 P3000", "M204 T4000"});
    CHECK(travels == 1);
    const Accelerations a = accelerations_of(gcode);
    CHECK(a.travel == std::set<long>{4000});
    CHECK(a.by_type.at("1.000 Internal infill") == std::set<long>{3000});
}

namespace {

// A solid funnel 10 mm high: radius 5 on the bed, opening upward at `angle`
// degrees from the vertical (speeds_accels.md, V6).
IndexedMesh funnel(double angle) {
    constexpr int kSides = 128;
    const double r0 = 5.0, r1 = 5.0 + 10.0 * std::tan(angle * std::numbers::pi / 180.0);
    IndexedMesh m;
    for (const auto& [r, z] : {std::pair{r0, 0.0}, std::pair{r1, 10.0}})
        for (int k = 0; k < kSides; ++k) {
            const double a = 2.0 * std::numbers::pi * k / kSides;
            m.vertices.push_back({float(r * std::cos(a)), float(r * std::sin(a)), float(z)});
        }
    m.vertices.push_back({0.0f, 0.0f, 0.0f});
    m.vertices.push_back({0.0f, 0.0f, 10.0f});
    const auto bottom = std::uint32_t(2 * kSides), top = bottom + 1;
    for (std::uint32_t i = 0; i < kSides; ++i) {
        const std::uint32_t j = (i + 1) % kSides;
        m.triangles.push_back({i, j, kSides + j});
        m.triangles.push_back({i, kSides + j, kSides + i});
        m.triangles.push_back({bottom, j, i});
        m.triangles.push_back({top, kSides + i, kSides + j});
    }
    return m;
}

} // namespace

TEST_CASE("speeds: walls slower the less of their bead rests on the layer below (speeds_accels.md, rules 6 and 7; V6)",
          "[gcode][speeds]") {
    Settings s = test_printer();
    s.dynamic_overhangs = true; // 15, 15, 20 and 25 mm/s; the outer wall at 30
    s.detect_overhangs = false;
    s.wall_width = 0.45; // as measured
    // Rule 7 with w = 0.45 and d = 0.2 tan(angle): V6's 26.8, 24.87, 22.54,
    // 21.11, 17.30 and 15.0 (Tisma Slicer: 27.0, 24.9, 22.6, 21.1, 17.31, 15.0).
    for (const auto& [angle, expected] : {std::pair{20.0, 26.76}, {30.0, 24.87}, {40.0, 22.54}, {45.0, 21.11},
                                          {55.0, 17.30}, {60.0, 15.0}}) {
        INFO(angle << " degrees");
        std::set<double> outer, inner;
        double length = 0.0, sum = 0.0;
        for (const Move& m : moves_of(gcode_of(funnel(angle), s)))
            if (extrudes(m) && m.z == "6.200" && m.length() > 0.05) {
                if (m.type == "External perimeter") {
                    outer.insert(m.feed / 60.0);
                    length += m.length(), sum += m.length() * m.feed / 60.0;
                } else if (m.type == "Perimeter") {
                    inner.insert(m.feed / 60.0);
                }
            }
        REQUIRE(length > 0.0);
        // The outline of each layer, simplified within slicing.simplify_tolerance,
        // moves the wall a little: Tisma Slicer's spread is 0.2 mm/s.
        CHECK_THAT(sum / length, WithinAbs(expected, 0.15));
        CHECK_THAT(*outer.begin(), WithinAbs(expected, 0.4));
        CHECK_THAT(*outer.rbegin(), WithinAbs(expected, 0.4));
        // The inner wall rests on the inner wall below.
        CHECK(inner == std::set<double>{60.0});
    }
    // Without it, the outer wall at its own.
    s.dynamic_overhangs = false;
    std::set<double> outer;
    for (const Move& m : moves_of(gcode_of(funnel(45.0), s)))
        if (extrudes(m) && m.z == "6.200" && m.type == "External perimeter")
            outer.insert(m.feed / 60.0);
    CHECK(outer == std::set<double>{30.0});
}

TEST_CASE("cooling: the walls' fan by how much of their bead rests on the layer below (cooling_more.md, rules 3 and 4; "
          "C2)",
          "[gcode][speeds]") {
    Settings s = test_printer();
    s.dynamic_overhang_fan = true;
    s.overhang_fans = {100.0, 80.0, 60.0, 40.0};
    s.detect_overhangs = false;
    s.wall_width = 0.45; // as measured
    s.min_layer_time = 0.0; // no slowing down by time
    // The fans (%) the walls of layer 31 (Z 6.2) are laid at, by type.
    const auto fans = [&](double angle) {
        std::istringstream in(gcode_of(funnel(angle), s));
        std::map<std::string, std::set<long>> by_type;
        std::string type;
        long fan = 0;
        int layer = 0;
        for (std::string line; std::getline(in, line);) {
            if (line == ";LAYER_CHANGE")
                ++layer;
            else if (line.rfind(";TYPE:", 0) == 0)
                type = line.substr(6);
            else if (line.rfind("M106 S", 0) == 0)
                fan = std::lround(std::stod(line.substr(6)) * 100.0 / 255.0);
            else if (line.rfind("M107", 0) == 0)
                fan = 0;
            else if (layer == 31 && line.rfind("G1 X", 0) == 0 && line.find(" E") != std::string::npos)
                by_type[type].insert(fan);
        }
        return by_type;
    };
    // Rule 3 with w = 0.45 and d = 0.2 tan(angle), the layer's fan off (C2:
    // Tisma Slicer gives 81, 70, 53 to 55, 43 to 44 and 37 to 40, up to
    // three points below the rule; the rule is the contract).
    s.fan_min = s.fan_max = 0.0;
    for (const auto& [angle, expected] : {std::pair{60.0, 81L}, {55.0, 71L}, {45.0, 56L}, {35.0, 45L}, {30.0, 41L},
                                          {27.0, 0L}, {20.0, 0L}}) {
        INFO(angle << " degrees");
        const auto by_type = fans(angle);
        REQUIRE(by_type.count("External perimeter"));
        const std::set<long>& outer = by_type.at("External perimeter");
        CHECK(*outer.begin() >= expected - 2);
        CHECK(*outer.rbegin() <= expected + 2);
        // The inner wall rests on the inner wall below: the layer's.
        CHECK(by_type.at("Perimeter") == std::set<long>{0});
    }
    // Over 75 % held the fan only where the layer has one (rule 4): from
    // fan_3 at 75 % down to 0 at 100 % (C2 with the layer's at 1 %: 32 to
    // 36, 29 to 30 and 22 to 23).
    s.fan_always = true;
    s.fan_min = s.fan_max = 1.0;
    for (const auto& [angle, expected] : {std::pair{27.0, 36L}, {25.0, 33L}, {20.0, 26L}}) {
        INFO(angle << " degrees, the layer's at 1 %");
        const std::set<long>& outer = fans(angle).at("External perimeter");
        CHECK(*outer.begin() >= expected - 2);
        CHECK(*outer.rbegin() <= expected + 2);
    }
    // The layer's when it is higher.
    s.fan_min = s.fan_max = 60.0;
    CHECK(fans(45.0).at("External perimeter") == std::set<long>{60});
    // Without it, the layer's.
    s.dynamic_overhang_fan = false;
    s.fan_min = s.fan_max = 0.0;
    s.fan_always = false;
    CHECK(fans(60.0).at("External perimeter") == std::set<long>{0});
}

TEST_CASE("flow equalizer: the flow changes at most speed.flow_slope_down or _up per second (speeds_accels.md, rules "
          "8 and 9; V7)",
          "[gcode][speeds]") {
    // V7: the inner wall (60 mm/s) ends in pieces down to the outer wall's
    // 30, 2.44 mm3/s lower, over about 1.4 s at 1.8 mm3/s2.
    Settings s = test_printer();
    s.outer_wall_width = s.wall_width; // the outer wall as the others
    s.flow_slope_down = 1.8;
    std::vector<Move> inner; // the layer's inner wall, up to the outer
    bool seen_outer = false;
    for (const Move& m : moves_of(gcode_of(cube(20.0f, 4.0f), s)))
        if (m.z == "1.400" && extrudes(m) && !seen_outer) {
            if (m.type == "Perimeter")
                inner.push_back(m);
            seen_outer = m.type == "External perimeter";
        }
    REQUIRE(inner.size() > 8);
    // From 60 down, never up, to just over 30.
    double time = 0.0, last = 60.0;
    int pieces = 0;
    for (const Move& m : inner) {
        const double v = m.feed / 60.0;
        CHECK(v <= last + 1e-9);
        if (v < 60.0)
            time += m.length() / v, pieces += v < last;
        last = v;
    }
    CHECK(last > 30.0);
    CHECK(last < 32.0);
    CHECK(pieces >= 10);
    CHECK_THAT(time, WithinAbs(2.443 / 1.8, 0.15));

    // Up: the outer wall at 60 after the inner at 30 starts low and climbs.
    s.flow_slope_down = 0.0;
    s.flow_slope_up = 1.8;
    s.wall_speed = 30.0;
    s.outer_wall_speed = {60.0, false};
    std::vector<double> outer;
    for (const Move& m : moves_of(gcode_of(cube(20.0f, 4.0f), s)))
        if (m.z == "1.400" && extrudes(m) && m.type == "External perimeter")
            outer.push_back(m.feed / 60.0);
    REQUIRE(outer.size() > 8);
    CHECK(outer.front() > 30.0);
    CHECK(outer.front() < 33.0);
    CHECK(std::is_sorted(outer.begin(), outer.end()));
    CHECK(outer.back() == 60.0);

    // Rule 9: after a retraction, no ramp up: the sparse infill starts at its 80.
    s = test_printer();
    s.flow_slope_up = 1.8;
    s.retract_min_travel = 0.5;
    bool retracted = false;
    int checked = 0;
    std::istringstream in(gcode_of(cube(20.0f, 4.0f), s));
    std::string type;
    for (std::string line; std::getline(in, line);) {
        if (line.rfind(";TYPE:", 0) == 0)
            type = line.substr(6);
        else if (line.rfind("G1 E-", 0) == 0)
            retracted = true;
        else if (line.rfind("G1 X", 0) == 0 && line.find(" E") != std::string::npos) {
            if (retracted && type == "Internal infill") {
                INFO(line);
                CHECK(line.find(" F4800") != std::string::npos);
                ++checked;
            }
            retracted = false;
        }
    }
    CHECK(checked > 3);
}

TEST_CASE("speeds: the check takes the new speeds as the fastest a G-code may ask", "[gcode][speeds]") {
    // A bridge faster than every other speed (the fuzzer's run 211, seed 20261030).
    Settings s = test_printer();
    s.outer_wall_width = s.wall_width; // the outer wall as the others
    s.max_speed = 0.0;
    s.bridge_speed = 150.0; // past the travels' 130 too
    s.relative_extrusion = true;
    const SlicedModel model = slice_model(table(), s);
    std::ostringstream out;
    write_gcode(out, model, s);
    REQUIRE(out.str().find(" F9000") != std::string::npos);
    CHECK(check_gcode(out.str(), gcode_limits(model, s)).ok());
}

TEST_CASE("walls: one width with gap fill in the middle, at speed.gap_fill (walls_extra.md, rules 1 and 2, W6; "
          "speeds_accels.md, V1)",
          "[gcode][speeds][walls]") {
    // W6: a straight wall 1.5 x 20 x 5 mm, 2 walls of 0.45, no infill and no
    // solid layers: layer 10 has an outer wall of 41.14 mm and 19.14 mm of gap fill.
    IndexedMesh m;
    add_box(m, 0, 0, 1.5f, 20, 5);
    Settings s = test_printer();
    s.wall_generator = WallGenerator::Fixed;
    s.wall_count = 2;
    s.wall_width = 0.45;
    s.infill_density = 0.0;
    s.top_layers = s.bottom_layers = 0;
    s.gap_fill_speed = 20.0;
    const auto layer10 = [&](const Settings& settings) {
        std::map<std::string, double> length;
        std::set<double> gap_feeds;
        for (const Move& mv : moves_of(gcode_of(m, settings)))
            if (extrudes(mv) && mv.z == "2.200") {
                length[mv.type] += mv.length();
                if (mv.type == "Gap fill")
                    gap_feeds.insert(mv.feed / 60.0);
            }
        return std::pair{length, gap_feeds};
    };
    auto [length, feeds] = layer10(s);
    CHECK_THAT(length["External perimeter"], WithinAbs(41.14, 0.15));
    CHECK(length.count("Perimeter") == 0); // no room for the inner one
    CHECK_THAT(length["Gap fill"], WithinAbs(19.14, 0.3));
    CHECK(feeds == std::set<double>{20.0});
    // V1: at 10 mm/s.
    s.gap_fill_speed = 10.0;
    CHECK(layer10(s).second == std::set<double>{10.0});
    // W6: without gap fill, only the perimeter.
    s.wall_gap_fill = false;
    std::tie(length, feeds) = layer10(s);
    CHECK(length.count("Gap fill") == 0);
    CHECK_THAT(length["External perimeter"], WithinAbs(41.14, 0.15));
    // The variable walls fill it with beads of their own: no gap fill.
    s.wall_generator = WallGenerator::Variable;
    s.wall_gap_fill = true;
    CHECK(layer10(s).first.count("Gap fill") == 0);
}
