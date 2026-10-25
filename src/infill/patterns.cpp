// Line, honeycomb and concentric infill, with Clipper2 (BSL-1.0) clipping and
// offsets. Rules of infill_patterns.md; the construction is own design, see
// PROVENANCE.md.

#include "infill/patterns.hpp"

#include "infill/connect.hpp"
#include "infill/rectilinear.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>

namespace tisma {
namespace {

using Clipper2Lib::Point64;

// The region shrunk by half a bead, in a frame turned by -angle: where the
// pattern is drawn. Also its bounding box.
struct Frame {
    double c = 1.0, s = 0.0;
    Polygons turned;
    std::int64_t xmin = INT64_MAX, xmax = INT64_MIN, ymin = INT64_MAX, ymax = INT64_MIN;

    Frame(const Polygons& region, double width, double angle_degrees) {
        const double a = angle_degrees * std::numbers::pi / 180.0;
        c = std::cos(a), s = std::sin(a);
        for (const Polygon& path : offset(region, -width / 2.0 + 1e-5)) {
            Polygon t;
            t.reserve(path.size());
            for (const Point64& p : path) {
                const Point64 q{std::llround(double(p.x) * c + double(p.y) * s),
                                std::llround(-double(p.x) * s + double(p.y) * c)};
                xmin = std::min(xmin, q.x), xmax = std::max(xmax, q.x);
                ymin = std::min(ymin, q.y), ymax = std::max(ymax, q.y);
                t.push_back(q);
            }
            turned.push_back(std::move(t));
        }
    }

    // Clips the open paths to the region, joins the pieces along its boundary
    // (ends at most max_join units apart along it) and turns them back.
    std::vector<Toolpath> clip(const Polygons& paths, double max_join, double width) const {
        Clipper2Lib::Clipper64 clipper;
        clipper.AddOpenSubject(paths);
        clipper.AddClip(turned);
        Polygons closed, open;
        clipper.Execute(Clipper2Lib::ClipType::Intersection, Clipper2Lib::FillRule::NonZero, closed, open);
        open.erase(std::remove_if(open.begin(), open.end(), [](const Polygon& p) { return p.size() < 2; }),
                   open.end());
        // Across the frame, then along it, so that the joins and the order are stable.
        for (Polygon& piece : open)
            if (piece.front().x > piece.back().x)
                std::reverse(piece.begin(), piece.end());
        std::sort(open.begin(), open.end(), [](const Polygon& l, const Polygon& r) {
            return l.front().y < r.front().y || (l.front().y == r.front().y && l.front().x < r.front().x);
        });
        std::vector<Toolpath> out;
        for (const Polygon& piece : join_along_boundary(open, turned, max_join)) {
            if (piece.size() < 2)
                continue;
            Toolpath t;
            t.points.reserve(piece.size());
            for (const Point64& q : piece)
                t.points.push_back({{to_mm(std::llround(double(q.x) * c - double(q.y) * s)),
                                     to_mm(std::llround(double(q.x) * s + double(q.y) * c))},
                                    width});
            out.push_back(std::move(t));
        }
        return out;
    }
};

} // namespace

std::vector<Toolpath> line_fill(const Polygons& region, double spacing, double angle_degrees, double tilt_degrees,
                                double width) {
    if (!(spacing > 0.0) || region.empty())
        return {};
    const Frame f(region, width, angle_degrees);
    if (f.turned.empty())
        return {};
    const double step = spacing * kUnitsPerMm;
    const double slope = std::tan(tilt_degrees * std::numbers::pi / 180.0);
    const double x0 = double(f.xmin - 1), x1 = double(f.xmax + 1), mid = 0.5 * (x0 + x1);
    // A turned line rises or falls this much over half the box: lines that far
    // past the box still cross it.
    const double reach = std::fabs(slope) * (mid - x0);
    Polygons lines;
    for (auto k = static_cast<std::int64_t>(std::floor((double(f.ymin) - reach) / step));
         double(k) * step <= double(f.ymax) + reach; ++k) {
        const double y = double(k) * step;
        const double t = (k % 2 != 0) ? slope : 0.0; // every other line turned, about its point on the middle
        lines.push_back({{std::llround(x0), std::llround(y + t * (x0 - mid))},
                         {std::llround(x1), std::llround(y + t * (x1 - mid))}});
    }
    return f.clip(lines, kMaxJoin * step, width);
}

double honeycomb_side(double width, double density) {
    // A hexagonal tiling of side s has 2 / (sqrt 3 s) mm of walls per mm2.
    return 2.0 * width / (std::numbers::sqrt3 * density);
}

std::vector<Toolpath> honeycomb_fill(const Polygons& region, double side, double principal_degrees, double width) {
    if (!(side > 0.0) || region.empty())
        return {};
    const Frame f(region, width, principal_degrees);
    if (f.turned.empty())
        return {};
    // In the turned frame the hexagons have their flat sides along x, one
    // centred on the origin: centres at (1.5 s i, 2 h j + h (i mod 2)), h the
    // half height. Wavy line m runs between y = m h and (m + 1) h: along x on
    // [-s/2, s/2] + 3 s n at one of them, on [s, 2 s] + 3 s n at the other.
    const double s = side * kUnitsPerMm, h = 0.5 * std::numbers::sqrt3 * s, period = 3.0 * s;
    const auto n0 = static_cast<std::int64_t>(std::floor((double(f.xmin) - 2.0 * s) / period)) - 1;
    const auto n1 = static_cast<std::int64_t>(std::ceil((double(f.xmax) + s) / period)) + 1;
    Polygons waves;
    for (auto m = static_cast<std::int64_t>(std::floor(double(f.ymin) / h)) - 1;
         double(m) * h <= double(f.ymax); ++m) {
        const bool even = m % 2 == 0;
        const double y_first = double(m + (even ? 1 : 0)) * h; // the level on [-s/2, s/2]
        const double y_second = double(m + (even ? 0 : 1)) * h;
        Polygon wave;
        wave.reserve(std::size_t(4 * (n1 - n0 + 1)));
        for (std::int64_t n = n0; n <= n1; ++n) {
            const double x = double(n) * period;
            wave.push_back({std::llround(x - 0.5 * s), std::llround(y_first)});
            wave.push_back({std::llround(x + 0.5 * s), std::llround(y_first)});
            wave.push_back({std::llround(x + s), std::llround(y_second)});
            wave.push_back({std::llround(x + 2.0 * s), std::llround(y_second)});
        }
        waves.push_back(std::move(wave));
    }
    return f.clip(waves, kMaxJoin * h, width);
}

std::vector<Toolpath> concentric_fill(const Polygons& region, double spacing, double width) {
    std::vector<Toolpath> out;
    if (!(spacing > 0.0) || region.empty())
        return out;
    for (Polygons loops = offset(region, -width / 2.0); !loops.empty(); loops = offset(loops, -spacing))
        for (const Polygon& loop : loops) {
            if (loop.size() < 3)
                continue;
            Toolpath t;
            t.closed = true;
            t.points.reserve(loop.size());
            for (const Point64& q : loop)
                t.points.push_back({{to_mm(q.x), to_mm(q.y)}, width});
            out.push_back(std::move(t));
        }
    return out;
}

} // namespace tisma
