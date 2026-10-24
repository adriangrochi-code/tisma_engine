#pragma once

#include "mesh/indexed_mesh.hpp"
#include "slice/layers.hpp"

#include <cstdint>
#include <vector>

namespace tisma {

// A part as a grid of box voxels: hx by hy in the plane, and between
// consecutive planes z[k] and z[k + 1] in height, so that the voxels can
// follow the layers (every layer boundary on a plane) and be finer across a
// line (for example four per line width). Voxel (i, j, k) spans
// [x0 + i hx, x0 + (i + 1) hx] x [y0 + j hy, ...] x [z[k], z[k + 1]] and is
// solid when its centre lies inside the part. The finite element analysis
// (PLAN.md, A1) uses each solid voxel as one hexahedral element.
struct VoxelGrid {
    double x0 = 0.0, y0 = 0.0;
    double hx = 1.0, hy = 1.0;
    std::vector<double> z; // nz + 1 planes, ascending
    int nx = 0, ny = 0, nz = 0;
    std::vector<std::uint8_t> solid; // nx * ny * nz, x fastest

    double hz(int k) const { return z[std::size_t(k) + 1] - z[std::size_t(k)]; }
    std::size_t index(int i, int j, int k) const {
        return (std::size_t(k) * std::size_t(ny) + std::size_t(j)) * std::size_t(nx) + std::size_t(i);
    }
    bool at(int i, int j, int k) const {
        return i >= 0 && j >= 0 && k >= 0 && i < nx && j < ny && k < nz && solid[index(i, j, k)] != 0;
    }
    std::size_t count() const;
};

// Planes that split every layer into per_layer equal slabs (the layer
// boundaries included).
std::vector<double> layer_planes(const std::vector<Layer>& layers, int per_layer);

// Voxels of a closed mesh, from the engine's own slicing: each row of voxels
// is the mesh sliced at its mid height, filled along rows (even-odd, on the
// layer's region). Own design; see PROVENANCE.md.
VoxelGrid voxelize(const IndexedMesh& mesh, double hx, double hy, std::vector<double> z);
// Cubes of side h from the bottom of the mesh up.
VoxelGrid voxelize(const IndexedMesh& mesh, double h);

} // namespace tisma
