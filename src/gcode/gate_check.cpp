#include "gcode/gate_check.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <unordered_set>

namespace tisma {

namespace {

bool part_kind(LineKind k) {
    switch (k) {
    case LineKind::Travel:
    case LineKind::SkirtBrim:
    case LineKind::Support:
    case LineKind::SupportInterface:
    case LineKind::WipeTower:
    case LineKind::Custom:
    case LineKind::Other:
        return false;
    default:
        return true;
    }
}

bool support_kind(LineKind k) { return k == LineKind::Support || k == LineKind::SupportInterface; }

// Cells of `cell` mm crossed by the moves, sampled every half cell.
struct CellSet {
    double cell;
    std::unordered_set<std::uint64_t> cells;
    static std::uint64_t key(std::int64_t i, std::int64_t j) {
        return (std::uint64_t(std::uint32_t(std::int32_t(i))) << 32) | std::uint32_t(std::int32_t(j));
    }
    void add(double x, double y) { cells.insert(key(std::int64_t(std::floor(x / cell)), std::int64_t(std::floor(y / cell)))); }
    void add_move(const PreviewMove& m) {
        const double len = std::hypot(m.x1 - m.x0, m.y1 - m.y0);
        const int n = std::max(1, int(std::ceil(len / (cell * 0.5))));
        for (int s = 0; s <= n; ++s) {
            const double t = double(s) / n;
            add(m.x0 + (m.x1 - m.x0) * t, m.y0 + (m.y1 - m.y0) * t);
        }
    }
    // Something within `reach` of (x, y), to a cell.
    bool near(double x, double y, double reach) const {
        const std::int64_t ci = std::int64_t(std::floor(x / cell)), cj = std::int64_t(std::floor(y / cell));
        const std::int64_t r = std::int64_t(std::ceil(reach / cell));
        for (std::int64_t i = ci - r; i <= ci + r; ++i)
            for (std::int64_t j = cj - r; j <= cj + r; ++j) {
                const double dx = std::max(0.0, std::abs((double(i) + 0.5) * cell - x) - cell * 0.5);
                const double dy = std::max(0.0, std::abs((double(j) + 0.5) * cell - y) - cell * 0.5);
                if (dx * dx + dy * dy <= reach * reach && cells.count(key(i, j)))
                    return true;
            }
        return false;
    }
};

} // namespace

std::array<double, 3> part_extent(const GcodePreview& p) {
    double lo[3] = {1e30, 1e30, 1e30}, hi[3] = {-1e30, -1e30, -1e30};
    bool any = false;
    for (std::uint32_t i : p.extrusions) {
        const PreviewMove& m = p.moves[i];
        if (!part_kind(m.kind))
            continue;
        any = true;
        const double w = m.width * 0.5;
        lo[0] = std::min({lo[0], m.x0 - w, m.x1 - w});
        hi[0] = std::max({hi[0], m.x0 + w, m.x1 + w});
        lo[1] = std::min({lo[1], m.y0 - w, m.y1 - w});
        hi[1] = std::max({hi[1], m.y0 + w, m.y1 + w});
        lo[2] = std::min({lo[2], double(m.z0) - m.height, double(m.z1) - m.height});
        hi[2] = std::max({hi[2], double(m.z0), double(m.z1)});
    }
    if (!any)
        return {0, 0, 0};
    return {hi[0] - lo[0], hi[1] - lo[1], hi[2] - std::min(lo[2], 0.0)};
}

double floating_support_share(const GcodePreview& p, double reach, std::vector<std::array<float, 3>>* where) {
    // The first layer that extrudes sits on the bed.
    std::size_t first = p.layers.size();
    for (std::size_t l = 0; l < p.layers.size(); ++l)
        if (p.layers[l].extrusion_count) {
            first = l;
            break;
        }
    const double cell = std::max(0.1, reach / 4.0);
    double total = 0.0, floating = 0.0;
    std::vector<CellSet> below; // per layer, built when needed
    below.reserve(p.layers.size());
    for (std::size_t l = 0; l < p.layers.size(); ++l) {
        CellSet s{cell, {}};
        const PreviewLayer& layer = p.layers[l];
        for (std::size_t e = 0; e < layer.extrusion_count; ++e)
            s.add_move(p.moves[p.extrusions[layer.first_extrusion + e]]);
        below.push_back(std::move(s));
    }
    for (std::size_t l = first + 1; l < p.layers.size(); ++l) {
        const PreviewLayer& layer = p.layers[l];
        const double bottom = layer.z - layer.height;
        for (std::size_t e = 0; e < layer.extrusion_count; ++e) {
            const PreviewMove& m = p.moves[p.extrusions[layer.first_extrusion + e]];
            if (!support_kind(m.kind))
                continue;
            const double len = std::hypot(m.x1 - m.x0, m.y1 - m.y0);
            const int n = std::max(1, int(std::ceil(len / 0.5)));
            for (int s = 0; s < n; ++s) {
                const double t = (s + 0.5) / n;
                const double x = m.x0 + (m.x1 - m.x0) * t, y = m.y0 + (m.y1 - m.y0) * t;
                bool held = false;
                // The layers under it whose top reaches its bottom, and at
                // least the one just below.
                for (std::size_t k = l; k-- > 0 && !held;) {
                    if (k + 1 < l && p.layers[k].z < bottom - 0.05)
                        break;
                    held = below[k].near(x, y, reach);
                }
                total += len / n;
                if (!held) {
                    floating += len / n;
                    if (where)
                        where->push_back({float(x), float(y), float(layer.z)});
                }
            }
        }
    }
    return total > 0.0 ? floating / total : 0.0;
}

std::vector<OverhangPoint> overhangs_without_support(const GcodePreview& p, const std::vector<OverhangPoint>& points,
                                                     double reach, double depth) {
    std::vector<OverhangPoint> bare;
    for (const OverhangPoint& q : points) {
        bool found = false;
        for (std::size_t l = 0; l < p.layers.size() && !found; ++l) {
            const PreviewLayer& layer = p.layers[l];
            if (layer.z > q.z + 0.01 || layer.z < q.z - depth)
                continue;
            for (std::size_t e = 0; e < layer.extrusion_count && !found; ++e) {
                const PreviewMove& m = p.moves[p.extrusions[layer.first_extrusion + e]];
                if (!support_kind(m.kind))
                    continue;
                // Distance from the point to the move, sideways.
                const double dx = m.x1 - m.x0, dy = m.y1 - m.y0, l2 = dx * dx + dy * dy;
                double t = l2 > 0.0 ? ((q.x - m.x0) * dx + (q.y - m.y0) * dy) / l2 : 0.0;
                t = std::clamp(t, 0.0, 1.0);
                found = std::hypot(m.x0 + dx * t - q.x, m.y0 + dy * t - q.y) <= reach;
            }
        }
        if (!found)
            bare.push_back(q);
    }
    return bare;
}

} // namespace tisma
