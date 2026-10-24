#pragma once

#include "geom/medial_axis.hpp"
#include "perimeters/beading.hpp"

#include <cstdint>
#include <vector>

namespace tisma {

// Bead count of every central node of a marked medial graph: q(2R) (Kuipers
// et al. 2020, Section 3.4, Eq. 4). Nodes outside the centre get -1.
std::vector<int> central_bead_counts(const MedialGraph& graph, const BeadingParams& params);

// Where along a central edge the bead count changes from `from_count` to
// `to_count` (Section 3.4, Eqs. 5 and 6): at the point whose thickness 2R
// equals the transition thickness, R interpolated linearly along the edge.
struct TransitionAnchor {
    std::uint32_t edge;   // index in graph.edges
    double t;             // 0 at edge.a, 1 at edge.b
    Point2d p;            // mm
    int from_count;       // bead count on the thinner side
    int to_count;         // bead count on the thicker side
};

// All transition anchors on central edges whose two ends have different bead
// counts, ordered by edge and then by position along it.
std::vector<TransitionAnchor> transition_anchors(const MedialGraph& graph, const std::vector<int>& counts,
                                                 const BeadingParams& params);

struct TransitionSettings {
    // Two consecutive anchors in opposite directions closer than this along the
    // centre are both dropped (Section 3.4: d_max^transition = 1 mm).
    double filter_distance = 1.0;
    // Ramp length t(n); a negative value means the preferred width w*
    // (Section 3.4: t(n) = w*).
    double transition_length = -1.0;
};

struct TransitionResult {
    // Bead count per node of the (modified) graph: integer counts on the centre,
    // fractional ones inside ramps (Eq. 7), -1 off the centre.
    std::vector<double> bead_counts;
    std::size_t anchors = 0;          // anchors found
    std::size_t anchors_filtered = 0; // dropped by the opposite-direction filter
    std::size_t anchors_dropped = 0;  // dropped because the ramp does not fit in the centre
};

// Section 3.4 on a marked graph: quantized counts on the centre, anchors,
// the filter, and ramps of length t(n) that start t(n) (q^-1(n) / w* - n)
// before the anchor on the thinner side. Ramp ends become new nodes (edges are
// split there) and nodes inside a ramp get n + distance / t(n). Works along
// chains of central edges between nodes where the centre branches or ends.
TransitionResult apply_transitions(MedialGraph& graph, const BeadingParams& params,
                                   const TransitionSettings& settings = {});

} // namespace tisma
