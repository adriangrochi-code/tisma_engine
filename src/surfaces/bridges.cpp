// Bridge detection and direction, with Clipper2 (BSL-1.0). Own design from the
// behaviour in docs/spec/BEHAVIOR.md; see PROVENANCE.md.

#include "surfaces/bridges.hpp"

#include "infill/rectilinear.hpp"
#include "perimeters/coverage.hpp"

#include <oneapi/tbb/parallel_for.h>

#include <cmath>

namespace tisma {
namespace {

using Clipper2Lib::FillRule;

bool inside(const Polygons& region, double x, double y) {
    const Clipper2Lib::Point64 p{to_units(x), to_units(y)};
    int winding = 0;
    for (const Polygon& path : region)
        if (Clipper2Lib::PointInPolygon(p, path) != Clipper2Lib::PointInPolygonResult::IsOutside)
            winding += Clipper2Lib::Area(path) > 0.0 ? 1 : -1;
    return winding > 0;
}

double best_angle(const Polygons& bridge, const Polygons& support, double width) {
    double best = 0.0, best_anchored = -1.0, best_mean = 0.0;
    for (double a = 0.0; a < 180.0 - 1e-9; a += kBridgeAngleStep) {
        double total = 0.0, anchored = 0.0;
        std::size_t count = 0;
        for (const Toolpath& t : rectilinear_fill(bridge, width, a, width, false, true)) {
            const ToolpathPoint& p = t.points.front();
            const ToolpathPoint& q = t.points.back();
            const double length = std::hypot(q.p.x - p.p.x, q.p.y - p.p.y);
            total += length;
            ++count;
            if (inside(support, p.p.x, p.p.y) && inside(support, q.p.x, q.p.y))
                anchored += length;
        }
        if (count == 0)
            continue;
        const double mean = total / double(count);
        const double fraction = anchored / total;
        // More anchored length wins; within 1 % of it, shorter lines win.
        if (fraction > best_anchored + 0.01 || (std::fabs(fraction - best_anchored) <= 0.01 && mean < best_mean)) {
            best = a, best_anchored = std::max(best_anchored, fraction), best_mean = mean;
        }
    }
    return best;
}

} // namespace

void find_bridges(std::vector<LayerSurfaces>& surfaces, const std::vector<Polygons>& regions,
                  const std::vector<LayerWalls>& walls, bool sparse_printed, double width) {
    // First every bridge from the unchanged surfaces, then move them out of `bottom`.
    std::vector<Polygons> bridges(surfaces.size());
    std::vector<double> angles(surfaces.size(), 0.0);
    tbb::parallel_for(std::size_t(1), surfaces.size(), [&](std::size_t i) {
        if (surfaces[i].bottom.empty())
            return;
        auto open = [&](const Polygons& p) { return offset(offset(p, -width / 2.0), width / 2.0); };
        // All over what is printed below but for slits: the region below, less
        // its sparse infill when that is not printed (a hollow part, whose
        // bottom layers would otherwise each weigh a whole layer's beads).
        const Polygons& region = regions[i - 1];
        if (open(Clipper2Lib::Difference(
                     surfaces[i].bottom,
                     sparse_printed ? region : Clipper2Lib::Difference(region, surfaces[i - 1].sparse, FillRule::NonZero),
                     FillRule::NonZero))
                .empty())
            return;
        // What is printed below, only near the bottom surface: the bridge
        // comes out of the surface, and its anchors (within the bottom
        // surface's box) look at most twice the width away (the mitred offset
        // below), so a window three widths wider changes nothing. A whole
        // layer's walls cost the most on detailed parts.
        Clipper2Lib::Rect64 window = Clipper2Lib::GetBounds(surfaces[i].bottom);
        const std::int64_t margin = to_units(3.0 * width);
        window.left -= margin, window.top -= margin, window.right += margin, window.bottom += margin;
        const LayerSurfaces& below = surfaces[i - 1];
        Polygons printed = bead_area(walls[i - 1].toolpaths, window);
        for (const Polygons* p : {&below.top, &below.bottom, &below.bridge})
            printed = Clipper2Lib::Union(printed, Clipper2Lib::RectClip(window, *p), FillRule::NonZero);
        if (sparse_printed)
            printed = Clipper2Lib::Union(printed, Clipper2Lib::RectClip(window, below.sparse), FillRule::NonZero);
        // To a micrometre: the beads' fine arcs cost most of what follows.
        printed = Clipper2Lib::SimplifyPaths(printed, 0.001 * kUnitsPerMm);
        Polygons raw = Clipper2Lib::Difference(surfaces[i].bottom, printed, FillRule::NonZero);
        raw = open(raw); // nothing narrower than a bead
        if (raw.empty())
            return;
        bridges[i] = Clipper2Lib::Intersect(surfaces[i].bottom, offset(raw, 2.0 * width), FillRule::NonZero);
        // The anchors are the bridge's line ends: the support only matters
        // around the bridge, in the same kind of window.
        Clipper2Lib::Rect64 around = Clipper2Lib::GetBounds(bridges[i]);
        around.left -= margin, around.top -= margin, around.right += margin, around.bottom += margin;
        angles[i] = best_angle(bridges[i], offset(Clipper2Lib::RectClip(around, printed), width), width);
    });
    for (std::size_t i = 1; i < surfaces.size(); ++i) {
        if (bridges[i].empty())
            continue;
        surfaces[i].bottom = Clipper2Lib::Difference(surfaces[i].bottom, bridges[i], FillRule::NonZero);
        surfaces[i].bridge = std::move(bridges[i]);
        surfaces[i].bridge_angle = angles[i];
    }
}

} // namespace tisma
