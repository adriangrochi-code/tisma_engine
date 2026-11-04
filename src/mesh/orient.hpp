#pragma once

// Automatic orientation, «menos material de soporte» for FDM
// (docs/spec/features/orientation.md, rules 1 to 6): the part rests on each
// face of its convex hull in turn (and as it is), each candidate costs the
// support it would need minus its grip on the bed, and the cheapest wins;
// then it is turned about Z so its smallest box in XY lies landscape. Own
// design over the spec; see PROVENANCE.md.

#include "mesh/mesh.hpp"

#include <array>
#include <cstddef>

namespace tisma {

struct OrientOptions {
    // Faces looking down at less than this from the horizontal need support
    // (support.angle_limit; 45 when automatic).
    double support_angle = 45.0;
};

struct OrientResult {
    // Row-major 3 x 3 rotation to apply to the part (about its own origin;
    // the caller lays it on the bed again).
    std::array<double, 9> rotation{1, 0, 0, 0, 1, 0, 0, 0, 1};
    double cost = 0.0;        // mm3 of support minus the bed contact x 1 mm
    double cost_before = 0.0; // the same for the orientation it had
    double support_mm3 = 0.0, bed_area_mm2 = 0.0;
    std::size_t candidates = 0;
};

// The candidate orientation that needs least support (rule 4). An empty mesh
// gives the identity.
OrientResult orient_least_support(const TriangleMesh& mesh, const OrientOptions& options = {});

// The cost of resting the part with `up` (a unit vector in the part's own
// coordinates) pointing up: rule 4's support volume and bed area.
struct OrientCost {
    double cost = 0.0, support_mm3 = 0.0, bed_area_mm2 = 0.0, height = 0.0;
};
OrientCost orientation_cost(const TriangleMesh& mesh, const std::array<double, 3>& up, const OrientOptions& options = {});

} // namespace tisma
