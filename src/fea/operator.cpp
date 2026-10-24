// The voxels' stiffness matrix, applied without assembling it. Own design;
// see PROVENANCE.md.

#include "fea/operator.hpp"

#include <oneapi/tbb/blocked_range.h>
#include <oneapi/tbb/parallel_for.h>
#include <oneapi/tbb/parallel_reduce.h>

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace tisma {
namespace {

constexpr std::size_t kGrain = 4096;

} // namespace

double deterministic_dot(const std::vector<double>& a, const std::vector<double>& b) {
    return tbb::parallel_deterministic_reduce(
        tbb::blocked_range<std::size_t>(0, a.size(), kGrain), 0.0,
        [&](const tbb::blocked_range<std::size_t>& r, double s) {
            for (std::size_t i = r.begin(); i != r.end(); ++i)
                s += a[i] * b[i];
            return s;
        },
        [](double x, double y) { return x + y; });
}

VoxelOperator::VoxelOperator(const VoxelGrid& g, std::span<const Stiffness> kinds,
                             std::span<const std::uint16_t> kind_of, std::span<const double> scale)
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

void VoxelOperator::apply(const std::vector<double>& x, std::vector<double>& y) const {
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

std::vector<double> VoxelOperator::diagonal() const {
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

void VoxelOperator::stress(const std::vector<double>& u, int i, int j, int k, double out[6]) const {
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

} // namespace tisma
