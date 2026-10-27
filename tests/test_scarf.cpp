#include "gcode/scarf.hpp"

#include "config/settings.hpp"
#include "gcode/check.hpp"
#include "gcode/order.hpp"
#include "gcode/writer.hpp"
#include "mesh/indexed_mesh.hpp"
#include "slice/pipeline.hpp"
#include "test_printer.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <map>
#include <numbers>
#include <sstream>
#include <string>

using namespace tisma;
using Catch::Matchers::WithinAbs;

namespace {

Toolpath circle(double r, int n) {
    Toolpath t;
    t.closed = true;
    for (int i = 0; i < n; ++i) {
        const double a = 2.0 * std::numbers::pi * i / n;
        t.points.push_back({{r * std::cos(a), r * std::sin(a)}, 0.4});
    }
    return t;
}

Toolpath square(double side) {
    Toolpath t;
    t.closed = true;
    t.points = {{{0, 0}, 0.4}, {{side, 0}, 0.4}, {{side, side}, 0.4}, {{0, side}, 0.4}};
    return t;
}

double loop_length(const Toolpath& t) {
    double sum = 0.0;
    for (std::size_t i = 0; i < t.points.size(); ++i) {
        const Point2d& a = t.points[i].p;
        const Point2d& b = t.points[(i + 1) % t.points.size()].p;
        sum += std::hypot(b.x - a.x, b.y - a.y);
    }
    return sum;
}

// Along an open path: its length, and the volume per mm of width it lays
// with each bead as thick as the emitter makes it (the layer's height, plus
// the nozzle's offset, less what lies under it).
struct Laid {
    double length = 0.0, area = 0.0, longest = 0.0;
};
Laid laid(const Toolpath& t, double height) {
    Laid out;
    for (std::size_t i = 1; i < t.points.size(); ++i) {
        const ToolpathPoint& a = t.points[i - 1];
        const ToolpathPoint& b = t.points[i];
        const double d = std::hypot(b.p.x - a.p.x, b.p.y - a.p.y);
        const double thick = height + (a.dz + b.dz) / 2.0 - (a.below + b.below) / 2.0;
        CHECK(thick >= -1e-12);
        CHECK(thick <= height + 1e-12);
        out.length += d;
        out.area += d * thick;
        out.longest = std::max(out.longest, d);
    }
    return out;
}

// A tube around (100, 100) from z 0 to `height`: outer and inner radius, n sides.
IndexedMesh tube(double outer, double inner, double height, int n) {
    IndexedMesh m;
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
        m.triangles.push_back({v(0, i), v(0, j), v(2, j)}); // outer wall
        m.triangles.push_back({v(0, i), v(2, j), v(2, i)});
        m.triangles.push_back({v(1, i), v(3, j), v(1, j)}); // inner wall
        m.triangles.push_back({v(1, i), v(3, i), v(3, j)});
        m.triangles.push_back({v(3, i), v(2, i), v(2, j)}); // top
        m.triangles.push_back({v(3, i), v(2, j), v(3, j)});
        m.triangles.push_back({v(1, i), v(0, j), v(0, i)}); // bottom
        m.triangles.push_back({v(1, i), v(1, j), v(0, j)});
    }
    return m;
}

// The outer walls of one layer of the G-code: (lowest Z, path length) of each
// run of External perimeter extrusions.
std::vector<std::pair<double, double>> outer_walls(const std::string& gcode, int layer) {
    std::vector<std::pair<double, double>> out;
    std::istringstream in(gcode);
    std::string line, type;
    int at = -1;
    double x = 0, y = 0, z = 0, e = 0;
    bool open = false;
    while (std::getline(in, line)) {
        if (line.rfind(";LAYER_CHANGE", 0) == 0) {
            ++at;
            open = false;
            continue;
        }
        if (line.rfind(";TYPE:", 0) == 0) {
            type = line.substr(6);
            open = false;
            continue;
        }
        if (line.rfind("G92", 0) == 0) {
            if (const auto k = line.find('E'); k != std::string::npos)
                e = std::stod(line.substr(k + 1));
            continue;
        }
        if (line.rfind("G1", 0) != 0)
            continue;
        double nx = x, ny = y, nz = z, ne = e;
        bool moves = false;
        std::istringstream words(line.substr(2));
        std::string w;
        while (words >> w) {
            if (w[0] == ';')
                break;
            const double value = std::stod(w.substr(1));
            if (w[0] == 'X')
                nx = value, moves = true;
            else if (w[0] == 'Y')
                ny = value, moves = true;
            else if (w[0] == 'Z')
                nz = value;
            else if (w[0] == 'E')
                ne = value;
        }
        if (at == layer && type == "External perimeter" && moves && ne > e) {
            if (!open)
                out.push_back({z, 0.0}), open = true;
            out.back().first = std::min(out.back().first, std::min(z, nz));
            out.back().second += std::hypot(nx - x, ny - y);
        } else if (moves && ne <= e) {
            open = false; // a travel ends the run
        }
        x = nx, y = ny, z = nz, e = ne;
    }
    return out;
}

} // namespace

TEST_CASE("scarf seam: a ramp up to the seam, the loop, and the ramp covered (rule 4)", "[scarf]") {
    const Toolpath loop = circle(10.0, 64);
    const double total = loop_length(loop);
    const double h = 0.2;
    const Toolpath ramp = scarf_loop(loop, h, 0.0, 20.0, 1.0);
    CHECK_FALSE(ramp.closed);
    CHECK(ramp.wavy);
    const Laid l = laid(ramp, h);
    CHECK_THAT(l.length, WithinAbs(total + 20.0, 1e-9));
    CHECK_THAT(l.area, WithinAbs(total * h, 1e-9)); // the filament of a plain loop
    // It starts down at the layer below and ends at the seam, at the top.
    CHECK_THAT(ramp.points.front().dz, WithinAbs(-h, 1e-12));
    CHECK(ramp.points.back().p.x == loop.points.front().p.x);
    CHECK(ramp.points.back().p.y == loop.points.front().p.y);
    CHECK(ramp.points.back().dz == 0.0);
    // Halfway up at the start: the nozzle half a layer down.
    const Toolpath half = scarf_loop(loop, h, 0.5, 20.0, 1.0);
    CHECK_THAT(half.points.front().dz, WithinAbs(-0.5 * h, 1e-12));
    CHECK_THAT(laid(half, h).area, WithinAbs(total * h, 1e-9));
    // Shorter ramp, and the whole way round.
    CHECK_THAT(laid(scarf_loop(loop, h, 0.0, 10.0, 1.0), h).length, WithinAbs(total + 10.0, 1e-9));
    const Toolpath round = scarf_loop(loop, h, 0.0, std::numeric_limits<double>::infinity(), 1.0);
    CHECK_THAT(laid(round, h).length, WithinAbs(2.0 * total, 1e-9));
    CHECK_THAT(laid(round, h).area, WithinAbs(total * h, 1e-9));
    CHECK(round.points.front().p.x == loop.points.front().p.x);
}

TEST_CASE("scarf seam: the ramp and its cover in pieces of at most the longest (rule 4)", "[scarf]") {
    // A 20 mm square: straight sides of 20 mm, the ramp along the last one.
    const Toolpath t = scarf_loop(square(20.0), 0.2, 0.0, 20.0, 1.0);
    std::size_t ramp_pieces = 0, cover_pieces = 0;
    for (std::size_t i = 1; i < t.points.size(); ++i) {
        const ToolpathPoint& a = t.points[i - 1];
        const ToolpathPoint& b = t.points[i];
        const double d = std::hypot(b.p.x - a.p.x, b.p.y - a.p.y);
        if (b.dz < 0.0 || a.dz < 0.0) {
            CHECK(d <= 1.0 + 1e-9);
            ++ramp_pieces;
        }
        if (b.below > 0.0) {
            CHECK(d <= 1.0 + 1e-9);
            ++cover_pieces;
        }
    }
    CHECK(ramp_pieces == 20);
    CHECK(cover_pieces == 20);
    CHECK_THAT(laid(t, 0.2).area, WithinAbs(80.0 * 0.2, 1e-9));
}

TEST_CASE("sharp corners: a square has them, a circle does not", "[scarf]") {
    CHECK(has_sharp_corner(square(20.0)));
    CHECK_FALSE(has_sharp_corner(circle(10.0, 64)));
    CHECK_FALSE(has_sharp_corner(circle(10.0, 360)));
}

TEST_CASE("scarf seam in the G-code: contours, everywhere, smooth only (W1, W1b, W1c)", "[scarf]") {
    const IndexedMesh m = tube(10.0, 5.0, 3.0, 128);
    Settings s = test_printer();
    s.skirt_loops = 0;
    s.wall_count = 2;
    const SlicedModel sliced = slice_model(m, s);
    const auto gcode = [&](const Settings& with) {
        std::ostringstream out;
        write_gcode(out, sliced, with);
        const GcodeCheck check = check_gcode(out.str(), gcode_limits(sliced, with));
        INFO((check.problems.empty() ? std::string() : check.problems.front().message));
        CHECK(check.ok());
        return out.str();
    };
    const std::string plain = gcode(s);
    const auto walls_plain = outer_walls(plain, 10);
    REQUIRE(walls_plain.size() == 2); // the outer contour and the hole
    const double z = walls_plain[0].first;
    Settings everywhere = s;
    everywhere.scarf = ScarfMode::Everywhere;
    const auto walls_every = outer_walls(gcode(everywhere), 10);
    REQUIRE(walls_every.size() == 2);
    Settings contours = s;
    contours.scarf = ScarfMode::Contours;
    const auto walls_contours = outer_walls(gcode(contours), 10);
    REQUIRE(walls_contours.size() == 2);
    // Each loop 20 mm longer with its ramp, from the layer below's top.
    std::multimap<double, double> by_length; // plain length -> lowest Z with the scarf
    for (std::size_t k = 0; k < 2; ++k) {
        CHECK_THAT(walls_every[k].second, WithinAbs(walls_plain[k].second + 20.0, 0.01));
        CHECK_THAT(walls_every[k].first, WithinAbs(z - s.layer_height, 1e-3));
    }
    // contours: only the outer contour's loop (the longer one) has the ramp.
    for (std::size_t k = 0; k < 2; ++k) {
        const bool outer = walls_plain[k].second > walls_plain[1 - k].second;
        CHECK_THAT(walls_contours[k].second, WithinAbs(walls_plain[k].second + (outer ? 20.0 : 0.0), 0.01));
        CHECK_THAT(walls_contours[k].first, WithinAbs(outer ? z - s.layer_height : z, 1e-3));
    }
    // The first layer keeps its plain loops: under it there is only the bed.
    CHECK(outer_walls(gcode(everywhere), 0).size() == 2);
    CHECK_THAT(outer_walls(gcode(everywhere), 0)[0].second, WithinAbs(outer_walls(plain, 0)[0].second, 1e-6));
}

TEST_CASE("scarf seam: a cube's sharp corners keep the plain loop unless asked (W1b)", "[scarf]") {
    IndexedMesh m;
    for (int k = 0; k < 8; ++k)
        m.vertices.push_back({float(90 + 20 * (k & 1)), float(90 + 20 * ((k >> 1) & 1)), float(5 * ((k >> 2) & 1))});
    const std::uint32_t faces[6][4] = {{0, 2, 3, 1}, {4, 5, 7, 6}, {0, 1, 5, 4}, {2, 6, 7, 3}, {0, 4, 6, 2}, {1, 3, 7, 5}};
    for (const auto& f : faces) {
        m.triangles.push_back({f[0], f[1], f[2]});
        m.triangles.push_back({f[0], f[2], f[3]});
    }
    Settings s = test_printer();
    s.skirt_loops = 0;
    s.scarf = ScarfMode::Everywhere;
    const SlicedModel sliced = slice_model(m, s);
    std::ostringstream smooth, any;
    write_gcode(smooth, sliced, s);
    s.scarf_only_smooth = false;
    write_gcode(any, sliced, s);
    const auto kept = outer_walls(smooth.str(), 10), ramped = outer_walls(any.str(), 10);
    REQUIRE(kept.size() == 1);
    REQUIRE(ramped.size() == 1);
    CHECK_THAT(ramped[0].second, WithinAbs(kept[0].second + 20.0, 0.01));
    CHECK(ramped[0].first < kept[0].first - 0.1);
    CHECK(check_gcode(any.str(), gcode_limits(sliced, s)).ok());
}
