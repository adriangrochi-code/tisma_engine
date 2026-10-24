#pragma once

#include "fea/voxels.hpp"

#include <span>
#include <vector>

namespace tisma {

struct SlicedModel;

// The direction of the printed line through every voxel, from the sliced
// model's toolpaths (walls, solid, top, sparse and bridge infill, gap fill)
// in the layer that holds the voxel's centre: the line closest to the centre
// among those whose half width reaches it. Radians from x in [0, pi); NaN
// where no line passes. Own design; see PROVENANCE.md.
std::vector<float> line_directions(const VoxelGrid& grid, const SlicedModel& model);

// The part as printed: only the voxels that a line passes through, so the
// gaps of sparse infill are empty. Needs voxels finer than the lines (four
// per line width for the high quality analysis).
VoxelGrid printed_voxels(VoxelGrid grid, std::span<const float> direction);

} // namespace tisma
