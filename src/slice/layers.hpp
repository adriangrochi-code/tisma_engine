#pragma once

#include "slice/contours.hpp"

#include <span>
#include <vector>

namespace tisma {

struct LayerSettings {
    double first_layer_height = 0.2; // mm
    double layer_height = 0.2;       // mm
};

// One planar layer between bottom and top. It is sliced at its middle height.
struct Layer {
    double bottom = 0.0;
    double top = 0.0;
    double slice_z() const { return 0.5 * (bottom + top); }
};

// Layers from min_z upwards: the first one first_layer_height thick, the rest
// layer_height thick. A layer whose middle would be at or above max_z holds no
// material and is not created. Throws std::invalid_argument on heights <= 0.
std::vector<Layer> plan_layers(double min_z, double max_z, const LayerSettings& settings);

// Contours of the level sets values == isos[i] for every i, isos sorted
// ascending. Each triangle is visited only for the levels its value range
// crosses, so the cost grows with the segments produced rather than with
// triangles x levels. Planar slicing passes height_field(mesh) and the layers'
// slice heights; a curved-layer field can be passed the same way.
std::vector<std::vector<Contour>> slice_levels(const IndexedMesh& mesh, std::span<const float> values,
                                               std::span<const float> isos);

} // namespace tisma
