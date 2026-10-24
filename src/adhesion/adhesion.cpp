// Skirt and brim. Own design; see PROVENANCE.md.

#include "adhesion/adhesion.hpp"

#include <cmath>

namespace tisma {
namespace {

// The region's outer contours (counter-clockwise ones), holes left out.
Polygons outer_contours(const Polygons& region) {
    Polygons out;
    for (const Polygon& p : region)
        if (Clipper2Lib::Area(p) > 0.0)
            out.push_back(p);
    return out;
}

Toolpath loop_of(const Polygon& polygon, double bead, int index) {
    Toolpath t;
    t.closed = true;
    t.index = index;
    t.points.reserve(polygon.size());
    for (const auto& q : polygon)
        t.points.push_back({{to_mm(q.x), to_mm(q.y)}, bead});
    return t;
}

int brim_count(double width, double bead) { return width > 0.0 ? int(std::floor(width / bead + 1e-9)) : 0; }

} // namespace

std::vector<Toolpath> brim_loops(const Polygons& region, double width, double bead) {
    std::vector<Toolpath> out;
    const Polygons outers = outer_contours(region);
    for (int k = 0; k < brim_count(width, bead); ++k)
        for (const Polygon& p : offset(outers, bead * (k + 0.5)))
            out.push_back(loop_of(p, bead, k));
    return out;
}

std::vector<Toolpath> skirt_loops(const Polygons& region, double brim_width, int loops, double distance,
                                  double bead) {
    std::vector<Toolpath> out;
    const Polygons outers = outer_contours(region);
    const double base = brim_count(brim_width, bead) * bead; // where the brim ends
    for (int k = 0; k < loops; ++k)
        for (const Polygon& p : offset(outers, base + distance + bead * (k + 0.5)))
            if (Clipper2Lib::Area(p) > 0.0)
                out.push_back(loop_of(p, bead, k));
    return out;
}

} // namespace tisma
