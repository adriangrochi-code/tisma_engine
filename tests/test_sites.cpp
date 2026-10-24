#include "perimeters/sites.hpp"
#include "perimeters/transitions.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <set>
#include <tuple>

using namespace tisma;
using Catch::Matchers::WithinAbs;

namespace {

Polygon rect(double x0, double y0, double w, double h) {
    return {{to_units(x0), to_units(y0)}, {to_units(x0 + w), to_units(y0)}, {to_units(x0 + w), to_units(y0 + h)},
            {to_units(x0), to_units(y0 + h)}};
}

struct Prepared {
    MedialGraph graph;
    TransitionResult transitions;
    std::vector<HalfBeading> beadings;
    std::vector<Site> sites;
};

Prepared prepare(const Polygons& region, const BeadingParams& p) {
    Prepared r;
    r.graph = medial_graph(region);
    mark_central(r.graph);
    filter_marking(r.graph, p.preferred_width);
    r.transitions = apply_transitions(r.graph, p);
    r.beadings = node_beadings(r.graph, r.transitions.bead_counts, p);
    r.sites = support_sites(r.graph, r.beadings);
    return r;
}

} // namespace

TEST_CASE("a 2 mm strip gets five 0.4 mm beads in straight rows", "[sites]") {
    BeadingParams p; // distributed, w* = 0.4: 2 / 0.4 = 5 beads of 0.4 mm
    const Prepared r = prepare({rect(0, 0, 30, 2)}, p);
    // The middle line is one edge from (1, 1) to (29, 1); its trapezoids carry
    // the sites of the straight part at both ends, one row per bead centre.
    std::size_t middle_sites = 0;
    for (const Site& s : r.sites) {
        const MedialGraph::Edge& e = r.graph.edges[s.edge];
        const bool middle_line = std::fabs(r.graph.nodes[e.a].p.y - 1.0) < 1e-6 &&
                                 std::fabs(r.graph.nodes[e.b].p.y - 1.0) < 1e-6;
        if (!middle_line)
            continue;
        ++middle_sites;
        const double expected[] = {0.2, 0.6, 1.0, 1.4, 1.8};
        double nearest = 1e9;
        for (double y : expected)
            nearest = std::min(nearest, std::fabs(s.p.y - y));
        CHECK(nearest < 1e-6);
        CHECK((std::fabs(s.p.x - 1.0) < 1e-6 || std::fabs(s.p.x - 29.0) < 1e-6));
        CHECK_THAT(s.width, WithinAbs(0.4, 1e-9));
        CHECK(s.index <= 2); // three beads per side: 0, 1 and the middle one
    }
    // 2 ends x 2 sides x 3 beads.
    CHECK(middle_sites == 12);
}

TEST_CASE("non-central nodes take the beading of the centre above them", "[sites]") {
    BeadingParams p;
    const Prepared r = prepare({rect(0, 0, 30, 2)}, p);
    for (std::size_t n = 0; n < r.graph.nodes.size(); ++n) {
        // Every node of the strip, corners included, uses the 5-bead beading of the middle line.
        REQUIRE(r.beadings[n].widths.size() == 3);
        CHECK_THAT(r.beadings[n].distances[2], WithinAbs(1.0, 1e-9));
    }
    // So sites near the corners lie on the same rows, inside the region.
    for (const Site& s : r.sites) {
        CHECK(s.p.x >= -1e-9);
        CHECK(s.p.x <= 30.0 + 1e-9);
        CHECK(s.p.y >= 0.2 - 1e-6);
        CHECK(s.p.y <= 1.8 + 1e-6);
    }
}

TEST_CASE("wedge sites are inside the wedge and their widths stay in range", "[sites]") {
    BeadingParams p;
    const double h = std::tan(15.0 * std::numbers::pi / 180.0) * 20.0;
    const Prepared r = prepare({{{to_units(0), to_units(0)}, {to_units(20), to_units(-h)}, {to_units(20), to_units(h)}}}, p);
    REQUIRE_FALSE(r.sites.empty());
    std::set<int> indices;
    for (const Site& s : r.sites) {
        indices.insert(s.index);
        // Inside the wedge (with a little slack for the boundary itself).
        CHECK(s.p.x >= -1e-6);
        CHECK(std::fabs(s.p.y) <= s.p.x * std::tan(15.0 * std::numbers::pi / 180.0) + 1e-6);
        CHECK(s.width > 0.0);
        CHECK(s.width <= 0.6 + 1e-9); // distributed widths stay below 3/2 w*
    }
    CHECK(indices.size() > 2); // the wedge thickens enough for several beads per side
}

namespace {

double loop_area(const Toolpath& t) {
    double a = 0.0;
    for (std::size_t i = 0; i < t.points.size(); ++i) {
        const Point2d& p = t.points[i].p;
        const Point2d& q = t.points[(i + 1) % t.points.size()].p;
        a += p.x * q.y - q.x * p.y;
    }
    return std::fabs(a) / 2.0;
}

} // namespace

TEST_CASE("the strip's toolpaths: two loops and a middle line", "[sites]") {
    BeadingParams p;
    const Prepared r = prepare({rect(0, 0, 30, 2)}, p);
    const std::vector<Toolpath> paths = extract_toolpaths(r.graph, r.beadings);
    std::vector<const Toolpath*> loops, lines;
    for (const Toolpath& t : paths)
        (t.closed ? loops : lines).push_back(&t);
    REQUIRE(loops.size() == 2);
    REQUIRE(lines.size() == 1);
    for (const Toolpath* l : loops) {
        // Bead 0 runs 0.2 mm in from the outline, bead 1 0.6 mm in.
        const double inset = l->index == 0 ? 0.2 : 0.6;
        CHECK_THAT(loop_area(*l), WithinAbs((30 - 2 * inset) * (2 - 2 * inset), 1e-6));
        for (const ToolpathPoint& q : l->points)
            CHECK_THAT(q.width, WithinAbs(0.4, 1e-9));
    }
    // The middle bead is one line along y = 1 between the ends of the middle edge.
    const Toolpath& middle = *lines[0];
    CHECK(middle.index == 2);
    for (const ToolpathPoint& q : middle.points)
        CHECK_THAT(q.p.y, WithinAbs(1.0, 1e-6));
}

TEST_CASE("wedge toolpaths have positive widths and stay inside", "[sites]") {
    BeadingParams p;
    const double h = std::tan(15.0 * std::numbers::pi / 180.0) * 20.0;
    const Prepared r = prepare({{{to_units(0), to_units(0)}, {to_units(20), to_units(-h)}, {to_units(20), to_units(h)}}}, p);
    const std::vector<Toolpath> paths = extract_toolpaths(r.graph, r.beadings);
    REQUIRE_FALSE(paths.empty());
    for (const Toolpath& t : paths)
        for (const ToolpathPoint& q : t.points) {
            CHECK(q.width > 0.0);
            CHECK(q.p.x >= -1e-6);
            CHECK(std::fabs(q.p.y) <= q.p.x * std::tan(15.0 * std::numbers::pi / 180.0) + 1e-6);
        }
}

namespace {

Polygon regular_polygon(int n, double radius) {
    Polygon out;
    for (int i = 0; i < n; ++i) {
        const double a = 2.0 * std::numbers::pi * i / n;
        out.push_back({to_units(radius * std::cos(a)), to_units(radius * std::sin(a))});
    }
    return out;
}

} // namespace

TEST_CASE("a regular 64-gon gets closed concentric loops", "[sites]") {
    // Its Voronoi diagram meets at the centre in a cluster of vertices a few
    // 1e-5 mm apart; the loops must still close.
    BeadingParams p;
    const double radius = 5.0;
    const Prepared r = prepare({regular_polygon(64, radius)}, p);
    const std::vector<Toolpath> paths = extract_toolpaths(r.graph, r.beadings);
    // The inscribed radius is 5 cos(pi / 64): 25 beads, so 12 loops and a dot at the centre.
    const double inradius = radius * std::cos(std::numbers::pi / 64);
    const double width = 2.0 * inradius / 25.0;
    REQUIRE(paths.size() == 12);
    std::set<int> indices;
    for (const Toolpath& t : paths) {
        CHECK(t.closed);
        indices.insert(t.index);
        // Bead i follows the outline (i + 1/2) widths in: a 64-gon of that inradius.
        const double in = inradius - (t.index + 0.5) * width;
        const double area = 64.0 * in * in * std::tan(std::numbers::pi / 64);
        CHECK_THAT(loop_area(t), WithinAbs(area, 1e-3 * area));
        for (const ToolpathPoint& q : t.points)
            CHECK_THAT(q.width, WithinAbs(width, 1e-6));
    }
    CHECK(indices.size() == 12);
}

TEST_CASE("an L-shaped strip: two loops around the reflex corner and a middle line", "[sites]") {
    // Arms 2 mm wide and 20 mm long. Near the reflex corner (2, 2) the loops
    // follow arcs around it, so the trapezoids of the corner and of the walls
    // must share their sites there. The joint is 2.34 mm thick, too short a
    // stretch for a sixth bead (its transitions are filtered), so the five
    // beads widen there.
    BeadingParams p;
    const std::vector<Point2d> corners = {{0, 0}, {20, 0}, {20, 2}, {2, 2}, {2, 20}, {0, 20}};
    Polygon l;
    for (const Point2d& c : corners)
        l.push_back({to_units(c.x), to_units(c.y)});
    const Prepared r = prepare({l}, p);
    const std::vector<Toolpath> paths = extract_toolpaths(r.graph, r.beadings);
    auto distance_to_outline = [&](const Point2d& q) {
        double best = 1e9;
        for (std::size_t i = 0; i < corners.size(); ++i) {
            const Point2d& a = corners[i];
            const Point2d& b = corners[(i + 1) % corners.size()];
            const double dx = b.x - a.x, dy = b.y - a.y;
            const double t = std::clamp(((q.x - a.x) * dx + (q.y - a.y) * dy) / (dx * dx + dy * dy), 0.0, 1.0);
            best = std::min(best, std::hypot(q.x - a.x - t * dx, q.y - a.y - t * dy));
        }
        return best;
    };
    std::vector<const Toolpath*> loops, lines;
    for (const Toolpath& t : paths)
        (t.closed ? loops : lines).push_back(&t);
    REQUIRE(loops.size() == 2);
    REQUIRE(lines.size() == 1);
    CHECK(lines[0]->index == 2);
    std::set<int> indices;
    for (const Toolpath& t : paths) {
        indices.insert(t.index);
        for (const ToolpathPoint& q : t.points) {
            // Equal widths: bead i is (i + 1/2) widths in from the outline.
            CHECK_THAT(distance_to_outline(q.p), WithinAbs((t.index + 0.5) * q.width, 1e-6));
            CHECK(q.width >= 0.4 - 1e-9);
            CHECK(q.width <= 2.0 * 2.0 * std::sqrt(2.0) / (1.0 + std::sqrt(2.0)) / 5.0 + 1e-9);
        }
    }
    CHECK(indices == std::set<int>{0, 1, 2});
}

TEST_CASE("every toolpath point is one of the sites, at its width", "[sites]") {
    BeadingParams p;
    const double h = std::tan(15.0 * std::numbers::pi / 180.0) * 20.0;
    const Polygons shapes[] = {
        {rect(0, 0, 30, 2)},
        {regular_polygon(64, 5.0)},
        {{{to_units(0), to_units(0)}, {to_units(20), to_units(-h)}, {to_units(20), to_units(h)}}},
    };
    for (const Polygons& shape : shapes) {
        const Prepared r = prepare(shape, p);
        std::set<std::tuple<double, double, double, int>> sites;
        for (const Site& s : r.sites)
            sites.insert({s.p.x, s.p.y, s.width, s.index});
        for (const Site& s : medial_sites(r.graph, r.beadings))
            sites.insert({s.p.x, s.p.y, s.width, s.index});
        const std::vector<Toolpath> paths = extract_toolpaths(r.graph, r.beadings);
        REQUIRE_FALSE(paths.empty());
        for (const Toolpath& t : paths)
            for (const ToolpathPoint& q : t.points)
                CHECK(sites.count({q.p.x, q.p.y, q.width, t.index}) == 1);
    }
}
