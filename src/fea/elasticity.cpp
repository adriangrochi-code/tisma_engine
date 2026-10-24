// Matrix-free linear elasticity on voxels with preconditioned conjugate
// gradients (textbook: Shewchuk 1994; Saad 2003). Own implementation; see
// PROVENANCE.md.

#include "fea/elasticity.hpp"

#include "fea/hex8.hpp"

#include <oneapi/tbb/blocked_range.h>
#include <oneapi/tbb/parallel_for.h>
#include <oneapi/tbb/parallel_reduce.h>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>

namespace tisma {
namespace {

constexpr std::size_t kGrain = 4096;

// A deterministic parallel dot product: the same blocks summed in the same
// order on every run.
double dot(const std::vector<double>& a, const std::vector<double>& b) {
    return tbb::parallel_deterministic_reduce(
        tbb::blocked_range<std::size_t>(0, a.size(), kGrain), 0.0,
        [&](const tbb::blocked_range<std::size_t>& r, double s) {
            for (std::size_t i = r.begin(); i != r.end(); ++i)
                s += a[i] * b[i];
            return s;
        },
        [](double x, double y) { return x + y; });
}

// The elements' stiffness: one per distinct row height (layers are 0.2 mm,
// the first one maybe thicker) and kind of material (an elasticity matrix D
// with its modulus, for example the printed material at one line direction),
// each as its 8 x 8 blocks of 3 x 3; every voxel says which one it uses.
class Operator {
public:
    Operator(const VoxelGrid& g, std::span<const Stiffness> kinds, std::span<const std::uint16_t> kind_of,
             std::span<const double> scale)
        : g_(g), nx_(g.nx + 1), ny_(g.ny + 1), nz_(g.nz + 1) {
        std::vector<int> layer_class(std::size_t(g.nz));
        std::vector<long long> heights;
        for (int k = 0; k < g.nz; ++k) {
            const long long key = std::llround(g.hz(k) * 1e9); // to the picometre
            const auto it = std::find(heights.begin(), heights.end(), key);
            layer_class[std::size_t(k)] = int(it - heights.begin());
            if (it != heights.end())
                continue;
            heights.push_back(key);
            for (const Stiffness& d : kinds) {
                const Hex8 e = hex8(d, g.hx, g.hy, g.hz(k));
                elements_.push_back(e);
                Blocks b;
                for (int a = 0; a < 8; ++a)
                    for (int c = 0; c < 8; ++c)
                        for (int r = 0; r < 3; ++r)
                            for (int q = 0; q < 3; ++q)
                                b.k[a][c][r][q] = e.stiffness[std::size_t(3 * a + r) * 24 + std::size_t(3 * c + q)];
                blocks_.push_back(b);
            }
        }
        if (blocks_.size() > 65536)
            throw std::invalid_argument("elasticity: too many kinds of element");
        block_of_.resize(g.solid.size());
        scale_.resize(g.solid.size());
        const std::size_t layer = std::size_t(g.nx) * std::size_t(g.ny);
        for (std::size_t v = 0; v < g.solid.size(); ++v) {
            const std::size_t kind = kind_of.empty() ? 0 : kind_of[v];
            block_of_[v] = std::uint16_t(std::size_t(layer_class[v / layer]) * kinds.size() + kind);
            scale_[v] = g.solid[v] ? (scale.empty() ? 1.0 : scale[v]) : 0.0;
        }
    }

    std::size_t nodes() const { return std::size_t(nx_) * std::size_t(ny_) * std::size_t(nz_); }
    std::size_t node(int i, int j, int k) const {
        return (std::size_t(k) * std::size_t(ny_) + std::size_t(j)) * std::size_t(nx_) + std::size_t(i);
    }
    // The voxel (i, j, k), or the grid's size outside it.
    std::size_t voxel(int i, int j, int k) const {
        return i >= 0 && j >= 0 && k >= 0 && i < g_.nx && j < g_.ny && k < g_.nz ? g_.index(i, j, k) : g_.solid.size();
    }
    double scale(std::size_t v) const { return v < scale_.size() ? scale_[v] : 0.0; }

    // y = K x, node by node: node n at local corner a of each of its (up to)
    // eight elements gets block (a, b) times the displacement of corner b.
    void apply(const std::vector<double>& x, std::vector<double>& y) const {
        tbb::parallel_for(tbb::blocked_range<std::size_t>(0, nodes(), kGrain / 8), [&](const tbb::blocked_range<std::size_t>& r) {
            for (std::size_t n = r.begin(); n != r.end(); ++n) {
                const int i = int(n % std::size_t(nx_)), j = int(n / std::size_t(nx_) % std::size_t(ny_)),
                          k = int(n / (std::size_t(nx_) * std::size_t(ny_)));
                double acc[3] = {0.0, 0.0, 0.0};
                for (int a = 0; a < 8; ++a) {
                    const int ei = i - kHexCorner[a][0], ej = j - kHexCorner[a][1], ek = k - kHexCorner[a][2];
                    const std::size_t v = voxel(ei, ej, ek);
                    const double s = scale(v);
                    if (s == 0.0)
                        continue;
                    const auto& block = blocks_[block_of_[v]].k;
                    for (int b = 0; b < 8; ++b) {
                        const std::size_t m = node(ei + kHexCorner[b][0], ej + kHexCorner[b][1], ek + kHexCorner[b][2]);
                        const double* xm = &x[3 * m];
                        for (int rr = 0; rr < 3; ++rr)
                            acc[rr] += s * (block[a][b][rr][0] * xm[0] + block[a][b][rr][1] * xm[1] +
                                            block[a][b][rr][2] * xm[2]);
                    }
                }
                y[3 * n] = acc[0], y[3 * n + 1] = acc[1], y[3 * n + 2] = acc[2];
            }
        });
    }

    // The diagonal of K.
    std::vector<double> diagonal() const {
        std::vector<double> d(3 * nodes(), 0.0);
        tbb::parallel_for(std::size_t(0), nodes(), [&](std::size_t n) {
            const int i = int(n % std::size_t(nx_)), j = int(n / std::size_t(nx_) % std::size_t(ny_)),
                      k = int(n / (std::size_t(nx_) * std::size_t(ny_)));
            for (int a = 0; a < 8; ++a) {
                const std::size_t v = voxel(i - kHexCorner[a][0], j - kHexCorner[a][1], k - kHexCorner[a][2]);
                const double s = scale(v);
                if (s == 0.0)
                    continue;
                const auto& block = blocks_[block_of_[v]].k;
                for (int r = 0; r < 3; ++r)
                    d[3 * n + std::size_t(r)] += s * block[a][a][r][r];
            }
        });
        return d;
    }

    // The stress at the centre of voxel (i, j, k), in global axes.
    void stress(const std::vector<double>& u, int i, int j, int k, double out[6]) const {
        const std::size_t v = g_.index(i, j, k);
        const Hex8& e = elements_[block_of_[v]];
        double ue[24];
        for (int a = 0; a < 8; ++a)
            for (int d = 0; d < 3; ++d)
                ue[3 * a + d] = u[3 * node(i + kHexCorner[a][0], j + kHexCorner[a][1], k + kHexCorner[a][2]) + std::size_t(d)];
        double strain[6] = {};
        for (int s = 0; s < 6; ++s)
            for (int c = 0; c < 24; ++c)
                strain[s] += e.b_centre[std::size_t(s) * 24 + std::size_t(c)] * ue[c];
        for (int s = 0; s < 6; ++s) {
            out[s] = 0.0;
            for (int c = 0; c < 6; ++c)
                out[s] += scale_[v] * e.elasticity[std::size_t(s) * 6 + std::size_t(c)] * strain[c];
        }
    }

    int nx() const { return nx_; }
    int ny() const { return ny_; }
    int nz() const { return nz_; }

private:
    struct Blocks {
        double k[8][8][3][3];
    };
    const VoxelGrid& g_;
    int nx_, ny_, nz_;
    std::vector<Blocks> blocks_;
    std::vector<Hex8> elements_;           // the same order as blocks_
    std::vector<std::uint16_t> block_of_;  // per voxel
    std::vector<double> scale_;            // per voxel; 0 for empty voxels
};

// Solves K u = f for the operator's elements and fills the displacement, the
// von Mises stress and their maxima.

ElasticityResult solve(const VoxelGrid& g, const Operator& op, std::span<const Fixture> fixtures,
                       std::span<const SurfaceLoad> loads, double tolerance, int max_iterations) {
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
        if (total == 0.0)
            continue;
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
    auto precondition = [&]() {
        tbb::parallel_for(std::size_t(0), dofs, [&](std::size_t i) { z[i] = free[i] ? r[i] / diag[i] : 0.0; });
    };
    const double norm_f = std::sqrt(dot(f, f));
    if (norm_f == 0.0) {
        out.converged = true;
        return out;
    }
    precondition();
    p = z;
    double rz = dot(r, z);
    for (out.iterations = 0; out.iterations < max_iterations; ++out.iterations) {
        op.apply(p, q);
        tbb::parallel_for(std::size_t(0), dofs, [&](std::size_t i) {
            if (!free[i])
                q[i] = 0.0;
        });
        const double alpha = rz / dot(p, q);
        tbb::parallel_for(std::size_t(0), dofs, [&](std::size_t i) {
            u[i] += alpha * p[i];
            r[i] -= alpha * q[i];
        });
        out.residual = std::sqrt(dot(r, r)) / norm_f;
        if (out.residual < tolerance) {
            out.converged = true;
            ++out.iterations;
            break;
        }
        precondition();
        const double rz_next = dot(r, z);
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

ElasticityResult solve_elasticity(const VoxelGrid& g, const Material& material, std::span<const Fixture> fixtures,
                                  std::span<const SurfaceLoad> loads, double tolerance, int max_iterations,
                                  std::span<const double> element_scale) {
    const Stiffness d = isotropic_stiffness(material.young, material.poisson);
    return solve(g, Operator(g, std::span(&d, 1), {}, element_scale), fixtures, loads, tolerance, max_iterations);
}

AsPrintedResult solve_as_printed(const VoxelGrid& g, const OrthotropicMaterial& material, std::span<const float> angle,
                                 std::span<const Fixture> fixtures, std::span<const SurfaceLoad> loads,
                                 double tolerance, int max_iterations, std::span<const double> element_scale) {
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
    const Operator op(g, kinds, kind_of, element_scale);
    out.elasticity = solve(g, op, fixtures, loads, tolerance, max_iterations);
    out.safety.assign(g.solid.size(), std::numeric_limits<double>::infinity());
    out.mode.assign(g.solid.size(), FailureMode::None);
    tbb::parallel_for(0, g.nz, [&](int k) {
        for (int j = 0; j < g.ny; ++j)
            for (int i = 0; i < g.nx; ++i) {
                if (!g.at(i, j, k))
                    continue;
                const std::size_t v = g.index(i, j, k);
                double global[6];
                op.stress(out.elasticity.displacement, i, j, k, global);
                // In the line's axes: its own direction, or the weakest one.
                const int first = kind_of[v] == kDirections ? 0 : kind_of[v];
                const int last = kind_of[v] == kDirections ? kDirections - 1 : kind_of[v];
                for (int a = first; a <= last; ++a) {
                    Voigt local{};
                    for (int r = 0; r < 6; ++r)
                        for (int c = 0; c < 6; ++c)
                            local[std::size_t(r)] += rotation[std::size_t(a)][std::size_t(r * 6 + c)] * global[c];
                    const double safety = tsai_wu_safety(material, local);
                    if (safety < out.safety[v])
                        out.safety[v] = safety, out.mode[v] = failure_mode(material, local);
                }
            }
    });
    for (std::size_t v = 0; v < out.safety.size(); ++v)
        if (out.safety[v] < out.min_safety)
            out.min_safety = out.safety[v], out.weakest = v;
    return out;
}

} // namespace tisma
