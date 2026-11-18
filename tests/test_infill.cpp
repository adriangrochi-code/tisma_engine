#include "config/settings.hpp"
#include "gcode/order.hpp"
#include "infill/gyroid.hpp"
#include "infill/patterns.hpp"
#include "infill/rectilinear.hpp"
#include "infill/smooth.hpp"
#include "perimeters/coverage.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <algorithm>
#include <cmath>
#include <set>
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

TEST_CASE("infill.max_link: lines joined over a longer stretch of the boundary (surfaces_infill.md, rule 7)",
          "[infill]") {
    // A triangle whose top side is 10 degrees off the lines: the ends of two
    // neighbouring lines on it lie spacing / sin(10 deg), almost 6 spacings,
    // apart along it, past the 3 spacings the engine joined over before.
    const Polygons wedge = {{{to_units(0), to_units(0)}, {to_units(40), to_units(0)}, {to_units(40), to_units(7)}}};
    const std::size_t short_links = rectilinear_fill(wedge, 1.0, 0.0, 0.4, true).size();
    const std::size_t long_links = rectilinear_fill(wedge, 1.0, 0.0, 0.4, true, false, false, 0.0, 0.0, 50.0).size();
    CHECK(short_links >= 2);
    CHECK(long_links < short_links);
    CHECK(rectilinear_fill(wedge, 1.0, 0.0, 0.4, true, false, false, 0.0, 0.0, 1.0).size() >= short_links);
}

TEST_CASE("surfaces.bottom_pattern: monotonic bottom lines, unjoined; the solid under a top zig-zags", "[infill]") {
    LayerSurfaces s;
    s.bottom = square(0, 0, 10);
    FillParams p;
    p.bottom_pattern = SurfacePattern::Monotonic;
    const LayerFill mono = fill_layer(s, 0, p, 0.2);
    REQUIRE_FALSE(mono.solid.empty());
    for (const Toolpath& t : mono.solid)
        CHECK(t.points.size() == 2);
    p.bottom_pattern = SurfacePattern::Rectilinear;
    const LayerFill zig = fill_layer(s, 0, p, 0.2);
    CHECK(zig.solid.size() < mono.solid.size());
}

TEST_CASE("surfaces.top_angle and bottom_angle: a fixed angle for the surfaces' lines (orca_2_5.md, rule 9)", "[infill]") {
    // The direction of a path's longest stretch, in degrees from 0 to 180.
    const auto directions = [](const std::vector<Toolpath>& paths) {
        std::set<int> out;
        for (const Toolpath& t : paths) {
            double best = 0.0, dir = 0.0;
            for (std::size_t i = 1; i < t.points.size(); ++i) {
                const double dx = t.points[i].p.x - t.points[i - 1].p.x, dy = t.points[i].p.y - t.points[i - 1].p.y;
                if (std::hypot(dx, dy) > best) {
                    best = std::hypot(dx, dy);
                    dir = std::fmod(std::atan2(dy, dx) * 180.0 / std::acos(-1.0) + 360.0, 180.0);
                }
            }
            out.insert(int(std::lround(dir)) % 180);
        }
        return out;
    };
    LayerSurfaces top;
    top.top = square(0, 0, 10);
    LayerSurfaces bottom;
    bottom.bottom = square(0, 0, 10);
    FillParams p; // 45 degrees, turning 90 a layer
    CHECK(directions(fill_layer(top, 0, p, 0.2).top) != directions(fill_layer(top, 1, p, 0.4).top));
    p.top_angle = 30.0;
    p.bottom_angle = 120.0;
    for (std::size_t layer : {0u, 1u}) {
        CHECK(directions(fill_layer(top, layer, p, 0.2).top) == std::set<int>{30});
        CHECK(directions(fill_layer(bottom, layer, p, 0.2).solid) == std::set<int>{120});
        p.bottom_pattern = SurfacePattern::Rectilinear;
        CHECK(directions(fill_layer(bottom, layer, p, 0.2).solid) == std::set<int>{120});
        p.bottom_pattern = SurfacePattern::Monotonic;
    }
    // The solid under a top surface keeps the layer's angle.
    LayerSurfaces under;
    under.top = square(0, 0, 10);
    under.top_under = square(0, 0, 10);
    CHECK(directions(fill_layer(under, 0, p, 0.2).solid) == std::set<int>{45});
    // The settings read and are checked.
    const Settings read = parse_settings(R"({"surfaces": {"top_angle": 30, "bottom_angle": -1}})");
    CHECK(read.top_angle == 30.0);
    CHECK(read.bottom_angle == -1.0);
    CHECK_THROWS(parse_settings(R"({"surfaces": {"top_angle": -5}})"));
}

TEST_CASE("surfaces.top_fill_order and bottom_fill_order: centre out or edge in (orca_2_5.md, rule 8)", "[infill]") {
    // How far each path's first point lies from the centre, in the order given.
    const auto distances = [](const std::vector<Toolpath>& paths, Point2d c) {
        std::vector<double> out;
        for (const Toolpath& t : paths)
            if (!t.points.empty()) {
                double d = 0.0;
                for (const ToolpathPoint& q : t.points) // a loop: its mean distance
                    d += std::hypot(q.p.x - c.x, q.p.y - c.y);
                const double front = std::hypot(t.points.front().p.x - c.x, t.points.front().p.y - c.y);
                const double back = std::hypot(t.points.back().p.x - c.x, t.points.back().p.y - c.y);
                out.push_back(t.closed ? d / double(t.points.size()) : std::min(front, back));
            }
        return out;
    };
    LayerSurfaces s;
    s.top = square(0, 0, 20);
    FillParams p;
    p.centre = {10.0, 10.0};
    for (const SurfacePattern pattern : {SurfacePattern::Concentric, SurfacePattern::Archimedean, SurfacePattern::Octagram}) {
        INFO(int(pattern));
        p.top_pattern = pattern;
        p.top_order = FillOrder::Outward;
        const LayerFill out = fill_layer(s, 0, p, 0.2);
        REQUIRE(out.top.size() >= 2);
        const std::vector<double> d = distances(out.top, p.centre);
        CHECK(std::is_sorted(d.begin(), d.end()));
        for (const Toolpath& t : out.top) {
            CHECK(t.in_order);
            CHECK(t.one_way == !t.closed);
        }
        p.top_order = FillOrder::Inward;
        const LayerFill in = fill_layer(s, 0, p, 0.2);
        const std::vector<double> e = distances(in.top, p.centre);
        CHECK(std::is_sorted(e.rbegin(), e.rend()));
        // Each spiral piece runs towards the centre.
        for (const Toolpath& t : in.top)
            if (!t.closed)
                CHECK(std::hypot(t.points.back().p.x - 10.0, t.points.back().p.y - 10.0) <=
                      std::hypot(t.points.front().p.x - 10.0, t.points.front().p.y - 10.0));
        p.top_order = FillOrder::Default;
        for (const Toolpath& t : fill_layer(s, 0, p, 0.2).top)
            CHECK_FALSE(t.in_order);
    }
    // Ordered paths keep their order when the G-code orders a group.
    std::vector<Toolpath> group;
    for (int i = 0; i < 3; ++i) {
        Toolpath t;
        t.points = {{{double(10 * (2 - i)), 0.0}, 0.4}, {{double(10 * (2 - i)) + 1.0, 0.0}, 0.4}};
        t.in_order = t.one_way = true;
        group.push_back(t);
    }
    Point2d cursor{0.0, 0.0};
    SeamMemory memory;
    const std::vector<Toolpath> ordered = order_paths(group, cursor, SeamMode::Nearest, 1, memory);
    REQUIRE(ordered.size() == 3);
    CHECK(ordered[0].points.front().p.x == 20.0);
    CHECK(ordered[2].points.front().p.x == 0.0);
    const Settings read = parse_settings(R"({"surfaces": {"top_fill_order": "outward", "bottom_fill_order": "inward"}})");
    CHECK(read.top_fill_order == FillOrder::Outward);
    CHECK(read.bottom_fill_order == FillOrder::Inward);
    CHECK_THROWS(parse_settings(R"({"surfaces": {"top_fill_order": "sideways"}})"));
}

TEST_CASE("infill.smooth_factor: the sparse infill's turns become curves (orca_2_5.md, rule 11)", "[infill]") {
    // A path with a right angle, a hairpin and a nearly straight join.
    Toolpath t;
    for (const Point2d p : {Point2d{0, 0}, Point2d{10, 0}, Point2d{10, 10}, Point2d{10.5, 0.2}, Point2d{20.5, 0.25}})
        t.points.push_back({p, 0.4});
    std::vector<Toolpath> paths{t};
    smooth_corners(paths, 0.0);
    CHECK(paths[0].points.size() == 5);
    smooth_corners(paths, 1.0);
    const std::vector<ToolpathPoint>& out = paths[0].points;
    // The ends stay; the right angle becomes a curve from (5, 0) to (10, 5).
    CHECK(out.front().p.x == 0.0);
    CHECK(out.back().p.x == 20.5);
    CHECK(out.size() > 5);
    CHECK_THAT(out[1].p.x, WithinAbs(5.0, 1e-9));
    bool corner_kept = false, hairpin_kept = false;
    for (const ToolpathPoint& q : out) {
        corner_kept = corner_kept || (std::abs(q.p.x - 10.0) < 1e-9 && std::abs(q.p.y) < 1e-9);
        hairpin_kept = hairpin_kept || (std::abs(q.p.x - 10.0) < 1e-9 && std::abs(q.p.y - 10.0) < 1e-9);
    }
    CHECK_FALSE(corner_kept);
    CHECK(hairpin_kept);
    // No point of the curve is farther from the corner than half the shorter stretch.
    for (const ToolpathPoint& q : out)
        if (q.p.y < 5.0 + 1e-9 && q.p.x > 4.99 && q.p.x < 10.0 + 1e-9)
            CHECK(std::hypot(q.p.x - 10.0, q.p.y) <= 5.0 + 1e-9);
    // The setting reads and is checked.
    CHECK(parse_settings(R"({"infill": {"smooth_factor": 40}})").infill_smooth_factor == 40.0);
    CHECK_THROWS(parse_settings(R"({"infill": {"smooth_factor": 140}})"));
}

TEST_CASE("spiral_inset: the concentric loops joined into one spiral (orca_2_5.md, rule 13 bis)", "[infill]") {
    const auto length = [](const std::vector<Toolpath>& paths) {
        double total = 0.0;
        for (const Toolpath& t : paths)
            for (std::size_t i = 1; i < t.points.size(); ++i)
                total += std::hypot(t.points[i].p.x - t.points[i - 1].p.x, t.points[i].p.y - t.points[i - 1].p.y);
        return total;
    };
    // A 20 mm square: one spiral, as long as the loops less a spacing each, plus the steps.
    const std::vector<Toolpath> loops = concentric_fill(square(0, 0, 20), 0.4, 0.4);
    const std::vector<Toolpath> spiral = spiral_inset_fill(square(0, 0, 20), 0.4, 0.4);
    REQUIRE(spiral.size() == 1);
    CHECK_FALSE(spiral[0].closed);
    double loops_length = 0.0;
    for (const Toolpath& t : loops)
        for (std::size_t i = 0; i < t.points.size(); ++i)
            loops_length += std::hypot(t.points[(i + 1) % t.points.size()].p.x - t.points[i].p.x,
                                       t.points[(i + 1) % t.points.size()].p.y - t.points[i].p.y);
    CHECK(std::abs(length(spiral) - loops_length) < 0.5 * double(loops.size()));
    // It starts at a corner of the outer loop (a right angle) and runs inwards.
    const Point2d s = spiral[0].points.front().p;
    CHECK((std::abs(s.x - 0.2) < 1e-6 || std::abs(s.x - 19.8) < 1e-6));
    CHECK((std::abs(s.y - 0.2) < 1e-6 || std::abs(s.y - 19.8) < 1e-6));
    const Point2d e = spiral[0].points.back().p;
    CHECK(std::hypot(e.x - 10.0, e.y - 10.0) < 2.0);
    // A long rectangle: the last loop leaves a strip in the middle, filled by
    // a line along it at the spiral's end.
    const Polygons strip{{{to_units(0), to_units(0)}, {to_units(30), to_units(0)}, {to_units(30), to_units(3.1)},
                          {to_units(0), to_units(3.1)}}};
    const std::vector<Toolpath> long_spiral = spiral_inset_fill(strip, 0.4, 0.4);
    REQUIRE(long_spiral.size() == 1);
    const auto& tail = long_spiral[0].points;
    REQUIRE(tail.size() >= 2);
    const Point2d c0 = tail[tail.size() - 2].p, c1 = tail.back().p;
    CHECK(std::abs(c0.y - 1.55) < 0.3); // along the middle
    CHECK(std::abs(c1.y - 1.55) < 0.3);
    CHECK(std::abs(c1.x - c0.x) > 20.0); // along the long axis
    // Two islands far apart: a spiral each.
    const Polygons two = Clipper2Lib::Union(square(0, 0, 10), square(30, 0, 10), Clipper2Lib::FillRule::NonZero);
    CHECK(spiral_inset_fill(two, 0.4, 0.4).size() == 2);
    // As a top surface, rule 8's order turns it round.
    LayerSurfaces top;
    top.top = square(0, 0, 20);
    FillParams p;
    p.top_pattern = SurfacePattern::SpiralInset;
    p.top_order = FillOrder::Outward;
    const LayerFill out = fill_layer(top, 0, p, 0.2);
    REQUIRE(out.top.size() == 1);
    CHECK(std::hypot(out.top[0].points.front().p.x - 10.0, out.top[0].points.front().p.y - 10.0) < 2.0);
    CHECK(out.top[0].in_order);
    CHECK(parse_settings(R"({"surfaces": {"top_pattern": "spiral_inset"}})").top_pattern == SurfacePattern::SpiralInset);
}

TEST_CASE("surfaces.internal_solid_pattern: the solid inside the part (orca_2_5.md, rule 13 bis)", "[infill]") {
    // Layer over the bottom surface: its bottom solid is not exposed.
    LayerSurfaces s;
    s.bottom = square(0, 0, 20);
    FillParams p;
    const std::size_t zigzags = fill_layer(s, 1, p, 0.4).solid.size();
    p.internal_pattern = SurfacePattern::SpiralInset;
    const LayerFill spiral = fill_layer(s, 1, p, 0.4);
    REQUIRE(spiral.solid.size() == 1); // one spiral
    CHECK_FALSE(spiral.solid[0].closed);
    CHECK(spiral.solid[0].points.size() > 20);
    // The exposed bottom surface keeps the bottom pattern.
    s.bottom_exposed = s.bottom;
    p.bottom_pattern = SurfacePattern::Monotonic;
    const LayerFill bottom = fill_layer(s, 0, p, 0.2);
    for (const Toolpath& t : bottom.solid)
        CHECK(t.points.size() == 2);
    CHECK(zigzags >= 1);
    CHECK(parse_settings(R"({"surfaces": {"internal_solid_pattern": "concentric"}})").internal_solid_pattern ==
          SurfacePattern::Concentric);
    CHECK(Settings{}.internal_solid_pattern == SurfacePattern::Rectilinear);
}

TEST_CASE("infill.multiline: each path as N touching lines, the same material (orca_2_5.md, rule 13)", "[infill]") {
    const auto length = [](const std::vector<Toolpath>& paths) {
        double total = 0.0;
        for (const Toolpath& t : paths)
            for (std::size_t i = 1; i < t.points.size(); ++i)
                total += std::hypot(t.points[i].p.x - t.points[i - 1].p.x, t.points[i].p.y - t.points[i - 1].p.y);
        return total;
    };
    LayerSurfaces s;
    s.sparse = square(0, 0, 40);
    for (const InfillPattern pattern : {InfillPattern::Rectilinear, InfillPattern::Cubic, InfillPattern::Gyroid,
                                        InfillPattern::Honeycomb}) {
        INFO(int(pattern));
        FillParams p;
        p.pattern = pattern;
        p.density = 0.15;
        const double one = length(fill_layer(s, 0, p, 5.0).sparse);
        p.multiline = 3;
        const double three = length(fill_layer(s, 0, p, 5.0).sparse);
        REQUIRE(one > 0.0);
        // The same line length, within 3 % (OR12). Cubic's few lines on a
        // 40 mm square change length in steps with the density: within 8 %
        // until its own multiline paths (rule 13, part 2).
        CHECK(std::abs(three - one) < (pattern == InfillPattern::Cubic ? 0.08 : 0.03) * one);
    }
    // OR12: cubic at 15 % with three lines, no path crosses itself.
    {
        FillParams p;
        p.pattern = InfillPattern::Cubic;
        p.density = 0.15;
        p.multiline = 3;
        const auto crosses = [](const Toolpath& t) {
            const auto& q = t.points;
            const auto side = [](Point2d a, Point2d b, Point2d c) {
                return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
            };
            for (std::size_t i = 1; i < q.size(); ++i)
                for (std::size_t j = i + 2; j < q.size(); ++j) {
                    const Point2d a = q[i - 1].p, b = q[i].p, c = q[j - 1].p, d = q[j].p;
                    if (side(a, b, c) * side(a, b, d) < -1e-12 && side(c, d, a) * side(c, d, b) < -1e-12)
                        return true;
                }
            return false;
        };
        for (const double z : {1.0, 5.0, 9.0})
            for (const Toolpath& t : fill_layer(s, 0, p, z).sparse)
                CHECK_FALSE(crosses(t));
    }
    // A straight path becomes three lines a spacing apart, the middle one on it.
    Toolpath line;
    line.points = {{{0.0, 5.0}, 0.4}, {{10.0, 5.0}, 0.4}};
    const std::vector<Toolpath> copies = multiline_paths({line}, 3, 0.4, 0.4, square(-1, -1, 20));
    REQUIRE(copies.size() == 3);
    std::vector<double> ys;
    for (const Toolpath& t : copies)
        ys.push_back(t.points.front().p.y);
    std::sort(ys.begin(), ys.end());
    CHECK_THAT(ys[0], WithinAbs(4.6, 1e-6));
    CHECK_THAT(ys[1], WithinAbs(5.0, 1e-6));
    CHECK_THAT(ys[2], WithinAbs(5.4, 1e-6));
    CHECK(parse_settings(R"({"infill": {"multiline": 3}})").infill_multiline == 3);
    CHECK_THROWS(parse_settings(R"({"infill": {"multiline": 11}})"));
}

TEST_CASE("P10: the top and bottom surface patterns (infill_patterns.md, rules 14 to 17)", "[infill]") {
    const auto length = [](const std::vector<Toolpath>& paths) {
        double total = 0.0;
        for (const Toolpath& t : paths)
            for (std::size_t i = 1; i <= t.points.size() && t.points.size() > 1; ++i) {
                if (i == t.points.size() && !t.closed)
                    break; // a loop closes back to its first point
                const auto& a = t.points[i - 1].p;
                const auto& c = t.points[i % t.points.size()].p;
                total += std::hypot(c.x - a.x, c.y - a.y);
            }
        return total;
    };
    LayerSurfaces s;
    s.top = square(0, 0, 20);
    FillParams p; // 0.4 mm beads
    p.top_pattern = SurfacePattern::Monotonic;
    const double lines = length(fill_layer(s, 0, p, 10.0).top);
    REQUIRE(lines > 0.0);
    // Every pattern covers the surface about as much as the lines (P10: 924 to
    // 960 mm on the cube against 956 for the lines).
    for (const SurfacePattern pattern :
         {SurfacePattern::Rectilinear, SurfacePattern::MonotonicLines, SurfacePattern::AlignedRectilinear,
          SurfacePattern::Concentric, SurfacePattern::Hilbert, SurfacePattern::Archimedean, SurfacePattern::Octagram}) {
        INFO(surface_pattern_name(pattern));
        p.top_pattern = pattern;
        const LayerFill f = fill_layer(s, 0, p, 10.0);
        CHECK(length(f.top) > 0.9 * lines);
        CHECK(length(f.top) < 1.15 * lines);
    }
    // aligned_rectilinear does not turn between layers; rectilinear does (rule 15).
    const auto first_direction = [](const LayerFill& f) {
        const Toolpath& t = f.top.front();
        return std::atan2(t.points[1].p.y - t.points[0].p.y, t.points[1].p.x - t.points[0].p.x);
    };
    p.top_pattern = SurfacePattern::AlignedRectilinear;
    CHECK(std::abs(std::sin(first_direction(fill_layer(s, 0, p)) - first_direction(fill_layer(s, 1, p)))) < 1e-6);
    // monotonic_lines (BH21): every line the same way; monotonic goes back and forth.
    const auto directions = [](const LayerFill& f) {
        std::set<int> signs;
        for (const Toolpath& t : f.top)
            if (t.points.size() >= 2)
                signs.insert((t.points.back().p.x - t.points.front().p.x) + (t.points.back().p.y - t.points.front().p.y) > 0.0
                                 ? 1
                                 : -1);
        return signs.size();
    };
    p.top_pattern = SurfacePattern::MonotonicLines;
    CHECK(directions(fill_layer(s, 0, p)) == 1);
    CHECK(directions(fill_layer(s, 1, p)) == 1);
    // concentric: loops, each closed.
    p.top_pattern = SurfacePattern::Concentric;
    const LayerFill loops = fill_layer(s, 0, p);
    REQUIRE(loops.top.size() > 5);
    // Rule 17: on the bottom, only the bottom surface itself takes the pattern;
    // the solid layer over it (nothing exposed) zig-zags as always.
    LayerSurfaces bottom;
    bottom.bottom = square(0, 0, 20);
    bottom.bottom_exposed = bottom.bottom;
    FillParams b;
    b.bottom_pattern = SurfacePattern::Concentric;
    const LayerFill exposed = fill_layer(bottom, 0, b);
    bottom.bottom_exposed.clear();
    const LayerFill inner = fill_layer(bottom, 1, b);
    b.bottom_pattern = SurfacePattern::Rectilinear;
    const LayerFill zigzag = fill_layer(bottom, 1, b);
    CHECK(exposed.solid.size() > 5); // loops
    CHECK(inner.solid.size() == zigzag.solid.size());
    CHECK(std::abs(length(inner.solid) - length(zigzag.solid)) < 1e-9);
    // The settings read every name and write it back.
    for (const char* name : {"monotonic", "rectilinear", "monotonic_lines", "aligned_rectilinear", "concentric", "hilbert",
                             "archimedean", "octagram"}) {
        const Settings read = parse_settings(std::string(R"({"surfaces": {"top_pattern": ")") + name +
                                             R"(", "bottom_pattern": ")" + name + R"("}})");
        CHECK(std::string(surface_pattern_name(read.top_pattern)) == name);
        CHECK(std::string(surface_pattern_name(read.bottom_pattern)) == name);
    }
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

TEST_CASE("TPMS-D and TPMS-FK infill: points on their surfaces, the density's material (orca_2_5.md, rule 10)", "[infill]") {
    const Polygons region = square(0, 0, 20);
    LayerSurfaces s;
    s.sparse = region;
    for (const auto [pattern, kind] : {std::pair{InfillPattern::TpmsD, Tpms::SchwarzD},
                                       std::pair{InfillPattern::TpmsFk, Tpms::FischerKochS}}) {
        INFO("pattern " << int(pattern));
        FillParams p;
        p.pattern = pattern;
        p.density = 0.2;
        p.width = 0.4;
        p.overlap = 0.0;
        CHECK(fill_period(p) == 0); // changes with z, never repeats by layers
        const double period = tpms_period(kind, 0.4, 0.2);
        const LayerFill at = fill_layer(s, 0, p, 0.7);
        REQUIRE_FALSE(at.sparse.empty());
        double worst = 0.0;
        for (const Toolpath& t : at.sparse)
            for (const ToolpathPoint& q : t.points) {
                const bool on_boundary = std::fabs(q.p.x - 0.2) < 1e-3 || std::fabs(q.p.x - 19.8) < 1e-3 ||
                                         std::fabs(q.p.y - 0.2) < 1e-3 || std::fabs(q.p.y - 19.8) < 1e-3;
                if (!on_boundary)
                    worst = std::max(worst, std::fabs(tpms_value(kind, q.p.x, q.p.y, 0.7, period)));
            }
        CHECK(worst < 0.02);
        // About the density's material, averaged over a period of z (the
        // joins along the edge add a little, as with the gyroid).
        double laid = 0.0;
        const int heights = 8;
        for (int i = 0; i < heights; ++i)
            laid += measure_coverage(region, fill_layer(s, 0, p, period * i / heights).sparse).deposited;
        laid /= heights;
        std::printf("tpms %d at 20%% on a 20 mm square: lays %.1f%%\n", int(pattern), laid / 4.0);
        CHECK_THAT(laid / 400.0, WithinAbs(0.21, 0.025));
    }
    CHECK(parse_settings(R"({"infill": {"pattern": "tpms_fk"}})").infill_pattern == InfillPattern::TpmsFk);
    CHECK(parse_settings(R"({"infill": {"pattern": "tpms_d"}})").infill_pattern == InfillPattern::TpmsD);
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
    CHECK(parse_settings(R"({"surfaces": {"top_pattern": "rectilinear"}})").top_pattern == SurfacePattern::Rectilinear);
    CHECK(parse_settings(R"({"surfaces": {"top_pattern": "monotonic"}})").top_pattern == SurfacePattern::Monotonic);
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

TEST_CASE("rectilinear: joins only inside join_inside, as a bridge's over what holds it (toolpath_time.md, rule 1)", "[infill]") {
    // Lines along X across a 10 mm square, 1 mm apart; joined, they make one
    // zig-zag. Held only on the left (x < 2): only the joins at the left end.
    const Polygons region = square(0, 0, 10);
    const std::vector<Toolpath> loose = rectilinear_fill(region, 1.0, 0.0, 0.4, false, true);
    const std::vector<Toolpath> all = rectilinear_fill(region, 1.0, 0.0, 0.4, true, true, false, 0.0, 0.0, 4.0 * 0.4 * 2.0);
    Polygons held = {Polygon{{-1 * kUnitsPerMm, -1 * kUnitsPerMm}, {2 * kUnitsPerMm, -1 * kUnitsPerMm},
                             {2 * kUnitsPerMm, 11 * kUnitsPerMm}, {-1 * kUnitsPerMm, 11 * kUnitsPerMm}}};
    const std::vector<Toolpath> some =
        rectilinear_fill(region, 1.0, 0.0, 0.4, true, true, false, 0.0, 0.0, 4.0 * 0.4 * 2.0, &held);
    REQUIRE(loose.size() >= 8);
    CHECK(all.size() == 1);
    // Every other pair joined: half the pieces, about.
    CHECK(some.size() > all.size());
    CHECK(some.size() < loose.size());
    // No join leaves the held strip: every point off the lines' rows lies at x < 2.
    for (const Toolpath& t : some)
        for (std::size_t k = 1; k < t.points.size(); ++k) {
            const auto& a = t.points[k - 1].p;
            const auto& b = t.points[k].p;
            if (std::fabs(a.y - b.y) > 1e-6) // a join, across the rows
                CHECK((a.x < 2.0 && b.x < 2.0));
        }
}
