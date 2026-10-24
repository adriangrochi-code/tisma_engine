#include "perimeters/sites.hpp"
#include "perimeters/transitions.hpp"
#include "perimeters/walls.hpp"

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

TEST_CASE("wall limit: a strip narrow enough keeps its beads, a wider limit drops the middle", "[sites]") {
    BeadingParams p;
    p.wall_count = 3; // 5 beads fit in 2 mm: under the limit of 6, nothing changes
    {
        const Prepared r = prepare({rect(0, 0, 30, 2)}, p);
        std::size_t loops = 0, lines = 0;
        for (const Toolpath& t : extract_toolpaths(r.graph, r.beadings))
            (t.closed ? loops : lines) += 1;
        CHECK(loops == 2);
        CHECK(lines == 1);
    }
    p.wall_count = 2; // 2 mm is the end of the shell ramp for 2 walls: two loops at w*, a 0.4 mm gap
    const Prepared r = prepare({rect(0, 0, 30, 2)}, p);
    const std::vector<Toolpath> paths = extract_toolpaths(r.graph, r.beadings);
    REQUIRE(paths.size() == 2);
    for (const Toolpath& t : paths) {
        CHECK(t.closed);
        const double inset = t.index == 0 ? 0.2 : 0.6;
        CHECK_THAT(loop_area(t), WithinAbs((30 - 2 * inset) * (2 - 2 * inset), 1e-6));
        for (const ToolpathPoint& q : t.points)
            CHECK_THAT(q.width, WithinAbs(0.4, 1e-9));
    }
}

TEST_CASE("wall limit: a 20 mm square gets exactly M loops of w* and an empty middle", "[sites]") {
    for (int m : {1, 3}) {
        BeadingParams p;
        p.wall_count = m;
        const Prepared r = prepare({rect(0, 0, 20, 20)}, p);
        const std::vector<Toolpath> paths = extract_toolpaths(r.graph, r.beadings);
        REQUIRE(paths.size() == std::size_t(m));
        std::set<int> indices;
        for (const Toolpath& t : paths) {
            CHECK(t.closed);
            indices.insert(t.index);
            const double inset = 0.2 + 0.4 * t.index;
            CHECK_THAT(loop_area(t), WithinAbs((20 - 2 * inset) * (20 - 2 * inset), 1e-6));
            for (const ToolpathPoint& q : t.points)
                CHECK_THAT(q.width, WithinAbs(0.4, 1e-9));
        }
        CHECK(int(indices.size()) == m);
    }
}

TEST_CASE("wall limit: the shell ramp gets its own nodes on the centre", "[sites]") {
    // A wedge thickens steadily, so its centre crosses both ends of the ramp.
    BeadingParams p;
    p.wall_count = 2;
    const double h = std::tan(15.0 * std::numbers::pi / 180.0) * 20.0;
    const Prepared r = prepare({{{to_units(0), to_units(0)}, {to_units(20), to_units(-h)}, {to_units(20), to_units(h)}}}, p);
    const auto ramp = shell_ramp(p);
    REQUIRE(ramp);
    bool at_start = false, at_end = false;
    for (const MedialGraph::Node& n : r.graph.nodes) {
        if (!n.central)
            continue;
        at_start = at_start || std::fabs(2 * n.radius - ramp->start) < 1e-9;
        at_end = at_end || std::fabs(2 * n.radius - ramp->end) < 1e-9;
    }
    CHECK(at_start);
    CHECK(at_end);
    for (const Toolpath& t : extract_toolpaths(r.graph, r.beadings)) {
        CHECK(t.index < 2);
        for (const ToolpathPoint& q : t.points)
            CHECK(q.width > 0.0);
    }
}

TEST_CASE("layer_walls: what the walls leave for infill", "[sites]") {
    BeadingParams p;
    p.wall_count = 2; // the 2 mm strip: two walls of 0.4 mm and a 0.4 mm middle
    LayerWalls w = layer_walls({rect(0, 0, 30, 2)}, p);
    CHECK(w.toolpaths.size() == 2);
    CHECK_THAT(area_mm2(w.infill), WithinAbs(28.4 * 0.4, 1e-6));
    p.wall_count = 3; // five beads fill it: nothing left
    w = layer_walls({rect(0, 0, 30, 2)}, p);
    CHECK(w.toolpaths.size() == 3);
    CHECK(w.infill.empty());
    p.wall_count = 3; // the 20 mm square keeps an inner square of 20 - 2 * 1.2
    w = layer_walls({rect(0, 0, 20, 20)}, p);
    CHECK_THAT(area_mm2(w.infill), WithinAbs(17.6 * 17.6, 1e-6));
    p.wall_count = 0; // no limit: the beads fill everything
    w = layer_walls({rect(0, 0, 20, 20)}, p);
    CHECK(w.infill.empty());
    CHECK(layer_walls({}, p).toolpaths.empty());
}

TEST_CASE("beading conflicts: up from a thin fin, the beading turns into the body's over w*", "[sites]") {
    // A 1 mm fin (3 beads of 1/3 mm) on a 6 mm body (15 beads). The fin's centre
    // runs down to (8.5, 6) and a little beyond, between the two reflex corners,
    // while R grows slowly; from the last marked node an unmarked bone rises to
    // the body's centre.
    const std::vector<Point2d> corners = {{0, 0}, {20, 0}, {20, 6}, {9, 6}, {9, 14}, {8, 14}, {8, 6}, {0, 6}};
    Polygon tee;
    for (const Point2d& c : corners)
        tee.push_back({to_units(c.x), to_units(c.y)});
    BeadingParams p;
    const Prepared r = prepare({tee}, p);
    std::uint32_t base = 0xFFFFFFFFu, top = 0xFFFFFFFFu;
    for (std::uint32_t n = 0; n < r.graph.nodes.size(); ++n) {
        const MedialGraph::Node& node = r.graph.nodes[n];
        if (std::fabs(node.p.x - 8.5) > 1e-9 || !node.central)
            continue;
        if (node.p.y <= 6.0 + 1e-9 && node.p.y > 3.1 && (base == 0xFFFFFFFFu || node.p.y < r.graph.nodes[base].p.y))
            base = n; // the lowest marked node of the fin
        if (node.p.y < 3.1 && node.p.y > 2.9)
            top = n;
    }
    REQUIRE(base != 0xFFFFFFFFu);
    REQUIRE(top != 0xFFFFFFFFu);
    CHECK(r.transitions.bead_counts[base] == 3.0);
    const double base_y = r.graph.nodes[base].p.y;
    const HalfBeading& low = r.beadings[base];
    const HalfBeading& high = r.beadings[top];
    REQUIRE(low.distances.size() == 2);
    // Nodes on the bone below the base: blended within w*, the body's beading beyond.
    std::size_t blended = 0;
    for (std::uint32_t n = 0; n < r.graph.nodes.size(); ++n) {
        const MedialGraph::Node& node = r.graph.nodes[n];
        if (node.central || std::fabs(node.p.x - 8.5) > 1e-9 || node.p.y >= base_y || node.p.y < 3.1)
            continue;
        const double d = base_y - node.p.y; // path length from the base along the straight bone
        const HalfBeading& b = r.beadings[n];
        if (d < p.preferred_width - 1e-9) {
            ++blended;
            const double f = d / p.preferred_width;
            CHECK_THAT(b.widths[0], WithinAbs(low.widths[0] + f * (high.widths[0] - low.widths[0]), 1e-9));
            CHECK_THAT(b.distances[0], WithinAbs(low.distances[0] + f * (high.distances[0] - low.distances[0]), 1e-9));
            CHECK(b.widths.size() == high.widths.size());
        } else {
            CHECK(b.widths == high.widths);
            CHECK(b.distances == high.distances);
        }
    }
    CHECK(blended >= 1);
    // The toolpaths stay inside the T with positive widths.
    auto inside = [&](const Point2d& q) {
        bool in = false;
        for (std::size_t i = 0, j = corners.size() - 1; i < corners.size(); j = i++)
            if ((corners[i].y > q.y) != (corners[j].y > q.y) &&
                q.x < corners[j].x + (q.y - corners[j].y) * (corners[i].x - corners[j].x) / (corners[i].y - corners[j].y))
                in = !in;
        return in;
    };
    for (const Toolpath& t : extract_toolpaths(r.graph, r.beadings))
        for (const ToolpathPoint& q : t.points) {
            CHECK(q.width > 0.0);
            CHECK(inside(q.p));
        }
}
