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

} // namespace tisma
