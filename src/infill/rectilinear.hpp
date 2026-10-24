#pragma once

#include "geom/polygons.hpp"
#include "perimeters/sites.hpp"
#include "surfaces/surfaces.hpp"

#include <vector>

namespace tisma {

// Parallel lines `spacing` mm apart at `angle_degrees` from the X axis, on a
// grid fixed to the origin so that the lines of different layers line up,
// clipped to the region shrunk by width / 2 (the beads stay inside). Each piece
// is an open toolpath of constant width, ordered across the lines and along
// each line. With `connect`, pieces on adjacent lines whose ends follow each
// other along the boundary, at most kMaxJoin spacings apart along it, are
// joined by that stretch of boundary into zig-zag polylines: the line ends are
// anchored and part of the edge band is filled, without entering the walls.
// With `fit`, the lines are not on the grid: as many as fit across the region
// (per connected piece of the whole region, measured across its extent), centred,
// so the outer ones run along the shrunk boundary; for solid surfaces.
inline constexpr double kMaxJoin = 3.0;
std::vector<Toolpath> rectilinear_fill(const Polygons& region, double spacing, double angle_degrees, double width,
                                       bool connect = false, bool fit = false);

// The infill toolpaths of one layer.
struct LayerFill {
    std::vector<Toolpath> solid;  // top and bottom surfaces, at 100%
    std::vector<Toolpath> sparse; // the rest, at the infill density
};

struct FillParams {
    double width = 0.4;   // bead width
    double density = 0.2; // sparse infill, 0 to 1
    double angle = 45.0;  // degrees; odd layers add 90
};

// Rectilinear infill of layer `layer`: solid surfaces with lines one bead
// apart fitted to the region, sparse infill with lines width / density apart
// on the grid, both at the layer's angle and connected into zig-zags.
LayerFill fill_layer(const LayerSurfaces& surfaces, std::size_t layer, const FillParams& params);

} // namespace tisma
