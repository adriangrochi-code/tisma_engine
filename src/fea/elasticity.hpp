#pragma once

#include "fea/voxels.hpp"

#include <span>
#include <vector>

namespace tisma {

// Linear elasticity on the voxels of a part (PLAN.md, A1). Units: mm, N, MPa.
struct Material {
    double young = 3500.0;  // MPa (a PLA-like default)
    double poisson = 0.35;
};

struct Box {
    double min[3] = {-1e300, -1e300, -1e300};
    double max[3] = {1e300, 1e300, 1e300};
    bool contains(double x, double y, double z) const {
        return x >= min[0] && x <= max[0] && y >= min[1] && y <= max[1] && z >= min[2] && z <= max[2];
    }
};

// The nodes of the part inside the box do not move along the chosen axes.
struct Fixture {
    Box box;
    bool x = true, y = true, z = true;
};

// A total force (N) spread evenly over the part's boundary faces whose centre
// lies inside the box: a uniform traction there.
struct SurfaceLoad {
    Box box;
    double force[3] = {0.0, 0.0, 0.0};
};

struct ElasticityResult {
    std::vector<double> displacement; // 3 per node; node (i, j, k) of the grid's (nx + 1) (ny + 1) (nz + 1)
    std::vector<double> von_mises;    // per voxel, at its centre; 0 for empty voxels
    int iterations = 0;
    double residual = 0.0; // |r| / |f| at the end
    bool converged = false;
    double max_displacement = 0.0;
    double max_von_mises = 0.0;
};

// Solves K u = f by conjugate gradients with a Jacobi preconditioner, without
// assembling K: each product gathers, node by node, the 3 x 3 blocks of the
// elements around it (parallel over nodes, no write conflicts; the same sums
// in the same order on every run). element_scale, when given (one per voxel),
// multiplies each element's stiffness (for topology optimization, A2).
// Textbook method (Shewchuk 1994, "An Introduction to the Conjugate Gradient
// Method Without the Agonizing Pain"; Saad 2003, Ch. 9). Own implementation;
// see PROVENANCE.md.
ElasticityResult solve_elasticity(const VoxelGrid& grid, const Material& material, std::span<const Fixture> fixtures,
                                  std::span<const SurfaceLoad> loads, double tolerance = 1e-8,
                                  int max_iterations = 20000, std::span<const double> element_scale = {});

} // namespace tisma
