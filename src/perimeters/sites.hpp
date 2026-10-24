#pragma once

#include "geom/medial_axis.hpp"
#include "perimeters/beading.hpp"

#include <cstdint>
#include <vector>

namespace tisma {

// The beading every node of a marked medial graph uses (Kuipers et al. 2020,
// Section 3.5): central nodes from their bead count (integer, or fractional in
// a ramp) and their thickness 2R; any other node takes the beading of the
// central node at the top of its upward ramp, found by climbing to the
// neighbour with the largest radius. A node that climbs to a peak with no
// central node uses the beading of that peak's own thickness.
std::vector<HalfBeading> node_beadings(const MedialGraph& graph, const std::vector<double>& bead_counts,
                                       const BeadingParams& params);

// A toolpath site (Section 3.6): where bead `index` (counted from the outline)
// passes, and how wide it is there.
struct Site {
    Point2d p;
    double width;
    int index;
    std::uint32_t edge; // medial edge whose trapezoid side holds the site
    int side;           // 0 or 1: which of the edge's two trapezoids
    bool at_b;          // on the side from node b to its support (else from node a)
};

// Sites on the trapezoid sides that run from each node to its support: along
// such a side the distance to the outline grows linearly from 0 at the support
// to R at the node, so bead i sits at the fraction l_i / R of it. Beads with
// l_i > R do not reach this node. A bead exactly at R (the middle bead of an
// odd count) sits on the node itself and appears on both sides.
std::vector<Site> support_sites(const MedialGraph& graph, const std::vector<HalfBeading>& beadings);

} // namespace tisma
