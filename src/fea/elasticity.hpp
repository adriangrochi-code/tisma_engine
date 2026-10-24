#pragma once

#include "fea/material.hpp"
#include "fea/voxels.hpp"

#include <array>
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

// How to solve: conjugate gradients until |r| / |f| < tolerance, with a
// multigrid V-cycle as the preconditioner (fast at every voxel size) or
// Jacobi's (the simple reference).
enum class Preconditioner : std::uint8_t { Multigrid, Jacobi };
struct SolverOptions {
    double tolerance = 1e-8;
    int max_iterations = 20000;
    Preconditioner preconditioner = Preconditioner::Multigrid;
};

struct ElasticityResult {
    std::vector<double> displacement; // 3 per node; node (i, j, k) of the grid's (nx + 1) (ny + 1) (nz + 1)
    std::vector<double> von_mises;    // per voxel, at its centre; 0 for empty voxels
    int iterations = 0;
    int levels = 1; // multigrid levels (1 with Jacobi)
    // Voxels left out because no fixture holds them (joined by faces to a
    // voxel with a corner in a fixture's box), and loads that found no
    // boundary face of the held part in their box: both mean the setup is
    // probably wrong.
    std::size_t loose_voxels = 0;
    int unapplied_loads = 0;
    double residual = 0.0; // |r| / |f| at the end
    bool converged = false;
    double max_displacement = 0.0;
    double max_von_mises = 0.0;
};

// The part without the pieces that no fixture holds: voxels joined by faces
// to a voxel with a corner in a fixture's box that fixes some axis. A loose
// piece moves freely (its displacement is undetermined), so the solvers below
// leave it out; loose counts its voxels.
VoxelGrid held_voxels(const VoxelGrid& grid, std::span<const Fixture> fixtures, std::size_t& loose);

// Solves K u = f by preconditioned conjugate gradients without assembling K:
// each product gathers, node by node, the 3 x 3 blocks of the elements around
// it (parallel over nodes, no write conflicts; the same sums in the same
// order on every run). The preconditioner is a multigrid V-cycle
// (fea/multigrid.hpp) or Jacobi's. element_scale, when given (one per voxel),
// multiplies each element's stiffness (for topology optimization, A2).
// Textbook method (Shewchuk 1994, "An Introduction to the Conjugate Gradient
// Method Without the Agonizing Pain"; Saad 2003, Ch. 9). Own implementation;
// see PROVENANCE.md.
ElasticityResult solve_elasticity(const VoxelGrid& grid, const Material& material, std::span<const Fixture> fixtures,
                                  std::span<const SurfaceLoad> loads, const SolverOptions& options = {},
                                  std::span<const double> element_scale = {});

// Where the safety factor averages the stress (see solve_as_printed): x and y
// in mm (0: the voxel alone), and the layers' rows in z (layer_planes splits
// every layer into rows_per_layer rows; 0 or 1: the voxel's own row).
struct StressWindow {
    double x = 0.0, y = 0.0;
    int rows_per_layer = 0;
};

// The part as printed: every voxel is the orthotropic material turned to the
// direction of the line that fills it (PLAN.md, A1: adhesion between layers
// and between lines). The safety factor is Tsai-Wu's in the line's axes.
struct AsPrintedResult {
    ElasticityResult elasticity; // von Mises of the stresses in global axes
    // Per voxel, from its stress averaged over the window around it (the
    // voxel's own without a window); +infinity for empty or unstressed voxels.
    std::vector<double> safety;
    std::vector<FailureMode> mode; // per voxel: what gives way first at that safety factor
    double min_safety = std::numeric_limits<double>::infinity();
    std::size_t weakest = 0; // the voxel with min_safety
    // From each voxel's own stress. Where linear elasticity has no finite
    // stress (the edge of a clamp, a sharp inner corner) this keeps falling
    // as the voxels shrink; the averaged safety does not.
    std::vector<double> peak_safety;
    double min_peak_safety = std::numeric_limits<double>::infinity();
};

// angle: per voxel, the line's direction in the layer (radians from x, any
// turn); NaN, or an empty span, where it is unknown: those voxels take the
// stiffness averaged over every direction and the weakest direction for the
// safety factor. Directions are rounded to kDirections steps over half a
// turn, so the solver needs only a few element matrices.
//
// window: the safety factor compares the strengths with the stress averaged
// over the part's voxels around each voxel, a box window.x by window.y wide
// in the layer and the rows of the voxel's layer up (the volume method of the
// theory of critical distances: Taylor, "The Theory of Critical Distances",
// 2007). The strengths of a printed material are averages over its lines and
// layers, so the analysis uses one line width across and one layer up.
inline constexpr int kDirections = 36; // over half a turn: steps of 5 degrees
AsPrintedResult solve_as_printed(const VoxelGrid& grid, const OrthotropicMaterial& material,
                                 std::span<const float> angle, std::span<const Fixture> fixtures,
                                 std::span<const SurfaceLoad> loads, const SolverOptions& options = {},
                                 std::span<const double> element_scale = {}, const StressWindow& window = {});

} // namespace tisma
