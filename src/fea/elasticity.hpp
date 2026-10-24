#pragma once

#include "fea/material.hpp"
#include "fea/voxels.hpp"

#include <cstdint>
#include <limits>
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

// The part as printed: every voxel is the orthotropic material turned to the
// direction of the line that fills it (PLAN.md, A1: adhesion between layers
// and between lines). The safety factor is Tsai-Wu's in the line's axes.
struct AsPrintedResult {
    ElasticityResult elasticity;   // von Mises of the stresses in global axes
    std::vector<double> safety;    // per voxel; +infinity for empty or unstressed voxels
    std::vector<FailureMode> mode; // per voxel: what gives way first at the safety factor
    double min_safety = std::numeric_limits<double>::infinity();
    std::size_t weakest = 0; // the voxel with min_safety
};

// angle: per voxel, the line's direction in the layer (radians from x, any
// turn); NaN, or an empty span, where it is unknown: those voxels take the
// stiffness averaged over every direction and the weakest direction for the
// safety factor. Directions are rounded to kDirections steps over half a
// turn, so the solver needs only a few element matrices.
inline constexpr int kDirections = 36; // over half a turn: steps of 5 degrees
AsPrintedResult solve_as_printed(const VoxelGrid& grid, const OrthotropicMaterial& material,
                                 std::span<const float> angle, std::span<const Fixture> fixtures,
                                 std::span<const SurfaceLoad> loads, double tolerance = 1e-8,
                                 int max_iterations = 20000, std::span<const double> element_scale = {});

} // namespace tisma
