// Fewer, longer G-code moves: each move reaches as far along the path as the
// tolerance allows, with the bead's width, Z and the bead below as extra
// coordinates. Own design; see PROVENANCE.md.

#include "gcode/resolution.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <utility>
#include <vector>

namespace tisma {

namespace {

// How far point p strays from the move a to b, as a multiple of what is
// allowed: above 1, the point stays.
double stray(const ToolpathPoint& a, const ToolpathPoint& b, const ToolpathPoint& p, double tolerance) {
    const double dx = b.p.x - a.p.x, dy = b.p.y - a.p.y;
    const double len2 = dx * dx + dy * dy;
    const double t = len2 > 0.0 ? std::clamp(((p.p.x - a.p.x) * dx + (p.p.y - a.p.y) * dy) / len2, 0.0, 1.0) : 0.0;
    const double off = std::hypot(p.p.x - (a.p.x + t * dx), p.p.y - (a.p.y + t * dy)) / tolerance;
    const auto along = [&](double va, double vb, double vp) {
        return std::fabs(vp - (va + t * (vb - va))) / kResolutionAttribute;
    };
    return std::max({off, along(a.width, b.width, p.width), along(a.dz, b.dz, p.dz), along(a.below, b.below, p.below)});
}

} // namespace

Toolpath simplify_toolpath(const Toolpath& path, double tolerance) {
    if (!(tolerance > 0.0) || path.points.size() < 3)
        return path;
    // A closed path as an open one back to its first point.
    std::vector<ToolpathPoint> pts = path.points;
    if (path.closed)
        pts.push_back(pts.front());
    const std::size_t n = pts.size();
    const auto fits = [&](std::size_t from, std::size_t to) {
        for (std::size_t k = from + 1; k < to; ++k)
            if (stray(pts[from], pts[to], pts[k], tolerance) > 1.0)
                return false;
        return true;
    };
    Toolpath out = path;
    out.points.assign(1, pts.front());
    for (std::size_t i = 0; i + 1 < n;) {
        // The farthest point the move from i can reach: the reach doubles
        // while the move fits, then the gap to the first that does not is
        // halved. Every point a move passes is checked, so a point found
        // fits even where a nearer one would not.
        std::size_t good = i + 1, bad = n;
        for (std::size_t reach = 2;; reach *= 2) {
            const std::size_t j = std::min(i + reach, n - 1);
            if (j <= good)
                break;
            if (!fits(i, j)) {
                bad = j;
                break;
            }
            good = j;
        }
        while (bad < n && bad - good > 1) {
            const std::size_t j = good + (bad - good) / 2;
            (fits(i, j) ? good : bad) = j;
        }
        out.points.push_back(pts[good]);
        i = good;
    }
    if (path.closed) {
        out.points.pop_back(); // the first point again
        if (out.points.size() < 3)
            return path;
    }
    return out;
}

} // namespace tisma
