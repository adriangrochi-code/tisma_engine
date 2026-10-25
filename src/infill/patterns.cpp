// Line, honeycomb and concentric infill, with Clipper2 (BSL-1.0) clipping and
// offsets. Rules of infill_patterns.md; the construction is own design, see
// PROVENANCE.md.

#include "infill/patterns.hpp"

#include "infill/connect.hpp"
#include "infill/rectilinear.hpp"

#include <algorithm>
#include <array>
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

namespace {

// A box in mm, for the curves' pruning.
struct Box {
    double x0, y0, x1, y1;
    bool misses(double ax, double ay, double bx, double by) const {
        return std::max(ax, bx) < x0 || std::min(ax, bx) > x1 || std::max(ay, by) < y0 || std::min(ay, by) > y1;
    }
};

// Appends the centres of the cells of the Hilbert curve that fills the square
// with corner (ox, oy), spanned by a (from the curve's start towards its end)
// and b, 2^level cells a side: four such curves of half the size, the first
// turned so that it ends next to the second's start, the last so that it ends
// next to the corner o + a. Squares that miss the box are skipped, and the
// curve starts a new path after them.
void hilbert(int level, double ox, double oy, double ax, double ay, double bx, double by, const Box& box,
             std::vector<std::vector<Point2d>>& out, bool& gap) {
    const double xs[4] = {ox, ox + ax, ox + bx, ox + ax + bx}, ys[4] = {oy, oy + ay, oy + by, oy + ay + by};
    if (box.misses(*std::min_element(xs, xs + 4), *std::min_element(ys, ys + 4), *std::max_element(xs, xs + 4),
                   *std::max_element(ys, ys + 4))) {
        gap = true;
        return;
    }
    if (level == 0) {
        if (gap || out.empty())
            out.emplace_back();
        out.back().push_back({ox + 0.5 * (ax + bx), oy + 0.5 * (ay + by)});
        gap = false;
        return;
    }
    const double hax = 0.5 * ax, hay = 0.5 * ay, hbx = 0.5 * bx, hby = 0.5 * by;
    hilbert(level - 1, ox, oy, hbx, hby, hax, hay, box, out, gap);
    hilbert(level - 1, ox + hbx, oy + hby, hax, hay, hbx, hby, box, out, gap);
    hilbert(level - 1, ox + hax + hbx, oy + hay + hby, hax, hay, hbx, hby, box, out, gap);
    hilbert(level - 1, ox + ax + hbx, oy + ay + hby, -hbx, -hby, -hax, -hay, box, out, gap);
}

Polygons to_paths(const std::vector<std::vector<Point2d>>& curves) {
    Polygons out;
    for (const std::vector<Point2d>& curve : curves) {
        if (curve.size() < 2)
            continue;
        Polygon path;
        path.reserve(curve.size());
        for (const Point2d& p : curve)
            path.push_back({to_units(p.x), to_units(p.y)});
        out.push_back(std::move(path));
    }
    return out;
}

// The distances from p to the nearest and the farthest point of the box.
std::array<double, 2> radii(const Box& box, Point2d p) {
    const double dx = std::max({box.x0 - p.x, 0.0, p.x - box.x1}), dy = std::max({box.y0 - p.y, 0.0, p.y - box.y1});
    const double fx = std::max(std::fabs(p.x - box.x0), std::fabs(p.x - box.x1));
    const double fy = std::max(std::fabs(p.y - box.y0), std::fabs(p.y - box.y1));
    return {std::hypot(dx, dy), std::hypot(fx, fy)};
}

// The spiral of `radius(phi)` around the centre, through the angles `step`
// gives, over the turns that reach the box (`growth` mm apart).
template <class Radius, class Step>
Polygons spiral(Point2d centre, const Box& box, double growth, Radius radius, Step step) {
    const auto [near, far] = radii(box, centre);
    const double turn = 2.0 * std::numbers::pi;
    std::vector<Point2d> curve;
    for (double phi = std::max(0.0, std::floor((near - growth) / growth) * turn); radius(phi) <= far + growth;
         phi += step(phi)) {
        const double r = radius(phi);
        curve.push_back({centre.x + r * std::cos(phi), centre.y + r * std::sin(phi)});
    }
    return to_paths({curve});
}

} // namespace

std::vector<Toolpath> hilbert_fill(const Polygons& region, double spacing, double angle_degrees, double width) {
    if (!(spacing > 0.0) || region.empty())
        return {};
    const Frame f(region, width, angle_degrees);
    if (f.turned.empty())
        return {};
    // A square of cells around the origin big enough for any print (2 m a
    // side), so that the curve is the same in every layer; only the cells
    // near the region are visited.
    int level = 1;
    while (std::ldexp(spacing, level) < 2000.0)
        ++level;
    const double half = std::ldexp(spacing, level - 1);
    const Box box{to_mm(f.xmin) - spacing, to_mm(f.ymin) - spacing, to_mm(f.xmax) + spacing, to_mm(f.ymax) + spacing};
    std::vector<std::vector<Point2d>> curves;
    bool gap = true;
    hilbert(level, -half, -half, 2.0 * half, 0.0, 0.0, 2.0 * half, box, curves, gap);
    return f.clip(to_paths(curves), kMaxJoin * spacing * kUnitsPerMm, width);
}

std::vector<Toolpath> archimedean_fill(const Polygons& region, double spacing, Point2d centre, double width) {
    if (!(spacing > 0.0) || region.empty())
        return {};
    const Frame f(region, width, 0.0);
    if (f.turned.empty())
        return {};
    const Box box{to_mm(f.xmin), to_mm(f.ymin), to_mm(f.xmax), to_mm(f.ymax)};
    // r = a phi with the turns `spacing` apart; chords that stray at most
    // 0.05 mm from the curve (a chord of length c on radius r strays c^2 / 8r),
    // and at most an eighth of a turn.
    const double a = spacing / (2.0 * std::numbers::pi);
    const Polygons paths = spiral(
        centre, box, spacing, [&](double phi) { return a * phi; },
        [&](double phi) {
            const double r = std::max(a * phi, spacing);
            return std::min(std::sqrt(8.0 * 0.05 * r) / r, 0.25 * std::numbers::pi);
        });
    return f.clip(paths, kMaxJoin * spacing * kUnitsPerMm, width);
}

std::vector<Toolpath> octagram_fill(const Polygons& region, double spacing, Point2d centre, double width) {
    if (!(spacing > 0.0) || region.empty())
        return {};
    const Frame f(region, width, 0.0);
    if (f.turned.empty())
        return {};
    const Box box{to_mm(f.xmin), to_mm(f.ymin), to_mm(f.xmax), to_mm(f.ymax)};
    // The outline of two squares turned 45 degrees from each other has 16
    // sides, each on a line whose normal points at a multiple of 45 degrees
    // (in this order going round), all at the same distance from the centre.
    // Moving each line out by a sixteenth of `spacing` from the one before
    // turns the outline into a spiral whose sides keep their directions (0,
    // 45, 90 and 135 degrees) and lie `spacing` from the same side a turn
    // later; each corner is where two neighbouring lines cross.
    constexpr int kNormals[16] = {45, 0, 90, 45, 135, 90, 180, 135, 225, 180, 270, 225, 315, 270, 0, 315};
    const double step = spacing / 16.0;
    const auto [near, far] = radii(box, centre);
    const auto line = [&](std::int64_t i) {
        const double a = kNormals[i % 16] * std::numbers::pi / 180.0;
        return std::array<double, 3>{std::cos(a), std::sin(a), step * double(i)};
    };
    // The corners lie within sqrt(2) of a side's distance from the centre.
    std::vector<Point2d> curve;
    for (auto i = std::max<std::int64_t>(0, std::int64_t((near / std::numbers::sqrt2 - spacing) / step) / 16 * 16);
         step * double(i) <= far + spacing; ++i) {
        const auto [c1, s1, h1] = line(i);
        const auto [c2, s2, h2] = line(i + 1);
        const double det = c1 * s2 - s1 * c2;
        curve.push_back({centre.x + (h1 * s2 - h2 * s1) / det, centre.y + (h2 * c1 - h1 * c2) / det});
    }
    return f.clip(to_paths({curve}), kMaxJoin * spacing * kUnitsPerMm, width);
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
