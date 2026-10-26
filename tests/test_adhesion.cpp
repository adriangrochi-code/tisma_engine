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

TEST_CASE("skirt: rounded loops at the beads' spacing (supports_skirt.md, rule 8, S7, S8)", "[adhesion]") {
    const double pi = 3.14159265358979;
    const double spacing = loop_spacing(0.4, 0.2); // 0.357 mm: beads 0.4 wide and 0.2 high overlap
    CHECK_THAT(spacing, WithinAbs(0.4 - 0.2 * (1.0 - pi / 4.0), 1e-12));
    const Polygons cube{square(0, 0, 20, 20)};
    // S7: one loop 6 mm away, its axis 6.18 mm from the part, the corners round: 118.5 mm in Tisma Slicer.
    const std::vector<Toolpath> one = skirt_loops(cube, 0.0, 1, 6.0, 0.4, 0.2);
    REQUIRE(one.size() == 1);
    CHECK(one[0].closed);
    const double r = 6.0 + spacing / 2.0;
    CHECK_THAT(r, WithinAbs(6.18, 0.005));
    CHECK_THAT(loop_length(one[0]), WithinAbs(80.0 + 2.0 * pi * r, 0.05));
    CHECK_THAT(loop_length(one[0]), WithinAbs(118.5, 0.5));
    CHECK(skirt_loops(cube, 0.0, 0, 6.0, 0.4, 0.2).empty());
    // S8: three loops 2 mm away, the outer axis at 2.89 mm; 287.0 mm a layer in Tisma Slicer.
    const std::vector<Toolpath> three = skirt_loops(cube, 0.0, 3, 2.0, 0.4, 0.2);
    REQUIRE(three.size() == 3);
    double total = 0.0;
    for (const Toolpath& t : three)
        total += loop_length(t);
    CHECK_THAT(loop_length(three[2]), WithinAbs(80.0 + 2.0 * pi * (2.0 + 2.5 * spacing), 0.05));
    CHECK_THAT(2.0 + 2.5 * spacing, WithinAbs(2.89, 0.01));
    CHECK_THAT(total, WithinAbs(287.0, 1.0));
    // Negative indices: a skirt's seam must not count as a wall's (aligned seams remember them by index).
    for (const Toolpath& t : skirt_loops(cube, 2.0, 3, 3.0, 0.4, 0.2))
        CHECK(t.index < 0);
    for (const Toolpath& t : brim_loops(cube, 2.0, 0.4, 0.2))
        CHECK(t.index < 0);
    // Around a brim of 2 mm (5 loops), 3 mm further out.
    const std::vector<Toolpath> around_brim = skirt_loops(cube, 2.0, 1, 3.0, 0.4, 0.2);
    REQUIRE(around_brim.size() == 1);
    CHECK_THAT(loop_length(around_brim[0]), WithinAbs(80.0 + 2.0 * pi * (5 * spacing + 3.0 + spacing / 2.0), 0.05));
    // A hole in the part gets no skirt inside it.
    const Polygons ring{square(0, 0, 30, 30), {{to_units(10), to_units(10)}, {to_units(10), to_units(20)},
                                                {to_units(20), to_units(20)}, {to_units(20), to_units(10)}}};
    CHECK(skirt_loops(ring, 0.0, 1, 3.0, 0.4, 0.2).size() == 1);
}

TEST_CASE("brim: loops at the beads' spacing up to its width, merged between close parts (rule 9, S9)", "[adhesion]") {
    const double pi = 3.14159265358979;
    const double spacing = loop_spacing(0.4, 0.2);
    // S9: a 5 mm brim on a 20 mm cube: 14 loops, the outer axis at 4.82 mm (its edge at 5.02); 1340.4 mm.
    const Polygons cube20{square(0, 0, 20, 20)};
    const std::vector<Toolpath> brim = brim_loops(cube20, 5.0, 0.4, 0.2);
    REQUIRE(brim.size() == 14);
    CHECK_THAT(loop_length(brim[0]), WithinAbs(80.0 + 2.0 * pi * spacing / 2.0, 0.05)); // touching the part
    CHECK_THAT(13.5 * spacing, WithinAbs(4.82, 0.005));
    double total = 0.0;
    for (const Toolpath& t : brim)
        total += loop_length(t);
    CHECK_THAT(total, WithinAbs(1340.4, 1.0));
    CHECK(brim_loops(cube20, 0.0, 0.4, 0.2).empty());
    // Two parts 1 mm apart: the first loops stay separate, the outer ones go round both.
    const Polygons two{square(0, 0, 10, 10), square(11, 0, 21, 10)};
    const std::vector<Toolpath> both = brim_loops(two, 2.0, 0.4, 0.2);
    int first = 0, last = 0;
    for (const Toolpath& t : both) {
        first += t.index == -1;
        last += t.index == -5;
    }
    CHECK(first == 2);
    CHECK(last == 1);
}
