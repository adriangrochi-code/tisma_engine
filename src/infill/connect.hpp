#pragma once

#include "geom/polygons.hpp"

namespace tisma {

// Joins open pieces (infill lines or curves) into longer polylines along the
// boundary of the region they were clipped to, so that their ends are anchored
// and the edge band between them is filled: two piece ends that follow each
// other along a boundary polygon, at most max_arc apart along it (in units),
// are joined by the stretch of boundary between them. Joins never close a
// cycle. With row_step > 0 the pieces are horizontal lines that far apart and
// only pieces on adjacent lines are joined (a zig-zag); with 0, any two.
Polygons join_along_boundary(const Polygons& pieces, const Polygons& boundary, double max_arc, double row_step = 0.0);

} // namespace tisma
