#pragma once

#include "config/settings.hpp"
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
    std::vector<Toolpath> bridge; // bridges: straight lines at the bridge's angle, at 100%
};

struct FillParams {
    double width = 0.4;   // bead width
    double density = 0.2; // sparse infill, 0 to 1
    double angle = 45.0;  // degrees; odd layers add 90 (rectilinear)
    InfillPattern pattern = InfillPattern::Rectilinear;
};

// Infill of layer `layer`. Solid surfaces: rectilinear lines one bead apart,
// fitted to the region, at the layer's angle, connected into zig-zags. Sparse
// infill, by pattern:
//  - rectilinear: lines width / density apart on the grid at the layer's
//    angle, connected into zig-zags;
//  - grid: lines at the angle and at the angle + 90 in every layer, each set
//    2 width / density apart (the same material), not connected: the lines
//    cross each other and their ends touch the walls;
//  - gyroid: the gyroid's section at height z (gyroid_fill), with the period
//    for the density.
LayerFill fill_layer(const LayerSurfaces& surfaces, std::size_t layer, const FillParams& params, double z = 0.0);

} // namespace tisma
