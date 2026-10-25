// Z anti-aliasing: the spec's rules (docs/spec/features/z_antialiasing.md)
// and the raise of Song, Ray, Sokolov and Lefebvre 2017 (Section 3.1); the
// surface index by cells of XY and the splitting are own design. See
// PROVENANCE.md.

#include "slice/zaa.hpp"

#include <oneapi/tbb/parallel_for.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <limits>

namespace tisma {

SurfaceIndex::SurfaceIndex(const IndexedMesh& mesh) {
    double x1 = 0, y1 = 0;
    for (const auto& t : mesh.triangles) {
        const Vec3f& a = mesh.vertices[t[0]];
        const Vec3f& b = mesh.vertices[t[1]];
        const Vec3f& c = mesh.vertices[t[2]];
        const double nz = (double(b.x) - a.x) * (double(c.y) - a.y) - (double(b.y) - a.y) * (double(c.x) - a.x);
        if (!(nz > 0.0))
            continue; // facing down, or standing: no surface seen from above
        Face f{{a.x, b.x, c.x}, {a.y, b.y, c.y}, {a.z, b.z, c.z}, std::min({a.z, b.z, c.z}), std::max({a.z, b.z, c.z})};
        const double lo_x = std::min({a.x, b.x, c.x}), lo_y = std::min({a.y, b.y, c.y});
        const double hi_x = std::max({a.x, b.x, c.x}), hi_y = std::max({a.y, b.y, c.y});
        if (faces_.empty())
            x0_ = lo_x, y0_ = lo_y, x1 = hi_x, y1 = hi_y;
        x0_ = std::min(x0_, lo_x), y0_ = std::min(y0_, lo_y), x1 = std::max(x1, hi_x), y1 = std::max(y1, hi_y);
        faces_.push_back(f);
    }
    if (faces_.empty())
        return;
    // About two faces per cell, no finer than 0.1 mm, at most 4 million cells.
    const double w = std::max(x1 - x0_, 1e-3), h = std::max(y1 - y0_, 1e-3);
    cell_ = std::max({std::sqrt(w * h / double(faces_.size()) * 2.0), 0.1, std::sqrt(w * h / 4e6)});
    nx_ = std::size_t(w / cell_) + 1;
    ny_ = std::size_t(h / cell_) + 1;
    const auto cells_of = [&](const Face& f, auto&& visit) {
        const auto cx = [&](double x) { return std::min(nx_ - 1, std::size_t(std::max(0.0, (x - x0_) / cell_))); };
        const auto cy = [&](double y) { return std::min(ny_ - 1, std::size_t(std::max(0.0, (y - y0_) / cell_))); };
        const std::size_t ax = cx(std::min({f.x[0], f.x[1], f.x[2]})), bx = cx(std::max({f.x[0], f.x[1], f.x[2]}));
        const std::size_t ay = cy(std::min({f.y[0], f.y[1], f.y[2]})), by = cy(std::max({f.y[0], f.y[1], f.y[2]}));
        for (std::size_t j = ay; j <= by; ++j)
            for (std::size_t i = ax; i <= bx; ++i)
                visit(j * nx_ + i);
    };
    start_.assign(nx_ * ny_ + 1, 0);
    for (const Face& f : faces_)
        cells_of(f, [&](std::size_t c) { ++start_[c + 1]; });
    for (std::size_t c = 0; c < nx_ * ny_; ++c)
        start_[c + 1] += start_[c];
    items_.resize(start_.back());
    std::vector<std::uint32_t> fill(start_.begin(), start_.end() - 1);
    for (std::uint32_t k = 0; k < faces_.size(); ++k)
        cells_of(faces_[k], [&](std::size_t c) { items_[fill[c]++] = k; });
}

double SurfaceIndex::surface(double x, double y, double low, double high, double near) const {
    double best = std::numeric_limits<double>::quiet_NaN();
    if (faces_.empty() || x < x0_ || y < y0_)
        return best;
    const auto i = std::size_t((x - x0_) / cell_), j = std::size_t((y - y0_) / cell_);
    if (i >= nx_ || j >= ny_)
        return best;
    const std::size_t c = j * nx_ + i;
    for (std::uint32_t k = start_[c]; k < start_[c + 1]; ++k) {
        const Face& f = faces_[items_[k]];
        if (f.z_high < low || f.z_low > high)
            continue;
        // Barycentric coordinates in XY; a point on an edge belongs to both faces.
        const double ax = f.x[0], ay = f.y[0];
        const double ux = f.x[1] - ax, uy = f.y[1] - ay, vx = f.x[2] - ax, vy = f.y[2] - ay;
        const double det = ux * vy - uy * vx;
        if (!(det > 0.0))
            continue;
        const double s = ((x - ax) * vy - (y - ay) * vx) / det;
        const double t = (ux * (y - ay) - uy * (x - ax)) / det;
        constexpr double kEdge = -1e-9;
        if (s < kEdge || t < kEdge || s + t > 1.0 - kEdge)
            continue;
        const double z = f.z[0] + s * (double(f.z[1]) - f.z[0]) + t * (double(f.z[2]) - f.z[0]);
        if (z < low || z > high)
            continue;
        if (std::isnan(best) || std::abs(z - near) < std::abs(best - near))
            best = z;
    }
    return best;
}

namespace {

// The point's height from the layer's top: down to the surface within the
// layer, never below the thinnest share; up to it above the top (raise).
double drop_at(const SurfaceIndex& surface, double x, double y, double bottom, double top, const ZaaParams& params) {
    const double h = top - bottom;
    const double ceiling = params.raise ? top + (params.above >= 0.0 ? params.above : 0.5 * h) : top;
    const double z = surface.surface(x, y, bottom, ceiling, top);
    if (std::isnan(z))
        return 0.0;
    const double floor = top - (1.0 - params.min_thickness) * h;
    return std::clamp(z, floor, ceiling) - top;
}

} // namespace

Toolpath antialias_path(const Toolpath& path, const SurfaceIndex& surface, double bottom, double top,
                        const ZaaParams& params) {
    Toolpath out;
    out.closed = path.closed;
    out.index = path.index;
    const std::size_t n = path.points.size();
    if (n == 0 || surface.empty())
        return path;
    out.points.reserve(n);
    std::vector<ToolpathPoint> samples;
    const std::size_t segments = path.closed ? n : n - 1;
    ToolpathPoint first = path.points.front();
    first.dz = drop_at(surface, first.p.x, first.p.y, bottom, top, params);
    out.points.push_back(first);
    ToolpathPoint from = first;
    for (std::size_t s = 0; s < segments; ++s) {
        const ToolpathPoint& next = path.points[(s + 1) % n];
        ToolpathPoint to = next;
        to.dz = (s + 1 == n) ? first.dz : drop_at(surface, to.p.x, to.p.y, bottom, top, params);
        const double length = std::hypot(to.p.x - from.p.x, to.p.y - from.p.y);
        const auto pieces = std::size_t(std::ceil(length / params.step));
        samples.clear();
        bool off_top = from.dz != 0.0 || to.dz != 0.0;
        for (std::size_t k = 1; k < pieces; ++k) {
            const double f = double(k) / double(pieces);
            ToolpathPoint q{{from.p.x + f * (to.p.x - from.p.x), from.p.y + f * (to.p.y - from.p.y)},
                            from.width + f * (to.width - from.width)};
            q.dz = drop_at(surface, q.p.x, q.p.y, bottom, top, params);
            off_top = off_top || q.dz != 0.0;
            samples.push_back(q);
        }
        if (off_top)
            out.points.insert(out.points.end(), samples.begin(), samples.end());
        if (s + 1 < n)
            out.points.push_back(to);
        from = to;
    }
    return out;
}

std::size_t antialias_layers(const std::vector<Layer>& layers, std::vector<LayerWalls>& walls,
                             std::vector<LayerFill>& fills, const std::vector<LayerGroup>& groups,
                             std::vector<ThickLayer>& thick, const SurfaceIndex& surface, const ZaaParams& params) {
    std::atomic<std::size_t> moved{0};
    // The bottom of each thick layer: its group's first layer.
    std::vector<double> thick_bottom(layers.size(), 0.0);
    for (const LayerGroup& g : groups)
        if (g.last < layers.size())
            thick_bottom[g.last] = layers[g.first].bottom;
    // The raise stops at the next layer's slice plane, half its height above
    // this top: past it, the next layer prints the same surface again (a thin
    // layer after a thick one, the fine layers after a thick zone).
    const auto next_height = [&](std::size_t l, double own) {
        return l + 1 < layers.size() ? layers[l + 1].top - layers[l + 1].bottom : own;
    };
    const auto paths = [&](std::vector<Toolpath>& list, double bottom, double top, double above) {
        ZaaParams capped = params;
        capped.above = above;
        std::size_t count = 0;
        for (Toolpath& t : list) {
            t = antialias_path(t, surface, bottom, top, capped);
            for (const ToolpathPoint& p : t.points)
                count += p.dz != 0.0;
        }
        return count;
    };
    tbb::parallel_for(std::size_t(0), layers.size(), [&](std::size_t l) {
        const double bottom = layers[l].bottom, top = layers[l].top;
        const double above = 0.5 * std::min(top - bottom, next_height(l, top - bottom));
        std::size_t count = 0;
        if (l < walls.size())
            count += paths(walls[l].toolpaths, bottom, top, above);
        if (l < fills.size()) {
            count += paths(fills[l].top, bottom, top, above);
            count += paths(fills[l].gaps, bottom, top, above);
        }
        if (l < thick.size() && !thick[l].empty()) {
            ThickLayer& t = thick[l];
            const double thick_above = 0.5 * std::min(top - thick_bottom[l], next_height(l, top - thick_bottom[l]));
            count += paths(t.walls, thick_bottom[l], top, thick_above);
            count += paths(t.fill.top, thick_bottom[l], top, thick_above);
            count += paths(t.fill.gaps, thick_bottom[l], top, thick_above);
        }
        moved += count;
    });
    return moved.load();
}

} // namespace tisma
