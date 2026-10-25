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
// central node uses the beading of that peak's own thickness. Beading
// conflicts (Fig. 9): on an unmarked bone that rises from a central node, the
// beading is blended from that node's into the top's over w* of path length.
std::vector<HalfBeading> node_beadings(const MedialGraph& graph, const std::vector<double>& bead_counts,
                                       const BeadingParams& params);

// A toolpath site (Section 3.6): where bead `index` (counted from the outline)
// passes, and how wide it is there.
struct Site {
    enum class Where { SideA, SideB, Medial };
    Point2d p;
    double width;
    int index;
    std::uint32_t edge; // medial edge of the trapezoid that holds the site
    int side;           // 0 or 1: which of the edge's two trapezoids (Medial sites belong to both)
    Where where;        // node a to its support, node b to its support, or on the medial edge itself
    bool on_node;       // at the node itself (a bead at distance R from the outline)
};

// A bead within this distance of a node's radius R sits on the node.
inline constexpr double kOnNode = 1e-9; // mm

// Sites on the trapezoid sides that run from each node to its support: along
// such a side the distance to the outline grows linearly from 0 at the support
// to R at the node, so bead i sits at the fraction l_i / R of it. Beads with
// l_i > R do not reach this node. A bead exactly at R (the middle bead of an
// odd count) sits on the node itself and appears on both sides.
std::vector<Site> support_sites(const MedialGraph& graph, const std::vector<HalfBeading>& beadings);

// Sites on the non-central medial edges (Section 3.6): on an edge whose
// radius goes from R_low to R_high, bead i of the upper node's beading crosses
// it where R = l_i, for R_low < l_i < R_high (a bead at either radius is a
// site of that node). No sites go on central edges.
std::vector<Site> medial_sites(const MedialGraph& graph, const std::vector<HalfBeading>& beadings);

// A variable-width toolpath: points with the bead width at each.
struct ToolpathPoint {
    Point2d p;
    double width;
    double dz = 0.0; // mm from the layer's top: Z anti-aliasing moves the nozzle to the surface (docs/ZAA.md)
    double below = 0.0; // mm from the layer below's top: where the bead below lies (wavy infill)
};
struct Toolpath {
    std::vector<ToolpathPoint> points;
    bool closed = false;
    int index = 0;      // bead index, counted from the outline
    bool wavy = false;  // nonplanar infill (docs/NONPLANAR_INFILL.md): each bead fills the gap down to the one below
};

// Connects the sites of each bead index inside every trapezoid into extrusion
// segments and chains the segments into polylines and loops (Section 3.6).
// Where a trapezoid has a site of some index on only one side (a transition)
// that site stays unconnected. The middle bead of an odd count lies on the
// medial edge, so of the two trapezoids of that edge only side 0 emits it.
// Segments are chained through the sites they share (the same rib, node or
// medial edge, and the same bead index), never by comparing positions; a site
// where more than two segments meet ends the polylines there. A bead that sits
// on a node and reaches no other node (the middle bead of an odd count at an
// isolated maximum) becomes a toolpath of one point.
std::vector<Toolpath> extract_toolpaths(const MedialGraph& graph, const std::vector<HalfBeading>& beadings);

} // namespace tisma
