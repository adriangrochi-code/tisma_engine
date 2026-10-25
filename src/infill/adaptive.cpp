// Adaptive cubic infill. The distance transform follows Felzenszwalb and
// Huttenlocher 2012; the rest is own design. See PROVENANCE.md.

#include "infill/adaptive.hpp"

#include "infill/rectilinear.hpp"

#include <oneapi/tbb/blocked_range.h>
#include <oneapi/tbb/parallel_for.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <numbers>
#include <utility>

namespace tisma {

namespace {

using Clipper2Lib::Point64;

constexpr float kFar = 1e20f; // a set voxel before any unset one reaches it
constexpr int kMaxLevel = 5;  // cubes up to 32 times the finest

// out[q] = min over p of (q - p)^2 + f[p]: the lower envelope of the
// parabolas rooted at each p (Felzenszwalb and Huttenlocher 2012, section
// 2.1). v holds the envelope's parabolas, b the bounds between them.
void transform_line(const std::vector<double>& f, std::vector<double>& out, std::vector<int>& v,
                    std::vector<double>& b) {
    const int n = int(f.size());
    v.assign(std::size_t(n), 0);
    b.assign(std::size_t(n) + 1, 0.0);
    const double inf = std::numeric_limits<double>::infinity();
    int k = 0;
    b[0] = -inf;
    b[1] = inf;
    const auto meet = [&](int q, int p) {
        return ((f[std::size_t(q)] + double(q) * q) - (f[std::size_t(p)] + double(p) * p)) / (2.0 * (q - p));
    };
    for (int q = 1; q < n; ++q) {
        double s = meet(q, v[std::size_t(k)]);
        while (s <= b[std::size_t(k)]) {
            --k;
            s = meet(q, v[std::size_t(k)]);
        }
        ++k;
        v[std::size_t(k)] = q;
        b[std::size_t(k)] = s;
        b[std::size_t(k) + 1] = inf;
    }
    k = 0;
    for (int q = 0; q < n; ++q) {
        while (b[std::size_t(k) + 1] < q)
            ++k;
        const double d = q - v[std::size_t(k)];
        out[std::size_t(q)] = d * d + f[std::size_t(v[std::size_t(k)])];
    }
}

// One pass of the 1D transform along every line of `len` values `stride`
// apart; line i starts at start(i).
template <class Start>
void transform_lines(std::vector<float>& d, int len, std::size_t stride, std::size_t lines, Start start) {
    tbb::parallel_for(tbb::blocked_range<std::size_t>(0, lines), [&](const tbb::blocked_range<std::size_t>& r) {
        const auto n = std::size_t(len);
        std::vector<double> f(n), g(n), b;
        std::vector<int> v;
        for (std::size_t line = r.begin(); line != r.end(); ++line) {
            const std::size_t s0 = start(line);
            for (int q = 0; q < len; ++q)
                f[std::size_t(q)] = d[s0 + std::size_t(q) * stride];
            transform_line(f, g, v, b);
            for (int q = 0; q < len; ++q)
                d[s0 + std::size_t(q) * stride] = float(g[std::size_t(q)]);
        }
    });
}

// The cells of the field's plane whose centres the region covers (non-zero
// rule): each row's crossings with the region's edges, swept by winding.
void rasterize(const Polygons& region, const DepthField& f, std::vector<char>& out) {
    std::fill(out.begin(), out.end(), char(0));
    std::vector<std::vector<std::pair<double, int>>> rows(std::size_t(f.ny));
    for (const Polygon& path : region)
        for (std::size_t e = 0; e < path.size(); ++e) {
            const Point64& pa = path[e];
            const Point64& pb = path[(e + 1) % path.size()];
            if (pa.y == pb.y)
                continue;
            const double ax = to_mm(pa.x), ay = to_mm(pa.y), bx = to_mm(pb.x), by = to_mm(pb.y);
            const double lo = std::min(ay, by), hi = std::max(ay, by);
            const int j0 = std::max(0, int(std::ceil((lo - f.y0) / f.cell - 0.5)));
            const int j1 = std::min(f.ny - 1, int(std::ceil((hi - f.y0) / f.cell - 0.5)));
            for (int j = j0; j <= j1; ++j) {
                const double yc = f.y0 + (j + 0.5) * f.cell;
                if (yc < lo || yc >= hi)
                    continue;
                rows[std::size_t(j)].push_back({ax + (yc - ay) * (bx - ax) / (by - ay), by > ay ? 1 : -1});
            }
        }
    for (int j = 0; j < f.ny; ++j) {
        auto& row = rows[std::size_t(j)];
        std::sort(row.begin(), row.end());
        int winding = 0;
        for (std::size_t c = 0; c + 1 < row.size(); ++c) {
            winding += row[c].second;
            if (winding == 0)
                continue;
            const int i0 = std::max(0, int(std::ceil((row[c].first - f.x0) / f.cell - 0.5)));
            const int i1 = std::min(f.nx - 1, int(std::ceil((row[c + 1].first - f.x0) / f.cell - 0.5)) - 1);
            for (int i = i0; i <= i1; ++i)
                out[std::size_t(j) * std::size_t(f.nx) + std::size_t(i)] = 1;
        }
    }
}

} // namespace

std::vector<float> distance_transform(const std::vector<char>& set, int nx, int ny, int nz) {
    const std::size_t n = std::size_t(nx) * std::size_t(ny) * std::size_t(nz);
    std::vector<float> d(n);
    for (std::size_t i = 0; i < n; ++i)
        d[i] = set[i] != 0 ? kFar : 0.0f;
    if (n == 0)
        return d;
    // Squared distances add up over the axes, so one pass per axis is exact (section 3).
    const std::size_t sx = std::size_t(nx), sxy = std::size_t(nx) * std::size_t(ny);
    transform_lines(d, nx, 1, std::size_t(ny) * std::size_t(nz), [&](std::size_t line) { return line * sx; });
    transform_lines(d, ny, sx, sx * std::size_t(nz),
                    [&](std::size_t line) { return (line / sx) * sxy + line % sx; });
    transform_lines(d, nz, sxy, sxy, [](std::size_t line) { return line; });
    for (std::size_t i = 0; i < n; ++i)
        d[i] = set[i] != 0 ? std::sqrt(d[i]) : 0.0f;
    return d;
}

DepthField sparse_depth(const std::vector<Polygons>& sparse, const std::vector<Layer>& layers, double cell,
                        DepthFrom from, std::size_t max_voxels) {
    DepthField f;
    const std::size_t count = std::min(sparse.size(), layers.size());
    std::int64_t xmin = INT64_MAX, xmax = INT64_MIN, ymin = INT64_MAX, ymax = INT64_MIN;
    for (std::size_t l = 0; l < count; ++l)
        for (const Polygon& path : sparse[l])
            for (const Point64& p : path) {
                xmin = std::min(xmin, p.x), xmax = std::max(xmax, p.x);
                ymin = std::min(ymin, p.y), ymax = std::max(ymax, p.y);
            }
    if (xmin > xmax || !(cell > 0.0) || max_voxels == 0)
        return f;
    const double w = to_mm(xmax - xmin), h = to_mm(ymax - ymin);
    const double bottom = layers.front().bottom, t = layers[count - 1].top - bottom;
    // One voxel of outside all round.
    const auto voxels = [&](double c) {
        return (std::ceil(w / c) + 2.0) * (std::ceil(h / c) + 2.0) * (std::ceil(t / c) + 2.0);
    };
    while (voxels(cell) > double(max_voxels))
        cell *= 1.25;
    f.cell = cell;
    f.nx = int(std::ceil(w / cell)) + 2;
    f.ny = int(std::ceil(h / cell)) + 2;
    f.nz = int(std::ceil(t / cell)) + 2;
    f.x0 = to_mm(xmin) - cell;
    f.y0 = to_mm(ymin) - cell;
    f.z0 = bottom - cell;
    const std::size_t plane = std::size_t(f.nx) * std::size_t(f.ny);
    std::vector<char> inside(plane * std::size_t(f.nz), 0);
    tbb::parallel_for(1, f.nz - 1, [&](int k) {
        const double za = f.z0 + k * cell, zb = za + cell;
        constexpr double kHair = 1e-6; // layers touching the slab only at an edge do not count
        auto l = std::size_t(std::upper_bound(layers.begin(), layers.begin() + std::ptrdiff_t(count), za + kHair,
                                              [](double z, const Layer& layer) { return z < layer.top; }) -
                             layers.begin());
        std::vector<char> slab(plane, 1), here(plane);
        bool any = false;
        for (; l < count && layers[l].bottom < zb - kHair; ++l) {
            rasterize(sparse[l], f, here);
            for (std::size_t c = 0; c < plane; ++c)
                slab[c] = char(slab[c] & here[c]);
            any = true;
        }
        if (any)
            std::copy(slab.begin(), slab.end(), inside.begin() + std::ptrdiff_t(std::size_t(k) * plane));
    });
    if (from == DepthFrom::Surfaces) {
        f.depth = distance_transform(inside, f.nx, f.ny, f.nz);
    } else {
        // What the infill holds up: the voxels right above it, outside it.
        std::vector<char> far(inside.size(), 1);
        for (std::size_t c = plane; c < inside.size(); ++c)
            if (inside[c] == 0 && inside[c - plane] != 0)
                far[c] = 0;
        f.depth = distance_transform(far, f.nx, f.ny, f.nz);
        // And the walls, in each voxel's plane.
        tbb::parallel_for(0, f.nz, [&](int k) {
            const auto first = inside.begin() + std::ptrdiff_t(std::size_t(k) * plane);
            const std::vector<float> side =
                distance_transform(std::vector<char>(first, first + std::ptrdiff_t(plane)), f.nx, f.ny, 1);
            for (std::size_t c = 0; c < plane; ++c)
                f.depth[std::size_t(k) * plane + c] = std::min(f.depth[std::size_t(k) * plane + c], side[c]);
        });
    }
    // From a voxel's centre to the nearest outside one's, less half a voxel:
    // the boundary lies between them.
    for (std::size_t c = 0; c < f.depth.size(); ++c)
        if (inside[c] != 0) {
            f.depth[c] = float(cell * (f.depth[c] - 0.5));
            f.deepest = std::max(f.deepest, double(f.depth[c]));
        } else {
            f.depth[c] = 0.0f;
        }
    return f;
}

double DepthField::at(double x, double y, double z) const {
    if (depth.empty())
        return 0.0;
    const double u = (x - x0) / cell - 0.5, v = (y - y0) / cell - 0.5, w = (z - z0) / cell - 0.5;
    const int i = int(std::floor(u)), j = int(std::floor(v)), k = int(std::floor(w));
    const double fu = u - i, fv = v - j, fw = w - k;
    double sum = 0.0;
    for (int c = 0; c < 8; ++c) {
        const int a = i + (c & 1), b = j + ((c >> 1) & 1), d = k + ((c >> 2) & 1);
        if (a < 0 || b < 0 || d < 0 || a >= nx || b >= ny || d >= nz)
            continue;
        const double weight = ((c & 1) ? fu : 1.0 - fu) * (((c >> 1) & 1) ? fv : 1.0 - fv) * (((c >> 2) & 1) ? fw : 1.0 - fw);
        sum += weight * double(depth[index(a, b, d)]);
    }
    return sum;
}

Polygons deep_region(const DepthField& f, double z, double depth) {
    if (f.empty())
        return {};
    const int k = int(std::floor((z - f.z0) / f.cell));
    if (k < 0 || k >= f.nz)
        return {};
    std::vector<char> deep(std::size_t(f.nx) * std::size_t(f.ny));
    for (int j = 0; j < f.ny; ++j)
        for (int i = 0; i < f.nx; ++i)
            deep[std::size_t(j) * std::size_t(f.nx) + std::size_t(i)] = f.depth[f.index(i, j, k)] >= float(depth);
    return cell_region(deep, f.nx, f.ny, f.x0, f.y0, f.cell, f.cell);
}

double level_depth(int level, double spacing) {
    // The cubic infill's planes are spacing sqrt(2/3) apart, the edge of its
    // cubes; a cube's corners lie edge sqrt(3) / 2 from its centre.
    return std::ldexp(spacing, level) / std::numbers::sqrt2;
}

std::vector<Toolpath> adaptive_cubic_fill(const Polygons& region, const DepthField* field, double spacing,
                                          double width, double z) {
    std::vector<Toolpath> out;
    if (!(spacing > 0.0) || region.empty())
        return out;
    // The same number of levels in every layer, so each line keeps its level
    // all the way up.
    int levels = 0;
    if (field != nullptr && !field->empty())
        while (levels < kMaxLevel && level_depth(levels + 1, spacing) <= field->deepest)
            ++levels;
    const double period = std::ldexp(spacing, levels);
    const double shift = std::fmod(-z / std::numbers::sqrt2, period);
    // Line k of a family lies at k spacing + shift. It belongs to level r when
    // 2^r divides k and 2^(r + 1) does not (to the top level when 2^levels
    // does), and is drawn where the infill is shallower than level r + 1
    // starts: the lines of level r lie 2^(r + 1) spacings apart, starting
    // 2^r in. Where no level deeper fits, all the lines left at once.
    const auto lines = [&](const Polygons& part, double step, double base) {
        for (int k = 0; k < 3; ++k)
            for (Toolpath& t : rectilinear_fill(part, step, 60.0 * k, width, false, false, false, 0.0, base))
                out.push_back(std::move(t));
    };
    for (int r = 0; r <= levels; ++r) {
        const Polygons deep = r < levels ? deep_region(*field, z, level_depth(r + 1, spacing)) : Polygons{};
        if (deep.empty()) {
            lines(region, std::ldexp(spacing, r), shift);
            break;
        }
        const Polygons part = Clipper2Lib::Difference(region, deep, Clipper2Lib::FillRule::NonZero);
        if (!part.empty())
            lines(part, std::ldexp(spacing, r + 1), shift + std::ldexp(spacing, r));
    }
    return out;
}

} // namespace tisma
