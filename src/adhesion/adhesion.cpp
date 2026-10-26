// Skirt and brim. Own design; see PROVENANCE.md.

#include "adhesion/adhesion.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

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

// Skirt and brim loops get negative indices (-1 for the first) so that their
// seams never count as the seams of a wall index (SeamMemory).
Toolpath loop_of(const Polygon& polygon, double bead, int index) {
    Toolpath t;
    t.closed = true;
    t.index = index;
    t.points.reserve(polygon.size());
    for (const auto& q : polygon)
        t.points.push_back({{to_mm(q.x), to_mm(q.y)}, bead});
    return t;
}

int brim_count(double width, double spacing) { return width > 0.0 ? int(std::floor(width / spacing + 1e-9)) : 0; }

// The outer contours grown by delta mm with round corners (arcs within 5 um).
Polygons grown(const Polygons& outers, double delta) {
    return Clipper2Lib::InflatePaths(outers, delta * kUnitsPerMm, Clipper2Lib::JoinType::Round,
                                     Clipper2Lib::EndType::Polygon, 2.0, 0.005 * kUnitsPerMm);
}

} // namespace

double loop_spacing(double bead, double height) {
    return std::max(bead - (1.0 - std::numbers::pi / 4.0) * std::max(height, 0.0), 0.5 * bead);
}

std::vector<Toolpath> brim_loops(const Polygons& region, double width, double bead, double height) {
    std::vector<Toolpath> out;
    const Polygons outers = outer_contours(region);
    const double spacing = loop_spacing(bead, height);
    for (int k = 0; k < brim_count(width, spacing); ++k)
        for (const Polygon& p : grown(outers, spacing * (k + 0.5)))
            out.push_back(loop_of(p, bead, -1 - k));
    return out;
}

std::vector<Toolpath> skirt_loops(const Polygons& region, double brim_width, int loops, double distance,
                                  double bead, double height) {
    std::vector<Toolpath> out;
    const Polygons outers = outer_contours(region);
    const double spacing = loop_spacing(bead, height);
    const double base = brim_count(brim_width, spacing) * spacing; // where the brim ends
    for (int k = 0; k < loops; ++k)
        for (const Polygon& p : grown(outers, base + distance + spacing * (k + 0.5)))
            if (Clipper2Lib::Area(p) > 0.0)
                out.push_back(loop_of(p, bead, -1 - k));
    return out;
}

} // namespace tisma
