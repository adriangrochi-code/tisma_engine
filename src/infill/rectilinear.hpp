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
// each line.
std::vector<Toolpath> rectilinear_fill(const Polygons& region, double spacing, double angle_degrees, double width);

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
// apart, sparse infill with lines width / density apart, both at the layer's
// angle.
LayerFill fill_layer(const LayerSurfaces& surfaces, std::size_t layer, const FillParams& params);

} // namespace tisma
