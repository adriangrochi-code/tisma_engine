#include "config/settings.hpp"
#include "gcode/resolution.hpp"
#include "gcode/writer.hpp"
#include "mesh/indexed_mesh.hpp"
#include "slice/pipeline.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <map>
#include <numbers>
#include <sstream>
#include <string>

using namespace tisma;
using Catch::Matchers::WithinRel;

namespace {

Toolpath line_path(int n, double length) {
    Toolpath t;
    for (int i = 0; i < n; ++i)
        t.points.push_back({{length * i / (n - 1), 0.0}, 0.4});
    return t;
}

// Every point of `from` within the tolerance of the path `to` (open or closed).
bool within(const Toolpath& from, const Toolpath& to, double tolerance) {
    for (const ToolpathPoint& p : from.points) {
        double best = 1e9;
        const std::size_t n = to.points.size();
        for (std::size_t i = 0; i + 1 < n + (to.closed ? 1 : 0); ++i) {
            const Point2d a = to.points[i].p, b = to.points[(i + 1) % n].p;
            const double dx = b.x - a.x, dy = b.y - a.y, len2 = dx * dx + dy * dy;
            const double t = len2 > 0.0 ? std::clamp(((p.p.x - a.x) * dx + (p.p.y - a.y) * dy) / len2, 0.0, 1.0) : 0.0;
            best = std::min(best, std::hypot(p.p.x - a.x - t * dx, p.p.y - a.y - t * dy));
        }
        if (best > tolerance + 1e-12)
            return false;
    }
    return true;
}

// A cylinder of `sides` flat sides, radius r and height h around (cx, cy).
IndexedMesh cylinder(int sides, float r, float h, float cx, float cy) {
    IndexedMesh m;
    for (float z : {0.0f, h}) {
        m.vertices.push_back({cx, cy, z});
        for (int i = 0; i < sides; ++i) {
            const double a = 2.0 * std::numbers::pi * i / sides;
            m.vertices.push_back({cx + r * float(std::cos(a)), cy + r * float(std::sin(a)), z});
        }
    }
    const auto top = std::uint32_t(sides + 1);
    for (std::uint32_t i = 0; i < std::uint32_t(sides); ++i) {
        const std::uint32_t a = 1 + i, b = 1 + (i + 1) % std::uint32_t(sides);
        m.triangles.push_back({0, b, a});
        m.triangles.push_back({top, top + a, top + b});
        m.triangles.push_back({a, b, top + b});
        m.triangles.push_back({a, top + b, top + a});
    }
    return m;
}

struct Counts {
    std::map<int, int> outer_moves;    // extrusion moves of the outer wall, by layer
    std::map<int, int> outer_starts;   // travels into the outer wall, by layer
    std::map<std::string, double> length; // extruded length by type
};

Counts count(const std::string& gcode) {
    Counts c;
    std::istringstream in(gcode);
    int layer = -1;
    std::string type, line;
    double x = 0.0, y = 0.0;
    bool travelled = false;
    while (std::getline(in, line)) {
        if (line.rfind(";LAYER_CHANGE", 0) == 0)
            ++layer;
        else if (line.rfind(";TYPE:", 0) == 0)
            type = line.substr(6);
        if (line.rfind("G1", 0) != 0)
            continue;
        const auto value = [&](char axis, double now) {
            const std::size_t at = line.find(std::string(" ") + axis);
            return at == std::string::npos ? now : std::stod(line.substr(at + 2));
        };
        const bool xy = line.find(" X") != std::string::npos || line.find(" Y") != std::string::npos;
        const double nx = value('X', x), ny = value('Y', y);
        const bool extrudes = xy && line.find(" E") != std::string::npos && line.find(" E-") == std::string::npos;
        if (extrudes) {
            c.length[type] += std::hypot(nx - x, ny - y);
            if (type == "External perimeter") {
                ++c.outer_moves[layer];
                c.outer_starts[layer] += travelled ? 1 : 0;
            }
            travelled = false;
        } else if (xy || line == "G1 F7800") {
            travelled = true;
        }
        x = nx, y = ny;
    }
    return c;
}

} // namespace

TEST_CASE("resolution: points on a straight line go, the ends stay", "[resolution]") {
    const Toolpath t = simplify_toolpath(line_path(50, 10.0), 0.0125);
    REQUIRE(t.points.size() == 2);
    CHECK(t.points.front().p.x == 0.0);
    CHECK(t.points.back().p.x == 10.0);
    // However long the line.
    const Toolpath longer = simplify_toolpath(line_path(200000, 100.0), 0.0125);
    CHECK(longer.points.size() == 2);
    // Nothing goes with a tolerance of 0, nor from a path of two points.
    CHECK(simplify_toolpath(line_path(50, 10.0), 0.0).points.size() == 50);
    CHECK(simplify_toolpath(line_path(2, 10.0), 0.0125).points.size() == 2);
}

TEST_CASE("resolution: a circle keeps its shape within the tolerance and its seam", "[resolution]") {
    // 128 sides of radius 10: the sagitta over two sides is 0.012 mm, over
    // three 0.027: every other corner goes (the spec's test C17: 65 moves).
    Toolpath circle;
    circle.closed = true;
    for (int i = 0; i < 128; ++i) {
        const double a = 2.0 * std::numbers::pi * i / 128;
        circle.points.push_back({{100.0 + 10.0 * std::cos(a), 100.0 + 10.0 * std::sin(a)}, 0.45});
    }
    const Toolpath t = simplify_toolpath(circle, 0.0125);
    CHECK(t.closed);
    CHECK(t.points.size() == 64);
    CHECK(t.points.front().p.x == circle.points.front().p.x);
    CHECK(t.points.front().p.y == circle.points.front().p.y);
    CHECK(within(circle, t, 0.0125));
    // A loop smaller than the tolerance keeps three points.
    Toolpath dot;
    dot.closed = true;
    for (int i = 0; i < 12; ++i)
        dot.points.push_back({{0.004 * std::cos(i * 0.5236), 0.004 * std::sin(i * 0.5236)}, 0.4});
    CHECK(simplify_toolpath(dot, 0.0125).points.size() >= 3);
}

TEST_CASE("resolution: the width, the Z and the bead below change only where the points stay", "[resolution]") {
    // A width that grows evenly along a straight line is the same with two points.
    Toolpath ramp = line_path(21, 10.0);
    for (std::size_t i = 0; i < ramp.points.size(); ++i)
        ramp.points[i].width = 0.4 + 0.01 * double(i);
    CHECK(simplify_toolpath(ramp, 0.0125).points.size() == 2);
    // A step in width keeps the points around it.
    Toolpath step = line_path(21, 10.0);
    for (std::size_t i = 10; i < step.points.size(); ++i)
        step.points[i].width = 0.5;
    const Toolpath s = simplify_toolpath(step, 0.0125);
    CHECK(s.points.size() == 4);
    CHECK(s.points[1].width == 0.4);
    CHECK(s.points[2].width == 0.5);
    // A bump in Z (Z anti-aliasing) or in the bead below (wavy infill) stays.
    Toolpath bump = line_path(21, 10.0);
    bump.points[10].dz = -0.05;
    CHECK(simplify_toolpath(bump, 0.0125).points.size() == 5);
    Toolpath wave = line_path(21, 10.0);
    wave.points[10].below = 0.1;
    CHECK(simplify_toolpath(wave, 0.0125).points.size() == 5);
    // Up to half the last digit written is the same.
    Toolpath close = line_path(21, 10.0);
    close.points[10].width = 0.4004;
    CHECK(simplify_toolpath(close, 0.0125).points.size() == 2);
}

TEST_CASE("resolution: a cylinder's outer wall is one loop of about 64 moves per layer", "[resolution]") {
    // The spec's test C17 (slicing.md): 128 sides, radius 10, 5 mm tall, with
    // slicing.simplify_tolerance 0. Every side's two triangles leave a corner
    // in the middle of the side, which broke the walls into open pieces.
    const IndexedMesh m = cylinder(128, 10.0f, 5.0f, 100.0f, 100.0f);
    Settings s;
    s.simplify_tolerance = 0.0;
    std::ostringstream fine, coarse;
    write_gcode(coarse, slice_model(m, s), s);
    Settings off = s;
    off.gcode_resolution = 0.0;
    write_gcode(fine, slice_model(m, off), off);
    const Counts c = count(coarse.str()), f = count(fine.str());
    REQUIRE(c.outer_moves.count(11));
    CHECK(c.outer_moves.at(11) >= 63);
    CHECK(c.outer_moves.at(11) <= 70);
    CHECK(f.outer_moves.at(11) >= 128);
    for (const auto& [layer, starts] : c.outer_starts)
        CHECK(starts == 1);
    // The same beads: the length laid down moves by the chords' sagitta only.
    for (const auto& [type, length] : f.length)
        CHECK_THAT(c.length.at(type), WithinRel(length, 1e-3));
}
