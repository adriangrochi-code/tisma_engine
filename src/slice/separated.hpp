#pragma once

#include "geom/medial_axis.hpp"
#include "geom/polygons.hpp"
#include "slice/layers.hpp"

#include <vector>

namespace tisma {

// The infill of each part on its own (orca_2_5.md, rule 12): the parts are
// the pieces of the sliced solid that hold together, an island of a layer
// belonging to the part of every island it overlaps in the layer below. Two
// pieces that only touch side by side within a layer are one island, so one
// part; separate footprints, loose islands of one mesh and interlocked pieces
// that do not touch are parts of their own. Each part's centre is the centre
// of its 3D box.
struct PartIsland {
    Polygons island;
    Point2d centre{};
    double z_centre = 0.0;
};
std::vector<std::vector<PartIsland>> separated_parts(const std::vector<Polygons>& regions, const std::vector<Layer>& layers);

} // namespace tisma
