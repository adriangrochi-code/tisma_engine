#include "slice/local_layers.hpp"

#include "geom/bead.hpp"
#include "slice/variable_layers.hpp"

#include <oneapi/tbb/parallel_for.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <utility>

namespace tisma {
namespace {

using Clipper2Lib::FillRule;

struct Edge {
    double ax, ay, bx, by;
};

// The zone's edges in millimetres, its box, and the edges each band of Y
// meets, so a test looks only at the edges of its bands.
struct Zone {
    std::vector<Edge> edges;
    double x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    bool empty = true;
    std::vector<std::vector<std::uint32_t>> bands;
    double band_height = 1.0;
    mutable std::vector<std::uint32_t> seen; // per edge: the last query that looked at it
    mutable std::uint32_t query = 0;

    explicit Zone(const Polygons& polygons) {
        for (const Polygon& poly : polygons)
            for (std::size_t i = 0; i < poly.size(); ++i) {
                const auto& a = poly[i];
                const auto& b = poly[(i + 1) % poly.size()];
                edges.push_back({to_mm(a.x), to_mm(a.y), to_mm(b.x), to_mm(b.y)});
                const double x = to_mm(a.x), y = to_mm(a.y);
                if (empty) {
                    x0 = x1 = x, y0 = y1 = y, empty = false;
                } else {
                    x0 = std::min(x0, x), x1 = std::max(x1, x), y0 = std::min(y0, y), y1 = std::max(y1, y);
                }
            }
        if (empty)
            return;
        const std::size_t count = std::clamp<std::size_t>(edges.size() / 4, 1, 1024);
        band_height = std::max((y1 - y0) / double(count), 1e-9);
        bands.resize(count);
        for (std::size_t e = 0; e < edges.size(); ++e) {
            const auto [lo, hi] = band_range(std::min(edges[e].ay, edges[e].by), std::max(edges[e].ay, edges[e].by));
            for (std::size_t b = lo; b <= hi; ++b)
                bands[b].push_back(std::uint32_t(e));
        }
        seen.assign(edges.size(), 0);
    }

    std::pair<std::size_t, std::size_t> band_range(double ya, double yb) const {
        const auto band = [&](double y) {
            return std::size_t(std::clamp((y - y0) / band_height, 0.0, double(bands.size() - 1)));
        };
        return {band(ya), band(yb)};
    }

    // Inside by the non-zero winding rule of the region: the edges of y's band
    // hold every edge a ray along x at y can meet.
    bool contains(double x, double y) const {
        if (empty || x < x0 || x > x1 || y < y0 || y > y1)
            return false;
        int winding = 0;
        for (const std::uint32_t i : bands[band_range(y, y).first]) {
            const Edge& e = edges[i];
            if (e.ay <= y) {
                if (e.by > y && (e.bx - e.ax) * (y - e.ay) - (x - e.ax) * (e.by - e.ay) > 0.0)
                    ++winding;
            } else if (e.by <= y && (e.bx - e.ax) * (y - e.ay) - (x - e.ax) * (e.by - e.ay) < 0.0) {
                --winding;
            }
        }
        return winding != 0;
    }

    // Where the segment p-q crosses an edge, as fractions of the way along it.
    void crossings(const Point2d& p, const Point2d& q, std::vector<double>& out) const {
        out.clear();
        if (empty || std::max(p.x, q.x) < x0 || std::min(p.x, q.x) > x1 || std::max(p.y, q.y) < y0 ||
            std::min(p.y, q.y) > y1)
            return;
        const double dx = q.x - p.x, dy = q.y - p.y;
        const auto [lo, hi] = band_range(std::min(p.y, q.y), std::max(p.y, q.y));
        if (++query == 0) { // wrapped: start the marks again
            std::fill(seen.begin(), seen.end(), 0u);
            query = 1;
        }
        for (std::size_t b = lo; b <= hi; ++b)
            for (const std::uint32_t i : bands[b]) {
                if (seen[i] == query)
                    continue;
                seen[i] = query;
                const Edge& e = edges[i];
                if (std::max(e.ax, e.bx) < std::min(p.x, q.x) || std::min(e.ax, e.bx) > std::max(p.x, q.x) ||
                    std::max(e.ay, e.by) < std::min(p.y, q.y) || std::min(e.ay, e.by) > std::max(p.y, q.y))
                    continue;
                const double ex = e.bx - e.ax, ey = e.by - e.ay;
                const double den = dx * ey - dy * ex;
                if (den == 0.0)
                    continue; // parallel: no single crossing
                const double s = ((e.ax - p.x) * ey - (e.ay - p.y) * ex) / den;
                const double u = ((e.ax - p.x) * dy - (e.ay - p.y) * dx) / den;
                if (s > 0.0 && s < 1.0 && u >= 0.0 && u <= 1.0)
                    out.push_back(s);
            }
        std::sort(out.begin(), out.end());
    }
};

ToolpathPoint between(const ToolpathPoint& a, const ToolpathPoint& b, double t) {
    return {{a.p.x + (b.p.x - a.p.x) * t, a.p.y + (b.p.y - a.p.y) * t}, a.width + (b.width - a.width) * t};
}

double length(const Toolpath& t) {
    double sum = 0.0;
    for (std::size_t i = 1; i < t.points.size(); ++i)
        sum += std::hypot(t.points[i].p.x - t.points[i - 1].p.x, t.points[i].p.y - t.points[i - 1].p.y);
    return sum;
}

} // namespace

int detail_ratio(const Settings& s) {
    // No sub-layer thinner than a printer lays down (and than the G-code's
    // ;HEIGHT: carries well with three decimals).
    return sub_layers(s.layer_height, s.detail_height, kThinnestSubLayer);
}

std::vector<ThickLayer> split_thick_zones(const std::vector<LayerGroup>& groups, std::vector<LayerWalls>& walls,
                                          std::vector<LayerFill>& fills, const std::vector<double>& group_heights,
                                          double overlap) {
    std::vector<ThickLayer> thick(walls.size());
    tbb::parallel_for(std::size_t(0), groups.size(), [&](std::size_t gi) {
        const LayerGroup& g = groups[gi];
        if (g.coarse.empty() || g.last >= walls.size())
            return;
        const Polygons wider = offset(g.coarse, overlap);
        ThickLayer& t = thick[g.last];
        t.height = group_heights[gi];
        t.walls = clip_toolpaths(walls[g.last].toolpaths, g.coarse, true);
        LayerFill& f = fills[g.last];
        t.fill.top = clip_toolpaths(f.top, wider, true);
        t.fill.solid = clip_toolpaths(f.solid, wider, true);
        t.fill.sparse = clip_toolpaths(f.sparse, wider, true);
        t.fill.bridge = clip_toolpaths(f.bridge, wider, true);
        t.fill.gaps = clip_toolpaths(f.gaps, wider, true);
        // The beads were laid out for the fine layers: thicker, they overlap
        // more (perimeters.md, rule 1), so each is that much wider to lay the
        // same material per spacing.
        const double fine = t.height / double(g.last - g.first + 1);
        const double wider_by = bead_overlap(t.height) - bead_overlap(fine);
        for (std::vector<Toolpath>* kind :
             {&t.walls, &t.fill.top, &t.fill.solid, &t.fill.sparse, &t.fill.bridge, &t.fill.gaps})
            for (Toolpath& path : *kind)
                for (ToolpathPoint& p : path.points)
                    if (p.width > 0.0)
                        p.width += wider_by;
        for (std::size_t l = g.first; l <= g.last; ++l) {
            walls[l].toolpaths = clip_toolpaths(walls[l].toolpaths, g.coarse, false);
            LayerFill& fl = fills[l];
            for (std::vector<Toolpath>* kind : {&fl.top, &fl.solid, &fl.sparse, &fl.bridge, &fl.gaps})
                *kind = clip_toolpaths(*kind, g.coarse, false);
        }
    });
    return thick;
}

namespace {

// Each group's thick zone; groups of fewer than `least` layers have none.
void find_coarse(std::vector<LayerGroup>& groups, const std::vector<Polygons>& regions, const LocalLayerParams& params,
                 std::size_t least) {
    const std::size_t n = regions.size();
    tbb::parallel_for(std::size_t(0), groups.size(), [&](std::size_t i) {
        LayerGroup& g = groups[i];
        if (g.last - g.first + 1 < least || g.first == 0 || g.last >= n)
            return;
        Polygons all = regions[g.first], some = regions[g.first];
        for (std::size_t l = g.first + 1; l <= g.last; ++l) {
            all = Clipper2Lib::Intersect(all, regions[l], FillRule::NonZero);
            some = Clipper2Lib::Union(some, regions[l], FillRule::NonZero);
        }
        if (all.empty())
            return;
        // Where the sections differ, without the slivers narrower than the
        // tolerance (an opening by half of it), grown by the margin.
        Polygons differ = Clipper2Lib::Difference(some, all, FillRule::NonZero);
        // Bottoms and tops (nothing below the first layer, or above the last)
        // count as change too: they show, and a bridge must not go thick.
        differ = Clipper2Lib::Union(differ, Clipper2Lib::Difference(all, regions[g.first - 1], FillRule::NonZero),
                                    FillRule::NonZero);
        if (g.last + 1 < n)
            differ = Clipper2Lib::Union(differ, Clipper2Lib::Difference(all, regions[g.last + 1], FillRule::NonZero),
                                        FillRule::NonZero);
        else
            return; // the top group of the part: all of it is top
        const Polygons detail = offset(offset(differ, -params.tolerance / 2.0), params.tolerance / 2.0);
        const Polygons fine = detail.empty() ? Polygons{} : offset(detail, params.margin);
        const Polygons coarse = Clipper2Lib::Difference(all, fine, FillRule::NonZero);
        // Thick pieces narrower than min_coarse are not worth a thick bead.
        g.coarse = offset(offset(coarse, -params.min_coarse / 2.0), params.min_coarse / 2.0);
    });
}

} // namespace

std::vector<LayerGroup> plan_layer_groups(const std::vector<Polygons>& regions, const LocalLayerParams& params) {
    std::vector<LayerGroup> groups;
    const std::size_t n = regions.size();
    if (n < 2 || params.ratio < 2)
        return groups;
    const auto ratio = std::size_t(params.ratio);
    for (std::size_t first = 1; first < n; first += ratio)
        groups.push_back({first, std::min(n - 1, first + ratio - 1), {}});
    find_coarse(groups, regions, params, ratio);
    return groups;
}

std::vector<LayerGroup> plan_layer_groups(const std::vector<Polygons>& regions,
                                          const std::vector<std::array<std::size_t, 2>>& runs,
                                          const LocalLayerParams& params) {
    std::vector<LayerGroup> groups;
    for (const auto& r : runs)
        groups.push_back({r[0], r[1], {}});
    find_coarse(groups, regions, params, 2);
    return groups;
}

std::vector<Toolpath> clip_toolpaths(const std::vector<Toolpath>& paths, const Polygons& polygons, bool inside) {
    std::vector<Toolpath> out;
    const Zone zone(polygons);
    std::vector<double> cuts;
    for (const Toolpath& path : paths) {
        if (path.points.size() < 2)
            continue;
        const std::size_t segments = path.closed ? path.points.size() : path.points.size() - 1;
        std::vector<Toolpath> pieces;
        Toolpath current;
        current.index = path.index;
        bool cut = false;
        const auto keep = [&](const ToolpathPoint& a, const ToolpathPoint& b) {
            const Point2d mid{(a.p.x + b.p.x) / 2.0, (a.p.y + b.p.y) / 2.0};
            return zone.contains(mid.x, mid.y) == inside;
        };
        for (std::size_t s = 0; s < segments; ++s) {
            const ToolpathPoint& a = path.points[s];
            const ToolpathPoint& b = path.points[(s + 1) % path.points.size()];
            zone.crossings(a.p, b.p, cuts);
            double from = 0.0;
            cuts.push_back(1.0);
            for (const double to : cuts) {
                if (to - from <= 1e-12)
                    continue;
                const ToolpathPoint pa = between(a, b, from), pb = between(a, b, to);
                if (keep(pa, pb)) {
                    if (current.points.empty())
                        current.points.push_back(pa);
                    current.points.push_back(pb);
                } else {
                    cut = true;
                    if (current.points.size() >= 2)
                        pieces.push_back(std::move(current));
                    current = Toolpath{};
                    current.index = path.index;
                }
                from = to;
            }
        }
        if (!cut) { // all on the kept side: the path as it was
            out.push_back(path);
            continue;
        }
        if (current.points.size() >= 2)
            pieces.push_back(std::move(current));
        // A closed path's piece through its start: the last piece runs into the first.
        if (path.closed && pieces.size() >= 2) {
            const ToolpathPoint& end = pieces.back().points.back();
            const ToolpathPoint& start = pieces.front().points.front();
            if (std::hypot(end.p.x - start.p.x, end.p.y - start.p.y) < 1e-9) {
                Toolpath joined = std::move(pieces.back());
                joined.points.insert(joined.points.end(), pieces.front().points.begin() + 1, pieces.front().points.end());
                pieces.front() = std::move(joined);
                pieces.pop_back();
            }
        }
        for (Toolpath& p : pieces)
            if (length(p) > 1e-6)
                out.push_back(std::move(p));
    }
    return out;
}

} // namespace tisma
