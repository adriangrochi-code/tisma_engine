// Rounded corners (toolpath_time.md, rule 7). Own design; see PROVENANCE.md.

#include "gcode/corners.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace tisma {

namespace {

constexpr double kStraight = 179.0 * std::numbers::pi / 180.0; // a vertex this open stays
constexpr double kArcStep = 15.0 * std::numbers::pi / 180.0;   // of turn per arc segment
constexpr double kShare = 0.45;                                // of a segment an arc may take

} // namespace

Toolpath round_corners(const Toolpath& path, double deviation) {
    const std::size_t n = path.points.size();
    if (!(deviation > 0.0) || n < 3)
        return path;
    Toolpath out = path;
    out.points.clear();
    out.points.reserve(n * 3);
    for (std::size_t i = 0; i < n; ++i) {
        const ToolpathPoint& p = path.points[i];
        const bool end = !path.closed && (i == 0 || i + 1 == n);
        if (end) {
            out.points.push_back(p);
            continue;
        }
        const ToolpathPoint& a = path.points[(i + n - 1) % n];
        const ToolpathPoint& b = path.points[(i + 1) % n];
        const double ax = a.p.x - p.p.x, ay = a.p.y - p.p.y, bx = b.p.x - p.p.x, by = b.p.y - p.p.y;
        const double la = std::hypot(ax, ay), lb = std::hypot(bx, by);
        if (la < 1e-9 || lb < 1e-9) {
            out.points.push_back(p);
            continue;
        }
        const double ux = ax / la, uy = ay / la, vx = bx / lb, vy = by / lb;
        // The angle between the two segments at the vertex: pi is straight on.
        const double alpha = std::acos(std::clamp(ux * vx + uy * vy, -1.0, 1.0));
        if (alpha > kStraight || alpha < 1e-6) {
            out.points.push_back(p);
            continue;
        }
        // An arc of radius r tangent to both strays r (1 / sin(alpha/2) - 1)
        // from the vertex and touches them t = r / tan(alpha/2) from it.
        const double half = alpha / 2.0;
        double r = deviation * std::sin(half) / (1.0 - std::sin(half));
        double t = r / std::tan(half);
        if (const double most = kShare * std::min(la, lb); t > most)
            t = most, r = t * std::tan(half);
        const double bxs = ux + vx, bys = uy + vy, bl = std::hypot(bxs, bys);
        const double cx = p.p.x + bxs / bl * r / std::sin(half), cy = p.p.y + bys / bl * r / std::sin(half);
        const double sx = p.p.x + ux * t, sy = p.p.y + uy * t; // where the arc starts
        const double ex = p.p.x + vx * t, ey = p.p.y + vy * t; // and ends
        const double a0 = std::atan2(sy - cy, sx - cx);
        double sweep = std::atan2(ey - cy, ex - cx) - a0;
        // The short way round: the arc turns by pi - alpha.
        while (sweep > std::numbers::pi)
            sweep -= 2.0 * std::numbers::pi;
        while (sweep < -std::numbers::pi)
            sweep += 2.0 * std::numbers::pi;
        const int pieces = std::max(2, int(std::ceil(std::fabs(sweep) / kArcStep)));
        for (int k = 0; k <= pieces; ++k) {
            const double angle = a0 + sweep * double(k) / double(pieces);
            ToolpathPoint q = p;
            q.p = {cx + r * std::cos(angle), cy + r * std::sin(angle)};
            out.points.push_back(q);
        }
    }
    return out;
}

} // namespace tisma
