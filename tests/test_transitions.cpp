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

TEST_CASE("ramps of length w* around each anchor with fractional counts", "[transitions]") {
    MedialGraph g = medial_graph({wedge(30.0, 20.0)});
    mark_central(g);
    BeadingParams p; // distributed: q^-1(n) = (n + 1/2) w*, so ramps are centred on the anchors
    const auto anchors = transition_anchors(g, central_bead_counts(g, p), p);
    const TransitionResult r = apply_transitions(g, p);
    REQUIRE(r.bead_counts.size() == g.nodes.size());
    CHECK(r.anchors == anchors.size());
    CHECK(r.anchors_filtered == 0); // the wedge only thickens
    // Each kept anchor has nodes at both ramp ends, w*/2 before and after it on the axis,
    // with the integer counts, and fractional counts in between.
    std::size_t ramps_checked = 0;
    for (const TransitionAnchor& a : anchors) {
        bool start = false, end = false;
        for (std::size_t i = 0; i < g.nodes.size(); ++i) {
            if (std::fabs(g.nodes[i].p.y) > 1e-6)
                continue;
            if (std::fabs(g.nodes[i].p.x - (a.p.x - 0.2)) < 1e-6 && r.bead_counts[i] == a.from_count)
                start = true;
            if (std::fabs(g.nodes[i].p.x - (a.p.x + 0.2)) < 1e-6 && r.bead_counts[i] == a.to_count)
                end = true;
        }
        if (start && end)
            ++ramps_checked;
    }
    CHECK(ramps_checked + r.anchors_dropped == anchors.size());
    CHECK(ramps_checked > 0);
    for (std::size_t i = 0; i < g.nodes.size(); ++i)
        if (g.nodes[i].central)
            CHECK(r.bead_counts[i] >= 0.0);
    // Nodes added at the ramp ends keep supports at their radius (the wedge's axis is straight).
    for (const MedialGraph::Edge& e : g.edges)
        for (int k = 0; k < 2; ++k) {
            const MedialGraph::Node& a = g.nodes[e.a];
            const MedialGraph::Node& b = g.nodes[e.b];
            CHECK_THAT(std::hypot(a.p.x - e.support_a[k].x, a.p.y - e.support_a[k].y), WithinAbs(a.radius, 1e-6));
            CHECK_THAT(std::hypot(b.p.x - e.support_b[k].x, b.p.y - e.support_b[k].y), WithinAbs(b.radius, 1e-6));
        }
}

TEST_CASE("opposite anchors closer than 1 mm cancel out", "[transitions]") {
    // A strip 0.98 mm wide with a short bulge to 1.02 mm: with w* = 0.4 the count
    // goes 2 -> 3 -> 2 across the bulge (the step is at 1.0 mm), over less than 1 mm.
    const auto u = [](double v) { return to_units(v); };
    const Polygon strip = {{u(0), u(0)},       {u(10), u(0)},      {u(10.2), u(-0.02)}, {u(10.6), u(-0.02)},
                           {u(10.8), u(0)},    {u(20), u(0)},      {u(20), u(0.98)},    {u(10.8), u(0.98)},
                           {u(10.6), u(1.0)},  {u(10.2), u(1.0)},  {u(10), u(0.98)},    {u(0), u(0.98)}};
    MedialGraph g = medial_graph({strip});
    mark_central(g);
    BeadingParams p;
    const TransitionResult r = apply_transitions(g, p);
    CHECK(r.anchors_filtered >= 2);
    for (std::size_t i = 0; i < g.nodes.size(); ++i)
        if (g.nodes[i].central && r.bead_counts[i] >= 0.0)
            CHECK(r.bead_counts[i] == 2.0); // the bulge does not get a third bead
}
