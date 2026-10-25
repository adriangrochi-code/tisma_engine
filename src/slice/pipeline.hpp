#pragma once

#include "config/settings.hpp"
#include "geom/polygons.hpp"
#include "perimeters/walls.hpp"
#include "infill/rectilinear.hpp"
#include "surfaces/surfaces.hpp"
#include "slice/layers.hpp"
#include "slice/local_layers.hpp"
#include "slice/variable_layers.hpp"
#include "slice/zaa.hpp"

#include <vector>

namespace tisma {

namespace gpu {
class Context;
}

// The planar slicing of a model: one region per layer, with its walls.
struct SlicedModel {
    std::vector<Layer> layers;
    std::vector<Polygons> regions; // one per layer, after fill mode and offsets
    std::vector<LayerWalls> walls; // one per layer: wall toolpaths and the region left for infill
    std::vector<LayerSurfaces> surfaces; // one per layer: the infill region as top, bottom and sparse
    std::vector<LayerFill> fills;        // one per layer: solid and sparse infill toolpaths
    std::vector<Toolpath> skirt; // around the first layer, on the first adhesion.skirt_layers layers
    std::vector<Toolpath> brim;  // around the first layer, on the first layer
    std::vector<Polygons> support_areas; // one per layer (empty unless support.enabled): where support goes
    std::size_t open_contours = 0; // contours that stayed open after gap closing (dropped)
    std::vector<std::size_t> open_by_layer; // the same per layer
    std::vector<double> thin_area; // per layer, mm2 of the region thinner than the thinnest bead (not printed)
    // Layer heights by zone (docs/LOCAL_LAYERS.md): the layers above are the
    // fine ones; each group of them prints its thick zone once, at its top.
    std::vector<LayerGroup> groups;
    std::vector<ThickLayer> thick; // one per layer, empty but at a group's top
    std::size_t zaa_points = 0;    // points Z anti-aliasing moved off their layer's top (docs/ZAA.md)
};

// The mesh as the compensation settings ask (docs/CALIBRATION.md): scaled by
// compensation.scale_x, _y and _z around the centre of its footprint and its
// bottom, so it stays where it was on the bed. The mesh itself when the
// scales are all 100 %.
IndexedMesh compensated(const IndexedMesh& mesh, const Settings& settings);

// Slices a welded mesh with the given settings: layer plan, level-set
// contours with gap closing, simplification, regions by fill mode, XY offset,
// the extra first-layer shrink, the walls of every region, the top, bottom
// and sparse parts of what the walls leave, and their infill toolpaths (all
// layers in parallel). The mesh is compensated first, and the holes of every
// region offset by compensation.hole_offset. Uses the GPU when gpu is not null.
SlicedModel slice_model(const IndexedMesh& mesh, const Settings& settings, gpu::Context* gpu = nullptr);

} // namespace tisma
