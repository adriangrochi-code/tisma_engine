#pragma once

#include "geom/polygons.hpp"
#include "perimeters/sites.hpp"

#include <vector>

namespace tisma {

// The area a set of variable-width toolpaths covers, as polygons: each piece
// between two points is the quadrilateral of half widths w0/2 and w1/2 across
// it, and each point adds a disc of radius w/2 (round joins and caps), as a
// polygon of kCoverageDiscSides sides.
inline constexpr int kCoverageDiscSides = 64;
Polygons bead_area(const std::vector<Toolpath>& toolpaths);

// How well toolpaths fill a target area (the F2 criterion: no gaps, no excess).
// All areas in mm^2. The overlap counts material laid twice within one
// toolpath too, such as the inner side of a sharp corner or a zig-zag's
// connection over its own line ends; the open ends' half discs count as laid.
struct Coverage {
    double target = 0.0;  // area of the target
    double covered = 0.0; // target covered by beads
    double gaps = 0.0;    // target not covered
    double outside = 0.0;   // beads outside the target
    double deposited = 0.0; // material laid down: length times mean width of every piece, plus round ends
    double overlap = 0.0;   // deposited minus the area of the union: material laid twice
};
Coverage measure_coverage(const Polygons& target, const std::vector<Toolpath>& toolpaths);

} // namespace tisma
