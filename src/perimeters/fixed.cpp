// Walls of one width with gap fill (docs/spec/features/walls_extra.md, rules 1
// and 2). Own design; see PROVENANCE.md.

#include "perimeters/fixed.hpp"

#include "gcode/order.hpp"

#include <algorithm>
#include <cmath>
#include <iterator>

namespace tisma {

LayerWalls fixed_walls(const Polygons& region, double width, int count, bool gap_fill) {
    LayerWalls out;
    std::vector<Polygons> centres; // each loop's centre lines
    for (int k = 0; k < count; ++k) {
        const double depth = width / 2.0 + double(k) * width;
        Polygons centre = offset(offset(region, -(depth + width / 2.0)), width / 2.0);
        if (centre.empty())
            break;
        for (Polygon ring : centre) {
            if (ring.size() < 3)
                continue;
            if (!Clipper2Lib::IsPositive(ring)) // holes too run counter-clockwise
                std::reverse(ring.begin(), ring.end());
            Toolpath t;
            t.closed = true;
            t.index = k;
            for (const auto& q : ring)
                t.points.push_back({{to_mm(q.x), to_mm(q.y)}, width});
            out.toolpaths.push_back(std::move(t));
        }
        centres.push_back(std::move(centre));
    }
    if (int(centres.size()) == count && count > 0)
        out.infill = offset(centres.back(), -width / 2.0);
    if (!gap_fill || region.empty())
        return out;
    // What the loops leave: outside the first loop's beads (thin parts,
    // corners), between one loop's beads and the next's, and inside the last
    // when it is not the last wall (no room for infill).
    const auto Difference = [](const Polygons& a, const Polygons& b) {
        return Clipper2Lib::Difference(a, b, Clipper2Lib::FillRule::NonZero);
    };
    Polygons left = centres.empty() ? region : Difference(region, offset(centres.front(), width / 2.0));
    for (std::size_t k = 0; k < centres.size(); ++k) {
        const Polygons inner = offset(centres[k], -width / 2.0);
        const Polygons part = k + 1 < centres.size() ? Difference(inner, offset(centres[k + 1], width / 2.0))
                              : out.infill.empty()   ? inner
                                                     : Polygons{};
        left.insert(left.end(), part.begin(), part.end());
    }
    left = Clipper2Lib::Union(left, Clipper2Lib::FillRule::NonZero);
    // Thinner than half a bead: no bead (and no hair-thin channels between beads).
    left = offset(offset(left, -width / 4.0), width / 4.0);
    if (left.empty())
        return out;
    BeadingParams beads;
    beads.preferred_width = width;
    // Island by island: smaller diagrams, and the medial axis of some sets of
    // islands takes an open bisector between two of them as inside (PLAN.md,
    // "Eje medio con varias islas").
    for (const Polygons& island : split_islands(left)) {
        std::vector<Toolpath> beads_in = layer_walls(island, beads).toolpaths;
        out.gaps.insert(out.gaps.end(), std::make_move_iterator(beads_in.begin()), std::make_move_iterator(beads_in.end()));
    }
    // A bead's centre line stops half its width from where the gap ends; the
    // bead's section is a rectangle (width by height), so half a width more
    // at each free end fills the gap to its end (W6: 19.14 mm of gap fill in
    // a gap 19.1 mm long). An end near another bead's end is a joint.
    std::vector<Point2d> ends;
    for (const Toolpath& t : out.gaps)
        if (!t.closed && t.points.size() >= 2)
            ends.push_back(t.points.front().p), ends.push_back(t.points.back().p);
    const auto free_end = [&](const ToolpathPoint& p) {
        int near = 0;
        for (const Point2d& q : ends)
            near += std::hypot(q.x - p.p.x, q.y - p.p.y) < p.width;
        return near == 1; // itself
    };
    const auto extend = [](ToolpathPoint& end, const ToolpathPoint& before) {
        const double dx = end.p.x - before.p.x, dy = end.p.y - before.p.y, d = std::hypot(dx, dy);
        if (d > 0.0)
            end.p = {end.p.x + dx / d * end.width / 2.0, end.p.y + dy / d * end.width / 2.0};
    };
    for (Toolpath& t : out.gaps) {
        if (t.closed || t.points.size() < 2)
            continue;
        const bool front = free_end(t.points.front()), back = free_end(t.points.back());
        if (front)
            extend(t.points.front(), t.points[1]);
        if (back)
            extend(t.points.back(), t.points[t.points.size() - 2]);
    }
    return out;
}

} // namespace tisma
