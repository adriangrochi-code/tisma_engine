#pragma once

#include "slice/contours.hpp"

namespace tisma::gpu {
class Context;
}

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
// layer_height thick. A layer is created while its middle is below max_z or on
// it, within kOnTop (slicing.md, rule 3: the plane on a flat top slices its
// outline). Throws std::invalid_argument on heights <= 0.
inline constexpr double kOnTop = 1e-6; // mm
std::vector<Layer> plan_layers(double min_z, double max_z, const LayerSettings& settings);

// For each triangle, the levels its value range crosses: [first[t], end[t]).
// A triangle is crossed by level iso when min < iso <= max over its vertices
// (the value >= iso rule of level_set_segments). isos sorted ascending. This
// is the reference for the GPU kernel of the same name.
void triangle_level_ranges(const IndexedMesh& mesh, std::span<const float> values, std::span<const float> isos,
                           std::vector<std::uint32_t>& first, std::vector<std::uint32_t>& end);

// Contours of the level sets values == isos[i] for every i, isos sorted
// ascending. Each triangle is visited only for the levels its value range
// crosses, so the cost grows with the segments produced rather than with
// triangles x levels. Planar slicing passes height_field(mesh) and the layers'
// slice heights; a curved-layer field can be passed the same way.
// With max_gap > 0, gaps up to that length are closed in every level (see
// close_gaps).
std::vector<std::vector<Contour>> slice_levels(const IndexedMesh& mesh, std::span<const float> values,
                                               std::span<const float> isos, double max_gap = 0.0);

// Same, with the level ranges and the segments computed on the GPU when gpu is
// not null. The contours use the same edges in the same order as on the CPU;
// their points agree to a few ULP.
std::vector<std::vector<Contour>> slice_levels(const IndexedMesh& mesh, std::span<const float> values,
                                               std::span<const float> isos, double max_gap, gpu::Context* gpu);

} // namespace tisma
