// Area covered by variable-width toolpaths, with Clipper2 (BSL-1.0). Own
// design; see PROVENANCE.md.

#include "perimeters/coverage.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace tisma {
namespace {

using Clipper2Lib::FillRule;

Polygon counter_clockwise(Polygon p) {
    if (Clipper2Lib::Area(p) < 0.0)
        std::reverse(p.begin(), p.end());
    return p;
}

void add_disc(Polygons& pieces, const Point2d& c, double width) {
    if (!(width > 0.0))
        return;
    Polygon disc;
    for (int k = 0; k < kCoverageDiscSides; ++k) {
        const double a = 2.0 * std::numbers::pi * k / kCoverageDiscSides;
        disc.push_back({to_units(c.x + width / 2.0 * std::cos(a)), to_units(c.y + width / 2.0 * std::sin(a))});
    }
    pieces.push_back(std::move(disc));
}

void add_piece(Polygons& pieces, const ToolpathPoint& a, const ToolpathPoint& b) {
    const double dx = b.p.x - a.p.x, dy = b.p.y - a.p.y;
    const double length = std::hypot(dx, dy);
    if (!(length > 0.0))
        return;
    const double nx = -dy / length, ny = dx / length;
    pieces.push_back(counter_clockwise({
        {to_units(a.p.x + nx * a.width / 2.0), to_units(a.p.y + ny * a.width / 2.0)},
        {to_units(a.p.x - nx * a.width / 2.0), to_units(a.p.y - ny * a.width / 2.0)},
        {to_units(b.p.x - nx * b.width / 2.0), to_units(b.p.y - ny * b.width / 2.0)},
        {to_units(b.p.x + nx * b.width / 2.0), to_units(b.p.y + ny * b.width / 2.0)},
    }));
}

Polygons toolpath_area(const Toolpath& t) {
    Polygons pieces;
    for (std::size_t i = 0; i < t.points.size(); ++i) {
        add_disc(pieces, t.points[i].p, t.points[i].width);
        if (i + 1 < t.points.size())
            add_piece(pieces, t.points[i], t.points[i + 1]);
        else if (t.closed && t.points.size() > 2)
            add_piece(pieces, t.points[i], t.points[0]);
    }
    return Clipper2Lib::Union(pieces, FillRule::NonZero);
}

double area(const Polygons& p) { return area_mm2(p); }

// Area of the polygonal disc relative to the true one.
const double kDiscRatio =
    kCoverageDiscSides / (2.0 * std::numbers::pi) * std::sin(2.0 * std::numbers::pi / kCoverageDiscSides);

// Material a toolpath lays down: each piece's length times its mean width,
// plus a half disc at each end of an open path (a whole disc for a dot).
double deposited(const Toolpath& t) {
    const std::size_t n = t.points.size();
    double sum = 0.0;
    for (std::size_t i = 0; i + 1 < n; ++i) {
        const ToolpathPoint& a = t.points[i];
        const ToolpathPoint& b = t.points[i + 1];
        sum += std::hypot(b.p.x - a.p.x, b.p.y - a.p.y) * (a.width + b.width) / 2.0;
    }
    if (t.closed && n > 2) {
        const ToolpathPoint& a = t.points.back();
        const ToolpathPoint& b = t.points.front();
        sum += std::hypot(b.p.x - a.p.x, b.p.y - a.p.y) * (a.width + b.width) / 2.0;
    } else if (n >= 1) {
        const double w0 = t.points.front().width, w1 = t.points.back().width;
        sum += kDiscRatio * std::numbers::pi * (w0 * w0 + w1 * w1) / 8.0;
    }
    return sum;
}

} // namespace

Polygons bead_area(const std::vector<Toolpath>& toolpaths) {
    Polygons all;
    for (const Toolpath& t : toolpaths)
        for (Polygon& p : toolpath_area(t))
            all.push_back(std::move(p));
    return Clipper2Lib::Union(all, FillRule::NonZero);
}



Coverage measure_coverage(const Polygons& target, const std::vector<Toolpath>& toolpaths) {
    Coverage c;
    Polygons all;
    for (const Toolpath& t : toolpaths) {
        c.deposited += deposited(t);
        for (Polygon& p : toolpath_area(t))
            all.push_back(std::move(p));
    }
    const Polygons beads = Clipper2Lib::Union(all, FillRule::NonZero);
    c.target = area(target);
    c.covered = area(Clipper2Lib::Intersect(beads, target, FillRule::NonZero));
    c.gaps = c.target - c.covered;
    c.outside = area(beads) - c.covered;
    c.overlap = c.deposited - area(beads);
    return c;
}

} // namespace tisma
