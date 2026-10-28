// Retraction and travel options (docs/spec/features/retraction_travel.md,
// tests R1 to R11).

#include "config/settings.hpp"
#include "gcode/check.hpp"
#include "gcode/writer.hpp"
#include "mesh/indexed_mesh.hpp"
#include "slice/pipeline.hpp"
#include "test_printer.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <numbers>
#include <set>
#include <sstream>
#include <string>
#include <vector>

using namespace tisma;
using Catch::Matchers::ContainsSubstring;
using Catch::Matchers::WithinAbs;

namespace {

// The cube of the spec, 20 mm, around (100, 100).
IndexedMesh cube() {
    IndexedMesh m;
    for (float z : {0.0f, 20.0f})
        for (const auto& p : {std::array<float, 2>{90, 90}, {110, 90}, {110, 110}, {90, 110}})
            m.vertices.push_back({p[0], p[1], z});
    const auto quad = [&](std::uint32_t a, std::uint32_t b, std::uint32_t c, std::uint32_t d) {
        m.triangles.push_back({a, b, c});
        m.triangles.push_back({a, c, d});
    };
    quad(0, 3, 2, 1);
    quad(4, 5, 6, 7);
    quad(0, 1, 5, 4);
    quad(1, 2, 6, 5);
    quad(2, 3, 7, 6);
    quad(3, 0, 4, 7);
    return m;
}

// A tube around (100, 100) from z 0 to `height`: outer and inner radius, n
// sides; no hole is a cylinder.
IndexedMesh tube(double outer, double inner, double height, int n = 96) {
    IndexedMesh m;
    if (inner <= 0.0) {
        for (int ring = 0; ring < 2; ++ring)
            for (int i = 0; i < n; ++i) {
                const double a = 2.0 * std::numbers::pi * i / n;
                m.vertices.push_back(
                    {float(100.0 + outer * std::cos(a)), float(100.0 + outer * std::sin(a)), float(ring * height)});
            }
        m.vertices.push_back({100.0f, 100.0f, 0.0f});
        m.vertices.push_back({100.0f, 100.0f, float(height)});
        const std::uint32_t bottom = std::uint32_t(2 * n), top = bottom + 1;
        for (int i = 0; i < n; ++i) {
            const std::uint32_t a = std::uint32_t(i), b = std::uint32_t((i + 1) % n);
            m.triangles.push_back({a, b, b + std::uint32_t(n)});
            m.triangles.push_back({a, b + std::uint32_t(n), a + std::uint32_t(n)});
            m.triangles.push_back({bottom, b, a});
            m.triangles.push_back({top, a + std::uint32_t(n), b + std::uint32_t(n)});
        }
        return m;
    }
    for (int ring = 0; ring < 4; ++ring)
        for (int i = 0; i < n; ++i) {
            const double r = ring % 2 == 0 ? outer : inner;
            const double z = ring < 2 ? 0.0 : height;
            const double a = 2.0 * std::numbers::pi * i / n;
            m.vertices.push_back({float(100.0 + r * std::cos(a)), float(100.0 + r * std::sin(a)), float(z)});
        }
    const auto v = [&](int ring, int i) { return std::uint32_t(ring * n + (i % n)); };
    for (int i = 0; i < n; ++i) {
        const int j = i + 1;
        m.triangles.push_back({v(0, i), v(0, j), v(2, j)});
        m.triangles.push_back({v(0, i), v(2, j), v(2, i)});
        m.triangles.push_back({v(1, i), v(3, j), v(1, j)});
        m.triangles.push_back({v(1, i), v(3, i), v(3, j)});
        m.triangles.push_back({v(3, i), v(2, i), v(2, j)});
        m.triangles.push_back({v(3, i), v(2, j), v(3, j)});
        m.triangles.push_back({v(1, i), v(0, j), v(0, i)});
        m.triangles.push_back({v(1, i), v(1, j), v(0, j)});
    }
    return m;
}

// The spec's base: 0.4 nozzle, 0.2 layers, 2 walls, no solid layers, no
// skirt or brim; a retraction of 2 mm at 40 mm/s, retraction.min_travel 2,
// travels at 130 mm/s; relative E.
Settings base() {
    Settings s = test_printer();
    s.wall_count = 2;
    s.top_layers = s.bottom_layers = 0;
    s.skirt_loops = 0;
    s.brim_width = 0.0;
    s.retract_length = 2.0;
    s.retract_speed = 40.0;
    s.retract_min_travel = 2.0;
    s.travel_speed = 130.0;
    s.relative_extrusion = true;
    return s;
}

// The G-code of a mesh, checked.
std::string gcode_of(const IndexedMesh& mesh, const Settings& s) {
    const SlicedModel model = slice_model(mesh, s);
    std::ostringstream out;
    write_gcode(out, model, s);
    const GcodeCheck check = check_gcode(out.str(), gcode_limits(model, s));
    INFO((check.problems.empty() ? std::string() : "line " + std::to_string(check.problems.front().line) + ": " +
                                                         check.problems.front().message));
    CHECK(check.ok());
    return out.str();
}

// One move of the G-code, read back.
struct Move {
    int layer = -1;
    std::string type;
    double x0 = 0, y0 = 0, z0 = 0, x = 0, y = 0, z = 0;
    double e = 0.0;  // what E moved
    double f = 0.0;  // mm/min
    bool xy = false; // it moves in X or Y
    bool zmove = false;
    double length() const { return std::hypot(x - x0, y - y0); }
};

struct Read {
    std::vector<Move> moves;
    int g10 = 0, g11 = 0;
    std::vector<std::size_t> layer_starts; // the first move of each layer
};

Read read(const std::string& gcode) {
    Read out;
    double x = 0, y = 0, z = 0, e = 0, f = 0;
    bool relative = false;
    int layer = -1;
    std::string type;
    std::istringstream in(gcode);
    for (std::string line; std::getline(in, line);) {
        if (line.starts_with(";LAYER_CHANGE")) {
            ++layer;
            out.layer_starts.push_back(out.moves.size());
            continue;
        }
        if (line.starts_with(";TYPE:")) {
            type = line.substr(6);
            continue;
        }
        const std::string code = line.substr(0, line.find(';'));
        if (code.starts_with("M83"))
            relative = true;
        else if (code.starts_with("M82"))
            relative = false;
        if (code == "G10" || code == "G10 ")
            ++out.g10;
        if (code == "G11" || code == "G11 ")
            ++out.g11;
        const bool g92 = code.starts_with("G92");
        if (!g92 && !code.starts_with("G1 ") && !code.starts_with("G0 "))
            continue;
        Move m;
        m.layer = layer;
        m.type = type;
        m.x0 = x, m.y0 = y, m.z0 = z;
        std::istringstream words(code.substr(3));
        for (std::string w; words >> w;) {
            const double v = std::atof(w.c_str() + 1);
            switch (w[0]) {
            case 'X':
                x = v, m.xy = true;
                break;
            case 'Y':
                y = v, m.xy = true;
                break;
            case 'Z':
                z = v, m.zmove = true;
                break;
            case 'E':
                if (g92) {
                    e = v;
                } else {
                    m.e = relative ? v : v - e;
                    e = relative ? e + v : v;
                }
                break;
            case 'F':
                f = v;
                break;
            default:
                break;
            }
        }
        if (g92)
            continue;
        m.x = x, m.y = y, m.z = z, m.f = f;
        out.moves.push_back(m);
    }
    return out;
}

bool standing_retraction(const Move& m) { return !m.xy && !m.zmove && m.e < -1e-9; }
bool unretraction(const Move& m) { return !m.xy && !m.zmove && m.e > 1e-9; }
bool wipe(const Move& m) { return m.xy && m.e < -1e-9; }

int count(const Read& r, bool (*what)(const Move&)) {
    int n = 0;
    for (const Move& m : r.moves)
        n += what(m) ? 1 : 0;
    return n;
}

// The distance from p to the segment ab.
double to_segment(double px, double py, double ax, double ay, double bx, double by) {
    const double dx = bx - ax, dy = by - ay, l2 = dx * dx + dy * dy;
    const double t = l2 > 0.0 ? std::clamp(((px - ax) * dx + (py - ay) * dy) / l2, 0.0, 1.0) : 0.0;
    return std::hypot(px - ax - t * dx, py - ay - t * dy);
}

} // namespace

TEST_CASE("retraction: the push back has its own speed and extra (R1)", "[retraction]") {
    Settings s = base();
    s.unretract_speed = 20.0;
    s.restart_extra = 0.5;
    const Read r = read(gcode_of(cube(), s));
    int retractions = 0, unretractions = 0;
    for (const Move& m : r.moves) {
        if (standing_retraction(m)) {
            ++retractions;
            CHECK_THAT(m.e, WithinAbs(-2.0, 1e-5));
            CHECK(m.f == 2400.0);
        } else if (unretraction(m)) {
            ++unretractions;
            CHECK_THAT(m.e, WithinAbs(2.5, 1e-5));
            CHECK(m.f == 1200.0);
        }
    }
    CHECK(retractions > 10);
    CHECK(unretractions == retractions);
}

TEST_CASE("retraction: every layer change retracts with retraction.on_layer_change (R2)", "[retraction]") {
    Settings s = base();
    s.seam = SeamMode::Aligned;
    s.infill_density = 0.0; // the walls alone: each layer starts next to where the last ended
    const IndexedMesh cylinder = tube(10.0, 0.0, 10.0);
    const Read without = read(gcode_of(cylinder, s));
    s.retract_layer_change = true;
    const std::string text = gcode_of(cylinder, s);
    const Read with = read(text);
    const int layers = int(with.layer_starts.size());
    // Each change from the second layer on: retracted before its move up,
    // pushed back after.
    int retracted_changes = 0;
    for (std::size_t l = 1; l < with.layer_starts.size(); ++l) {
        bool retracted = false;
        for (std::size_t k = 0; k < with.layer_starts[l]; ++k) {
            if (standing_retraction(with.moves[k]))
                retracted = true;
            else if (unretraction(with.moves[k]))
                retracted = false;
        }
        retracted_changes += retracted ? 1 : 0;
    }
    CHECK(retracted_changes == layers - 1);
    CHECK(count(with, standing_retraction) >= layers - 1);
    CHECK(count(with, standing_retraction) > count(without, standing_retraction) + layers / 2);
}

TEST_CASE("retraction: a wipe retracts over the path just printed (R3)", "[retraction]") {
    Settings s = base();
    s.wipe = true;
    // One wipe in the fifth layer: its length, feed and E (2 * 104 / (0.95 * 40) = 5.47 mm).
    const auto first_wipe = [](const Read& r, double& length, double& pulled, double& standing, double& feed) {
        length = pulled = standing = feed = 0.0;
        bool in = false;
        for (std::size_t k = r.layer_starts[5]; k < r.layer_starts[6]; ++k) {
            const Move& m = r.moves[k];
            if (wipe(m)) {
                // Over the extrusion just before it.
                std::size_t last = k;
                while (last > 0 && wipe(r.moves[last - 1]))
                    --last;
                while (last > 0 && (standing_retraction(r.moves[last - 1]) ||
                                    (r.moves[last - 1].zmove && !r.moves[last - 1].xy))) // a layer change
                    --last;
                if (!in) {
                    const Move& printed = r.moves[last - 1];
                    REQUIRE(printed.e > 0.0);
                    CHECK(to_segment(m.x0, m.y0, printed.x0, printed.y0, printed.x, printed.y) < 2e-3);
                }
                in = true;
                length += m.length();
                pulled -= m.e;
                feed = m.f;
            } else if (in) {
                if (standing_retraction(m))
                    pulled -= m.e;
                return;
            } else if (standing_retraction(m)) {
                standing -= m.e;
            } else {
                standing = 0.0;
            }
        }
    };
    double length = 0, pulled = 0, standing = 0, feed = 0;
    first_wipe(read(gcode_of(cube(), s)), length, pulled, standing, feed);
    CHECK_THAT(length, WithinAbs(2.0 * 104.0 / (0.95 * 40.0), 0.01));
    CHECK_THAT(pulled, WithinAbs(2.0, 1e-4));
    CHECK(standing == 0.0);
    CHECK(feed == 6240.0);
    // Slower retraction, faster travels, a shorter retraction.
    s.retract_speed = 20.0;
    first_wipe(read(gcode_of(cube(), s)), length, pulled, standing, feed);
    CHECK_THAT(length, WithinAbs(10.94, 0.01));
    s.retract_speed = 40.0;
    s.travel_speed = 200.0;
    first_wipe(read(gcode_of(cube(), s)), length, pulled, standing, feed);
    CHECK_THAT(length, WithinAbs(8.42, 0.01));
    CHECK(feed == 9600.0);
    s.travel_speed = 130.0;
    s.retract_length = 1.0;
    first_wipe(read(gcode_of(cube(), s)), length, pulled, standing, feed);
    CHECK_THAT(length, WithinAbs(2.73, 0.01));
    // Half of it standing first.
    s.retract_length = 2.0;
    s.before_wipe = 50.0;
    first_wipe(read(gcode_of(cube(), s)), length, pulled, standing, feed);
    CHECK_THAT(standing, WithinAbs(1.0, 1e-5));
    CHECK_THAT(pulled, WithinAbs(1.0, 1e-4));
    CHECK_THAT(length, WithinAbs(2.73, 0.01));
}

TEST_CASE("retraction: the lift only above or below a height (R4)", "[retraction]") {
    Settings s = base();
    s.retract_lift = 0.4;
    // The layers whose travels lift: a move up by 0.4 that is not a layer change.
    const auto lifted = [](const Read& r) {
        std::vector<double> out;
        for (const Move& m : r.moves)
            if (m.zmove && !m.xy && std::abs(m.z - m.z0 - 0.4) < 1e-6 && m.layer >= 0) {
                const double layer_z = m.z0;
                if (out.empty() || std::abs(out.back() - layer_z) > 1e-6)
                    out.push_back(layer_z);
            }
        return out;
    };
    const std::vector<double> all = lifted(read(gcode_of(cube(), s)));
    REQUIRE(!all.empty());
    CHECK(all.front() < 1.0);
    CHECK(all.back() > 2.0);
    s.lift_above = 2.0;
    for (const double z : lifted(read(gcode_of(cube(), s))))
        CHECK(z >= 2.0 - 1e-9);
    s.lift_above = 0.0;
    s.lift_below = 1.0;
    const std::vector<double> below = lifted(read(gcode_of(cube(), s)));
    REQUIRE(!below.empty());
    for (const double z : below)
        CHECK(z <= 1.0 + 1e-9);
}

TEST_CASE("retraction: a ramping lift climbs while travelling (R5)", "[retraction]") {
    Settings s = base();
    s.ramping_lift = true;
    s.travel_slope = 10.0;
    s.max_lift = 1.0;
    const Read r = read(gcode_of(cube(), s));
    const double slope = std::tan(10.0 * std::numbers::pi / 180.0);
    int ramps = 0, layer_change_ramps = 0, short_ramps = 0;
    for (std::size_t k = 0; k < r.moves.size(); ++k) {
        const Move& m = r.moves[k];
        if (!(m.xy && m.zmove && m.z > m.z0 + 1e-6 && m.e == 0.0))
            continue;
        // The start of a ramp: from the layer's Z (or the one below's).
        if (k > 0 && r.moves[k - 1].xy && r.moves[k - 1].zmove && r.moves[k - 1].z > r.moves[k - 1].z0 + 1e-6)
            continue;
        ++ramps;
        const double base_z = m.z0;
        double along = 0.0, top = base_z;
        std::size_t j = k;
        for (; j < r.moves.size() && r.moves[j].xy && r.moves[j].e == 0.0; ++j) {
            along += r.moves[j].length();
            top = r.moves[j].z;
            CHECK(r.moves[j].z <= base_z + 1.0 + 1e-3);
            CHECK_THAT(r.moves[j].z, WithinAbs(std::min(base_z + 1.0, base_z + slope * along), 2e-3));
        }
        REQUIRE(j < r.moves.size());
        // Straight down at the end, to the layer's Z.
        const Move& down = r.moves[j];
        CHECK(down.zmove);
        CHECK_FALSE(down.xy);
        CHECK(down.z < top);
        if (down.z > base_z + 1e-6)
            ++layer_change_ramps; // it climbed from the layer below
        if (top < base_z + 1.0 - 1e-3)
            ++short_ramps;
    }
    CHECK(ramps > 20);
    CHECK(layer_change_ramps > 5);
    CHECK(short_ramps >= 0);
}

TEST_CASE("retraction: the firmware's retraction with G10 and G11 (R6)", "[retraction]") {
    Settings s = base();
    s.retract_lift = 0.4;
    s.firmware_retraction = true;
    const std::string text = gcode_of(cube(), s);
    const Read r = read(text);
    CHECK(r.g10 > 10);
    CHECK(r.g11 == r.g10);
    CHECK(count(r, standing_retraction) == 0);
    CHECK(count(r, unretraction) == 0);
    int lifts = 0;
    for (const Move& m : r.moves)
        lifts += m.zmove && !m.xy && std::abs(m.z - m.z0 - 0.4) < 1e-6 ? 1 : 0;
    CHECK(lifts > 10);
    // With a wipe: a configuration error.
    s.wipe = true;
    CHECK_THROWS_AS(check_settings(s), SettingsError);
}

TEST_CASE("retraction: travels round the walls, unless told not to or the detour is long (R7)", "[retraction]") {
    Settings s = base();
    s.layer_height = s.first_layer_height = 0.2;
    const IndexedMesh pipe = tube(10.0, 5.0, 4.0);
    const auto travelled = [&](const Settings& settings) {
        double length = 0.0;
        for (const Move& m : read(gcode_of(pipe, settings)).moves)
            if (m.layer == 10 && m.xy && m.e == 0.0)
                length += m.length();
        return length;
    };
    s.avoid_crossing_walls = false;
    const double straight = travelled(s);
    s.avoid_crossing_walls = true;
    const double inside = travelled(s);
    CHECK(inside > straight);
    // This layer's detours add up to half a millimetre (the spec's R7 has
    // 5.5 mm with its travels): a cap of 0.2 mm sends them straight.
    s.max_detour = {0.2, false};
    const double limited = travelled(s);
    CHECK(limited < inside);
    CHECK(limited >= straight - 1e-6);
    s.max_detour = {200.0, true};
    CHECK_THAT(travelled(s), WithinAbs(inside, 1e-6));
}

TEST_CASE("retraction: wall loops counter-clockwise, or clockwise with walls.clockwise (R8)", "[retraction]") {
    Settings s = base();
    const IndexedMesh pipe = tube(10.0, 5.0, 4.0);
    // The signed area of each wall loop of layer 10.
    const auto loops = [&](const Settings& settings) {
        std::vector<double> out;
        const Read r = read(gcode_of(pipe, settings));
        double area = 0.0;
        bool open = false;
        for (const Move& m : r.moves) {
            const bool wall = m.layer == 10 && m.e > 0.0 && m.xy &&
                              (m.type == "External perimeter" || m.type == "Perimeter" || m.type == "Overhang perimeter");
            if (wall) {
                area += m.x0 * m.y - m.x * m.y0;
                open = true;
            } else if (open && m.xy) {
                out.push_back(area / 2.0);
                area = 0.0;
                open = false;
            }
        }
        if (open)
            out.push_back(area / 2.0);
        return out;
    };
    const std::vector<double> ccw = loops(s);
    REQUIRE(ccw.size() == 4);
    for (const double a : ccw)
        CHECK(a > 0.0);
    s.walls_clockwise = true;
    const std::vector<double> cw = loops(s);
    REQUIRE(cw.size() == 4);
    for (const double a : cw)
        CHECK(a < 0.0);
}

TEST_CASE("retraction: the filament's own values replace the printer's (R9)", "[retraction]") {
    Settings s = base();
    s.retract_layer_change = true;
    const IndexedMesh cylinder = tube(10.0, 0.0, 4.0);
    const auto with = [&](const std::string& filament) {
        Settings f = parse_settings(R"({"extrusion": {"retraction": )" + filament + "}}");
        Settings out = s;
        out.filament_retraction = f.filament_retraction;
        return read(gcode_of(cylinder, out));
    };
    Read r = with(R"({"length": 0.8})");
    REQUIRE(count(r, standing_retraction) > 0);
    for (const Move& m : r.moves) {
        if (standing_retraction(m))
            CHECK_THAT(m.e, WithinAbs(-0.8, 1e-5));
        if (unretraction(m))
            CHECK_THAT(m.e, WithinAbs(0.8, 1e-5));
    }
    r = with(R"({"speed": 25, "unretract_speed": 10})");
    for (const Move& m : r.moves) {
        if (standing_retraction(m))
            CHECK(m.f == 1500.0);
        if (unretraction(m))
            CHECK(m.f == 600.0);
    }
    r = with(R"({"restart_extra": 0.4})");
    for (const Move& m : r.moves)
        if (unretraction(m))
            CHECK_THAT(m.e, WithinAbs(2.4, 1e-5));
    CHECK(count(with(R"({"length": 0})"), standing_retraction) == 0);
    r = with(R"({"length": null})");
    for (const Move& m : r.moves)
        if (standing_retraction(m))
            CHECK_THAT(m.e, WithinAbs(-2.0, 1e-5));
}

TEST_CASE("retraction: the filament's lift, even 0, replaces the printer's (R10)", "[retraction]") {
    // Moves in Z: the layer changes, and two per lift.
    const auto z_moves = [](double printer, const char* filament) {
        Settings s = base();
        s.retract_lift = printer;
        if (filament)
            s.filament_retraction = parse_settings(std::string(R"({"extrusion": {"retraction": {"lift": )") + filament + "}}}")
                                        .filament_retraction;
        const Read r = read(gcode_of(cube(), s));
        int n = 0;
        double highest_lift = 0.0;
        for (const Move& m : r.moves)
            if (m.zmove && !m.xy) {
                ++n;
                highest_lift = std::max(highest_lift, m.z - m.z0);
            }
        return std::pair{n, highest_lift};
    };
    const auto [layers, none] = z_moves(0.0, nullptr);
    CHECK(none < 0.21);
    const auto [lifted_03, by_03] = z_moves(0.0, "0.3");
    CHECK(lifted_03 > layers + 20);
    CHECK_THAT(by_03, WithinAbs(0.3, 1e-6));
    const auto [lifted_04, by_04] = z_moves(0.4, nullptr);
    CHECK(lifted_04 == lifted_03);
    CHECK_THAT(by_04, WithinAbs(0.4, 1e-6));
    const auto [unlifted, by_0] = z_moves(0.4, "0");
    CHECK(unlifted == layers);
    CHECK(by_0 < 0.21);
}

TEST_CASE("retraction: the filament's minimum travel and wipe (R11)", "[retraction]") {
    Settings s = base();
    s.outer_wall_width = s.wall_width; // the outer wall as the others
    const int printer = count(read(gcode_of(cube(), s)), standing_retraction);
    s.filament_retraction = parse_settings(R"({"extrusion": {"retraction": {"min_travel": 20}}})").filament_retraction;
    const int filament = count(read(gcode_of(cube(), s)), standing_retraction);
    CHECK(filament < printer / 4);
    s.filament_retraction = parse_settings(R"({"extrusion": {"retraction": {"wipe": true}}})").filament_retraction;
    const Read wiped = read(gcode_of(cube(), s));
    // Every retraction wipes: a standing pull only for what a short path
    // could not take, right after its wipe.
    int wipes = 0;
    for (std::size_t k = 0; k < wiped.moves.size(); ++k) {
        if (wipe(wiped.moves[k]) && (k == 0 || !wipe(wiped.moves[k - 1])))
            ++wipes;
        if (standing_retraction(wiped.moves[k])) {
            REQUIRE(k > 0);
            CHECK(wipe(wiped.moves[k - 1]));
        }
    }
    CHECK(wipes == printer);
}

TEST_CASE("retraction: the settings read and write the options", "[retraction][settings]") {
    const Settings s = parse_settings(R"({
        "retraction": {"unretract_speed": 20, "restart_extra": 0.5, "on_layer_change": true, "wipe": true,
                       "before_wipe": 30, "lift_above": 1, "lift_below": 5, "firmware": false},
        "travel": {"ramping_lift": true, "slope": 10, "max_lift": 1, "lift_before_obstacle": true,
                   "avoid_crossing_walls": false, "max_detour": "200%"},
        "walls": {"clockwise": true},
        "extrusion": {"retraction": {"length": 0.8, "wipe": false}, "travel": {"slope": 5}}})");
    CHECK(s.unretract_speed == 20.0);
    CHECK(s.restart_extra == 0.5);
    CHECK(s.retract_layer_change);
    CHECK(s.wipe);
    CHECK(s.before_wipe == 30.0);
    CHECK(s.lift_above == 1.0);
    CHECK(s.lift_below == 5.0);
    CHECK(s.ramping_lift);
    CHECK(s.travel_slope == 10.0);
    CHECK(s.max_lift == 1.0);
    CHECK_FALSE(s.avoid_crossing_walls);
    CHECK(s.max_detour.percent);
    CHECK(s.max_detour.value == 200.0);
    CHECK(s.walls_clockwise);
    const Settings f = with_filament_retraction(s);
    CHECK(f.retract_length == 0.8);
    CHECK_FALSE(f.wipe);
    CHECK(f.travel_slope == 5.0);
    // Written and read back the same.
    const Settings again = parse_settings(settings_json(s));
    CHECK(again.filament_retraction == s.filament_retraction);
    CHECK(again.max_detour == s.max_detour);
    CHECK(again.before_wipe == s.before_wipe);
    CHECK_THROWS_AS(parse_settings(R"({"travel": {"max_detour": "abc"}})"), SettingsError);
    CHECK_THROWS_AS(parse_settings(R"({"extrusion": {"retraction": {"temperature": 1}}})"), SettingsError);
    CHECK_THROWS_AS(parse_settings(R"({"travel": {"slope": 95}})"), SettingsError);
}

TEST_CASE("coasting: each wall's and infill's last retraction.coast mm with no E, at its feed (coasting.md, CO1)",
          "[retraction]") {
    Settings s = base();
    s.min_layer_time = 0.0; // the paths at their own feeds
    // The moves of layer 11 that go at a print feed with no E: the coasted ends.
    const auto coasted = [](const Read& r) {
        std::vector<Move> out;
        for (std::size_t k = r.layer_starts[10]; k < r.layer_starts[11]; ++k) {
            const Move& m = r.moves[k];
            if (m.xy && std::abs(m.e) < 1e-9 && k > 0 && r.moves[k - 1].e > 0.0 && m.f < 6000.0)
                out.push_back(m);
        }
        return out;
    };
    const std::string plain = gcode_of(cube(), s);
    CHECK(coasted(read(plain)).empty());
    s.coast = 0.5;
    const std::string with = gcode_of(cube(), s);
    const std::vector<Move> ends = coasted(read(with));
    REQUIRE_FALSE(ends.empty());
    std::set<std::string> kinds;
    for (const Move& m : ends) {
        INFO(m.type << " " << m.length() << " mm at F" << m.f);
        CHECK(m.length() <= 0.5 + 2e-3); // a whole end or the last of its pieces
        kinds.insert(m.type);
        if (m.type == "Perimeter")
            CHECK(m.f == 3600.0);
        else if (m.type == "External perimeter")
            CHECK(m.f == 1800.0);
        else
            CHECK(m.type == "Internal infill");
    }
    CHECK(kinds == std::set<std::string>{"External perimeter", "Internal infill", "Perimeter"});
    // Each coasted end, its moves together, is 0.5 mm.
    const Read r = read(with);
    double end = 0.0;
    std::vector<double> ends_length;
    for (std::size_t k = r.layer_starts[10]; k < r.layer_starts[11]; ++k) {
        const Move& m = r.moves[k];
        if (m.xy && std::abs(m.e) < 1e-9 && m.f < 6000.0 && k > 0 &&
            (r.moves[k - 1].e > 0.0 || (end > 0.0 && std::abs(r.moves[k - 1].e) < 1e-9))) {
            end += m.length();
        } else if (end > 0.0) {
            ends_length.push_back(end);
            end = 0.0;
        }
    }
    REQUIRE_FALSE(ends_length.empty());
    for (const double l : ends_length)
        CHECK_THAT(l, WithinAbs(0.5, 2e-3));
    // Less filament in all (rule 4).
    double e_plain = 0.0, e_with = 0.0;
    for (const Move& m : read(plain).moves)
        e_plain += std::max(m.e, 0.0);
    for (const Move& m : read(with).moves)
        e_with += std::max(m.e, 0.0);
    CHECK(e_with < e_plain);
}
