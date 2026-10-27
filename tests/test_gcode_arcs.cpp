#include "config/settings.hpp"
#include "gcode/arcs.hpp"
#include "gcode/check.hpp"
#include "gcode/preview.hpp"
#include "gcode/writer.hpp"
#include "mesh/indexed_mesh.hpp"
#include "slice/pipeline.hpp"
#include "test_printer.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <cstdint>
#include <numbers>
#include <sstream>
#include <string>
#include <vector>

using namespace tisma;
using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

namespace {

// n + 1 points on a circle of radius r around (cx, cy), from angle a0 over
// sweep (negative: clockwise).
std::vector<ToolpathPoint> on_circle(double cx, double cy, double r, double a0, double sweep, int n, double width = 0.4) {
    std::vector<ToolpathPoint> out;
    for (int i = 0; i <= n; ++i) {
        const double a = a0 + sweep * i / n;
        out.push_back({{cx + r * std::cos(a), cy + r * std::sin(a)}, width});
    }
    return out;
}

// A cylinder of radius r and height h around the origin, as `sides` facets.
IndexedMesh cylinder(double r, double h, int sides) {
    IndexedMesh m;
    m.vertices.push_back({0.0f, 0.0f, 0.0f});
    m.vertices.push_back({0.0f, 0.0f, float(h)});
    for (int i = 0; i < sides; ++i) {
        const double a = 2.0 * std::numbers::pi * i / sides;
        const float x = float(r * std::cos(a)), y = float(r * std::sin(a));
        m.vertices.push_back({x, y, 0.0f});
        m.vertices.push_back({x, y, float(h)});
    }
    for (std::uint32_t i = 0; i < std::uint32_t(sides); ++i) {
        const std::uint32_t j = (i + 1) % std::uint32_t(sides);
        const std::uint32_t b0 = 2 + 2 * i, t0 = b0 + 1, b1 = 2 + 2 * j, t1 = b1 + 1;
        m.triangles.push_back({0, b1, b0});
        m.triangles.push_back({1, t0, t1});
        m.triangles.push_back({b0, b1, t1});
        m.triangles.push_back({b0, t1, t0});
    }
    return m;
}

std::size_t count_of(const std::string& gcode, const std::string& start) {
    std::size_t n = 0;
    for (std::size_t at = 0; at < gcode.size();) {
        const std::size_t end = gcode.find('\n', at);
        n += gcode.compare(at, start.size(), start) == 0;
        at = end == std::string::npos ? gcode.size() : end + 1;
    }
    return n;
}

} // namespace

TEST_CASE("G2/G3 arcs: points on a circle make arcs of half a turn at most", "[gcode_arcs]") {
    // A whole counter-clockwise turn in 64 moves, back to its start.
    const std::vector<ToolpathPoint> turn = on_circle(5.0, -3.0, 10.0, 0.3, 2.0 * std::numbers::pi, 64);
    const std::vector<ArcRun> arcs = fit_arcs(turn, 0.0125);
    REQUIRE(arcs.size() == 2);
    CHECK(arcs[0].first == 0);
    CHECK(arcs[0].last == arcs[1].first);
    CHECK(arcs[1].last == 64);
    for (const ArcRun& a : arcs) {
        CHECK(a.ccw);
        CHECK_THAT(a.center.x, WithinAbs(5.0, 1e-9));
        CHECK_THAT(a.center.y, WithinAbs(-3.0, 1e-9));
        CHECK_THAT(a.radius, WithinAbs(10.0, 1e-9));
        CHECK(a.sweep <= kArcMaxSweep + 1e-9);
    }
    CHECK_THAT(arcs[0].length() + arcs[1].length(), WithinRel(2.0 * std::numbers::pi * 10.0, 1e-9));
    // Clockwise: G2.
    const std::vector<ArcRun> cw = fit_arcs(on_circle(0.0, 0.0, 3.0, 0.0, -1.0, 20), 0.0125);
    REQUIRE(cw.size() == 1);
    CHECK_FALSE(cw[0].ccw);
    CHECK_THAT(cw[0].sweep, WithinAbs(1.0, 1e-9));
}

TEST_CASE("G2/G3 arcs: what is not one circle stays straight", "[gcode_arcs]") {
    // A hexagon's corners lie on a circle, but its sides are far from it.
    CHECK(fit_arcs(on_circle(0.0, 0.0, 10.0, 0.0, 2.0 * std::numbers::pi, 6), 0.0125).empty());
    // A line, and a zigzag.
    std::vector<ToolpathPoint> line, zigzag;
    for (int i = 0; i < 10; ++i) {
        line.push_back({{double(i), 0.0}, 0.4});
        zigzag.push_back({{double(i), i % 2 ? 0.5 : 0.0}, 0.4});
    }
    CHECK(fit_arcs(line, 0.0125).empty());
    CHECK(fit_arcs(zigzag, 0.0125).empty());
    // Too few moves.
    CHECK(fit_arcs(on_circle(0.0, 0.0, 10.0, 0.0, 0.3, 2), 0.0125).empty());
    // Off the circle by more than the tolerance in the middle: two arcs, or
    // none, but never one through it.
    std::vector<ToolpathPoint> bump = on_circle(0.0, 0.0, 10.0, 0.0, 1.0, 20);
    bump[10].p.x *= 1.01, bump[10].p.y *= 1.01;
    for (const ArcRun& a : fit_arcs(bump, 0.0125))
        CHECK((a.last <= 10 || a.first >= 10));
    // A change of width, a Z offset (Z anti-aliasing) or a wavy bead below
    // ends an arc there.
    for (int what = 0; what < 3; ++what) {
        std::vector<ToolpathPoint> p = on_circle(0.0, 0.0, 10.0, 0.0, 1.0, 20);
        (what == 0 ? p[12].width : what == 1 ? p[12].dz : p[12].below) += 0.05;
        for (const ArcRun& a : fit_arcs(p, 0.0125))
            CHECK((a.last < 12 || a.first > 12));
    }
}

TEST_CASE("G2/G3 arcs: G2 and G3 in the G-code (gcode_extras.md, rule 10, X10)", "[gcode_arcs][gcode]") {
    // X10: a cylinder of 10 mm radius, 5 mm high.
    const IndexedMesh m = cylinder(10.0, 5.0, 128);
    Settings s = test_printer();
    s.infill_density = 20.0;
    const SlicedModel model = slice_model(m, s);
    std::ostringstream plain_out;
    write_gcode(plain_out, model, s);
    const std::string plain = plain_out.str();
    s.gcode_arcs = true;
    std::ostringstream arcs_out;
    write_gcode(arcs_out, model, s);
    const std::string arcs = arcs_out.str();
    const std::size_t g1 = count_of(arcs, "G1 "), g2 = count_of(arcs, "G2 "), g3 = count_of(arcs, "G3 ");
    INFO("G1 " << g1 << " G2 " << g2 << " G3 " << g3 << ", without arcs G1 " << count_of(plain, "G1 "));
    CHECK(count_of(plain, "G2 ") + count_of(plain, "G3 ") == 0);
    CHECK(g3 >= 2 * 3 * model.layers.size()); // every wall loop in two halves at least
    CHECK(g1 * 3 < count_of(plain, "G1 "));
    // The check reads the arcs, and finds the same filament along the same path.
    const GcodeCheck a = check_gcode(arcs, gcode_limits(model, s)), p = check_gcode(plain, gcode_limits(model, s));
    CHECK(a.ok());
    CHECK_THAT(a.stats.filament, WithinRel(p.stats.filament, 0.002));
    CHECK_THAT(a.stats.extruded_path, WithinRel(p.stats.extruded_path, 0.002));
    // So does the preview, and the time hardly changes (the arcs keep to the path's moves).
    const GcodePreview pa = read_gcode_preview(arcs), pp = read_gcode_preview(plain);
    CHECK_THAT(pa.extruded, WithinRel(pp.extruded, 0.002));
    CHECK(pa.arcs == g2 + g3);
    CHECK_THAT(pa.estimated_seconds, WithinRel(pp.estimated_seconds, 0.02));
    // With more tolerance, no more arcs (X10: fewer and longer).
    s.gcode_resolution = 0.05;
    std::ostringstream loose;
    write_gcode(loose, model, s);
    CHECK(count_of(loose.str(), "G2 ") + count_of(loose.str(), "G3 ") <= g2 + g3);
    CHECK(check_gcode(loose.str(), gcode_limits(model, s)).ok());
}

TEST_CASE("G2/G3 arcs: the check finds an arc that ends off its circle", "[gcode_arcs][check]") {
    const IndexedMesh m = cylinder(10.0, 1.0, 64);
    Settings s = test_printer();
    s.gcode_arcs = true;
    const SlicedModel model = slice_model(m, s);
    std::ostringstream out;
    write_gcode(out, model, s);
    const std::string gcode = out.str();
    const GcodeLimits limits = gcode_limits(model, s);
    REQUIRE(check_gcode(gcode, limits).ok());
    const std::size_t at = gcode.find("\nG3 ");
    REQUIRE(at != std::string::npos);
    const std::size_t j = gcode.find(" J", at), end = gcode.find(' ', j + 1);
    // The centre moved 0.2 mm: the end is no longer on the start's circle.
    std::string off = gcode;
    off.replace(j + 2, end - j - 2, std::to_string(std::stod(gcode.substr(j + 2, end - j - 2)) + 0.2));
    const GcodeCheck c = check_gcode(off, limits);
    REQUIRE_FALSE(c.ok());
    CHECK(c.problems.front().message.find("an arc that starts") != std::string::npos);
}
