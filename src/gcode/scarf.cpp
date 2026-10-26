// The scarf seam (docs/spec/features/walls_extra.md, rule 4). Own design; see
// PROVENANCE.md.

#include "gcode/scarf.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

namespace tisma {

Toolpath scarf_loop(const Toolpath& loop, double height, double start, double length, double max_segment) {
    const std::size_t n = loop.points.size();
    Toolpath out;
    out.closed = false;
    out.index = loop.index;
    out.wavy = true;
    if (n < 2)
        return loop;
    // Where each point is along the loop, the seam (point 0) at 0 and again,
    // after the whole way round, at the loop's length.
    std::vector<double> along(n + 1, 0.0);
    for (std::size_t i = 1; i <= n; ++i) {
        const Point2d& a = loop.points[i - 1].p;
        const Point2d& b = loop.points[i % n].p;
        along[i] = along[i - 1] + std::hypot(b.x - a.x, b.y - a.y);
    }
    const double total = along[n];
    if (!(total > 0.0) || !(length > 0.0))
        return loop;
    const double from = std::max(0.0, total - length); // where the ramp starts
    const double ramp = total - from;
    // The point at a distance along the loop, on the edge that holds it.
    std::size_t edge = 0;
    const auto at = [&](double s) {
        if (s < along[edge])
            edge = 0;
        while (edge + 1 < n && along[edge + 1] < s)
            ++edge;
        const ToolpathPoint& a = loop.points[edge];
        const ToolpathPoint& b = loop.points[(edge + 1) % n];
        const double span = along[edge + 1] - along[edge];
        const double t = span > 0.0 ? std::clamp((s - along[edge]) / span, 0.0, 1.0) : 0.0;
        ToolpathPoint p = a;
        p.p = {a.p.x + t * (b.p.x - a.p.x), a.p.y + t * (b.p.y - a.p.y)};
        p.width = a.width + t * (b.width - a.width);
        p.dz = 0.0;
        p.below = 0.0;
        return p;
    };
    // The distances the ramp passes: its ends, the loop's points between them,
    // and more so no piece is longer than max_segment.
    std::vector<double> stops{from};
    for (std::size_t i = 1; i < n; ++i)
        if (along[i] > from && along[i] < total)
            stops.push_back(along[i]);
    stops.push_back(total);
    std::vector<double> marks{from};
    for (std::size_t k = 1; k < stops.size(); ++k) {
        const double gap = stops[k] - stops[k - 1];
        const int pieces = max_segment > 0.0 ? std::max(1, int(std::ceil(gap / max_segment - 1e-9))) : 1;
        for (int j = 1; j <= pieces; ++j)
            marks.push_back(j == pieces ? stops[k] : stops[k - 1] + gap * double(j) / double(pieces));
    }
    // How much of the layer the ramp fills at a mark: from `start` at its
    // beginning to all of it at the seam.
    const auto filled = [&](double s) { return start + (1.0 - start) * (s - from) / ramp; };
    const auto add = [&](ToolpathPoint p) {
        if (!out.points.empty()) {
            const ToolpathPoint& last = out.points.back();
            if (last.p.x == p.p.x && last.p.y == p.p.y && last.dz == p.dz && last.below == p.below)
                return;
        }
        out.points.push_back(p);
    };
    // The ramp: the nozzle under the layer's top by what it does not fill yet,
    // the bead down to the layer below.
    for (const double s : marks) {
        ToolpathPoint p = s >= total ? loop.points.front() : at(s);
        p.dz = -height * (1.0 - filled(s));
        p.below = 0.0;
        add(p);
    }
    // The loop at the layer's top, from the seam to where the ramp starts.
    for (std::size_t i = 0; i < n && along[i] <= from; ++i) {
        ToolpathPoint p = loop.points[i];
        p.dz = 0.0;
        p.below = 0.0;
        add(p);
    }
    if (from > 0.0)
        add(at(from));
    // The cover: over the ramp, down to its top.
    for (const double s : marks) {
        ToolpathPoint p = s >= total ? loop.points.front() : at(s);
        p.dz = 0.0;
        p.below = height * filled(s);
        out.points.push_back(p); // where the cover starts, the same point with the ramp under it
    }
    return out;
}

} // namespace tisma
