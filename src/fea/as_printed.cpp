// The part as printed for the finite element analysis: line directions per
// voxel from the toolpaths. Own design; see PROVENANCE.md.

#include "fea/as_printed.hpp"

#include "slice/pipeline.hpp"

#include <oneapi/tbb/parallel_for.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>

namespace tisma {
namespace {

// Lines side by side touch at their half width: a voxel centre there belongs
// to either.
constexpr double kTouch = 1e-6; // mm

// The layer whose span holds z, or -1.
int layer_at(const std::vector<Layer>& layers, double z) {
    const auto it = std::upper_bound(layers.begin(), layers.end(), z, [](double v, const Layer& l) { return v < l.top; });
    return it != layers.end() && z >= it->bottom ? int(it - layers.begin()) : -1;
}

} // namespace

std::vector<float> line_directions(const VoxelGrid& g, const SlicedModel& model) {
    const std::size_t plane = std::size_t(g.nx) * std::size_t(g.ny);
    std::vector<float> out(g.solid.size(), std::numeric_limits<float>::quiet_NaN());
    // The thick layer whose height spans each layer (combined infill, layer
    // heights by zone): printed at the top of its last layer, it fills them all.
    std::vector<int> thick_over(model.layers.size(), -1);
    for (std::size_t m = 0; m < model.thick.size() && m < model.layers.size(); ++m) {
        if (model.thick[m].empty())
            continue;
        const double bottom = model.layers[m].top - model.thick[m].height;
        for (std::size_t l = m + 1; l-- > 0 && model.layers[l].bottom >= bottom - 1e-9;)
            thick_over[l] = int(m);
    }
    tbb::parallel_for(0, g.nz, [&](int k) {
        const int l = layer_at(model.layers, 0.5 * (g.z[std::size_t(k)] + g.z[std::size_t(k) + 1]));
        if (l < 0)
            return;
        const std::size_t layer = std::size_t(l);
        std::vector<double> best(plane, std::numeric_limits<double>::infinity());
        float* direction = &out[std::size_t(k) * plane];
        auto paint = [&](const Toolpath& t) {
            const std::size_t n = t.points.size();
            for (std::size_t e = 0; e + 1 < n + (t.closed ? 1 : 0); ++e) {
                const ToolpathPoint& p = t.points[e];
                const ToolpathPoint& q = t.points[(e + 1) % n];
                const double dx = q.p.x - p.p.x, dy = q.p.y - p.p.y, len2 = dx * dx + dy * dy;
                if (len2 == 0.0)
                    continue;
                const double half = 0.25 * (p.width + q.width) + kTouch;
                double angle = std::atan2(dy, dx);
                if (angle < 0.0)
                    angle += std::numbers::pi;
                if (angle >= std::numbers::pi)
                    angle -= std::numbers::pi;
                const int i0 = std::max(0, int(std::floor((std::min(p.p.x, q.p.x) - half - g.x0) / g.hx)));
                const int i1 = std::min(g.nx - 1, int(std::floor((std::max(p.p.x, q.p.x) + half - g.x0) / g.hx)));
                const int j0 = std::max(0, int(std::floor((std::min(p.p.y, q.p.y) - half - g.y0) / g.hy)));
                const int j1 = std::min(g.ny - 1, int(std::floor((std::max(p.p.y, q.p.y) + half - g.y0) / g.hy)));
                for (int j = j0; j <= j1; ++j)
                    for (int i = i0; i <= i1; ++i) {
                        const double cx = g.x0 + (i + 0.5) * g.hx, cy = g.y0 + (j + 0.5) * g.hy;
                        const double s = std::clamp(((cx - p.p.x) * dx + (cy - p.p.y) * dy) / len2, 0.0, 1.0);
                        const double d = std::hypot(cx - p.p.x - s * dx, cy - p.p.y - s * dy);
                        const std::size_t v = std::size_t(j) * std::size_t(g.nx) + std::size_t(i);
                        if (d <= half && d < best[v])
                            best[v] = d, direction[v] = float(angle);
                    }
            }
        };
        if (layer < model.walls.size())
            for (const Toolpath& t : model.walls[layer].toolpaths)
                paint(t);
        if (layer < model.fills.size()) {
            const LayerFill& f = model.fills[layer];
            for (const auto* group : {&f.top, &f.solid, &f.sparse, &f.bridge, &f.gaps})
                for (const Toolpath& t : *group)
                    paint(t);
        }
        if (const int m = thick_over[layer]; m >= 0) {
            const ThickLayer& t = model.thick[std::size_t(m)];
            for (const Toolpath& w : t.walls)
                paint(w);
            for (const auto* group : {&t.fill.top, &t.fill.solid, &t.fill.sparse, &t.fill.bridge, &t.fill.gaps})
                for (const Toolpath& p : *group)
                    paint(p);
        }
    });
    return out;
}

VoxelGrid printed_voxels(VoxelGrid grid, std::span<const float> direction) {
    for (std::size_t v = 0; v < grid.solid.size(); ++v)
        if (v >= direction.size() || std::isnan(direction[v]))
            grid.solid[v] = 0;
    return grid;
}

} // namespace tisma
