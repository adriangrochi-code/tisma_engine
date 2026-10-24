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
        CHECK_THAT(t.points[0].p.x, WithinAbs(0.2, 1e-4)); // 10 nm of slack at the ends
        CHECK_THAT(t.points[1].p.x, WithinAbs(9.8, 1e-4));
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
    auto direction = [](const Toolpath& t) { // of the longest piece: a line, not a connection
        std::size_t best = 0;
        double longest = 0.0;
        for (std::size_t i = 0; i + 1 < t.points.size(); ++i) {
            const double l = std::hypot(t.points[i + 1].p.x - t.points[i].p.x, t.points[i + 1].p.y - t.points[i].p.y);
            if (l > longest)
                longest = l, best = i;
        }
        double d = std::atan2(t.points[best + 1].p.y - t.points[best].p.y, t.points[best + 1].p.x - t.points[best].p.x) *
                   180.0 / 3.14159265358979323846;
        return d < 0.0 ? d + 180.0 : d; // a line, either way along it
    };
    CHECK_THAT(direction(even.solid.front()), WithinAbs(45.0, 1e-3));
    CHECK_THAT(direction(odd.solid.front()), WithinAbs(135.0, 1e-3));
    // Sparse lines are 0.4 / 0.2 = 2 mm apart: about a fifth of the solid lines' length over the same area.
    auto length = [](const std::vector<Toolpath>& paths) {
        double sum = 0.0;
        for (const Toolpath& t : paths)
            for (std::size_t i = 0; i + 1 < t.points.size(); ++i)
                sum += std::hypot(t.points[i + 1].p.x - t.points[i].p.x, t.points[i + 1].p.y - t.points[i].p.y);
        return sum;
    };
    CHECK(length(even.sparse) > 0.15 * length(even.solid));
    CHECK(length(even.sparse) < 0.3 * length(even.solid));
    p.density = 0.0;
    CHECK(fill_layer(s, 0, p).sparse.empty());
}

TEST_CASE("a solid layer lays down its area: connected lines fitted to the region", "[infill]") {
    // The 7.6 mm square left inside 3 walls of the 10 mm cube, and an L. The
    // zig-zag joins line ends along the boundary, half a bead in: nothing
    // enters the walls (but for 10 nm of slack), and on the square the material
    // laid down is the area within 1 % (measured: -0.06 % at 0 degrees, -0.9 %
    // at 30, -0.6 % at 45). The lines are fitted across the whole L, so at 0
    // degrees its inner corner leaves a 0.2 mm band along one arm (-2.9 %).
    const Polygons square_region = square(1.2, 1.2, 7.6);
    const Polygons l_region = {{{to_units(0), to_units(0)}, {to_units(10), to_units(0)}, {to_units(10), to_units(3)},
                                {to_units(3), to_units(3)}, {to_units(3), to_units(10)}, {to_units(0), to_units(10)}}};
    for (const Polygons* region : {&square_region, &l_region})
        for (double angle : {0.0, 30.0, 45.0}) {
            LayerSurfaces s;
            s.top = *region;
            FillParams p;
            p.angle = angle;
            const LayerFill fill = fill_layer(s, 0, p);
            const Coverage c = measure_coverage(*region, fill.solid);
            std::printf("solid fill, angle %2.0f, %s: %zu paths, gaps %.3f mm2 (%.2f%%), overlap %.3f, laid - area %+.3f\n",
                        angle, region == &square_region ? "square" : "L", fill.solid.size(), c.gaps,
                        100.0 * c.gaps / c.target, c.overlap, c.deposited - c.target);
            INFO("angle " << angle);
            CHECK(c.outside < 1e-3);
            CHECK(std::fabs(c.deposited - c.target) < (region == &square_region ? 0.01 : 0.03) * c.target);
            CHECK(c.gaps < 0.05 * c.target);
            CHECK(fill.solid.size() < 10); // joined into a few zig-zags
        }
    // The interior of the square, a bead in from the edges, is fully covered.
    LayerSurfaces s;
    s.top = square_region;
    const Coverage inside = measure_coverage(square(1.6, 1.6, 6.8), fill_layer(s, 0, FillParams{}).solid);
    CHECK(inside.gaps < 1e-3 * inside.target);
}
