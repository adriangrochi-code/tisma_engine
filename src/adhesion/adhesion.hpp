#pragma once

#include "geom/polygons.hpp"
#include "perimeters/sites.hpp"

#include <vector>

namespace tisma {

// Skirt and brim around the first layer (supports_skirt.md, rules 8 and 9).
// Own design with Clipper2 offsets of the outer contours of the layer's region
// (holes are left alone), with rounded corners: each loop is the part grown.
// Loops lie at the beads' spacing (loop_spacing), so neighbours overlap as
// printed beads do:
// - brim: floor(width / spacing) loops, the first touching the part, the
//   outer one reaching the width asked; loops of nearby parts merge where they
//   meet;
// - skirt: `loops` loops, the first `distance` mm away from the part (or from
//   its brim), outer contours only (no loops in the gaps between parts).
// Loop k (0 the innermost) has index -1 - k, apart from the walls' indices.
// `height` is the first layer's.
std::vector<Toolpath> brim_loops(const Polygons& region, double width, double bead, double height);
std::vector<Toolpath> skirt_loops(const Polygons& region, double brim_width, int loops, double distance,
                                  double bead, double height);

// The distance between the axes of two beads side by side that fill the gap
// between them: a bead's section is a rectangle with half-circle ends
// (Comminal et al. 2018), of area w h - (1 - pi / 4) h^2, so the spacing is
// w - (1 - pi / 4) h.
double loop_spacing(double bead, double height);

} // namespace tisma
