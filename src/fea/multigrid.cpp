// Multigrid V-cycle with Galerkin coarse operators built element by element,
// the preconditioner of conjugate gradients (textbook: Briggs, Henson and
// McCormick 2000; Trottenberg, Oosterlee and Schuller 2001; Tatebe 1993;
// Amir, Aage and Lazarov 2014; Chebyshev smoothing: Adams, Brezina, Hu and
// Tuminaro 2003). Own implementation; see PROVENANCE.md.

#include "fea/multigrid.hpp"

#include <oneapi/tbb/blocked_range.h>
#include <oneapi/tbb/parallel_for.h>

#include <algorithm>
#include <cmath>

namespace tisma {
namespace {

constexpr std::size_t kGrain = 512;     // nodes per parallel task
constexpr std::size_t kCoarsest = 256;  // nodes: solved exactly below this
// Chebyshev smoothing of degree kDegree on the eigenvalues of D^-1 K in
// [b / kRatio, b], b = 1.1 times the largest one's estimate (tuned on the
// benchmark: degrees 2 to 4, ratios 4 to 100).
constexpr int kDegree = 3;
constexpr double kRatio = 30.0;
constexpr int kPowerIterations = 15;    // for the largest eigenvalue of D^-1 K
constexpr double kDrop = 1e-10;         // a Cholesky pivot below this times the largest diagonal vanishes

// Where block (a, b), a <= b, of an element matrix sits among the upper blocks.
constexpr int pack(int a, int b) { return a * 8 - a * (a - 1) / 2 + (b - a); }

// A fixed pseudo-random value in [0.5, 1.5) for index i (the finalizer of
// splitmix64, Steele, Lea and Flood 2014).
double noise(std::size_t i) {
    std::uint64_t z = std::uint64_t(i) + 0x9e3779b97f4a7c15ULL;
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    z ^= z >> 31;
    return 0.5 + double(z >> 11) * 0x1.0p-53;
}

} // namespace

Multigrid::Multigrid(const VoxelOperator& fine, const std::vector<std::uint8_t>& free,
                     const std::vector<double>& diagonal)
    : fine_(fine) {
    const VoxelGrid& g = fine.grid();
    Level top;
    top.elems = {g.nx, g.ny, g.nz};
    top.n = {g.nx + 1, g.ny + 1, g.nz + 1};
    for (int i = 0; i < top.n[0]; ++i)
        top.coord[0].push_back(g.x0 + i * g.hx);
    for (int j = 0; j < top.n[1]; ++j)
        top.coord[1].push_back(g.y0 + j * g.hy);
    top.coord[2] = g.z;
    top.free = free;
    top.diagonal = diagonal;
    levels_.push_back(std::move(top));
    while (levels_.back().nodes() > kCoarsest &&
           std::any_of(levels_.back().elems.begin(), levels_.back().elems.end(), [](int e) { return e > 1; }))
        coarsen(levels_.size() - 1);
    for (std::size_t l = 0; l < levels_.size(); ++l) {
        Level& level = levels_[l];
        const std::size_t dofs = 3 * level.nodes();
        level.r.assign(dofs, 0.0);
        level.x.assign(dofs, 0.0);
        level.t.assign(dofs, 0.0);
        level.d.assign(dofs, 0.0);
        level.s.assign(dofs, 0.0);
        if (l + 1 < levels_.size())
            level.lambda = largest_eigenvalue(l);
    }
    factor_coarsest();
}

void Multigrid::apply(std::size_t l, const std::vector<double>& x, std::vector<double>& y) const {
    const Level& level = levels_[l];
    if (l == 0) {
        fine_.apply(x, y);
    } else {
        tbb::parallel_for(tbb::blocked_range<std::size_t>(0, level.nodes(), kGrain), [&](const tbb::blocked_range<std::size_t>& range) {
            for (std::size_t n = range.begin(); n != range.end(); ++n) {
                const int i = int(n % std::size_t(level.n[0])), j = int(n / std::size_t(level.n[0]) % std::size_t(level.n[1])),
                          k = int(n / (std::size_t(level.n[0]) * std::size_t(level.n[1])));
                double acc[3] = {0.0, 0.0, 0.0};
                for (int a = 0; a < 8; ++a) {
                    const int ei = i - kHexCorner[a][0], ej = j - kHexCorner[a][1], ek = k - kHexCorner[a][2];
                    if (ei < 0 || ej < 0 || ek < 0 || ei >= level.elems[0] || ej >= level.elems[1] || ek >= level.elems[2])
                        continue;
                    const std::size_t el = level.element(ei, ej, ek);
                    if (!level.present[el])
                        continue;
                    const float* m = &level.stiffness[el * kPacked];
                    for (int b = 0; b < 8; ++b) {
                        const double* xb = &x[3 * level.node(ei + kHexCorner[b][0], ej + kHexCorner[b][1], ek + kHexCorner[b][2])];
                        if (a <= b) {
                            const float* blk = m + pack(a, b) * 9;
                            for (int r = 0; r < 3; ++r)
                                acc[r] += double(blk[3 * r]) * xb[0] + double(blk[3 * r + 1]) * xb[1] + double(blk[3 * r + 2]) * xb[2];
                        } else {
                            const float* blk = m + pack(b, a) * 9; // block (a, b) is its transpose
                            for (int r = 0; r < 3; ++r)
                                acc[r] += double(blk[r]) * xb[0] + double(blk[3 + r]) * xb[1] + double(blk[6 + r]) * xb[2];
                        }
                    }
                }
                y[3 * n] = acc[0], y[3 * n + 1] = acc[1], y[3 * n + 2] = acc[2];
            }
        });
    }
    tbb::parallel_for(std::size_t(0), y.size(), [&](std::size_t q) {
        if (!level.free[q])
            y[q] = 0.0;
    });
}

void Multigrid::element_matrix(std::size_t l, int i, int j, int k, double out[24][24]) const {
    if (l == 0) {
        const std::size_t v = fine_.voxel(i, j, k);
        const double s = fine_.scale(v);
        const auto& blocks = fine_.blocks(v).k;
        for (int a = 0; a < 8; ++a)
            for (int b = 0; b < 8; ++b)
                for (int r = 0; r < 3; ++r)
                    for (int q = 0; q < 3; ++q)
                        out[3 * a + r][3 * b + q] = s * blocks[a][b][r][q];
        return;
    }
    const Level& level = levels_[l];
    const float* m = &level.stiffness[level.element(i, j, k) * kPacked];
    for (int a = 0; a < 8; ++a)
        for (int b = a; b < 8; ++b)
            for (int r = 0; r < 3; ++r)
                for (int q = 0; q < 3; ++q)
                    out[3 * a + r][3 * b + q] = out[3 * b + q][3 * a + r] = double(m[pack(a, b) * 9 + 3 * r + q]);
}

void Multigrid::coarsen(std::size_t l) {
    Level c;
    {
        const Level& f = levels_[l];
        for (int axis = 0; axis < 3; ++axis) {
            const int fn = f.n[std::size_t(axis)];
            const std::vector<double>& fc = f.coord[std::size_t(axis)];
            const int factor = f.elems[std::size_t(axis)] > 1 ? 2 : 1;
            c.factor[std::size_t(axis)] = factor;
            c.elems[std::size_t(axis)] = (f.elems[std::size_t(axis)] + factor - 1) / factor;
            c.n[std::size_t(axis)] = c.elems[std::size_t(axis)] + 1;
            std::vector<double>& cc = c.coord[std::size_t(axis)];
            for (int node = 0; node < c.n[std::size_t(axis)]; ++node) {
                const int fi = node * factor;
                cc.push_back(fi < fn ? fc[std::size_t(fi)] : fc[std::size_t(fn - 1)] + (fc[std::size_t(fn - 1)] - fc[std::size_t(fn - 2)]));
            }
            for (int fi = 0; fi < fn; ++fi) {
                const int lo = fi / factor;
                const double w = factor == 2 && fi % 2 == 1
                                     ? (fc[std::size_t(fi)] - cc[std::size_t(lo)]) / (cc[std::size_t(lo) + 1] - cc[std::size_t(lo)])
                                     : 0.0;
                c.lower[std::size_t(axis)].push_back(lo);
                c.upper_weight[std::size_t(axis)].push_back(w);
            }
        }
    }
    const std::size_t elements = std::size_t(c.elems[0]) * std::size_t(c.elems[1]) * std::size_t(c.elems[2]);
    c.stiffness.assign(elements * kPacked, 0.0f);
    c.present.assign(elements, 0);
    levels_.push_back(std::move(c));
    const Level& f = levels_[l];
    Level& coarse = levels_[l + 1];

    // The weight of coarse node cn in finer node fi's interpolation along an axis.
    auto weight = [&](int axis, int fi, int cn) {
        const int lo = coarse.lower[std::size_t(axis)][std::size_t(fi)];
        const double w = coarse.upper_weight[std::size_t(axis)][std::size_t(fi)];
        return (lo == cn ? 1.0 - w : 0.0) + (lo + 1 == cn ? w : 0.0);
    };
    auto present = [&](int i, int j, int k) {
        if (i >= f.elems[0] || j >= f.elems[1] || k >= f.elems[2])
            return false;
        return l == 0 ? fine_.scale(fine_.voxel(i, j, k)) != 0.0 : f.present[f.element(i, j, k)] != 0;
    };
    tbb::parallel_for(std::size_t(0), elements, [&](std::size_t el) {
        const int ci = int(el % std::size_t(coarse.elems[0])), cj = int(el / std::size_t(coarse.elems[0]) % std::size_t(coarse.elems[1])),
                  ck = int(el / (std::size_t(coarse.elems[0]) * std::size_t(coarse.elems[1])));
        double kc[24][24] = {};
        bool any = false;
        for (int dz = 0; dz < coarse.factor[2]; ++dz)
            for (int dy = 0; dy < coarse.factor[1]; ++dy)
                for (int dx = 0; dx < coarse.factor[0]; ++dx) {
                    const int fi = ci * coarse.factor[0] + dx, fj = cj * coarse.factor[1] + dy, fk = ck * coarse.factor[2] + dz;
                    if (!present(fi, fj, fk))
                        continue;
                    any = true;
                    double kf[24][24];
                    element_matrix(l, fi, fj, fk, kf);
                    // P_e: corner a's component d takes w[a][b][d] of coarse
                    // corner b's (0 where the fine degree of freedom is fixed).
                    double w[8][8][3];
                    for (int a = 0; a < 8; ++a) {
                        const int ni = fi + kHexCorner[a][0], nj = fj + kHexCorner[a][1], nk = fk + kHexCorner[a][2];
                        const std::size_t fn = f.node(ni, nj, nk);
                        for (int b = 0; b < 8; ++b) {
                            const double wb = weight(0, ni, ci + kHexCorner[b][0]) * weight(1, nj, cj + kHexCorner[b][1]) *
                                              weight(2, nk, ck + kHexCorner[b][2]);
                            for (int d = 0; d < 3; ++d)
                                w[a][b][d] = f.free[3 * fn + std::size_t(d)] ? wb : 0.0;
                        }
                    }
                    // T = K_e P_e, then K_c += P_e^T T.
                    double t[24][24];
                    for (int row = 0; row < 24; ++row)
                        for (int b = 0; b < 8; ++b)
                            for (int d = 0; d < 3; ++d) {
                                double s = 0.0;
                                for (int a = 0; a < 8; ++a)
                                    s += kf[row][3 * a + d] * w[a][b][d];
                                t[row][3 * b + d] = s;
                            }
                    for (int b = 0; b < 8; ++b)
                        for (int d = 0; d < 3; ++d)
                            for (int col = 0; col < 24; ++col) {
                                double s = 0.0;
                                for (int a = 0; a < 8; ++a)
                                    s += w[a][b][d] * t[3 * a + d][col];
                                kc[3 * b + d][col] += s;
                            }
                }
        if (!any)
            return;
        coarse.present[el] = 1;
        float* m = &coarse.stiffness[el * kPacked];
        for (int a = 0; a < 8; ++a)
            for (int b = a; b < 8; ++b)
                for (int r = 0; r < 3; ++r)
                    for (int q = 0; q < 3; ++q)
                        m[pack(a, b) * 9 + 3 * r + q] = float(kc[3 * a + r][3 * b + q]);
    });

    // The diagonal, node by node from the elements around it; degrees of
    // freedom without stiffness take no part.
    coarse.diagonal.assign(3 * coarse.nodes(), 0.0);
    tbb::parallel_for(std::size_t(0), coarse.nodes(), [&](std::size_t n) {
        const int i = int(n % std::size_t(coarse.n[0])), j = int(n / std::size_t(coarse.n[0]) % std::size_t(coarse.n[1])),
                  k = int(n / (std::size_t(coarse.n[0]) * std::size_t(coarse.n[1])));
        for (int a = 0; a < 8; ++a) {
            const int ei = i - kHexCorner[a][0], ej = j - kHexCorner[a][1], ek = k - kHexCorner[a][2];
            if (ei < 0 || ej < 0 || ek < 0 || ei >= coarse.elems[0] || ej >= coarse.elems[1] || ek >= coarse.elems[2])
                continue;
            const std::size_t el = coarse.element(ei, ej, ek);
            if (!coarse.present[el])
                continue;
            for (int r = 0; r < 3; ++r)
                coarse.diagonal[3 * n + std::size_t(r)] += double(coarse.stiffness[el * kPacked + std::size_t(pack(a, a) * 9 + 4 * r)]);
        }
    });
    coarse.free.resize(coarse.diagonal.size());
    for (std::size_t q = 0; q < coarse.free.size(); ++q)
        coarse.free[q] = coarse.diagonal[q] > 0.0;
}

double Multigrid::largest_eigenvalue(std::size_t l) {
    Level& level = levels_[l];
    std::vector<double>& v = level.x;
    std::vector<double>& t = level.t;
    for (std::size_t q = 0; q < v.size(); ++q)
        v[q] = level.free[q] ? noise(q) : 0.0;
    double lambda = 1.0;
    double norm = std::sqrt(deterministic_dot(v, v));
    if (norm == 0.0)
        return lambda;
    for (std::size_t q = 0; q < v.size(); ++q)
        v[q] /= norm;
    for (int it = 0; it < kPowerIterations; ++it) {
        apply(l, v, t);
        tbb::parallel_for(std::size_t(0), t.size(), [&](std::size_t q) { t[q] = level.free[q] ? t[q] / level.diagonal[q] : 0.0; });
        norm = std::sqrt(deterministic_dot(t, t));
        if (norm == 0.0)
            break;
        lambda = norm;
        tbb::parallel_for(std::size_t(0), t.size(), [&](std::size_t q) { v[q] = t[q] / norm; });
    }
    return lambda;
}

void Multigrid::factor_coarsest() {
    const std::size_t l = levels_.size() - 1;
    const Level& level = levels_[l];
    std::vector<std::ptrdiff_t> dense(3 * level.nodes(), -1);
    for (std::size_t q = 0; q < dense.size(); ++q)
        if (level.free[q]) {
            dense[q] = std::ptrdiff_t(dense_dofs_.size());
            dense_dofs_.push_back(q);
        }
    const std::size_t n = dense_dofs_.size();
    std::vector<double>& a = cholesky_;
    a.assign(n * n, 0.0);
    for (int k = 0; k < level.elems[2]; ++k)
        for (int j = 0; j < level.elems[1]; ++j)
            for (int i = 0; i < level.elems[0]; ++i) {
                const bool present = l == 0 ? fine_.scale(fine_.voxel(i, j, k)) != 0.0 : level.present[level.element(i, j, k)] != 0;
                if (!present)
                    continue;
                double m[24][24];
                element_matrix(l, i, j, k, m);
                std::ptrdiff_t index[24];
                for (int c = 0; c < 8; ++c)
                    for (int d = 0; d < 3; ++d)
                        index[3 * c + d] = dense[3 * level.node(i + kHexCorner[c][0], j + kHexCorner[c][1], k + kHexCorner[c][2]) + std::size_t(d)];
                for (int r = 0; r < 24; ++r)
                    for (int c = 0; c < 24; ++c)
                        if (index[r] >= 0 && index[c] >= 0)
                            a[std::size_t(index[r]) * n + std::size_t(index[c])] += m[r][c];
            }
    // Cholesky, row by row of the lower triangle; a pivot that vanishes drops
    // its degree of freedom.
    double largest = 0.0;
    for (std::size_t i = 0; i < n; ++i)
        largest = std::max(largest, a[i * n + i]);
    dropped_.assign(n, 0);
    for (std::size_t j = 0; j < n; ++j) {
        double d = a[j * n + j];
        for (std::size_t k = 0; k < j; ++k)
            d -= a[j * n + k] * a[j * n + k];
        if (d <= kDrop * largest) {
            dropped_[j] = 1;
            for (std::size_t i = j; i < n; ++i)
                a[i * n + j] = 0.0;
            continue;
        }
        const double pivot = std::sqrt(d);
        a[j * n + j] = pivot;
        tbb::parallel_for(j + 1, std::max(j + 1, n), [&](std::size_t i) {
            double s = a[i * n + j];
            for (std::size_t k = 0; k < j; ++k)
                s -= a[i * n + k] * a[j * n + k];
            a[i * n + j] = s / pivot;
        });
    }
}

void Multigrid::solve_coarsest(const std::vector<double>& r, std::vector<double>& e) const {
    const std::size_t n = dense_dofs_.size();
    std::vector<double> y(n);
    for (std::size_t i = 0; i < n; ++i) {
        if (dropped_[i]) {
            y[i] = 0.0;
            continue;
        }
        double s = r[dense_dofs_[i]];
        for (std::size_t k = 0; k < i; ++k)
            s -= cholesky_[i * n + k] * y[k];
        y[i] = s / cholesky_[i * n + i];
    }
    for (std::size_t i = n; i-- > 0;) {
        if (dropped_[i]) {
            y[i] = 0.0;
            continue;
        }
        double s = y[i];
        for (std::size_t k = i + 1; k < n; ++k)
            s -= cholesky_[k * n + i] * y[k];
        y[i] = s / cholesky_[i * n + i];
    }
    std::fill(e.begin(), e.end(), 0.0);
    for (std::size_t i = 0; i < n; ++i)
        e[dense_dofs_[i]] = y[i];
}

void Multigrid::cycle(std::size_t l) {
    Level& level = levels_[l];
    if (l + 1 == levels_.size()) {
        solve_coarsest(level.r, level.x);
        return;
    }
    const std::size_t dofs = level.r.size();
    // Chebyshev smoothing of K x = r preconditioned by D, kDegree steps from
    // the current x (the Chebyshev acceleration: Saad 2003, Ch. 12).
    // It is a fixed polynomial in D^-1 K, so the same smoothing before and
    // after keeps the cycle symmetric.
    auto smooth = [&](bool from_zero) {
        const double b = 1.1 * level.lambda, a = b / kRatio;
        const double theta = 0.5 * (b + a), delta = 0.5 * (b - a), sigma = theta / delta;
        double rho = 1.0 / sigma;
        if (from_zero)
            std::copy(level.r.begin(), level.r.end(), level.s.begin());
        else {
            apply(l, level.x, level.t);
            tbb::parallel_for(std::size_t(0), dofs, [&](std::size_t q) { level.s[q] = level.free[q] ? level.r[q] - level.t[q] : 0.0; });
        }
        tbb::parallel_for(std::size_t(0), dofs, [&](std::size_t q) { level.d[q] = level.free[q] ? level.s[q] / (theta * level.diagonal[q]) : 0.0; });
        for (int step = 1; step <= kDegree; ++step) {
            tbb::parallel_for(std::size_t(0), dofs, [&](std::size_t q) { level.x[q] += level.d[q]; });
            if (step == kDegree)
                break;
            apply(l, level.d, level.t);
            const double rho_next = 1.0 / (2.0 * sigma - rho);
            tbb::parallel_for(std::size_t(0), dofs, [&](std::size_t q) {
                level.s[q] -= level.t[q];
                level.d[q] = level.free[q] ? rho_next * rho * level.d[q] + 2.0 * rho_next / delta * level.s[q] / level.diagonal[q] : 0.0;
            });
            rho = rho_next;
        }
    };
    std::fill(level.x.begin(), level.x.end(), 0.0);
    smooth(true);

    // Restrict the residual: r_c = P^T (r - K e), coarse node by coarse node.
    apply(l, level.x, level.t);
    tbb::parallel_for(std::size_t(0), dofs, [&](std::size_t q) { level.t[q] = level.free[q] ? level.r[q] - level.t[q] : 0.0; });
    Level& coarse = levels_[l + 1];
    auto weight = [&](int axis, int fi, int cn) {
        const int lo = coarse.lower[std::size_t(axis)][std::size_t(fi)];
        const double w = coarse.upper_weight[std::size_t(axis)][std::size_t(fi)];
        return (lo == cn ? 1.0 - w : 0.0) + (lo + 1 == cn ? w : 0.0);
    };
    tbb::parallel_for(tbb::blocked_range<std::size_t>(0, coarse.nodes(), kGrain), [&](const tbb::blocked_range<std::size_t>& range) {
        for (std::size_t cn = range.begin(); cn != range.end(); ++cn) {
            const int c[3] = {int(cn % std::size_t(coarse.n[0])), int(cn / std::size_t(coarse.n[0]) % std::size_t(coarse.n[1])),
                              int(cn / (std::size_t(coarse.n[0]) * std::size_t(coarse.n[1])))};
            int lo[3], hi[3];
            for (int axis = 0; axis < 3; ++axis) {
                const int f = coarse.factor[std::size_t(axis)];
                lo[axis] = std::max(0, f * c[axis] - (f - 1));
                hi[axis] = std::min(level.n[std::size_t(axis)] - 1, f * c[axis] + (f - 1));
            }
            double acc[3] = {0.0, 0.0, 0.0};
            for (int k = lo[2]; k <= hi[2]; ++k) {
                const double wz = weight(2, k, c[2]);
                if (wz == 0.0)
                    continue;
                for (int j = lo[1]; j <= hi[1]; ++j) {
                    const double wy = wz * weight(1, j, c[1]);
                    if (wy == 0.0)
                        continue;
                    for (int i = lo[0]; i <= hi[0]; ++i) {
                        const double w = wy * weight(0, i, c[0]);
                        if (w == 0.0)
                            continue;
                        const double* t = &level.t[3 * level.node(i, j, k)];
                        acc[0] += w * t[0], acc[1] += w * t[1], acc[2] += w * t[2];
                    }
                }
            }
            for (int d = 0; d < 3; ++d)
                coarse.r[3 * cn + std::size_t(d)] = coarse.free[3 * cn + std::size_t(d)] ? acc[d] : 0.0;
        }
    });

    cycle(l + 1);

    // Interpolate the coarse correction: e += P e_c, fine node by fine node.
    tbb::parallel_for(tbb::blocked_range<std::size_t>(0, level.nodes(), kGrain), [&](const tbb::blocked_range<std::size_t>& range) {
        for (std::size_t n = range.begin(); n != range.end(); ++n) {
            const int f[3] = {int(n % std::size_t(level.n[0])), int(n / std::size_t(level.n[0]) % std::size_t(level.n[1])),
                              int(n / (std::size_t(level.n[0]) * std::size_t(level.n[1])))};
            double acc[3] = {0.0, 0.0, 0.0};
            for (int bz = 0; bz < 2; ++bz) {
                const int ck = coarse.lower[2][std::size_t(f[2])] + bz;
                const double wz = bz ? coarse.upper_weight[2][std::size_t(f[2])] : 1.0 - coarse.upper_weight[2][std::size_t(f[2])];
                if (wz == 0.0)
                    continue;
                for (int by = 0; by < 2; ++by) {
                    const int cj = coarse.lower[1][std::size_t(f[1])] + by;
                    const double wy = wz * (by ? coarse.upper_weight[1][std::size_t(f[1])] : 1.0 - coarse.upper_weight[1][std::size_t(f[1])]);
                    if (wy == 0.0)
                        continue;
                    for (int bx = 0; bx < 2; ++bx) {
                        const int ci = coarse.lower[0][std::size_t(f[0])] + bx;
                        const double w = wy * (bx ? coarse.upper_weight[0][std::size_t(f[0])] : 1.0 - coarse.upper_weight[0][std::size_t(f[0])]);
                        if (w == 0.0)
                            continue;
                        const double* e = &coarse.x[3 * coarse.node(ci, cj, ck)];
                        acc[0] += w * e[0], acc[1] += w * e[1], acc[2] += w * e[2];
                    }
                }
            }
            for (int d = 0; d < 3; ++d)
                if (level.free[3 * n + std::size_t(d)])
                    level.x[3 * n + std::size_t(d)] += acc[d];
        }
    });

    smooth(false);
}

void Multigrid::cycle(const std::vector<double>& r, std::vector<double>& z) {
    Level& top = levels_[0];
    std::copy(r.begin(), r.end(), top.r.begin());
    for (std::size_t q = 0; q < top.r.size(); ++q)
        if (!top.free[q])
            top.r[q] = 0.0;
    cycle(0);
    std::copy(top.x.begin(), top.x.end(), z.begin());
}

} // namespace tisma
