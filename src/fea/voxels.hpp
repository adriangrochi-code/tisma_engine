#pragma once

#include "mesh/indexed_mesh.hpp"

#include <cstdint>
#include <vector>

namespace tisma {

// A part as a grid of cubic voxels of side h (mm): voxel (i, j, k) spans
// [x0 + i h, x0 + (i + 1) h] and so on, and is solid when its centre lies
// inside the part. The finite element analysis (PLAN.md, A1) uses each solid
// voxel as one hexahedral element.
struct VoxelGrid {
    double x0 = 0.0, y0 = 0.0, z0 = 0.0;
    double h = 1.0;
    int nx = 0, ny = 0, nz = 0;
    std::vector<std::uint8_t> solid; // nx * ny * nz, x fastest

    std::size_t index(int i, int j, int k) const {
        return (std::size_t(k) * std::size_t(ny) + std::size_t(j)) * std::size_t(nx) + std::size_t(i);
    }
    bool at(int i, int j, int k) const {
        return i >= 0 && j >= 0 && k >= 0 && i < nx && j < ny && k < nz && solid[index(i, j, k)] != 0;
    }
    std::size_t count() const;
};

// Voxels of a closed mesh, from the engine's own slicing: each layer of voxels
// is the mesh sliced at the voxels' mid height, filled along rows (even-odd,
// on the layer's region). Own design; see PROVENANCE.md.
VoxelGrid voxelize(const IndexedMesh& mesh, double h);

} // namespace tisma
