// Matrix-free linear elasticity on voxels with preconditioned conjugate
// gradients (textbook: Shewchuk 1994; Saad 2003). Own implementation; see
// PROVENANCE.md.

#include "fea/elasticity.hpp"

#include "fea/hex8.hpp"
#include "fea/multigrid.hpp"
#include "fea/operator.hpp"

#include <oneapi/tbb/parallel_for.h>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <optional>
#include <stdexcept>

namespace tisma {
namespace {

// The weights of the voxels at offsets -r to r along an axis of voxel size h
// for an average over a window of width w centred on a voxel: how much of
// each voxel the window covers. A window of 0 is the voxel alone.
std::vector<double> window_weights(double w, double h) {
    if (!(w > 0.0))
        return {1.0};
    const double half = 0.5 * w / h;
    const int r = std::max(0, int(std::ceil(half - 0.5)));
    std::vector<double> out;
    for (int d = -r; d <= r; ++d)
        out.push_back(std::clamp(std::min(d + 0.5, half) - std::max(d - 0.5, -half), 0.0, 1.0));
    return out;
}

// Solves K u = f for the operator's elements and fills the displacement, the
// von Mises stress and their maxima.
ElasticityResult solve(const VoxelGrid& g, const VoxelOperator& op, std::span<const Fixture> fixtures,
                       std::span<const SurfaceLoad> loads, const SolverOptions& options) {
    ElasticityResult out;
    const std::size_t nodes = op.nodes(), dofs = 3 * nodes;
    out.displacement.assign(dofs, 0.0);
    out.von_mises.assign(g.solid.size(), 0.0);
    const std::vector<double> diag = op.diagonal();

    // Degrees of freedom that take part: those of nodes of the part, not fixed.
    std::vector<std::uint8_t> free(dofs, 0);
    for (std::size_t q = 0; q < dofs; ++q)
        free[q] = diag[q] > 0.0;
    for (int k = 0; k < op.nz(); ++k)
        for (int j = 0; j < op.ny(); ++j)
            for (int i = 0; i < op.nx(); ++i) {
                const double x = g.x0 + i * g.hx, y = g.y0 + j * g.hy, z = g.z[std::size_t(k)];
                const std::size_t n = op.node(i, j, k);
                for (const Fixture& f : fixtures)
                    if (f.box.contains(x, y, z)) {
                        if (f.x)
                            free[3 * n] = 0;
                        if (f.y)
                            free[3 * n + 1] = 0;
                        if (f.z)
                            free[3 * n + 2] = 0;
                    }
            }

    // Forces: each load's total over the boundary faces whose centre is in its
    // box, in proportion to area, a quarter of each face to each corner.
    std::vector<double> f(dofs, 0.0);
    static constexpr int kFace[6][4][3] = {
        {{0, 0, 0}, {0, 1, 0}, {0, 1, 1}, {0, 0, 1}}, {{1, 0, 0}, {1, 1, 0}, {1, 1, 1}, {1, 0, 1}},
        {{0, 0, 0}, {1, 0, 0}, {1, 0, 1}, {0, 0, 1}}, {{0, 1, 0}, {1, 1, 0}, {1, 1, 1}, {0, 1, 1}},
        {{0, 0, 0}, {1, 0, 0}, {1, 1, 0}, {0, 1, 0}}, {{0, 0, 1}, {1, 0, 1}, {1, 1, 1}, {0, 1, 1}}};
    static constexpr int kNeighbour[6][3] = {{-1, 0, 0}, {1, 0, 0}, {0, -1, 0}, {0, 1, 0}, {0, 0, -1}, {0, 0, 1}};
    for (const SurfaceLoad& load : loads) {
        std::vector<double> weight(nodes, 0.0);
        double total = 0.0;
        for (int k = 0; k < g.nz; ++k)
            for (int j = 0; j < g.ny; ++j)
                for (int i = 0; i < g.nx; ++i) {
                    if (!g.at(i, j, k))
                        continue;
                    for (int s = 0; s < 6; ++s) {
                        if (g.at(i + kNeighbour[s][0], j + kNeighbour[s][1], k + kNeighbour[s][2]))
                            continue;
                        double c[3] = {0.0, 0.0, 0.0};
                        for (const auto& corner : kFace[s])
                            for (int a = 0; a < 3; ++a)
                                c[a] += corner[a] / 4.0;
                        if (!load.box.contains(g.x0 + (i + c[0]) * g.hx, g.y0 + (j + c[1]) * g.hy,
                                               g.z[std::size_t(k)] + c[2] * g.hz(k)))
                            continue;
                        const double area = s < 2 ? g.hy * g.hz(k) : s < 4 ? g.hx * g.hz(k) : g.hx * g.hy;
                        for (const auto& corner : kFace[s])
                            weight[op.node(i + corner[0], j + corner[1], k + corner[2])] += area / 4.0;
                        total += area;
                    }
                }
        if (total == 0.0) {
            ++out.unapplied_loads;
            continue;
        }
        for (std::size_t n = 0; n < nodes; ++n)
            if (weight[n] > 0.0)
                for (int a = 0; a < 3; ++a)
                    f[3 * n + std::size_t(a)] += load.force[a] * weight[n] / total;
    }
    for (std::size_t q = 0; q < dofs; ++q)
        if (!free[q])
            f[q] = 0.0;

    // Preconditioned conjugate gradients on the free degrees of freedom.
    std::vector<double>& u = out.displacement;
    std::vector<double> r = f, z(dofs), p(dofs), q(dofs);
    const double norm_f = std::sqrt(deterministic_dot(f, f));
    if (norm_f == 0.0) {
        out.converged = true;
        return out;
    }
    std::optional<Multigrid> multigrid;
    if (options.preconditioner == Preconditioner::Multigrid) {
        multigrid.emplace(op, free, diag);
        out.levels = multigrid->levels();
    }
    auto precondition = [&]() {
        if (multigrid)
            multigrid->cycle(r, z);
        else
            tbb::parallel_for(std::size_t(0), dofs, [&](std::size_t i) { z[i] = free[i] ? r[i] / diag[i] : 0.0; });
    };
    precondition();
    p = z;
    double rz = deterministic_dot(r, z);
    for (out.iterations = 0; out.iterations < options.max_iterations; ++out.iterations) {
        op.apply(p, q);
        tbb::parallel_for(std::size_t(0), dofs, [&](std::size_t i) {
            if (!free[i])
                q[i] = 0.0;
        });
        const double alpha = rz / deterministic_dot(p, q);
        tbb::parallel_for(std::size_t(0), dofs, [&](std::size_t i) {
            u[i] += alpha * p[i];
            r[i] -= alpha * q[i];
        });
        out.residual = std::sqrt(deterministic_dot(r, r)) / norm_f;
        if (options.on_iteration)
            options.on_iteration(out.iterations + 1, out.residual);
        if (out.residual < options.tolerance) {
            out.converged = true;
            ++out.iterations;
            break;
        }
        precondition();
        const double rz_next = deterministic_dot(r, z);
        const double beta = rz_next / rz;
        rz = rz_next;
        tbb::parallel_for(std::size_t(0), dofs, [&](std::size_t i) { p[i] = z[i] + beta * p[i]; });
    }

    // Von Mises stress at the centre of every element.
    tbb::parallel_for(0, g.nz, [&](int k) {
        for (int j = 0; j < g.ny; ++j)
            for (int i = 0; i < g.nx; ++i)
                if (g.at(i, j, k)) {
                    double stress[6];
                    op.stress(u, i, j, k, stress);
                    out.von_mises[g.index(i, j, k)] = von_mises(stress);
                }
    });
    for (std::size_t n = 0; n < nodes; ++n)
        out.max_displacement = std::max(out.max_displacement, std::hypot(u[3 * n], u[3 * n + 1], u[3 * n + 2]));
    for (double v : out.von_mises)
        out.max_von_mises = std::max(out.max_von_mises, v);
    return out;
}

} // namespace

VoxelGrid held_voxels(const VoxelGrid& g, std::span<const Fixture> fixtures, std::size_t& loose) {
    VoxelGrid held = g;
    std::vector<std::uint8_t> reached(g.solid.size(), 0);
    std::vector<std::size_t> stack;
    for (int k = 0; k < g.nz; ++k)
        for (int j = 0; j < g.ny; ++j)
            for (int i = 0; i < g.nx; ++i) {
                if (!g.at(i, j, k))
                    continue;
                bool fixed = false;
                for (int c = 0; c < 8 && !fixed; ++c) {
                    const double x = g.x0 + (i + kHexCorner[c][0]) * g.hx, y = g.y0 + (j + kHexCorner[c][1]) * g.hy,
                                 z = g.z[std::size_t(k + kHexCorner[c][2])];
                    for (const Fixture& f : fixtures)
                        fixed = fixed || ((f.x || f.y || f.z) && f.box.contains(x, y, z));
                }
                if (fixed) {
                    reached[g.index(i, j, k)] = 1;
                    stack.push_back(g.index(i, j, k));
                }
            }
    const std::size_t plane = std::size_t(g.nx) * std::size_t(g.ny);
    while (!stack.empty()) {
        const std::size_t v = stack.back();
        stack.pop_back();
        const int i = int(v % std::size_t(g.nx)), j = int(v / std::size_t(g.nx) % std::size_t(g.ny)), k = int(v / plane);
        static constexpr int kFaces[6][3] = {{-1, 0, 0}, {1, 0, 0}, {0, -1, 0}, {0, 1, 0}, {0, 0, -1}, {0, 0, 1}};
        for (const auto& d : kFaces) {
            const int a = i + d[0], b = j + d[1], c = k + d[2];
            if (g.at(a, b, c) && !reached[g.index(a, b, c)]) {
                reached[g.index(a, b, c)] = 1;
                stack.push_back(g.index(a, b, c));
            }
        }
    }
    loose = 0;
    for (std::size_t v = 0; v < g.solid.size(); ++v)
        if (g.solid[v] && !reached[v]) {
            held.solid[v] = 0;
            ++loose;
        }
    return held;
}

ElasticityResult solve_elasticity(const VoxelGrid& g, const Material& material, std::span<const Fixture> fixtures,
                                  std::span<const SurfaceLoad> loads, const SolverOptions& options,
                                  std::span<const double> element_scale) {
    const Stiffness d = isotropic_stiffness(material.young, material.poisson);
    std::size_t loose = 0;
    const VoxelGrid held = held_voxels(g, fixtures, loose);
    ElasticityResult out = solve(held, VoxelOperator(held, std::span(&d, 1), {}, element_scale), fixtures, loads, options);
    out.loose_voxels = loose;
    return out;
}

AsPrintedResult solve_as_printed(const VoxelGrid& g, const OrthotropicMaterial& material, std::span<const float> angle,
                                 std::span<const Fixture> fixtures, std::span<const SurfaceLoad> loads,
                                 const SolverOptions& options, std::span<const double> element_scale,
                                 const StressWindow& window) {
    // Kinds 0 to kDirections - 1: the line at step a of half a turn; the last
    // one: the layer average, for unknown directions.
    constexpr double kStep = std::numbers::pi / kDirections;
    std::vector<Stiffness> kinds(kDirections + 1);
    std::vector<Stiffness> rotation(kDirections);
    for (int a = 0; a < kDirections; ++a) {
        kinds[std::size_t(a)] = global_stiffness(material, a * kStep);
        rotation[std::size_t(a)] = stress_rotation(a * kStep);
    }
    kinds[kDirections] = layer_average_stiffness(material);
    if (!angle.empty() && angle.size() != g.solid.size())
        throw std::invalid_argument("solve_as_printed: one angle per voxel");
    std::vector<std::uint16_t> kind_of(g.solid.size(), std::uint16_t(kDirections));
    if (!angle.empty())
        for (std::size_t v = 0; v < kind_of.size(); ++v)
            if (std::isfinite(angle[v])) {
                const double steps = std::floor(double(angle[v]) / kStep + 0.5);
                kind_of[v] = std::uint16_t(int(steps - kDirections * std::floor(steps / kDirections)));
            }

    AsPrintedResult out;
    std::size_t loose = 0;
    const VoxelGrid held = held_voxels(g, fixtures, loose);
    const VoxelOperator op(held, kinds, kind_of, element_scale);
    out.elasticity = solve(held, op, fixtures, loads, options);
    out.elasticity.loose_voxels = loose;
    // The stress of every voxel, then its average over the window around it
    // (only over the voxels of the part).
    const std::size_t count = g.solid.size();
    std::vector<std::array<float, 7>> sums(count), next(count);
    tbb::parallel_for(0, g.nz, [&](int k) {
        for (int j = 0; j < g.ny; ++j)
            for (int i = 0; i < g.nx; ++i) {
                if (!held.at(i, j, k))
                    continue;
                double global[6];
                op.stress(out.elasticity.displacement, i, j, k, global);
                std::array<float, 7>& t = sums[g.index(i, j, k)];
                for (int c = 0; c < 6; ++c)
                    t[std::size_t(c)] = float(global[c]);
                t[6] = 1.0f;
            }
    });
    std::vector<std::array<float, 7>> peak = sums;
    if (window.x > 0.0 || window.y > 0.0 || window.rows_per_layer > 1) {
        const std::vector<double> wx = window_weights(window.x, g.hx), wy = window_weights(window.y, g.hy);
        const int rx = int(wx.size() / 2), ry = int(wy.size() / 2);
        auto add = [](std::array<float, 7>& to, const std::array<float, 7>& from, double w) {
            for (std::size_t c = 0; c < 7; ++c)
                to[c] += float(w * from[c]);
        };
        tbb::parallel_for(0, g.nz, [&](int k) { // along x
            for (int j = 0; j < g.ny; ++j)
                for (int i = 0; i < g.nx; ++i) {
                    std::array<float, 7> t{};
                    for (int d = std::max(-rx, -i); d <= std::min(rx, g.nx - 1 - i); ++d)
                        add(t, sums[g.index(i + d, j, k)], wx[std::size_t(d + rx)]);
                    next[g.index(i, j, k)] = t;
                }
        });
        tbb::parallel_for(0, g.nz, [&](int k) { // along y
            for (int j = 0; j < g.ny; ++j)
                for (int i = 0; i < g.nx; ++i) {
                    std::array<float, 7> t{};
                    for (int d = std::max(-ry, -j); d <= std::min(ry, g.ny - 1 - j); ++d)
                        add(t, next[g.index(i, j + d, k)], wy[std::size_t(d + ry)]);
                    sums[g.index(i, j, k)] = t;
                }
        });
        tbb::parallel_for(0, g.nz, [&](int k) { // along z: the rows of the voxel's layer, by height
            const int rows = std::max(1, window.rows_per_layer);
            const int first = k / rows * rows, last = std::min(g.nz - 1, first + rows - 1);
            for (int j = 0; j < g.ny; ++j)
                for (int i = 0; i < g.nx; ++i) {
                    std::array<float, 7> t{};
                    for (int m = first; m <= last; ++m)
                        add(t, sums[g.index(i, j, m)], g.hz(m));
                    next[g.index(i, j, k)] = t;
                }
        });
        sums.swap(next);
    }

    out.safety.assign(count, std::numeric_limits<double>::infinity());
    out.peak_safety.assign(count, std::numeric_limits<double>::infinity());
    out.mode.assign(count, FailureMode::None);
    tbb::parallel_for(std::size_t(0), count, [&](std::size_t v) {
        if (peak[v][6] == 0.0f)
            return;
        // In the line's axes: its own direction, or the weakest one.
        const int first = kind_of[v] == kDirections ? 0 : kind_of[v];
        const int last = kind_of[v] == kDirections ? kDirections - 1 : kind_of[v];
        for (int which = 0; which < 2; ++which) {
            const std::array<float, 7>& t = which == 0 ? peak[v] : sums[v];
            double global[6];
            for (int c = 0; c < 6; ++c)
                global[c] = double(t[std::size_t(c)]) / double(t[6]);
            for (int a = first; a <= last; ++a) {
                Voigt local{};
                for (int r = 0; r < 6; ++r)
                    for (int c = 0; c < 6; ++c)
                        local[std::size_t(r)] += rotation[std::size_t(a)][std::size_t(r * 6 + c)] * global[c];
                const double safety = tsai_wu_safety(material, local);
                if (which == 0)
                    out.peak_safety[v] = std::min(out.peak_safety[v], safety);
                else if (safety < out.safety[v])
                    out.safety[v] = safety, out.mode[v] = failure_mode(material, local);
            }
        }
    });
    for (std::size_t v = 0; v < count; ++v)
        out.min_peak_safety = std::min(out.min_peak_safety, out.peak_safety[v]);
    for (std::size_t v = 0; v < out.safety.size(); ++v)
        if (out.safety[v] < out.min_safety)
            out.min_safety = out.safety[v], out.weakest = v;
    return out;
}

} // namespace tisma
