#include "infill/arcs.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <set>

using namespace tisma;
using Catch::Matchers::WithinAbs;

namespace {

Polygon rect(double x0, double y0, double x1, double y1) {
    return {{to_units(x0), to_units(y0)}, {to_units(x1), to_units(y0)}, {to_units(x1), to_units(y1)}, {to_units(x0), to_units(y1)}};
}

// The column of arc_overhangs.md (A1 to A5): X 90 to 100, Y 90 to 110.
const Polygons kColumn = {rect(90, 90, 100, 110)};

double length(const std::vector<Toolpath>& paths) {
    double sum = 0.0;
    for (const Toolpath& t : paths)
        for (std::size_t i = 1; i < t.points.size(); ++i)
            sum += std::hypot(t.points[i].p.x - t.points[i - 1].p.x, t.points[i].p.y - t.points[i - 1].p.y);
    return sum;
}

double radius(const Toolpath& t, const Point2d& c) { return std::hypot(t.points.front().p.x - c.x, t.points.front().p.y - c.y); }

} // namespace

TEST_CASE("arcs: A1, one group from 1.2 mm inside the column, round to the far corners", "[arcs]") {
    const std::vector<ArcZone> zones = overhang_arcs({rect(90, 90, 110, 110)}, kColumn, ArcParams{});
    REQUIRE(zones.size() == 1);
    const ArcZone& z = zones.front();
    // Rule 7: 1.2 mm inside, facing the middle of the edge at X 100.
    CHECK_THAT(z.centre.x, WithinAbs(98.8, 1e-6));
    CHECK_THAT(z.centre.y, WithinAbs(100.0, 1e-6));
    // Each arc on its circle, the radii 0.36 apart (rule 2), from the
    // smallest out; the whole ones at 0.686, 1.407...
    std::set<long> radii;
    double last = 0.0;
    for (const Toolpath& t : z.arcs) {
        const double r = radius(t, z.centre);
        for (const ToolpathPoint& p : t.points)
            CHECK_THAT(std::hypot(p.p.x - z.centre.x, p.p.y - z.centre.y), WithinAbs(r, 1e-6));
        CHECK(r >= last - 1e-9);
        last = r;
        radii.insert(std::lround(r * 1000.0));
        // Within the outline, half a bead in, and in the zone or the bite of
        // the column.
        for (const ToolpathPoint& p : t.points) {
            CHECK(p.p.x >= 98.8 - 1e-6);
            CHECK(p.p.x <= 109.8 + 1e-3);
            CHECK(p.p.y >= 90.2 - 1e-3);
            CHECK(p.p.y <= 109.8 + 1e-3);
        }
    }
    // (to 5 microns: the spec's are measured)
    const auto has = [&](long r) { return radii.lower_bound(r - 5) != radii.upper_bound(r + 5); };
    CHECK(has(686));
    CHECK(has(1407));
    CHECK(has(2128));
    CHECK(has(2849));
    // Rule 8: as far as the farthest corner, (109.8, 109.8).
    CHECK(last > std::hypot(109.8 - 98.8, 109.8 - 100.0) - 0.4);
    // Rule 10: every other arc in two pieces, split on Y 100 where one ends
    // and the next starts; one way round, then back.
    std::size_t split = 0, whole = 0;
    for (std::size_t i = 0; i + 1 < z.arcs.size(); ++i) {
        const double r = radius(z.arcs[i], z.centre);
        // (farther out the slab's sides cut them too)
        if (r < 9.0 && std::fabs(radius(z.arcs[i + 1], z.centre) - r) < 1e-6) {
            ++split;
            CHECK_THAT(z.arcs[i].points.back().p.y, WithinAbs(100.0, 1e-6));
            CHECK_THAT(z.arcs[i + 1].points.front().p.y, WithinAbs(100.0, 1e-6));
        }
    }
    // Each arc starts on the side where the one before ended.
    for (std::size_t i = 1; i < z.arcs.size(); ++i)
        if (std::fabs(radius(z.arcs[i], z.centre) - radius(z.arcs[i - 1], z.centre)) > 1e-6 && radius(z.arcs[i], z.centre) < 9.0)
            CHECK((z.arcs[i].points.front().p.y - 100.0) * (z.arcs[i - 1].points.back().p.y - 100.0) > 0.0);
    for (const Toolpath& t : z.arcs)
        whole += radius(t, z.centre) < 9.0 && std::fabs(t.points.front().p.y - 100.0) > 1e-3 &&
                 std::fabs(t.points.back().p.y - 100.0) > 1e-3;
    // The whole ones are those of A1's radii.
    for (const Toolpath& t : z.arcs)
        if (std::fabs(radius(t, z.centre) - 0.686) < 0.01 || std::fabs(radius(t, z.centre) - 1.407) < 0.01)
            CHECK(std::fabs(t.points.front().p.y - 100.0) > 1e-3);
    CHECK(split >= 10);
    CHECK(whole >= 10);
    // About as much as the spec's 606 mm over the 200 mm2 and the bite.
    CHECK(length(z.arcs) > 520.0);
    CHECK(length(z.arcs) < 700.0);
}

TEST_CASE("arcs: A2 to A4, an L, a hole and a strip keep one group; a bridge has none", "[arcs]") {
    // A2: the slab and 10 x 10 more in +Y: one group, its centre near the
    // middle of the L-shaped edge (98.8, 103.8 in the spec).
    const Polygons l_slab = Clipper2Lib::Union(Polygons{rect(90, 90, 110, 110)}, Polygons{rect(90, 110, 100, 120)},
                                               Clipper2Lib::FillRule::NonZero);
    const std::vector<ArcZone> l_zones = overhang_arcs(l_slab, kColumn, ArcParams{});
    REQUIRE(l_zones.size() == 1);
    CHECK(std::hypot(l_zones.front().centre.x - 98.8, l_zones.front().centre.y - 103.8) < 1.5);
    // A3: a 4 x 4 hole cuts the arcs into pieces, less of them.
    Polygons holed = {rect(90, 90, 110, 110)};
    Polygon hole = rect(103, 98, 107, 102);
    std::reverse(hole.begin(), hole.end());
    holed.push_back(hole);
    const std::vector<ArcZone> a1 = overhang_arcs({rect(90, 90, 110, 110)}, kColumn, ArcParams{});
    const std::vector<ArcZone> a3 = overhang_arcs(holed, kColumn, ArcParams{});
    REQUIRE(a3.size() == 1);
    CHECK_THAT(a3.front().centre.x, WithinAbs(98.8, 1e-6));
    CHECK(length(a3.front().arcs) < length(a1.front().arcs) - 30.0);
    CHECK(length(a3.front().arcs) > length(a1.front().arcs) - 80.0);
    for (const Toolpath& t : a3.front().arcs)
        for (const ToolpathPoint& p : t.points)
            CHECK_FALSE((p.p.x > 103.2 && p.p.x < 106.8 && p.p.y > 98.2 && p.p.y < 101.8));
    // A4: a strip 15 x 3 in +X: across it, nearly straight.
    const Polygons strip = Clipper2Lib::Union(kColumn, Polygons{rect(100, 98.5, 115, 101.5)}, Clipper2Lib::FillRule::NonZero);
    const std::vector<ArcZone> a4 = overhang_arcs(strip, kColumn, ArcParams{});
    REQUIRE(a4.size() == 1);
    CHECK(length(a4.front().arcs) > 80.0);
    CHECK(length(a4.front().arcs) < 160.0);
    // A table on two legs bridges: held on two sides, no arcs.
    const Polygons legs = {rect(90, 90, 95, 110), rect(105, 90, 110, 110)};
    CHECK(overhang_arcs({rect(90, 90, 110, 110)}, legs, ArcParams{}).empty());
    // A ring round a column, held all round: no arcs either.
    CHECK(overhang_arcs({rect(80, 80, 120, 120)}, {rect(95, 95, 105, 105)}, ArcParams{}).empty());
    // arcs.max_distance: no arc reaches past it.
    ArcParams near;
    near.max_distance = 5.0;
    const std::vector<ArcZone> short_arcs = overhang_arcs({rect(90, 90, 110, 110)}, kColumn, near);
    REQUIRE(short_arcs.size() == 1);
    for (const Toolpath& t : short_arcs.front().arcs)
        CHECK(radius(t, short_arcs.front().centre) <= 5.0 + 1e-6);
}
