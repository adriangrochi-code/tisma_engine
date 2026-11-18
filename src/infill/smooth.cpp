// Rounded corners for the sparse infill (orca_2_5.md, rule 11). Own design:
// each corner is replaced by a quadratic Bezier curve from a point on the
// stretch before to a point on the stretch after, with the corner as its
// control point, so the curve is tangent to both; see PROVENANCE.md.

#include "infill/smooth.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace tisma {

void smooth_corners(std::vector<Toolpath>& paths, double factor) {
    factor = std::clamp(factor, 0.0, 1.0);
    if (!(factor > 0.0))
        return;
    const double hairpin = std::cos(kHairpinDegrees * std::numbers::pi / 180.0);
    const double straight = std::cos(1.0 * std::numbers::pi / 180.0);
    for (Toolpath& t : paths) {
        const std::size_t n = t.points.size();
        if (n < 3)
            continue;
        std::vector<ToolpathPoint> out;
        out.reserve(n * 3);
        for (std::size_t i = 0; i < n; ++i) {
            const bool end = !t.closed && (i == 0 || i + 1 == n);
            if (end) {
                out.push_back(t.points[i]);
                continue;
            }
            const ToolpathPoint& a = t.points[(i + n - 1) % n];
            const ToolpathPoint& b = t.points[i];
            const ToolpathPoint& c = t.points[(i + 1) % n];
            const double ux = b.p.x - a.p.x, uy = b.p.y - a.p.y, vx = c.p.x - b.p.x, vy = c.p.y - b.p.y;
            const double lu = std::hypot(ux, uy), lv = std::hypot(vx, vy);
            if (!(lu > 1e-9) || !(lv > 1e-9)) {
                out.push_back(b);
                continue;
            }
            // The cosine of the turn: 1 straight on, -1 straight back.
            const double turn = (ux * vx + uy * vy) / (lu * lv);
            if (turn > straight || turn < hairpin) {
                out.push_back(b);
                continue;
            }
            const double reach = factor * 0.5 * std::min(lu, lv);
            const Point2d s{b.p.x - ux / lu * reach, b.p.y - uy / lu * reach};
            const Point2d e{b.p.x + vx / lv * reach, b.p.y + vy / lv * reach};
            // About one point every 15 degrees of turn, at least two pieces.
            const double degrees = std::acos(std::clamp(turn, -1.0, 1.0)) * 180.0 / std::numbers::pi;
            const int pieces = std::max(2, int(std::ceil(degrees / 15.0)));
            for (int k = 0; k <= pieces; ++k) {
                const double q = double(k) / pieces, r = 1.0 - q;
                ToolpathPoint p = b;
                p.p = {r * r * s.x + 2.0 * r * q * b.p.x + q * q * e.x, r * r * s.y + 2.0 * r * q * b.p.y + q * q * e.y};
                out.push_back(p);
            }
        }
        t.points = std::move(out);
    }
}

} // namespace tisma
