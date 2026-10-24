#include "infill/rectilinear.hpp"
#include "perimeters/coverage.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <cstdio>

using namespace tisma;
using Catch::Matchers::WithinAbs;

namespace {

Polygons square(double x0, double y0, double side) {
    return {{{to_units(x0), to_units(y0)},
             {to_units(x0 + side), to_units(y0)},
             {to_units(x0 + side), to_units(y0 + side)},
             {to_units(x0), to_units(y0 + side)}}};
}

} // namespace

TEST_CASE("rectilinear lines on a grid fixed to the origin", "[infill]") {
    // A 10 mm square, lines 1 mm apart, beads 0.4 mm: the lines run inside the
    // square shrunk by 0.2 mm, at y = 1, 2, ..., 9.
    const std::vector<Toolpath> lines = rectilinear_fill(square(0, 0, 10), 1.0, 0.0, 0.4);
    REQUIRE(lines.size() == 9);
    for (std::size_t i = 0; i < lines.size(); ++i) {
        const Toolpath& t = lines[i];
        CHECK_FALSE(t.closed);
        REQUIRE(t.points.size() == 2);
        CHECK_THAT(t.points[0].p.y, WithinAbs(double(i + 1), 1e-6));
        CHECK_THAT(t.points[1].p.y, WithinAbs(double(i + 1), 1e-6));
        CHECK_THAT(t.points[0].p.x, WithinAbs(0.2, 1e-6));
        CHECK_THAT(t.points[1].p.x, WithinAbs(9.8, 1e-6));
        CHECK(t.points[0].width == 0.4);
    }
    // At 90 degrees the lines are vertical, at x = 1, ..., 9; the same grid
    // lines whatever part of the plane the region covers.
    const std::vector<Toolpath> vertical = rectilinear_fill(square(0, 0, 10), 1.0, 90.0, 0.4);
    REQUIRE(vertical.size() == 9);
    for (const Toolpath& t : vertical) {
        CHECK_THAT(t.points[0].p.x, WithinAbs(std::round(t.points[0].p.x), 1e-6));
        CHECK_THAT(t.points[0].p.x, WithinAbs(t.points[1].p.x, 1e-6));
    }
    const std::vector<Toolpath> shifted = rectilinear_fill(square(0.5, 0.5, 10), 1.0, 0.0, 0.4);
    REQUIRE(shifted.size() == 10); // y = 1 .. 10 fit in 0.7 .. 10.3
    CHECK_THAT(shifted.front().points[0].p.y, WithinAbs(1.0, 1e-6));
    // Nothing fits, nothing to do.
    CHECK(rectilinear_fill(square(0, 0, 0.3), 1.0, 0.0, 0.4).empty());
    CHECK(rectilinear_fill({}, 1.0, 0.0, 0.4).empty());
}

TEST_CASE("fill_layer: solid at 100%, sparse at the density, angles turn by layer", "[infill]") {
    LayerSurfaces s;
    s.top = square(0, 0, 10);
    s.sparse = square(20, 0, 10);
    FillParams p; // 0.4 mm beads, 20%, 45 degrees
    const LayerFill even = fill_layer(s, 0, p);
    const LayerFill odd = fill_layer(s, 1, p);
    REQUIRE_FALSE(even.solid.empty());
    REQUIRE_FALSE(even.sparse.empty());
    auto direction = [](const Toolpath& t) {
        return std::atan2(t.points.back().p.y - t.points.front().p.y, t.points.back().p.x - t.points.front().p.x) * 180.0 /
               3.14159265358979323846;
    };
    CHECK_THAT(direction(even.solid.front()), WithinAbs(45.0, 1e-3));
    CHECK_THAT(std::fabs(direction(odd.solid.front())), WithinAbs(135.0, 1e-3));
    // Sparse lines are 0.4 / 0.2 = 2 mm apart, five times fewer than the solid ones over the same area.
    CHECK(std::abs(int(even.solid.size()) - 5 * int(even.sparse.size())) <= 5);
    p.density = 0.0;
    CHECK(fill_layer(s, 0, p).sparse.empty());
}

TEST_CASE("a solid layer is covered but for its edges", "[infill]") {
    // The 7.6 mm square left inside 3 walls of the 10 mm cube, filled at 45 degrees.
    const Polygons region = square(1.2, 1.2, 7.6);
    LayerSurfaces s;
    s.top = region;
    const LayerFill fill = fill_layer(s, 0, FillParams{});
    const Coverage c = measure_coverage(region, fill.solid);
    std::printf("solid 7.6 mm square at 45 degrees: %zu lines, gaps %.4f mm2 (%.2f%%), outside %.4f, overlap %.4f\n",
                fill.solid.size(), c.gaps, 100.0 * c.gaps / c.target, c.outside, c.overlap);
    // The gaps are along the edges, where the lines end with round caps inside
    // the square shrunk by half a bead (measured: 2.72 mm2, 4.7%); anchoring the
    // ends to the walls is a separate step. Less than the 0.2 mm band around it.
    CHECK(c.gaps < 4 * 7.6 * 0.2);
    // The interior, a bead in from the edges, is covered.
    const Coverage inside = measure_coverage(square(1.6, 1.6, 6.8), fill.solid);
    CHECK(inside.gaps < 1e-3 * inside.target);
    CHECK(c.overlap < 0.01 * c.target);
}
