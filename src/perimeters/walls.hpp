#pragma once

#include "geom/polygons.hpp"
#include "perimeters/sites.hpp"

#include <vector>

namespace tisma {

// The walls of one layer region and what they leave for the infill.
struct LayerWalls {
    std::vector<Toolpath> toolpaths; // variable-width walls (Kuipers et al. 2020), or of one width
    Polygons infill;                 // the part of the region the walls do not cover
    std::vector<Toolpath> gaps;      // gap fill between walls of one width (walls_extra.md, rule 1)
};

// Runs the whole wall chain on a region: medial axis, central marking and its
// filter, transitions, beadings and toolpath extraction. With a wall limit
// (params.wall_count = M > 0) the infill region is the region shrunk by M w*,
// where the shell keeps its M walls at w*, without the parts narrower than
// w*/2: those are where the beads fill the whole thickness (2M beads or
// fewer, or the start of the shell ramp, where the gap is still narrow). In
// the rest of the ramp the infill region overlaps the widened walls by up to
// w*/4 on each side. Without a limit the beads fill the region and nothing is
// left for infill.
LayerWalls layer_walls(const Polygons& region, const BeadingParams& params);

// Whether the walls leave a stretch of the outline bare: a point of the outer
// bead's centre line (the region shrunk by half of `outer`, opened by half a
// bead `width` so that sharp tips, where beads may stop short, are left out)
// farther than three quarters of a bead from every toolpath. The variable
// walls left the face of a T's stem bare from its inner corner (the support
// sweep, 2026-11-16). Own design.
bool walls_leave_gap(const Polygons& region, const std::vector<Toolpath>& toolpaths, double outer, double width);

} // namespace tisma
