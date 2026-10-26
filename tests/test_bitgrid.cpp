#include "geom/bitgrid.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <algorithm>
#include <cmath>
#include <numbers>

using namespace tisma;
using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

namespace {

Polygons square(double x0, double y0, double x1, double y1) {
    return {{{to_units(x0), to_units(y0)}, {to_units(x1), to_units(y0)}, {to_units(x1), to_units(y1)}, {to_units(x0), to_units(y1)}}};
}

// A grid of 0.1 mm pixels from (-1, -1), 200 x 150 pixels.
BitGrid grid() { return BitGrid(to_units(-1.0), to_units(-1.0), to_units(0.1), 200, 150); }

} // namespace

TEST_CASE("bitgrid: filling sets the pixels whose centre is inside", "[bitgrid]") {
    BitGrid g = grid();
    g.fill(square(0.0, 0.0, 10.0, 5.0));
    CHECK(g.count() == 100 * 50); // pixel edges on the square's edges
    CHECK(g.get(10, 10));         // centre (0.05, 0.05)
    CHECK_FALSE(g.get(9, 10));    // centre (-0.05, 0.05)
    CHECK(g.get(109, 59));
    CHECK_FALSE(g.get(110, 59));
    // A hole cancels by the non-zero rule.
    BitGrid h = grid();
    Polygons ring = square(0.0, 0.0, 10.0, 10.0);
    Polygons hole = square(2.0, 2.0, 8.0, 8.0);
    std::reverse(hole[0].begin(), hole[0].end());
    ring.push_back(hole[0]);
    h.fill(ring);
    CHECK(h.count() == 100 * 100 - 60 * 60);
    // A disk of radius 5 mm: about its area in pixels.
    BitGrid d = grid();
    Polygon circle;
    for (int k = 0; k < 720; ++k)
        circle.push_back({to_units(7.0 + 5.0 * std::cos(k * std::numbers::pi / 360.0)),
                          to_units(6.0 + 5.0 * std::sin(k * std::numbers::pi / 360.0))});
    d.fill({circle});
    CHECK_THAT(double(d.count()) * 0.01, WithinRel(std::numbers::pi * 25.0, 0.01));
}

TEST_CASE("bitgrid: dilation and erosion by a disk", "[bitgrid]") {
    BitGrid g = grid();
    g.set(50, 50);
    g.dilate(2); // dx * dx + dy * dy <= 4
    CHECK(g.count() == 13);
    CHECK(g.get(52, 50));
    CHECK(g.get(51, 51));
    CHECK_FALSE(g.get(52, 51));
    g.erode(2);
    CHECK(g.count() == 1);
    CHECK(g.get(50, 50));
    // Across word boundaries, by more than a word.
    BitGrid w = grid();
    w.set(63, 10);
    w.set(64, 100);
    w.dilate(70);
    CHECK(w.get(0, 10));
    CHECK(w.get(133, 10));
    CHECK_FALSE(w.get(134, 10));
    CHECK(w.get(134, 100));
    CHECK_FALSE(w.get(135, 100));
    // Closing fills a gap narrower than two radii, not a wider one.
    BitGrid c = grid();
    c.fill(square(0.0, 0.0, 4.0, 4.0));
    c.fill(square(4.3, 0.0, 8.0, 4.0)); // 0.3 mm apart
    c.fill(square(8.8, 0.0, 12.0, 4.0)); // 0.8 mm apart
    c.dilate(2);
    c.erode(2);
    CHECK(c.get(51, 20));       // in the narrow gap, centre x 4.15
    CHECK_FALSE(c.get(93, 20)); // in the wide one, centre x 8.35
    // Erosion takes everything within the radius of the grid's edge.
    BitGrid e = grid();
    e.fill(square(-1.0, -1.0, 19.0, 14.0));
    e.erode(3);
    CHECK_FALSE(e.get(2, 50));
    CHECK(e.get(3, 50));
}

TEST_CASE("bitgrid: outlines along the pixel edges", "[bitgrid]") {
    BitGrid g = grid();
    g.fill(square(0.0, 0.0, 10.0, 5.0));
    Polygons o = g.outline();
    REQUIRE(o.size() == 1);
    CHECK(o[0].size() == 4);
    CHECK_THAT(area_mm2(o), WithinAbs(50.0, 1e-9)); // counter-clockwise
    // A ring: its hole runs clockwise.
    BitGrid r = grid();
    Polygons ring = square(0.0, 0.0, 10.0, 10.0);
    Polygons hole = square(2.0, 2.0, 8.0, 8.0);
    std::reverse(hole[0].begin(), hole[0].end());
    ring.push_back(hole[0]);
    r.fill(ring);
    o = r.outline();
    REQUIRE(o.size() == 2);
    CHECK_THAT(area_mm2(o), WithinAbs(100.0 - 36.0, 1e-9));
    // Pixels touching at a corner are separate pieces.
    BitGrid d = grid();
    d.set(10, 10);
    d.set(11, 11);
    o = d.outline();
    REQUIRE(o.size() == 2);
    CHECK(o[0].size() == 4);
    CHECK(o[1].size() == 4);
    // Any shape: the outline holds exactly the set pixels.
    BitGrid s = grid();
    Polygon blob;
    for (int k = 0; k < 360; ++k) {
        const double a = k * std::numbers::pi / 180.0, rr = 4.0 + 1.5 * std::sin(5.0 * a);
        blob.push_back({to_units(8.0 + rr * std::cos(a)), to_units(6.0 + rr * std::sin(a))});
    }
    s.fill({blob});
    CHECK_THAT(area_mm2(s.outline()), WithinAbs(double(s.count()) * 0.01, 1e-9));
    BitGrid back = grid();
    back.fill(s.outline());
    CHECK(back == s);
}
