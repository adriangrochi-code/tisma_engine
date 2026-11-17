// The wall chain of one region (Kuipers et al. 2020, Sections 3 and 5.1) and
// the region it leaves for infill. Own design; see PROVENANCE.md.

#include "perimeters/walls.hpp"

#include "perimeters/transitions.hpp"

#include <algorithm>
#include <cmath>
#include <unordered_map>

namespace tisma {

LayerWalls layer_walls(const Polygons& input, const BeadingParams& given) {
    LayerWalls out;
    // In spacings (perimeters.md, rule 1): the outer bead's edge on the
    // outline puts its centre half a width in, half an overlap more than half
    // a spacing; between neighbours, the spacings laid edge to edge.
    BeadingParams params = given;
    const double overlap = std::clamp(given.overlap, 0.0, 0.5 * given.preferred_width);
    params.overlap = 0.0;
    params.preferred_width -= overlap;
    if (params.outer_width > 0.0)
        params.outer_width = std::max(params.outer_width - overlap, 0.1 * params.preferred_width);
    // Rings of no area, as a boolean operation may leave, break the medial
    // axis (a node 30 m from a 16 mm part): out.
    Polygons region;
    region.reserve(input.size());
    for (const Polygon& ring : overlap > 0.0 ? offset(input, -overlap / 2.0) : input)
        if (std::abs(Clipper2Lib::Area(ring)) > 1e6) // nm2: a thousandth of a mm squared
            region.push_back(ring);
    if (region.empty())
        return out;
    MedialGraph graph = medial_graph(region);
    mark_central(graph, 180.0 - std::clamp(params.transition_angle, 0.0, 179.0));
    filter_marking(graph, params.preferred_width);
    TransitionSettings ramps;
    if (params.transition_length > 0.0)
        ramps.transition_length = params.transition_length;
    ramps.margin = params.transition_margin;
    const TransitionResult transitions = apply_transitions(graph, params, ramps);
    out.toolpaths = extract_toolpaths(graph, node_beadings(graph, transitions.bead_counts, params));
    if (overlap > 0.0)
        for (Toolpath& t : out.toolpaths)
            for (ToolpathPoint& p : t.points)
                if (p.width > 0.0) // a tip that tapers to nothing stays nothing
                    p.width += overlap;
    // Every closed loop runs counter-clockwise (own choice), whatever way the
    // chaining of sites happened to walk it: the G-code does not depend on
    // the order of the Voronoi diagram's edges.
    for (Toolpath& t : out.toolpaths) {
        if (!t.closed || t.points.size() < 3)
            continue;
        double area = 0.0;
        for (std::size_t i = 0; i < t.points.size(); ++i) {
            const Point2d& a = t.points[i].p;
            const Point2d& b = t.points[(i + 1) % t.points.size()].p;
            area += a.x * b.y - b.x * a.y;
        }
        if (area < 0.0)
            std::reverse(t.points.begin(), t.points.end());
    }
    if (params.wall_count > 0) {
        const double w = params.preferred_width;
        const Polygons inner = offset(region, -(outer_bead(params) + (params.wall_count - 1) * w));
        out.infill = offset(offset(inner, -w / 4.0), w / 4.0); // drop what is narrower than w*/2
    }
    return out;
}

bool walls_leave_gap(const Polygons& region, const std::vector<Toolpath>& toolpaths, double outer, double width) {
    if (region.empty() || !(width > 0.0))
        return false;
    // Every outer bead a closed loop: nothing was dropped (a lost stretch
    // leaves the outer loop open), and the check is skipped.
    if (std::all_of(toolpaths.begin(), toolpaths.end(), [](const Toolpath& t) { return t.closed || t.index > 0; }))
        return false;
    // Half a bead inside the outer bead's line (no opening: one offset is
    // most of the cost): sharp tips drop out, and a stretch that lost its
    // beads is still farther than three quarters of a bead from any.
    const Polygons line = offset(region, -(outer / 2.0 + width / 2.0));
    if (line.empty())
        return false;
    // The toolpaths' segments in square cells of two beads.
    const double cell = 2.0 * width;
    const auto key = [&](double x, double y) {
        return (std::int64_t(std::floor(x / cell)) << 32) ^ (std::int64_t(std::floor(y / cell)) & 0xffffffff);
    };
    struct Seg {
        Point2d a, b;
    };
    std::vector<Seg> segs;
    std::unordered_map<std::int64_t, std::vector<std::uint32_t>> cells;
    for (const Toolpath& t : toolpaths) {
        const std::size_t n = t.points.size();
        const std::size_t count = t.closed ? n : (n > 0 ? n - 1 : 0);
        for (std::size_t i = 0; i < std::max<std::size_t>(count, n == 1 ? 1 : 0); ++i) {
            const Point2d a = t.points[i].p, b = n == 1 ? a : t.points[(i + 1) % n].p;
            const auto id = std::uint32_t(segs.size());
            segs.push_back({a, b});
            const double x0 = std::min(a.x, b.x), x1 = std::max(a.x, b.x), y0 = std::min(a.y, b.y), y1 = std::max(a.y, b.y);
            for (double x = std::floor(x0 / cell) * cell; x <= x1; x += cell)
                for (double y = std::floor(y0 / cell) * cell; y <= y1; y += cell)
                    cells[key(x + 0.5 * cell, y + 0.5 * cell)].push_back(id);
        }
    }
    const double limit = 0.75 * width;
    const auto near = [&](double px, double py) {
        for (int dx = -1; dx <= 1; ++dx)
            for (int dy = -1; dy <= 1; ++dy) {
                const auto it = cells.find(key(px + dx * cell, py + dy * cell));
                if (it == cells.end())
                    continue;
                for (std::uint32_t id : it->second) {
                    const Seg& s = segs[id];
                    const double ex = s.b.x - s.a.x, ey = s.b.y - s.a.y, len2 = ex * ex + ey * ey;
                    const double t = len2 > 0.0 ? std::clamp(((px - s.a.x) * ex + (py - s.a.y) * ey) / len2, 0.0, 1.0) : 0.0;
                    const double qx = s.a.x + t * ex - px, qy = s.a.y + t * ey - py;
                    if (qx * qx + qy * qy <= limit * limit)
                        return true;
                }
            }
        return false;
    };
    const double step = width; // a lost stretch is millimetres long
    for (const Polygon& ring : line) {
        const std::size_t n = ring.size();
        for (std::size_t i = 0; i < n; ++i) {
            const double ax = to_mm(ring[i].x), ay = to_mm(ring[i].y);
            const double bx = to_mm(ring[(i + 1) % n].x), by = to_mm(ring[(i + 1) % n].y);
            const double len = std::hypot(bx - ax, by - ay);
            for (double d = 0.0; d < len; d += step)
                if (!near(ax + (bx - ax) * d / len, ay + (by - ay) * d / len))
                    return true;
        }
    }
    return false;
}

} // namespace tisma
