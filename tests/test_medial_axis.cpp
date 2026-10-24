#include "geom/medial_axis.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <utility>

using namespace tisma;
using Catch::Matchers::WithinAbs;

namespace {

Polygon rect(double x0, double y0, double w, double h, bool clockwise = false) {
    Polygon p = {{to_units(x0), to_units(y0)},
                 {to_units(x0 + w), to_units(y0)},
                 {to_units(x0 + w), to_units(y0 + h)},
                 {to_units(x0), to_units(y0 + h)}};
    if (clockwise)
        std::reverse(p.begin(), p.end());
    return p;
}

double max_radius(const std::vector<MedialEdge>& edges) {
    double r = 0.0;
    for (const MedialEdge& e : edges)
        r = std::max({r, e.radius_a, e.radius_b});
    return r;
}

bool has_point(const std::vector<MedialEdge>& edges, double x, double y) {
    for (const MedialEdge& e : edges)
        if ((std::hypot(e.a.x - x, e.a.y - y) < 1e-6) || (std::hypot(e.b.x - x, e.b.y - y) < 1e-6))
            return true;
    return false;
}

} // namespace

TEST_CASE("medial axis of a square: four edges from the centre to the corners", "[medial]") {
    const auto edges = medial_axis({rect(0, 0, 10, 10)});
    REQUIRE(edges.size() == 4);
    for (const MedialEdge& e : edges) {
        CHECK_FALSE(e.curved);
        const bool a_is_centre = std::hypot(e.a.x - 5, e.a.y - 5) < 1e-6;
        const MedialEdge& m = e;
        CHECK_THAT(a_is_centre ? m.radius_a : m.radius_b, WithinAbs(5.0, 1e-6));
        CHECK_THAT(a_is_centre ? m.radius_b : m.radius_a, WithinAbs(0.0, 1e-6));
    }
    CHECK(has_point(edges, 0, 0));
    CHECK(has_point(edges, 10, 10));
}

TEST_CASE("medial axis of a rectangle: a centre line at half the width", "[medial]") {
    const auto edges = medial_axis({rect(0, 0, 20, 10)});
    CHECK(edges.size() == 5);
    CHECK(has_point(edges, 5, 5));
    CHECK(has_point(edges, 15, 5));
    CHECK_THAT(max_radius(edges), WithinAbs(5.0, 1e-6));
}

TEST_CASE("medial axis of a square ring bends around the hole's corners", "[medial]") {
    // 10 mm square with a centred 4 mm hole: walls 3 mm thick. Along a wall the
    // axis is 1.5 mm from both sides; near each corner of the hole it bends
    // into a parabola (equidistant from the hole's corner and the outer walls),
    // and on the diagonal it reaches t = sqrt(2) (3 - t), t = 3 sqrt(2) / (1 + sqrt(2)).
    const auto edges = medial_axis({rect(0, 0, 10, 10), rect(3, 3, 4, 4, true)});
    const double t = 3.0 * std::sqrt(2.0) / (1.0 + std::sqrt(2.0));
    CHECK_THAT(max_radius(edges), WithinAbs(t, 1e-6));
    CHECK(has_point(edges, t, t));
    CHECK(has_point(edges, 10.0 - t, 10.0 - t));
    CHECK(std::any_of(edges.begin(), edges.end(), [](const MedialEdge& e) { return e.curved; }));
    for (const MedialEdge& e : edges) {
        // Nothing inside the hole, and every end at least as close as the wall middle allows.
        CHECK_FALSE((e.a.x > 3.0 + 1e-6 && e.a.x < 7.0 - 1e-6 && e.a.y > 3.0 + 1e-6 && e.a.y < 7.0 - 1e-6));
        CHECK(e.radius_a <= t + 1e-6);
    }
}

TEST_CASE("medial axis of an empty region", "[medial]") {
    CHECK(medial_axis({}).empty());
}

TEST_CASE("parabolic edges come in pieces of at most 0.2 mm", "[medial]") {
    const auto edges = medial_axis({rect(0, 0, 10, 10), rect(3, 3, 4, 4, true)});
    std::size_t curved = 0;
    for (const MedialEdge& e : edges) {
        if (!e.curved)
            continue;
        ++curved;
        CHECK(std::hypot(e.b.x - e.a.x, e.b.y - e.a.y) <= kMaxCurvedPiece + 1e-9);
        // Every piece end is as far from the boundary as its radius says: here the
        // nearest boundary is a hole corner or an outer wall.
        const std::pair<Point2d, double> ends[] = {{e.a, e.radius_a}, {e.b, e.radius_b}};
        for (const auto& [p, r] : ends) {
            const double to_walls = std::min({p.x, p.y, 10.0 - p.x, 10.0 - p.y});
            const double to_corners = std::min({std::hypot(p.x - 3, p.y - 3), std::hypot(p.x - 7, p.y - 3),
                                                std::hypot(p.x - 3, p.y - 7), std::hypot(p.x - 7, p.y - 7)});
            CHECK_THAT(std::min(to_walls, to_corners), WithinAbs(r, 1e-6));
            CHECK_THAT(to_walls, WithinAbs(to_corners, 1e-6)); // on the parabola: equidistant
        }
    }
    CHECK(curved >= 8); // four hole corners, a parabola on each side of each
}

TEST_CASE("central marking: the middle line of a strip, not the corner bisectors", "[medial]") {
    MedialGraph g = medial_graph({rect(0, 0, 20, 10)});
    mark_central(g);
    std::size_t central_edges = 0;
    for (const MedialGraph::Edge& e : g.edges) {
        const MedialGraph::Node& a = g.nodes[e.a];
        const MedialGraph::Node& b = g.nodes[e.b];
        const bool middle_line = std::fabs(a.p.y - 5) < 1e-6 && std::fabs(b.p.y - 5) < 1e-6;
        CHECK(e.central == middle_line); // bisectors of 90 degree corners have alpha = 90 < 135
        central_edges += e.central ? 1 : 0;
    }
    CHECK(central_edges == 1);
}

TEST_CASE("central marking: a square's centre is a local maximum", "[medial]") {
    MedialGraph g = medial_graph({rect(0, 0, 10, 10)});
    mark_central(g);
    REQUIRE(g.nodes.size() == 5);
    for (const MedialGraph::Node& n : g.nodes)
        CHECK(n.central == (std::hypot(n.p.x - 5, n.p.y - 5) < 1e-6));
    for (const MedialGraph::Edge& e : g.edges)
        CHECK_FALSE(e.central);
}

TEST_CASE("central marking follows the bisector angle threshold", "[medial]") {
    // A thin wedge: the bisector angle along its axis is 180 - wedge angle.
    // With a 30 degree wedge (alpha = 150 > 135) the axis is central; raising
    // alpha_max to 160 degrees makes it not significant.
    const double h = std::tan(15.0 * std::numbers::pi / 180.0) * 20.0;
    const Polygon wedge = {{to_units(0), to_units(0)}, {to_units(20), to_units(-h)}, {to_units(20), to_units(h)}};
    MedialGraph g = medial_graph({wedge});
    mark_central(g, 135.0);
    bool axis_central = false;
    for (const MedialGraph::Edge& e : g.edges)
        if (std::fabs(g.nodes[e.a].p.y) < 1e-6 && std::fabs(g.nodes[e.b].p.y) < 1e-6)
            axis_central = axis_central || e.central;
    CHECK(axis_central);
    mark_central(g, 160.0);
    bool still_central = false;
    for (const MedialGraph::Edge& e : g.edges)
        if (std::fabs(g.nodes[e.a].p.y) < 1e-6 && std::fabs(g.nodes[e.b].p.y) < 1e-6)
            still_central = still_central || e.central;
    CHECK_FALSE(still_central);
}

TEST_CASE("marking filter closes short unmarked gaps on the way up", "[medial]") {
    // Hand-made graph along x: a central edge at radius 1.0, a steep unmarked edge
    // up to radius 1.3 (slope 1, not significant), then a central edge at 1.3.
    auto make = [](double gap_length) {
        MedialGraph g;
        g.nodes = {{{0.0, 0}, 1.0, true}, {{1.0, 0}, 1.0, true}, {{1.0 + gap_length, 0}, 1.3, true},
                   {{3.0 + gap_length, 0}, 1.3, true}};
        g.edges = {{0, 1, false, true}, {1, 2, false, false}, {2, 3, false, true}};
        return g;
    };
    MedialGraph short_gap = make(0.3);
    filter_marking(short_gap, 0.4); // w* = 0.4
    CHECK(short_gap.edges[1].central);
    MedialGraph long_gap = make(0.6);
    filter_marking(long_gap, 0.4);
    CHECK_FALSE(long_gap.edges[1].central);
}

TEST_CASE("marking filter only walks upwards", "[medial]") {
    // From the central node at radius 1.3 the unmarked edge goes down to 1.0, and
    // from the central node at 1.0 it goes up: only the upward walk counts, and it
    // finds the other central node, so the gap is closed either way.
    MedialGraph g;
    g.nodes = {{{0.0, 0}, 1.3, true}, {{0.3, 0}, 1.0, true}, {{0.6, 0}, 0.7, false}};
    g.edges = {{0, 1, false, false}, {1, 2, false, false}};
    filter_marking(g, 0.4);
    CHECK(g.edges[0].central);       // 1 -> 0 goes up and reaches a central node
    CHECK_FALSE(g.edges[1].central); // 1 -> 2 goes down: never walked
    CHECK_FALSE(g.nodes[2].central);
}
