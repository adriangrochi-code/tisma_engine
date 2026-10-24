#include "perimeters/sites.hpp"
#include "perimeters/transitions.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <numbers>
#include <set>

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
