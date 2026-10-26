// Area covered by variable-width toolpaths, with Clipper2 (BSL-1.0). Own
// design; see PROVENANCE.md.

#include "perimeters/coverage.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace tisma {
namespace {

using Clipper2Lib::FillRule;

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

// The stroke of a toolpath: Clipper2's round offset of its polyline with each
// point's own half width (a variable offset), round joins and caps; a closed
// path is an open one that ends where it began. Between two points the sides
// run straight from one half width to the other, as the pieces do; the round
// joins and caps are the discs on the outer side of a turn (where the width
// changes along a straight run, the discs' bulge of a few nanometres is not
// drawn), with arcs of at least kCoverageDiscSides steps per turn. Uniting a
// disc per point cost the time of all their crossings, which grow with the
// points' density (a detailed contour, every wall of a layer).
void add_stroke(Polygons& pieces, const Toolpath& t) {
    Polygon path;
    std::vector<double> half; // in units
    const auto add = [&](const ToolpathPoint& q) {
        const Clipper2Lib::Point64 p{to_units(q.p.x), to_units(q.p.y)};
        const double h = std::max(q.width, 0.0) / 2.0 * kUnitsPerMm;
        if (!path.empty() && path.back() == p) {
            half.back() = std::max(half.back(), h);
            return;
        }
        path.push_back(p);
        half.push_back(h);
    };
    for (const ToolpathPoint& q : t.points)
        add(q);
    if (t.closed && t.points.size() > 2)
        add(t.points.front());
    if (path.size() < 2) {
        if (!t.points.empty())
            add_disc(pieces, t.points.front().p, t.points.front().width);
        return;
    }
    double smallest = 0.0;
    for (double h : half)
        if (h > 0.0 && (smallest == 0.0 || h < smallest))
            smallest = h;
    if (!(smallest > 0.0))
        return;
    Clipper2Lib::ClipperOffset offset(2.0, smallest * (1.0 - std::cos(std::numbers::pi / kCoverageDiscSides)));
    offset.AddPath(path, Clipper2Lib::JoinType::Round, Clipper2Lib::EndType::Round);
    Polygons out;
    offset.Execute([&](const Polygon&, const Clipper2Lib::PathD&, std::size_t j, std::size_t) { return half[j]; },
                   out);
    for (Polygon& p : out)
        pieces.push_back(std::move(p));
}

Polygons toolpath_area(const Toolpath& t) {
    Polygons pieces;
    add_stroke(pieces, t);
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
