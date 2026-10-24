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
        if (sparse_printed && open(Clipper2Lib::Difference(surfaces[i].bottom, regions[i - 1], FillRule::NonZero)).empty())
            return; // all over the region below, which is printed but for slits
        const LayerSurfaces& below = surfaces[i - 1];
        Polygons printed = bead_area(walls[i - 1].toolpaths);
        for (const Polygons* p : {&below.top, &below.bottom, &below.bridge})
            printed = Clipper2Lib::Union(printed, *p, FillRule::NonZero);
        if (sparse_printed)
            printed = Clipper2Lib::Union(printed, below.sparse, FillRule::NonZero);
        Polygons raw = Clipper2Lib::Difference(surfaces[i].bottom, printed, FillRule::NonZero);
        raw = open(raw); // nothing narrower than a bead
        if (raw.empty())
            return;
        bridges[i] = Clipper2Lib::Intersect(surfaces[i].bottom, offset(raw, 2.0 * width), FillRule::NonZero);
        angles[i] = best_angle(bridges[i], offset(printed, width), width);
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
