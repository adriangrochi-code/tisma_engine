#pragma once

#include "geom/polygons.hpp"
#include "perimeters/sites.hpp"

#include <vector>

namespace tisma {

// Skirt and brim around the first layer (PLAN.md, F5). Own design with
// Clipper2 offsets of the outer contours of the layer's region (holes are left
// alone):
// - brim: floor(width / bead) loops of one bead, the first touching the part,
//   each the next bead further out; loops of nearby parts merge where they meet;
// - skirt: `loops` loops, the first `distance` mm away from the part (or from
//   its brim), outer contours only (no loops in the gaps between parts).
// Loop k (0 the innermost) has index -1 - k, apart from the walls' indices.
std::vector<Toolpath> brim_loops(const Polygons& region, double width, double bead);
std::vector<Toolpath> skirt_loops(const Polygons& region, double brim_width, int loops, double distance,
                                  double bead);

} // namespace tisma
