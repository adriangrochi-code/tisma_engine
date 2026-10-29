#pragma once

#include "fea/operator.hpp"

#include <array>
#include <cstdint>
#include <vector>

namespace tisma {

// A geometric multigrid V-cycle for the voxels' stiffness, used as the
// preconditioner of conjugate gradients (multigrid-preconditioned CG: Tatebe
// 1993, "The multigrid preconditioned conjugate gradient method"; Amir, Aage
// and Lazarov 2014, "On multigrid-CG for efficient topology optimization").
// Textbook parts: Briggs, Henson and McCormick, "A Multigrid Tutorial", 2000
// (V-cycle, interpolation and its transpose); Trottenberg,
// Oosterlee and Schuller, "Multigrid", 2001 (Galerkin coarse operators).
// Own design:
// - Each level halves the elements along every axis that has more than one
//   (an odd count gets an empty half element at the end); the interpolation
//   is trilinear, weighted by the nodes' real positions (the layers' rows may
//   differ in height).
// - The coarse operator is Galerkin's, P^T K P, built element by element: a
//   coarse element's 24 x 24 matrix is the sum over its (up to) eight child
//   elements of P_e^T K_e P_e, which is exact because a child's nodes
//   interpolate only from its parent's corners. Fixed and unused degrees of
//   freedom are left out of P, so holes, thin lines and supports carry down
//   to every level. Coarse matrices are stored in single precision, upper
//   blocks only.
// - Chebyshev polynomial smoothing (Adams, Brezina, Hu and Tuminaro 2003,
//   "Parallel multigrid smoothing: polynomial versus Gauss-Seidel"), the same
//   before and after, on the eigenvalues of D^-1 K up to a power-iteration
//   estimate of the largest on each level; it needs only products with K, so
//   it runs in parallel and on the GPU like Jacobi but smooths better. The
//   coarsest level (a few hundred nodes) is solved exactly by dense Cholesky,
//   dropping pivots that vanish (parts of the coarse level that nothing
//   holds).
// - Every step gathers node by node, in parallel, with the same sums in the
//   same order on every run.
// The cycle is symmetric and positive definite, as conjugate gradients need.
class Multigrid {
public:
    // free: per degree of freedom of the fine operator, whether it takes part;
    // diagonal: the fine operator's diagonal.
    Multigrid(const VoxelOperator& fine, const std::vector<std::uint8_t>& free, const std::vector<double>& diagonal);

    // z = B r: one V-cycle from zero.
    void cycle(const std::vector<double>& r, std::vector<double>& z);

    int levels() const { return int(levels_.size()); }

    // Elements of each level along each axis (for tests and reports).
    std::array<int, 3> elements(int level) const { return levels_[std::size_t(level)].elems; }

private:
    static constexpr int kPacked = 36 * 9; // the upper 3 x 3 blocks (a <= b) of a 24 x 24 element matrix

    struct Level {
        std::array<int, 3> elems{}; // elements along each axis
        std::array<int, 3> n{}; // nodes along each axis
        std::array<std::vector<double>, 3> coord; // node positions along each axis
        // How the finer level's nodes interpolate from this level's (empty on
        // level 0): per finer node along each axis, the lower coarse node and
        // the weight of the next one.
        std::array<int, 3> factor{1, 1, 1};
        std::array<std::vector<int>, 3> lower;
        std::array<std::vector<double>, 3> upper_weight;
        std::vector<float> stiffness;      // per element, kPacked values (levels above 0)
        std::vector<std::uint8_t> present; // per element: it has stiffness
        std::vector<double> diagonal;
        std::vector<std::uint8_t> free;
        double lambda = 1.0; // the largest eigenvalue of D^-1 K, estimated
        // The cycle's right-hand side and correction; the smoother's residual,
        // step and product.
        std::vector<double> r, x, s, d, t;

        std::size_t nodes() const { return std::size_t(n[0]) * std::size_t(n[1]) * std::size_t(n[2]); }
        std::size_t node(int i, int j, int k) const {
            return (std::size_t(k) * std::size_t(n[1]) + std::size_t(j)) * std::size_t(n[0]) + std::size_t(i);
        }
        std::size_t element(int i, int j, int k) const {
            return (std::size_t(k) * std::size_t(elems[1]) + std::size_t(j)) * std::size_t(elems[0]) + std::size_t(i);
        }
    };

    void apply(std::size_t level, const std::vector<double>& x, std::vector<double>& y) const;
    void coarsen(std::size_t level);
    void element_matrix(std::size_t level, int i, int j, int k, double out[24][24]) const;
    double largest_eigenvalue(std::size_t level);
    void factor_coarsest();
    void solve_coarsest(const std::vector<double>& r, std::vector<double>& e) const;
    void cycle(std::size_t level);

    const VoxelOperator& fine_;
    std::vector<Level> levels_;
    // The coarsest level: its free degrees of freedom and the Cholesky factor
    // (row-major lower triangle) of its matrix on them.
    std::vector<std::size_t> dense_dofs_;
    std::vector<double> cholesky_;
    std::vector<std::uint8_t> dropped_;
};

} // namespace tisma
