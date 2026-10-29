#pragma once

#include "fea/hex8.hpp"
#include "fea/material.hpp"
#include "fea/voxels.hpp"

#include <cstdint>
#include <span>
#include <vector>

namespace tisma {

// A deterministic parallel dot product: the same blocks summed in the same
// order on every run.
double deterministic_dot(const std::vector<double>& a, const std::vector<double>& b);

// The stiffness matrix K of the voxels, never assembled. The elements'
// stiffness: one per distinct row height (layers are 0.2 mm, the first one
// maybe thicker) and kind of material (an elasticity matrix D with its
// modulus, for example the printed material at one line direction), each as
// its 8 x 8 blocks of 3 x 3; every voxel says which one it uses and scales it.
// Degree of freedom 3 n + d is node n's displacement along axis d; node
// (i, j, k) of the (nx + 1) (ny + 1) (nz + 1) grid nodes is n = node(i, j, k).
class VoxelOperator {
public:
    // kinds: the distinct elasticity matrices; kind_of: per voxel, which one
    // (empty: all the first); scale: per voxel, a factor (empty: 1).
    VoxelOperator(const VoxelGrid& grid, std::span<const Stiffness> kinds, std::span<const std::uint16_t> kind_of,
                  std::span<const double> scale);

    struct Blocks {
        double k[8][8][3][3]; // block (a, b): corner a's forces from corner b's displacement
    };

    const VoxelGrid& grid() const { return g_; }
    std::size_t nodes() const { return std::size_t(nx_) * std::size_t(ny_) * std::size_t(nz_); }
    std::size_t node(int i, int j, int k) const {
        return (std::size_t(k) * std::size_t(ny_) + std::size_t(j)) * std::size_t(nx_) + std::size_t(i);
    }
    // The voxel (i, j, k), or the grid's size outside it.
    std::size_t voxel(int i, int j, int k) const {
        return i >= 0 && j >= 0 && k >= 0 && i < g_.nx && j < g_.ny && k < g_.nz ? g_.index(i, j, k) : g_.solid.size();
    }
    // A voxel's stiffness factor: 0 for empty voxels and outside the grid.
    double scale(std::size_t v) const { return v < scale_.size() ? scale_[v] : 0.0; }
    // A voxel's element stiffness, before its scale.
    const Blocks& blocks(std::size_t v) const { return blocks_[block_of_[v]]; }

    // y = K x, node by node: node n at local corner a of each of its (up to)
    // eight elements gets block (a, b) times the displacement of corner b.
    void apply(const std::vector<double>& x, std::vector<double>& y) const;
    // The diagonal of K.
    std::vector<double> diagonal() const;
    // The stress at the centre of voxel (i, j, k), in global axes.
    void stress(const std::vector<double>& u, int i, int j, int k, double out[6]) const;

    // Nodes along each axis.
    int nx() const { return nx_; }
    int ny() const { return ny_; }
    int nz() const { return nz_; }

private:
    const VoxelGrid& g_;
    int nx_, ny_, nz_;
    std::vector<Blocks> blocks_;
    std::vector<Hex8> elements_;          // the same order as blocks_
    std::vector<std::uint16_t> block_of_; // per voxel
    std::vector<double> scale_;           // per voxel; 0 for empty voxels
};

} // namespace tisma
