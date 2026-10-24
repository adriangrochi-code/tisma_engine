#pragma once

#include "config/settings.hpp"
#include "geom/polygons.hpp"
#include "slice/layers.hpp"

#include <vector>

namespace tisma {

namespace gpu {
class Context;
}

// The planar slicing of a model: one region per layer.
struct SlicedModel {
    std::vector<Layer> layers;
    std::vector<Polygons> regions; // one per layer, after fill mode and offsets
    std::size_t open_contours = 0; // contours that stayed open after gap closing (dropped)
};

// Slices a welded mesh with the given settings: layer plan, level-set
// contours with gap closing, simplification, regions by fill mode, XY offset,
// and the extra first-layer shrink. Uses the GPU when gpu is not null.
SlicedModel slice_model(const IndexedMesh& mesh, const Settings& settings, gpu::Context* gpu = nullptr);

} // namespace tisma
