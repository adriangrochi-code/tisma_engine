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

// The stroke of a polyline of one width: Clipper's round offset, the same
// shape as its pieces with a disc at every point, with arcs of the discs'
// kCoverageDiscSides steps per turn. Uniting a disc per point cost the time
// of all the discs' crossings, which grow with the points' density (a
// detailed contour, every wall of a layer).
void add_stroke(Polygons& pieces, Polygon path, double width, bool closed) {
    if (!(width > 0.0) || path.empty())
        return;
    const double r = width / 2.0 * kUnitsPerMm;
    const double arc_tolerance = r * (1.0 - std::cos(std::numbers::pi / kCoverageDiscSides));
    for (Polygon& p : Clipper2Lib::InflatePaths({std::move(path)}, r, Clipper2Lib::JoinType::Round,
                                                closed ? Clipper2Lib::EndType::Joined : Clipper2Lib::EndType::Round,
                                                2.0, arc_tolerance))
        pieces.push_back(std::move(p));
}

Polygons toolpath_area(const Toolpath& t) {
    const std::size_t n = t.points.size();
    Polygons pieces;
    if (n == 1)
        add_disc(pieces, t.points[0].p, t.points[0].width);
    // Piece k joins point k to the next, the last one back to the first on a
    // closed path. Runs of pieces of one width go as strokes; a piece whose
    // width changes is its quadrilateral with a disc at each end.
    const std::size_t count = n < 2 ? 0 : n - 1 + (t.closed && n > 2 ? 1 : 0);
    const auto point = [&](std::size_t i) -> const ToolpathPoint& { return t.points[i % n]; };
    const auto even = [&](std::size_t k) { return std::fabs(point(k).width - point(k + 1).width) <= 1e-12; };
    const auto unit = [](const ToolpathPoint& q) { return Clipper2Lib::Point64{to_units(q.p.x), to_units(q.p.y)}; };
    bool all_even = count > 0;
    for (std::size_t k = 0; k < count && all_even; ++k)
        all_even = even(k);
    if (all_even && t.closed && n > 2) {
        Polygon ring;
        for (const ToolpathPoint& q : t.points)
            ring.push_back(unit(q));
        add_stroke(pieces, std::move(ring), t.points[0].width, true);
    } else {
        for (std::size_t k = 0; k < count;) {
            if (!even(k)) {
                add_disc(pieces, point(k).p, point(k).width);
                add_piece(pieces, point(k), point(k + 1));
                add_disc(pieces, point(k + 1).p, point(k + 1).width);
                ++k;
                continue;
            }
            Polygon run = {unit(point(k))};
            const double width = point(k).width;
            for (; k < count && even(k); ++k)
                run.push_back(unit(point(k + 1)));
            add_stroke(pieces, std::move(run), width, false);
        }
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

Polygons bead_area(const std::vector<Toolpath>& toolpaths, const Clipper2Lib::Rect64& window) {
    // A piece reaches the window when its box, grown by its larger half
    // width, meets it; a point between a piece that does and one that does not
    // is out of the window with its disc, so cutting there changes nothing
    // inside.
    const auto reaches = [&](const ToolpathPoint& a, const ToolpathPoint& b) {
        const double r = std::max(a.width, b.width) / 2.0;
        return to_units(std::max(a.p.x, b.p.x) + r) >= window.left &&
               to_units(std::min(a.p.x, b.p.x) - r) <= window.right &&
               to_units(std::max(a.p.y, b.p.y) + r) >= window.top &&
               to_units(std::min(a.p.y, b.p.y) - r) <= window.bottom;
    };
    std::vector<Toolpath> near;
    for (const Toolpath& t : toolpaths) {
        const std::size_t n = t.points.size();
        if (n == 1) {
            if (reaches(t.points[0], t.points[0]))
                near.push_back(t);
            continue;
        }
        const std::size_t count = n < 2 ? 0 : n - 1 + (t.closed && n > 2 ? 1 : 0);
        std::vector<char> in(count);
        bool all = count > 0;
        for (std::size_t k = 0; k < count; ++k)
            all = (in[k] = reaches(t.points[k], t.points[(k + 1) % n]) ? 1 : 0) && all;
        if (all) {
            near.push_back(t);
            continue;
        }
        // Open runs of the pieces that reach it; on a closed path, a run may
        // go on over the first point.
        std::size_t start = 0;
        if (t.closed && count == n)
            while (start < count && in[start])
                ++start; // begin after a piece that stays out
        for (std::size_t step = 0; step < count;) {
            const std::size_t k = (start + step) % count;
            if (!in[k]) {
                ++step;
                continue;
            }
            Toolpath run;
            run.points.push_back(t.points[k]);
            for (; step < count && in[(start + step) % count]; ++step)
                run.points.push_back(t.points[((start + step) % count + 1) % n]);
            near.push_back(std::move(run));
        }
    }
    return bead_area(near);
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
