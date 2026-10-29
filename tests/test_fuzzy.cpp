#include "mesh/indexed_mesh.hpp"
#include "mesh/paint.hpp"
#include "perimeters/fuzzy.hpp"
#include "slice/pipeline.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <algorithm>
#include <cmath>
#include <numbers>

using namespace tisma;

namespace {

// A tube around (0, 0) from z 0 to `height`: outer and inner radius, n sides.
// Zero inner radius: a solid cylinder.
TriangleMesh tube(double outer, double inner, double height, int n) {
    TriangleMesh m;
    const auto at = [&](double r, int i, double z) {
        const double a = 2.0 * std::numbers::pi * i / n;
        return Vec3f{float(r * std::cos(a)), float(r * std::sin(a)), float(z)};
    };
    for (int i = 0; i < n; ++i) {
        const int j = i + 1;
        // Outer wall, facing out.
        m.add_triangle(at(outer, i, 0), at(outer, j, 0), at(outer, j, height));
        m.add_triangle(at(outer, i, 0), at(outer, j, height), at(outer, i, height));
        if (inner > 0.0) {
            // Inner wall, facing the axis; rings top and bottom.
            m.add_triangle(at(inner, i, 0), at(inner, j, height), at(inner, j, 0));
            m.add_triangle(at(inner, i, 0), at(inner, i, height), at(inner, j, height));
            m.add_triangle(at(inner, i, height), at(outer, i, height), at(outer, j, height));
            m.add_triangle(at(inner, i, height), at(outer, j, height), at(inner, j, height));
            m.add_triangle(at(inner, i, 0), at(outer, j, 0), at(outer, i, 0));
            m.add_triangle(at(inner, i, 0), at(inner, j, 0), at(outer, j, 0));
        } else {
            m.add_triangle({0, 0, float(height)}, at(outer, i, height), at(outer, j, height));
            m.add_triangle({0, 0, 0}, at(outer, j, 0), at(outer, i, 0));
        }
    }
    return m;
}

// The radii of the points of the walls with this bead index, at layer l.
std::pair<double, double> radii(const SlicedModel& m, std::size_t l, int index) {
    double lo = 1e9, hi = -1e9;
    for (const Toolpath& t : m.walls[l].toolpaths)
        if (t.index == index)
            for (const ToolpathPoint& p : t.points) {
                const double r = std::hypot(p.p.x, p.p.y);
                lo = std::min(lo, r), hi = std::max(hi, r);
            }
    return {lo, hi};
}

// The outer walls' points, at layer l, nearer the axis than `r` (the hole's).
std::vector<double> hole_radii(const SlicedModel& m, std::size_t l, double r) {
    std::vector<double> out;
    for (const Toolpath& t : m.walls[l].toolpaths)
        if (t.index == 0)
            for (const ToolpathPoint& p : t.points)
                if (std::hypot(p.p.x, p.p.y) < r)
                    out.push_back(std::hypot(p.p.x, p.p.y));
    return out;
}

} // namespace

TEST_CASE("fuzzy skin: the outer wall moves off its line by the thickness at most (F1)", "[fuzzy]") {
    Settings s;
    s.gpu = GpuMode::Off;
    const IndexedMesh cyl = weld_exact(tube(10.0, 0.0, 5.0, 256));
    const SlicedModel smooth = slice_model(cyl, s);
    s.fuzzy_mode = FuzzyMode::External;
    const SlicedModel rough = slice_model(cyl, s);
    const std::size_t l = 10;
    const auto [lo0, hi0] = radii(smooth, l, 0);
    const auto [lo, hi] = radii(rough, l, 0);
    // The outer bead's line is at 9.8 mm; its points now from 9.5 to 10.1.
    CHECK(hi0 - lo0 < 0.01);
    CHECK(lo >= lo0 - 0.3 - 1e-6);
    CHECK(hi <= hi0 + 0.3 + 1e-6);
    CHECK(lo < lo0 - 0.2);
    CHECK(hi > hi0 + 0.2);
    // About one point every 0.8 mm along 61.6 mm.
    std::size_t points = 0;
    double longest = 0.0, total = 0.0;
    for (const Toolpath& t : rough.walls[l].toolpaths)
        if (t.index == 0) {
            points += t.points.size();
            for (std::size_t i = 0; i < t.points.size(); ++i) {
                const Point2d& a = t.points[i].p;
                const Point2d& b = t.points[(i + 1) % t.points.size()].p;
                total += std::hypot(b.x - a.x, b.y - a.y);
                longest = std::max(longest, std::hypot(b.x - a.x, b.y - a.y));
            }
        }
    CHECK(points > 60);
    CHECK(points < 100);
    CHECK(longest < 1.2 + 0.6 + 0.1); // the widest gap and both its points moved off the line
    CHECK(total / double(points) > 0.6);
    // The inner walls never change.
    for (int index = 1; index <= 2; ++index) {
        CAPTURE(index);
        const auto a = radii(smooth, l, index), b = radii(rough, l, index);
        CHECK(a.first == b.first);
        CHECK(a.second == b.second);
    }
    // The same input, the same points.
    const SlicedModel again = slice_model(cyl, s);
    REQUIRE(again.walls[l].toolpaths.size() == rough.walls[l].toolpaths.size());
    for (std::size_t k = 0; k < again.walls[l].toolpaths.size(); ++k)
        CHECK(again.walls[l].toolpaths[k].points.size() == rough.walls[l].toolpaths[k].points.size());
}

TEST_CASE("fuzzy skin: holes are rough only in all mode (F2)", "[fuzzy]") {
    Settings s;
    s.wall_width = s.outer_wall_width = 0.4; // 0.4 mm beads (as measured), the outer wall as the others
    s.gpu = GpuMode::Off;
    const IndexedMesh t = weld_exact(tube(10.0, 5.0, 5.0, 256));
    s.fuzzy_mode = FuzzyMode::External;
    const SlicedModel external = slice_model(t, s);
    s.fuzzy_mode = FuzzyMode::All;
    const SlicedModel all = slice_model(t, s);
    const std::size_t l = 10;
    // The hole's outer bead lies at 5.2 mm.
    const std::vector<double> smooth = hole_radii(external, l, 7.0), rough = hole_radii(all, l, 7.0);
    REQUIRE_FALSE(smooth.empty());
    REQUIRE_FALSE(rough.empty());
    CHECK_THAT(*std::min_element(smooth.begin(), smooth.end()), Catch::Matchers::WithinAbs(5.2, 0.01));
    CHECK_THAT(*std::max_element(smooth.begin(), smooth.end()), Catch::Matchers::WithinAbs(5.2, 0.01));
    CHECK(*std::min_element(rough.begin(), rough.end()) < 5.0);
    CHECK(*std::max_element(rough.begin(), rough.end()) > 5.4);
    // The contour's outer wall is rough in both.
    CHECK(radii(external, l, 0).second > 9.95);
}

TEST_CASE("fuzzy skin: painted, only where the paint is, with the mode off (rule 4)", "[fuzzy][paint]") {
    Settings s;
    s.wall_width = s.outer_wall_width = 0.4; // 0.4 mm beads (as measured), the outer wall as the others
    s.gpu = GpuMode::Off;
    const TriangleMesh mesh = tube(10.0, 0.0, 5.0, 256);
    Painting p;
    // A spot on the side facing +X, from z 0.5 to 4.5.
    Brush brush;
    brush.centre = {10, 0, 2.5};
    brush.radius = 2.0;
    paint(p, PaintChannel::Fuzzy, mesh, brush);
    const SlicedModel m = slice_model(weld_exact(mesh), s, nullptr, nullptr, &p);
    const std::size_t l = 12; // z 2.5
    double off_spot = 0.0, on_spot = 0.0;
    std::size_t rough = 0;
    for (const Toolpath& t : m.walls[l].toolpaths)
        if (t.index == 0)
            for (const ToolpathPoint& q : t.points) {
                const double away = std::fabs(std::hypot(q.p.x, q.p.y) - 9.8);
                if (q.p.x < 5.0)
                    off_spot = std::max(off_spot, away);
                else
                    on_spot = std::max(on_spot, away);
                rough += away > 0.05 ? 1 : 0;
            }
    CHECK(off_spot < 0.01); // the rest of the loop keeps its line
    CHECK(on_spot > 0.2);
    CHECK(on_spot <= 0.3 + 1e-6);
    CHECK(rough >= 3);
    // Above the spot, nothing.
    double top = 0.0;
    for (const Toolpath& t : m.walls[23].toolpaths) // z 4.7
        if (t.index == 0)
            for (const ToolpathPoint& q : t.points)
                top = std::max(top, std::fabs(std::hypot(q.p.x, q.p.y) - 9.8));
    CHECK(top < 0.01);
}
