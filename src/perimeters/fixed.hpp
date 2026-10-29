#pragma once

#include "perimeters/walls.hpp"

namespace tisma {

// Walls of one width (walls_extra.md, rule 1; walls.generator "fixed"): up to
// `count` loops `width` apart, each where a whole bead fits (the region shrunk
// to the loop's centre, opened by half a bead), counter-clockwise, numbered
// from the outline. The infill region is the region shrunk by count widths.
// With `gap_fill`, what neither the loops nor the infill region cover, where
// it is at least half a bead thick, gets variable-width beads (Kuipers et al.
// 2020, as the solid surfaces' gaps) in `gaps`: the middle of a wall too thin
// for another loop. Own design.
// The outer loop is `outer` wide (walls.outer_width); 0: `width`. Neighbours
// overlap by `overlap` (bead_overlap, perimeters.md rule 1), the outer bead's
// edge on the outline.
LayerWalls fixed_walls(const Polygons& region, double width, int count, bool gap_fill, double outer = 0.0,
                       double overlap = 0.0);

} // namespace tisma
