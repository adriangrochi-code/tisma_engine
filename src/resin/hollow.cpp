#include "resin/hollow.hpp"

#include "mesh/indexed_mesh.hpp"
#include "resin/resin.hpp"
#include "slice/level_set.hpp"

#include <tbb/blocked_range.h>
#include <tbb/parallel_for.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <numbers>

namespace tisma {
namespace {

// d[q] = min over p of (q - p)² + f[p], for the n values `stride` apart from
// `line`, in place: the lower envelope of the parabolas rooted at each p.
struct LineBuffers {
    std::vector<double> f, z;
    std::vector<int> v;
    explicit LineBuffers(std::size_t n) : f(n), z(n + 1), v(n) {}
};

void line_transform(double* line, std::size_t n, std::size_t stride, LineBuffers& b) {
    for (std::size_t q = 0; q < n; ++q)
        b.f[q] = line[q * stride];
    int k = 0;
    b.v[0] = 0;
    b.z[0] = -std::numeric_limits<double>::infinity();
    b.z[1] = std::numeric_limits<double>::infinity();
    auto meet = [&](int q, int p) {
        return ((b.f[std::size_t(q)] + double(q) * q) - (b.f[std::size_t(p)] + double(p) * p)) / (2.0 * (q - p));
    };
    for (int q = 1; q < int(n); ++q) {
        double s = meet(q, b.v[std::size_t(k)]);
        while (s <= b.z[std::size_t(k)]) {
            --k;
            s = meet(q, b.v[std::size_t(k)]);
        }
        ++k;
        b.v[std::size_t(k)] = q;
        b.z[std::size_t(k)] = s;
        b.z[std::size_t(k) + 1] = std::numeric_limits<double>::infinity();
    }
    k = 0;
    for (int q = 0; q < int(n); ++q) {
        while (b.z[std::size_t(k) + 1] < q)
            ++k;
        const int p = b.v[std::size_t(k)];
        line[std::size_t(q) * stride] = double(q - p) * (q - p) + b.f[std::size_t(p)];
    }
}

// Every line of the grid along one axis: `count` lines of n cells `stride`
// apart, the line i starting at first(i).
template <class First>
void transform_lines(std::vector<double>& cells, std::size_t count, std::size_t n, std::size_t stride, First first) {
    tbb::parallel_for(tbb::blocked_range<std::size_t>(0, count, 64), [&](const tbb::blocked_range<std::size_t>& r) {
        LineBuffers b(n);
        for (std::size_t i = r.begin(); i < r.end(); ++i)
            line_transform(cells.data() + first(i), n, stride, b);
    });
}

Polygon rect(double cx, double cy, double wx, double wy) {
    return {{to_units(cx - wx / 2), to_units(cy - wy / 2)},
            {to_units(cx + wx / 2), to_units(cy - wy / 2)},
            {to_units(cx + wx / 2), to_units(cy + wy / 2)},
            {to_units(cx - wx / 2), to_units(cy + wy / 2)}};
}

} // namespace

void distance_transform(std::vector<double>& cells, int nx, int ny, int nz) {
    const std::size_t x = std::size_t(nx), y = std::size_t(ny), z = std::size_t(nz);
    if (cells.size() != x * y * z || cells.empty())
        return;
    transform_lines(cells, y * z, x, 1, [&](std::size_t i) { return i * x; });
    transform_lines(cells, x * z, y, x, [&](std::size_t i) { return (i / x) * x * y + i % x; });
    transform_lines(cells, x * y, z, x * y, [&](std::size_t i) { return i; });
}

std::vector<Polygons> hollow_sections(const std::vector<Polygons>& sections, const std::vector<Layer>& layers,
                                      const Settings& s, double cell) {
    std::vector<Polygons> out(sections.size());
    double lo[3] = {1e300, 1e300, 1e300}, hi[3] = {-1e300, -1e300, -1e300};
    for (std::size_t l = 0; l < sections.size() && l < layers.size(); ++l) {
        if (sections[l].empty())
            continue;
        const Clipper2Lib::Rect64 b = Clipper2Lib::GetBounds(sections[l]);
        lo[0] = std::min(lo[0], to_mm(b.left)), hi[0] = std::max(hi[0], to_mm(b.right));
        lo[1] = std::min(lo[1], to_mm(b.top)), hi[1] = std::max(hi[1], to_mm(b.bottom));
        lo[2] = std::min(lo[2], layers[l].bottom), hi[2] = std::max(hi[2], layers[l].top);
    }
    if (!(hi[0] > lo[0]))
        return out;
    const double r = cell > 0.0 ? cell : std::clamp(std::max({hi[0] - lo[0], hi[1] - lo[1], hi[2] - lo[2]}) / 250.0, 0.1, 1.0);
    // One cell of air all round.
    const double x0 = lo[0] - r, y0 = lo[1] - r, z0 = lo[2] - r;
    const int nx = int(std::ceil((hi[0] - lo[0]) / r)) + 2, ny = int(std::ceil((hi[1] - lo[1]) / r)) + 2,
              nz = int(std::ceil((hi[2] - lo[2]) / r)) + 2;
    const std::size_t plane = std::size_t(nx) * std::size_t(ny);
    std::vector<double> inside(plane * std::size_t(nz), 0.0);
    PixelGrid grid;
    grid.width = nx;
    grid.height = ny;
    grid.left = x0;
    grid.top = y0 + ny * r;
    grid.pixel_x = grid.pixel_y = r;
    tbb::parallel_for(0, nz, [&](int k) {
        const double z = z0 + (k + 0.5) * r;
        const auto layer = std::upper_bound(layers.begin(), layers.end(), z,
                                            [](double v, const Layer& l) { return v < l.top; });
        if (layer == layers.end() || z < layer->bottom)
            return;
        const std::size_t l = std::size_t(layer - layers.begin());
        if (l >= sections.size())
            return;
        double* cells = inside.data() + std::size_t(k) * plane;
        coverage_rows(sections[l], grid, [&](int row, int x, std::span<const double> share) {
            for (std::size_t i = 0; i < share.size(); ++i)
                if (share[i] >= 0.5)
                    cells[std::size_t(ny - 1 - row) * std::size_t(nx) + std::size_t(x) + i] = kFar;
        });
    });
    // The distance from each cell inside to the air; the core, at least the
    // wall and the closing in (half a cell more, as the cells' centres lie
    // half a cell off the surface); grown back by the closing.
    std::vector<double> depth = inside;
    distance_transform(depth, nx, ny, nz);
    // The closing never takes away a hollow the wall leaves room for
    // (resin_supports.md, RS4): no more than the part's depth past the wall.
    const double deepest = std::sqrt(*std::max_element(depth.begin(), depth.end())) * r;
    const double wall = s.resin_hollow_wall;
    const double closing = std::clamp(deepest - wall - r, 0.0, s.resin_hollow_closing);
    const double core = (wall + closing + 0.5 * r) / r;
    std::vector<char> hollow(depth.size(), 0);
    if (closing > 0.0) {
        std::vector<double> to_core(depth.size());
        for (std::size_t i = 0; i < depth.size(); ++i)
            to_core[i] = depth[i] >= core * core ? 0.0 : kFar;
        distance_transform(to_core, nx, ny, nz);
        const double reach = closing / r;
        for (std::size_t i = 0; i < depth.size(); ++i)
            hollow[i] = inside[i] > 0.0 && to_core[i] <= reach * reach;
    } else {
        for (std::size_t i = 0; i < depth.size(); ++i)
            hollow[i] = depth[i] >= core * core;
    }
    tbb::parallel_for(std::size_t(0), std::min(sections.size(), layers.size()), [&](std::size_t l) {
        const int k = int(std::floor((layers[l].slice_z() - z0) / r));
        if (k < 0 || k >= nz)
            return;
        const std::vector<char> mask(hollow.begin() + std::ptrdiff_t(std::size_t(k) * plane),
                                     hollow.begin() + std::ptrdiff_t(std::size_t(k + 1) * plane));
        if (std::find(mask.begin(), mask.end(), 1) != mask.end())
            out[l] = cell_region(mask, nx, ny, x0, y0, r, r);
    });
    return out;
}

double infill_bar_width(const Settings& s) {
    return s.resin_hollow_infill_cell * std::sqrt(s.resin_hollow_infill / 100.0 / 5.0);
}

Polygons infill_bars(const Settings& s, double z, const double box[4]) {
    const double w = infill_bar_width(s), c = s.resin_hollow_infill_cell;
    if (!(w > 0.0))
        return {};
    const double diagonal = w * std::numbers::sqrt2;
    Polygons bars;
    const auto i0 = long(std::floor((box[0] - c) / c)), i1 = long(std::ceil((box[2] + c) / c));
    const auto j0 = long(std::floor((box[1] - c) / c)), j1 = long(std::ceil((box[3] + c) / c));
    const double shift = std::fmod(z, c);
    for (long i = i0; i <= i1; ++i)
        for (long j = j0; j <= j1; ++j) {
            const double x = double(i) * c, y = double(j) * c;
            bars.push_back(rect(x, y, w, w));
            bars.push_back(rect(x + shift, y, diagonal, w));
            bars.push_back(rect(x - shift, y, diagonal, w));
            bars.push_back(rect(x, y + shift, w, diagonal));
            bars.push_back(rect(x, y - shift, w, diagonal));
        }
    return Clipper2Lib::Union(bars, Clipper2Lib::FillRule::NonZero);
}

std::vector<Polygons> drain_sections(const std::vector<ResinDrain>& drains, const std::vector<Layer>& layers) {
    if (drains.empty() || layers.empty())
        return std::vector<Polygons>(layers.size());
    TriangleMesh m;
    constexpr int kSides = 24;
    for (const ResinDrain& d : drains) {
        const double len = std::sqrt(d.nx * d.nx + d.ny * d.ny + d.nz * d.nz);
        if (!(len > 0.0) || !(d.diameter > 0.0) || !(d.depth > 0.0))
            continue;
        const double n[3] = {d.nx / len, d.ny / len, d.nz / len};
        // Two directions across the axis.
        const double pick[3] = {std::abs(n[0]) < 0.9 ? 1.0 : 0.0, std::abs(n[0]) < 0.9 ? 0.0 : 1.0, 0.0};
        double u[3] = {n[1] * pick[2] - n[2] * pick[1], n[2] * pick[0] - n[0] * pick[2], n[0] * pick[1] - n[1] * pick[0]};
        const double ul = std::sqrt(u[0] * u[0] + u[1] * u[1] + u[2] * u[2]);
        for (double& c : u)
            c /= ul;
        const double v[3] = {n[1] * u[2] - n[2] * u[1], n[2] * u[0] - n[0] * u[2], n[0] * u[1] - n[1] * u[0]};
        // From half a millimetre outside the surface to `depth` in.
        const double out = 0.5, in = d.depth, radius = d.diameter / 2;
        auto at = [&](int i, double along) {
            const double a = 2.0 * std::numbers::pi * (i % kSides) / kSides;
            const double cu = radius * std::cos(a), cv = radius * std::sin(a);
            return Vec3f{float(d.x + n[0] * along + u[0] * cu + v[0] * cv),
                         float(d.y + n[1] * along + u[1] * cu + v[1] * cv),
                         float(d.z + n[2] * along + u[2] * cu + v[2] * cv)};
        };
        const Vec3f top{float(d.x + n[0] * out), float(d.y + n[1] * out), float(d.z + n[2] * out)};
        const Vec3f bottom{float(d.x - n[0] * in), float(d.y - n[1] * in), float(d.z - n[2] * in)};
        for (int i = 0; i < kSides; ++i) {
            m.add_triangle(top, at(i, out), at(i + 1, out));
            m.add_triangle(at(i, out), at(i, -in), at(i + 1, -in));
            m.add_triangle(at(i, out), at(i + 1, -in), at(i + 1, out));
            m.add_triangle(bottom, at(i + 1, -in), at(i, -in));
        }
    }
    if (m.empty())
        return std::vector<Polygons>(layers.size());
    IndexedMesh mesh = weld_exact(m);
    orient_consistently(mesh);
    std::vector<float> heights;
    for (const Layer& l : layers)
        heights.push_back(float(l.slice_z()));
    return layer_regions(slice_levels(mesh, height_field(mesh), heights));
}

} // namespace tisma
