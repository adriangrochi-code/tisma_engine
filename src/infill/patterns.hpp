#pragma once

#include "geom/polygons.hpp"
#include "perimeters/sites.hpp"

#include <vector>

namespace tisma {

// More sparse infill patterns of docs/spec/features/infill_patterns.md. Each
// stays inside the region shrunk by width / 2, like rectilinear_fill.

// Lines `spacing` mm apart at `angle_degrees`, every other one turned by
// `tilt_degrees` about its point on the region's middle across the lines
// (rule 3), joined along the boundary into zig-zags.
std::vector<Toolpath> line_fill(const Polygons& region, double spacing, double angle_degrees, double tilt_degrees,
                                double width);

// A honeycomb of hexagons of side `side` mm centred on the origin, with sides
// at `principal_degrees`, +60 and +120 (rule 10): wavy lines along the
// principal direction, half of each along it, every two neighbours sharing
// their stretches along it, joined along the boundary. The same hexagons for
// any principal direction 60 degrees apart, so the layers line up.
std::vector<Toolpath> honeycomb_fill(const Polygons& region, double side, double principal_degrees, double width);

// The hexagon side whose honeycomb lays `density` of the area at `width`
// counting each wall once (the walls along the principal direction are laid
// twice, so about 4/3 of that), as Tisma Slicer measures (P8).
double honeycomb_side(double width, double density);

// Closed loops parallel to the region's boundary, the first half a bead in
// and then `spacing` mm apart inwards, outside first (rule 13).
std::vector<Toolpath> concentric_fill(const Polygons& region, double spacing, double width);

// The curves of rule 12, the same in every layer, clipped to the region and
// joined along its boundary:
//  - hilbert_fill: a Hilbert curve (Hilbert 1891) through cells `spacing` mm
//    wide, on a grid around the origin turned by `angle_degrees`;
//  - archimedean_fill: chords of an Archimedean spiral r = a phi around
//    `centre`, its turns `spacing` mm apart;
//  - octagram_fill: a spiral around `centre` with the outline of two squares
//    turned 45 degrees from each other, an eight-pointed star whose sides run
//    at 0, 45, 90 and 135 degrees, each side `spacing` mm from the same side
//    a turn before.
std::vector<Toolpath> hilbert_fill(const Polygons& region, double spacing, double angle_degrees, double width);
std::vector<Toolpath> archimedean_fill(const Polygons& region, double spacing, Point2d centre, double width);
std::vector<Toolpath> octagram_fill(const Polygons& region, double spacing, Point2d centre, double width);

} // namespace tisma
