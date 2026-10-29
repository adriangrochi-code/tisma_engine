// Arcs for G2 and G3 (docs/spec/features/gcode_extras.md, rule 10). Own
// design; the circle through three points is the circumcentre of their
// triangle, as in any geometry text. See PROVENANCE.md.

#include "gcode/arcs.hpp"

#include <cmath>
#include <optional>

namespace tisma {

namespace {

// The longest run tried from one point: a path's cost stays bounded.
constexpr std::size_t kArcMaxPoints = 512;

struct Circle {
    Point2d center;
    double radius = 0.0;
};

// The circle through a, b and c; none when they are (nearly) in a line.
std::optional<Circle> circle_through(const Point2d& a, const Point2d& b, const Point2d& c) {
    const double bx = b.x - a.x, by = b.y - a.y, cx = c.x - a.x, cy = c.y - a.y;
    const double d = 2.0 * (bx * cy - by * cx);
    if (std::abs(d) < 1e-12)
        return std::nullopt;
    const double b2 = bx * bx + by * by, c2 = cx * cx + cy * cy;
    const double ux = (cy * b2 - by * c2) / d, uy = (bx * c2 - cx * b2) / d;
    return Circle{{a.x + ux, a.y + uy}, std::hypot(ux, uy)};
}

// The arc from points[i] to points[j], when they make one as fit_arcs asks.
std::optional<ArcRun> arc_of(std::span<const ToolpathPoint> p, std::size_t i, std::size_t j, double tolerance) {
    const std::size_t m = (i + j) / 2;
    const std::optional<Circle> circle = circle_through(p[i].p, p[m].p, p[j].p);
    if (!circle || circle->radius < kArcMinRadius || circle->radius > kArcMaxRadius)
        return std::nullopt;
    const Point2d c = circle->center;
    const double r = circle->radius;
    const bool ccw = (p[m].p.x - p[i].p.x) * (p[j].p.y - p[m].p.y) - (p[m].p.y - p[i].p.y) * (p[j].p.x - p[m].p.x) > 0.0;
    const auto off = [&](double x, double y) { return std::abs(std::hypot(x - c.x, y - c.y) - r); };
    double sweep = 0.0;
    for (std::size_t k = i; k <= j; ++k) {
        const ToolpathPoint& q = p[k];
        if (q.dz != 0.0 || q.below != 0.0 || std::abs(q.width - p[i].width) > 1e-9 || off(q.p.x, q.p.y) > tolerance)
            return std::nullopt;
        if (k == j)
            break;
        const Point2d& next = p[k + 1].p;
        const double ax = q.p.x - c.x, ay = q.p.y - c.y, bx = next.x - c.x, by = next.y - c.y;
        const double turn = std::atan2(ax * by - ay * bx, ax * bx + ay * by);
        if ((ccw ? turn : -turn) <= 1e-9)
            return std::nullopt;
        sweep += std::abs(turn);
        if (sweep > kArcMaxSweep + 1e-9 || off((q.p.x + next.x) / 2.0, (q.p.y + next.y) / 2.0) > 2.0 * tolerance)
            return std::nullopt;
    }
    return ArcRun{i, j, c, r, ccw, sweep};
}

} // namespace

std::vector<ArcRun> fit_arcs(std::span<const ToolpathPoint> points, double tolerance) {
    std::vector<ArcRun> out;
    const std::size_t n = points.size();
    std::size_t i = 0;
    while (i + kArcMinSegments < n) {
        std::optional<ArcRun> best;
        for (std::size_t j = i + kArcMinSegments; j < n && j - i <= kArcMaxPoints; ++j) {
            std::optional<ArcRun> arc = arc_of(points, i, j, tolerance);
            if (!arc)
                break;
            best = arc;
        }
        if (best) {
            out.push_back(*best);
            i = best->last;
        } else {
            ++i;
        }
    }
    return out;
}

} // namespace tisma
