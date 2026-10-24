#include "adhesion/adhesion.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>

using namespace tisma;
using Catch::Matchers::WithinAbs;

namespace {

Polygon square(double x0, double y0, double x1, double y1) {
    return {{to_units(x0), to_units(y0)}, {to_units(x1), to_units(y0)}, {to_units(x1), to_units(y1)}, {to_units(x0), to_units(y1)}};
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

} // namespace

TEST_CASE("skirt: loops around the part at the given distance", "[adhesion]") {
    const Polygons cube{square(0, 0, 10, 10)};
    const std::vector<Toolpath> one = skirt_loops(cube, 0.0, 1, 3.0, 0.4);
    REQUIRE(one.size() == 1);
    CHECK(one[0].closed);
    // Centre line 3.2 mm out: a square of side 16.4 (mitered corners).
    CHECK_THAT(loop_length(one[0]), WithinAbs(4 * 16.4, 1e-6));
    CHECK(skirt_loops(cube, 0.0, 0, 3.0, 0.4).empty());
    // Negative indices: a skirt's seam must not count as a wall's (aligned seams remember them by index).
    for (const Toolpath& t : skirt_loops(cube, 2.0, 3, 3.0, 0.4))
        CHECK(t.index < 0);
    for (const Toolpath& t : brim_loops(cube, 2.0, 0.4))
        CHECK(t.index < 0);
    const std::vector<Toolpath> three = skirt_loops(cube, 0.0, 3, 3.0, 0.4);
    REQUIRE(three.size() == 3);
    CHECK_THAT(loop_length(three[2]), WithinAbs(4 * (10 + 2 * 4.0), 1e-6)); // 3 + 2.5 beads
    // Around a brim of 2 mm (5 beads), 3 mm further out.
    const std::vector<Toolpath> around_brim = skirt_loops(cube, 2.0, 1, 3.0, 0.4);
    REQUIRE(around_brim.size() == 1);
    CHECK_THAT(loop_length(around_brim[0]), WithinAbs(4 * (10 + 2 * 5.2), 1e-6));
    // A hole in the part gets no skirt inside it.
    const Polygons ring{square(0, 0, 30, 30), {{to_units(10), to_units(10)}, {to_units(10), to_units(20)},
                                                {to_units(20), to_units(20)}, {to_units(20), to_units(10)}}};
    CHECK(skirt_loops(ring, 0.0, 1, 3.0, 0.4).size() == 1);
}

TEST_CASE("brim: one loop per bead of its width, merged between close parts", "[adhesion]") {
    const Polygons cube{square(0, 0, 10, 10)};
    const std::vector<Toolpath> brim = brim_loops(cube, 2.0, 0.4);
    REQUIRE(brim.size() == 5);
    CHECK_THAT(loop_length(brim[0]), WithinAbs(4 * 10.4, 1e-6)); // touching the part
    CHECK(brim_loops(cube, 0.0, 0.4).empty());
    // Two parts 1 mm apart: the first loops stay separate, the outer ones go round both.
    const Polygons two{square(0, 0, 10, 10), square(11, 0, 21, 10)};
    const std::vector<Toolpath> both = brim_loops(two, 2.0, 0.4);
    int first = 0, last = 0;
    for (const Toolpath& t : both) {
        first += t.index == -1;
        last += t.index == -5;
    }
    CHECK(first == 2);
    CHECK(last == 1);
}
