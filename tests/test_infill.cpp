#include "config/settings.hpp"
#include "infill/gyroid.hpp"
#include "infill/rectilinear.hpp"
#include "perimeters/coverage.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <algorithm>
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
    s.bottom = square(0, 0, 10);
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
            s.bottom = *region; // zig-zags; top surfaces are monotonic
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
    s.bottom = square_region;
    const Coverage inside = measure_coverage(square(1.6, 1.6, 6.8), fill_layer(s, 0, FillParams{}).solid);
    CHECK(inside.gaps < 1e-3 * inside.target);
}

TEST_CASE("grid infill: both directions in every layer, the same material as rectilinear", "[infill]") {
    LayerSurfaces s;
    s.sparse = square(0, 0, 20);
    FillParams p; // 20 %
    p.pattern = InfillPattern::Grid;
    const LayerFill even = fill_layer(s, 0, p), odd = fill_layer(s, 1, p);
    auto directions = [](const LayerFill& f) {
        int at45 = 0, at135 = 0;
        for (const Toolpath& t : f.sparse) {
            const double d = std::atan2(t.points.back().p.y - t.points.front().p.y, t.points.back().p.x - t.points.front().p.x) *
                             180.0 / 3.14159265358979323846;
            const double a = d < 0.0 ? d + 180.0 : d;
            at45 += std::fabs(a - 45.0) < 1e-3;
            at135 += std::fabs(a - 135.0) < 1e-3;
        }
        return std::pair<int, int>{at45, at135};
    };
    const auto [e45, e135] = directions(even);
    const auto [o45, o135] = directions(odd);
    CHECK(e45 > 0);
    CHECK(e45 == e135); // a square: as many lines each way
    CHECK(o45 == e45);  // every layer the same
    CHECK(o135 == e135);
    CHECK(int(even.sparse.size()) == e45 + e135);
    // Material laid down: about the density times the area, as with rectilinear lines.
    const Coverage grid = measure_coverage(s.sparse, even.sparse);
    p.pattern = InfillPattern::Rectilinear;
    const Coverage lines = measure_coverage(s.sparse, fill_layer(s, 0, p).sparse);
    std::printf("20 mm square at 20%%: grid lays %.2f mm2, rectilinear %.2f mm2 (area %.0f)\n", grid.deposited,
                lines.deposited, grid.target);
    CHECK_THAT(grid.deposited / grid.target, WithinAbs(0.2, 0.02));
    CHECK_THAT(lines.deposited / lines.target, WithinAbs(0.2, 0.03));
}

TEST_CASE("gyroid infill: points on the surface, the density's material, curves that move with z", "[infill]") {
    const Polygons region = square(0, 0, 20);
    const double period = gyroid_period(0.4, 0.2);
    CHECK_THAT(period, WithinAbs(5.22, 1e-9));
    const std::vector<Toolpath> at0 = gyroid_fill(region, period, 0.3, 0.4);
    REQUIRE_FALSE(at0.empty());
    const double k = 2.0 * 3.14159265358979323846 / period;
    double worst = 0.0;
    for (const Toolpath& t : at0)
        for (const ToolpathPoint& q : t.points) {
            CHECK(q.width == 0.4);
            CHECK(q.p.x >= 0.2 - 1e-4);
            CHECK(q.p.x <= 19.8 + 1e-4);
            const bool on_boundary = std::fabs(q.p.x - 0.2) < 1e-3 || std::fabs(q.p.x - 19.8) < 1e-3 ||
                                     std::fabs(q.p.y - 0.2) < 1e-3 || std::fabs(q.p.y - 19.8) < 1e-3;
            if (on_boundary)
                continue; // the joins between curve ends follow the shrunk boundary
            const double X = k * q.p.x, Y = k * q.p.y, Z = k * 0.3;
            worst = std::max(worst, std::fabs(std::sin(X) * std::cos(Y) + std::sin(Y) * std::cos(Z) + std::sin(Z) * std::cos(X)));
        }
    // Linear interpolation on a grid of period / 32: the points are close to the surface.
    CHECK(worst < 0.01);
    // The material laid down is about the density times the area (averaged over a period of z);
    // the joins along the edge add a little (measured: 21.3 %).
    double laid = 0.0;
    const int heights = 8;
    for (int i = 0; i < heights; ++i)
        laid += measure_coverage(region, gyroid_fill(region, period, period * i / heights, 0.4)).deposited;
    laid /= heights;
    std::printf("gyroid at 20%% on a 20 mm square: lays %.2f mm2 of 400 (%.1f%%)\n", laid, laid / 4.0);
    CHECK_THAT(laid / 400.0, WithinAbs(0.21, 0.02));
    CHECK(at0.size() < 40); // the curve ends are joined into a few long paths
    // Another height, other curves.
    const std::vector<Toolpath> at1 = gyroid_fill(region, period, 1.3, 0.4);
    REQUIRE_FALSE(at1.empty());
    CHECK((at1.size() != at0.size() || std::fabs(at1[0].points[0].p.x - at0[0].points[0].p.x) > 1e-3 ||
           std::fabs(at1[0].points[0].p.y - at0[0].points[0].p.y) > 1e-3));
    // Through fill_layer, with the layer's z.
    LayerSurfaces s;
    s.sparse = region;
    FillParams p;
    p.pattern = InfillPattern::Gyroid;
    CHECK(fill_layer(s, 0, p, 0.3).sparse.size() == at0.size());
}

TEST_CASE("monotonic top surfaces: one direction, nothing before the touching line below", "[infill]") {
    // A U: a 10 x 3 mm bar with two 3 mm arms up to y = 10. At 0 degrees the
    // lines are horizontal; above y = 3 each line has a piece in each arm.
    const Polygons u = {{{to_units(0), to_units(0)}, {to_units(10), to_units(0)}, {to_units(10), to_units(10)},
                         {to_units(7), to_units(10)}, {to_units(7), to_units(3)}, {to_units(3), to_units(3)},
                         {to_units(3), to_units(10)}, {to_units(0), to_units(10)}}};
    for (const Polygons* region : {&u, static_cast<const Polygons*>(nullptr)}) {
        const Polygons sq = square(0, 0, 10);
        const Polygons& r = region ? *region : sq;
        LayerSurfaces s;
        s.top = r;
        FillParams p;
        p.angle = 0.0;
        const std::vector<Toolpath> lines = fill_layer(s, 0, p).top;
        REQUIRE_FALSE(lines.empty());
        for (std::size_t i = 0; i < lines.size(); ++i) {
            const Toolpath& t = lines[i];
            REQUIRE(t.points.size() == 2);
            CHECK(t.points.back().p.x > t.points.front().p.x); // every line left to right
            // Every piece of the line below that touches this one came earlier.
            for (std::size_t j = 0; j < lines.size(); ++j) {
                const Toolpath& o = lines[j];
                const bool below = std::fabs(t.points[0].p.y - o.points[0].p.y - 0.4) < 1e-3;
                const bool touches = o.points.front().p.x - 0.2 <= t.points.back().p.x + 0.2 &&
                                     t.points.front().p.x - 0.2 <= o.points.back().p.x + 0.2;
                if (below && touches)
                    CHECK(j < i);
            }
        }
        if (region) {
            // The arms are each laid in one go: after the bar, one arm, then the other.
            std::size_t switches = 0;
            for (std::size_t i = 1; i < lines.size(); ++i)
                if (lines[i].points[0].p.y > 2.9 && lines[i - 1].points[0].p.y > 2.9 &&
                    (lines[i].points[0].p.x < 5.0) != (lines[i - 1].points[0].p.x < 5.0))
                    ++switches;
            CHECK(switches == 1);
        }
    }
    CHECK(parse_settings(R"({"surfaces": {"top_pattern": "rectilinear"}})").monotonic_top == false);
    CHECK(parse_settings(R"({"surfaces": {"top_pattern": "monotonic"}})").monotonic_top == true);
}

TEST_CASE("gap fill: variable-width beads where the solid lines leave room", "[infill]") {
    // A monotonic top on the 7.6 mm square at 30 degrees leaves its line ends
    // unjoined (measured: 4.8 % short); gap filling adds beads where the gaps are
    // at least half a bead thick (measured: 3.4 % short).
    LayerSurfaces s;
    s.top = square(1.2, 1.2, 7.6);
    FillParams p;
    p.angle = 30.0;
    p.wall_overlap = 0.0; // lines inside the region, as gap filling expects
    const LayerFill plain = fill_layer(s, 0, p);
    CHECK(plain.gaps.empty()); // off by default
    p.gap_fill = true;
    const LayerFill filled = fill_layer(s, 0, p);
    REQUIRE_FALSE(filled.gaps.empty());
    std::vector<Toolpath> all = filled.top;
    all.insert(all.end(), filled.gaps.begin(), filled.gaps.end());
    const Coverage before = measure_coverage(s.top, plain.top);
    const Coverage after = measure_coverage(s.top, all);
    CHECK(after.gaps < before.gaps - 0.005 * before.target);
    CHECK(after.outside < 1e-3);
    CHECK(after.overlap < 0.01 * after.target);
    for (const Toolpath& t : filled.gaps)
        for (const ToolpathPoint& q : t.points)
            CHECK(q.width > 0.0);
}

TEST_CASE("monotonic tops reach a quarter bead into the walls: the area is laid", "[infill]") {
    // Unjoined lines end short of the edge; reaching 0.1 mm into the walls
    // balances it. Measured: laid minus area -0.5 to +1.5 %, 1.6 to 3.4 % of the
    // area inside the walls (25 % of a bead, the default).
    const Polygons sq = square(1.2, 1.2, 7.6);
    const Polygons l = {{{to_units(0), to_units(0)}, {to_units(10), to_units(0)}, {to_units(10), to_units(3)},
                         {to_units(3), to_units(3)}, {to_units(3), to_units(10)}, {to_units(0), to_units(10)}}};
    for (const Polygons* region : {&sq, &l})
        for (double angle : {0.0, 30.0, 45.0}) {
            LayerSurfaces s;
            s.top = *region;
            FillParams p;
            p.angle = angle;
            const Coverage c = measure_coverage(*region, fill_layer(s, 0, p).top);
            INFO("angle " << angle);
            CHECK(std::fabs(c.deposited - c.target) < 0.02 * c.target);
            CHECK(c.outside < 0.04 * c.target);
            p.wall_overlap = 0.0;
            const Coverage inside = measure_coverage(*region, fill_layer(s, 0, p).top);
            CHECK(inside.outside < 1e-3);
            CHECK(inside.deposited < c.deposited);
        }
    CHECK(parse_settings(R"({"infill": {"wall_overlap": 10}})").infill_wall_overlap == 10.0);
}

TEST_CASE("fill_period: the same surfaces fill alike that many layers apart, at any height", "[infill]") {
    LayerSurfaces s;
    s.top = square(0, 0, 10);
    s.bottom = square(20, 0, 10);
    s.sparse = square(0, 20, 30);
    s.bridge = square(40, 0, 10);
    s.bridge_angle = 30.0;
    const auto alike = [](const std::vector<Toolpath>& a, const std::vector<Toolpath>& b) {
        if (a.size() != b.size())
            return false;
        for (std::size_t i = 0; i < a.size(); ++i) {
            if (a[i].points.size() != b[i].points.size())
                return false;
            for (std::size_t k = 0; k < a[i].points.size(); ++k)
                if (a[i].points[k].p.x != b[i].points[k].p.x || a[i].points[k].p.y != b[i].points[k].p.y)
                    return false;
        }
        return true;
    };
    for (int k = 0; k <= int(InfillPattern::Octagram); ++k) {
        FillParams p;
        p.pattern = InfillPattern(k);
        p.density = 0.2;
        INFO("pattern " << k);
        const std::size_t period = fill_period(p);
        const bool moves = p.pattern == InfillPattern::Gyroid || p.pattern == InfillPattern::Cubic ||
                           p.pattern == InfillPattern::AdaptiveCubic || p.pattern == InfillPattern::SupportCubic;
        CHECK((period == 0) == moves);
        if (period == 0)
            continue;
        for (std::size_t l = 0; l < 3; ++l) {
            const LayerFill a = fill_layer(s, l, p, 0.2 + 0.2 * double(l));
            const LayerFill b = fill_layer(s, l + period, p, 7.0 + 0.2 * double(l));
            CHECK(alike(a.top, b.top));
            CHECK(alike(a.solid, b.solid));
            CHECK(alike(a.sparse, b.sparse));
            CHECK(alike(a.bridge, b.bridge));
            CHECK(alike(a.gaps, b.gaps));
        }
    }
    FillParams none;
    none.density = 0.0;
    none.pattern = InfillPattern::Gyroid;
    CHECK(fill_period(none) == 2); // only the solid lines turn
}
