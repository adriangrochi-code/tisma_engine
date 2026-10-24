#include "perimeters/coverage.hpp"
#include "perimeters/walls.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <numbers>

using namespace tisma;
using Catch::Matchers::WithinAbs;

namespace {

Polygon poly(std::initializer_list<Point2d> pts) {
    Polygon p;
    for (const Point2d& q : pts)
        p.push_back({to_units(q.x), to_units(q.y)});
    return p;
}

// The disc as kCoverageDiscSides-gon: its area relative to the true disc.
const double kDiscRatio = kCoverageDiscSides / (2.0 * std::numbers::pi) * std::sin(2.0 * std::numbers::pi / kCoverageDiscSides);

Coverage walls_coverage(const Polygons& region, const BeadingParams& p) {
    const LayerWalls w = layer_walls(region, p);
    const Polygons target =
        p.wall_count > 0 ? Clipper2Lib::Difference(region, w.infill, Clipper2Lib::FillRule::NonZero) : region;
    return measure_coverage(target, w.toolpaths);
}

} // namespace

TEST_CASE("bead area of a straight line: a rectangle with round caps", "[coverage]") {
    Toolpath t;
    t.points = {{{0, 0}, 1.0}, {{10, 0}, 1.0}};
    const double expected = 10.0 + kDiscRatio * std::numbers::pi / 4.0;
    CHECK_THAT(area_mm2(bead_area({t})), WithinAbs(expected, 1e-4));
    // Measured against the rectangle alone, the caps are outside and nothing overlaps.
    const Coverage c = measure_coverage({poly({{0, -0.5}, {10, -0.5}, {10, 0.5}, {0, 0.5}})}, {t});
    CHECK_THAT(c.covered, WithinAbs(10.0, 1e-6));
    CHECK_THAT(c.gaps, WithinAbs(0.0, 1e-6));
    CHECK_THAT(c.outside, WithinAbs(expected - 10.0, 1e-4));
    CHECK_THAT(c.overlap, WithinAbs(0.0, 1e-6));
    // Two copies overlap completely.
    CHECK_THAT(measure_coverage({}, {t, t}).overlap, WithinAbs(expected, 1e-4));
}

TEST_CASE("F2 criterion: walls from 0.3 to 1.2 mm leave no gaps and no excess", "[coverage]") {
    // The only gaps are the round ends at the strip's four corners: at most
    // (w/2)^2 (1 - pi/4) each, w being the widest bead there (0.6 mm), plus a
    // little for the polygonal discs.
    const BeadingParams p;
    for (int tenths = 3; tenths <= 12; ++tenths) {
        const double t = tenths / 10.0;
        const Coverage c = walls_coverage({poly({{0, 0}, {30, 0}, {30, t}, {0, t}})}, p);
        INFO("thickness " << t);
        CHECK_THAT(c.target, WithinAbs(30.0 * t, 1e-9));
        CHECK(c.gaps < 4 * 0.3 * 0.3 * (1 - std::numbers::pi / 4) * 1.05);
        CHECK(c.outside < 1e-6);
        CHECK(c.overlap < 1e-6);
    }
}

TEST_CASE("F2 criterion: a wedge from 0.3 to 1.2 mm", "[coverage]") {
    // Measured: gaps 0.073 mm^2 (0.3 %), overlap 0.28 mm^2 (1.3 %), at the
    // transitions where beads start or end with round caps.
    const Coverage c = walls_coverage({poly({{0, -0.15}, {30, -0.6}, {30, 0.6}, {0, 0.15}})}, BeadingParams{});
    CHECK_THAT(c.target, WithinAbs(22.5, 1e-9));
    CHECK(c.gaps < 0.01 * c.target);
    CHECK(c.overlap < 0.02 * c.target);
    CHECK(c.outside < 1e-6);
}

TEST_CASE("F2 criterion: a 20 mm square with 2 and 3 walls covers its wall band", "[coverage]") {
    for (int m : {2, 3}) {
        BeadingParams p;
        p.wall_count = m;
        const Coverage c = walls_coverage({poly({{0, 0}, {20, 0}, {20, 20}, {0, 20}})}, p);
        const double band = 400.0 - (20.0 - 0.8 * m) * (20.0 - 0.8 * m);
        CHECK_THAT(c.target, WithinAbs(band, 1e-6));
        // Each loop leaves a round corner of radius 0.2 at each of the 4 corners
        // (plus a little for the polygonal discs).
        CHECK(c.gaps < 4 * m * 0.2 * 0.2 * (1 - std::numbers::pi / 4) * 1.05);
        CHECK(c.outside < 1e-6);
        CHECK(c.overlap < 1e-6);
    }
}
