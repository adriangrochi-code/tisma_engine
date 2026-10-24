#include "perimeters/transitions.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <numbers>

using namespace tisma;
using Catch::Matchers::WithinAbs;

namespace {

// Thin isosceles wedge with its tip at the origin, opening along +x, total
// angle `degrees`, length `length` mm. Along its axis the inscribed radius
// grows linearly with x until the far side starts to matter.
Polygon wedge(double degrees, double length) {
    const double h = std::tan(degrees / 2.0 * std::numbers::pi / 180.0) * length;
    return {{to_units(0), to_units(0)}, {to_units(length), to_units(-h)}, {to_units(length), to_units(h)}};
}

} // namespace

TEST_CASE("central bead counts follow the thickness", "[transitions]") {
    MedialGraph g = medial_graph({wedge(30.0, 20.0)});
    mark_central(g);
    BeadingParams p;
    const std::vector<int> counts = central_bead_counts(g, p);
    REQUIRE(counts.size() == g.nodes.size());
    for (std::size_t i = 0; i < g.nodes.size(); ++i) {
        if (!g.nodes[i].central)
            CHECK(counts[i] == -1);
        else
            CHECK(counts[i] == bead_count(p, 2.0 * g.nodes[i].radius));
    }
}

TEST_CASE("transition anchors sit where the thickness crosses (n + 1/2) w*", "[transitions]") {
    MedialGraph g = medial_graph({wedge(30.0, 20.0)});
    mark_central(g);
    BeadingParams p; // distributed, w* = 0.4
    const std::vector<int> counts = central_bead_counts(g, p);
    const auto anchors = transition_anchors(g, counts, p);
    REQUIRE_FALSE(anchors.empty());
    for (const TransitionAnchor& a : anchors) {
        CHECK(a.to_count == a.from_count + 1);
        // The radius at the anchor, interpolated on its edge, is half the transition thickness.
        const MedialGraph::Edge& e = g.edges[a.edge];
        const double r = g.nodes[e.a].radius + a.t * (g.nodes[e.b].radius - g.nodes[e.a].radius);
        CHECK_THAT(2.0 * r, WithinAbs((a.from_count + 0.5) * 0.4, 1e-9));
        CHECK(std::fabs(a.p.y) < 1e-6); // on the wedge's axis
    }
    // Along the axis the anchors for 1->2, 2->3, ... appear in order of x.
    std::vector<double> x_of_count(20, -1.0);
    for (const TransitionAnchor& a : anchors)
        if (a.from_count < 20)
            x_of_count[std::size_t(a.from_count)] = a.p.x;
    for (int n = 1; n + 1 < 20; ++n)
        if (x_of_count[std::size_t(n)] >= 0.0 && x_of_count[std::size_t(n + 1)] >= 0.0)
            CHECK(x_of_count[std::size_t(n)] < x_of_count[std::size_t(n + 1)]);
}

TEST_CASE("uniform scheme anchors jump two beads at a time", "[transitions]") {
    MedialGraph g = medial_graph({wedge(30.0, 20.0)});
    mark_central(g);
    BeadingParams p;
    p.scheme = BeadingScheme::Uniform;
    const auto anchors = transition_anchors(g, central_bead_counts(g, p), p);
    REQUIRE_FALSE(anchors.empty());
    for (const TransitionAnchor& a : anchors)
        CHECK(a.to_count == a.from_count + 2);
}

TEST_CASE("no transitions in a strip of constant width", "[transitions]") {
    const Polygon strip = {{to_units(0), to_units(0)}, {to_units(30), to_units(0)}, {to_units(30), to_units(2)}, {to_units(0), to_units(2)}};
    MedialGraph g = medial_graph({strip});
    mark_central(g);
    BeadingParams p;
    CHECK(transition_anchors(g, central_bead_counts(g, p), p).empty());
}
