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
#include <limits>
#include <tuple>
#include <map>
#include <numbers>
#include <utility>

#include <oneapi/tbb/parallel_for.h>

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
        Polygons open = clip_in_strips(paths);
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

    // The open paths inside the region. Clipper2's sweep keeps every edge that
    // crosses the current row active, so a large region full of short
    // segments (Hilbert, Archimedes) costs more than linearly: 4x the area,
    // 9x the time. Wide regions are cut into vertical strips, clipped apart
    // (in parallel) and the pieces joined again where they meet on a strip's
    // edge. The strips depend only on the paths, not on the cores.
    Polygons clip_in_strips(const Polygons& paths) const {
        std::size_t points = 0;
        for (const Polygon& p : paths)
            points += p.size();
        const std::int64_t lo = xmin - 1, hi = xmax + 1, top = ymax + 1, bottom = ymin - 1;
        // About 16k points or 40 mm a strip, whichever gives more.
        const std::size_t strips = std::min<std::size_t>(
            64, std::max<std::size_t>(points / 16384, std::size_t((hi - lo) / to_units(40.0))) + 1);
        if (strips == 1)
            return clip_open(paths, turned);
        std::vector<std::int64_t> cuts(strips + 1);
        for (std::size_t i = 0; i <= strips; ++i)
            cuts[i] = lo + std::int64_t(double(hi - lo) * double(i) / double(strips));
        // The region with a margin past the strip, so that the cut ends stay
        // strictly inside it and come out unchanged.
        const std::int64_t margin = std::max<std::int64_t>(to_units(1.0), (hi - lo) / std::int64_t(strips) / 8);
        std::vector<Polygons> pieces(strips);
        tbb::parallel_for(std::size_t(0), strips, [&](std::size_t i) {
            const Clipper2Lib::Rect64 rect(cuts[i], bottom, cuts[i + 1], top);
            const Clipper2Lib::Rect64 wide(cuts[i] - margin, bottom - margin, cuts[i + 1] + margin, top + margin);
            pieces[i] = clip_open(Clipper2Lib::RectClipLines(rect, paths), Clipper2Lib::RectClip(wide, turned));
        });
        return join_at_cuts(std::move(pieces), cuts);
    }

    static Polygons clip_open(const Polygons& paths, const Polygons& region) {
        if (paths.empty() || region.empty())
            return {};
        Clipper2Lib::Clipper64 clipper;
        clipper.AddOpenSubject(paths);
        clipper.AddClip(region);
        Polygons closed, open;
        clipper.Execute(Clipper2Lib::ClipType::Intersection, Clipper2Lib::FillRule::NonZero, closed, open);
        return open;
    }

    // Joins the pieces whose ends meet on an inner cut: the same point, made
    // by the same rectangle edge in both strips.
    static Polygons join_at_cuts(std::vector<Polygons> strips, const std::vector<std::int64_t>& cuts) {
        Polygons all;
        for (Polygons& s : strips)
            for (Polygon& p : s)
                if (p.size() >= 2)
                    all.push_back(std::move(p));
        const auto on_cut = [&](const Point64& q) {
            return std::binary_search(cuts.begin() + 1, cuts.end() - 1, q.x);
        };
        // end = 2 * piece + (0: front, 1: back)
        std::map<std::pair<std::int64_t, std::int64_t>, std::vector<std::size_t>> at;
        for (std::size_t k = 0; k < all.size(); ++k)
            for (int e = 0; e < 2; ++e) {
                const Point64& q = e ? all[k].back() : all[k].front();
                if (on_cut(q))
                    at[{q.x, q.y}].push_back(2 * k + std::size_t(e));
            }
        std::vector<std::size_t> mate(2 * all.size(), SIZE_MAX);
        for (const auto& [point, ends] : at)
            if (ends.size() == 2 && ends[0] / 2 != ends[1] / 2) {
                mate[ends[0]] = ends[1];
                mate[ends[1]] = ends[0];
            }
        std::vector<char> used(all.size(), 0);
        Polygons out;
        const auto walk = [&](std::size_t k, int free_end) {
            Polygon chain;
            for (;;) {
                used[k] = 1;
                Polygon& p = all[k];
                if (free_end == 1) // enters by its back: read it backwards
                    std::reverse(p.begin(), p.end());
                chain.insert(chain.end(), chain.empty() ? p.begin() : p.begin() + 1, p.end());
                const std::size_t far = 2 * k + std::size_t(1 - free_end);
                const std::size_t next = mate[far];
                if (next == SIZE_MAX || used[next / 2])
                    break;
                k = next / 2;
                free_end = int(next % 2);
            }
            out.push_back(std::move(chain));
        };
        for (std::size_t k = 0; k < all.size(); ++k)
            if (!used[k] && (mate[2 * k] == SIZE_MAX || mate[2 * k + 1] == SIZE_MAX))
                walk(k, mate[2 * k] == SIZE_MAX ? 0 : 1);
        for (std::size_t k = 0; k < all.size(); ++k) // closed chains, if any
            if (!used[k])
                walk(k, 0);
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

std::vector<Toolpath> honeycomb3d_fill(const Polygons& region, double spacing, double z, double principal_degrees,
                                       double width) {
    if (!(spacing > 0.0) || region.empty())
        return {};
    const Frame f(region, width, principal_degrees);
    if (f.turned.empty())
        return {};
    // Along a row, a period of 2 spacings: along at +a, a slant down to -a,
    // along at -a, a slant up; a, from 0 to a quarter period, a triangle wave
    // in z of that period (straight rows, then full zigzags, then straight).
    const double period = 2.0 * spacing * kUnitsPerMm, quarter = period / 4.0;
    const double phase = z * kUnitsPerMm / period;
    const double a = quarter * std::fabs(2.0 * (phase - std::floor(phase)) - 1.0);
    const double gap = spacing * kUnitsPerMm;
    const auto n0 = static_cast<std::int64_t>(std::floor(double(f.xmin) / period)) - 1;
    const auto n1 = static_cast<std::int64_t>(std::ceil(double(f.xmax) / period)) + 1;
    Polygons rows;
    for (auto m = static_cast<std::int64_t>(std::floor(double(f.ymin) / gap)) - 1; double(m) * gap <= double(f.ymax) + gap;
         ++m) {
        const double y = double(m) * gap, sign = m % 2 == 0 ? 1.0 : -1.0;
        Polygon row;
        row.reserve(std::size_t(4 * (n1 - n0 + 1)));
        for (std::int64_t n = n0; n <= n1; ++n) {
            const double x = double(n) * period;
            row.push_back({std::llround(x + a), std::llround(y + sign * a)});
            row.push_back({std::llround(x + 2.0 * quarter - a), std::llround(y + sign * a)});
            row.push_back({std::llround(x + 2.0 * quarter + a), std::llround(y - sign * a)});
            row.push_back({std::llround(x + period - a), std::llround(y - sign * a)});
        }
        rows.push_back(std::move(row));
    }
    return f.clip(rows, kMaxJoin * gap, width);
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
    // A square of cells around the origin big enough for any print, so that
    // the curve is the same in every layer; only the cells near the region
    // are visited. 16 m a side keeps a part anywhere on a large bed inside it
    // once turned (2 m lost the corners of a 1 m bed); a region beyond that
    // grows the square.
    const double reach = std::max({8192.0, std::fabs(to_mm(f.xmin)), std::fabs(to_mm(f.xmax)),
                                   std::fabs(to_mm(f.ymin)), std::fabs(to_mm(f.ymax))}) +
                         spacing;
    int level = 1;
    while (std::ldexp(spacing, level) < 2.0 * reach)
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

std::vector<Toolpath> spiral_inset_fill(const Polygons& region, double spacing, double width) {
    std::vector<Toolpath> out;
    if (!(spacing > 0.0) || region.empty())
        return out;
    // The loops, level by level from the outside, in mm.
    struct Loop {
        std::vector<Point2d> points;
        std::size_t level = 0;
        bool positive = true;
        bool used = false;
    };
    std::vector<Loop> loops;
    std::size_t level = 0;
    for (Polygons ring = offset(region, -width / 2.0); !ring.empty(); ring = offset(ring, -spacing), ++level)
        for (const Polygon& p : ring) {
            if (p.size() < 3)
                continue;
            Loop loop;
            loop.level = level;
            loop.positive = Clipper2Lib::IsPositive(p);
            for (const Point64& q : p)
                loop.points.push_back({to_mm(q.x), to_mm(q.y)});
            loops.push_back(std::move(loop));
        }
    const Polygons inside = offset(region, -width / 4.0); // a step must stay in here
    const auto step_inside = [&](Point2d a, Point2d b) {
        Clipper2Lib::Clipper64 clipper;
        clipper.AddOpenSubject({{{to_units(a.x), to_units(a.y)}, {to_units(b.x), to_units(b.y)}}});
        clipper.AddClip(inside);
        Clipper2Lib::Paths64 closed, open;
        clipper.Execute(Clipper2Lib::ClipType::Difference, Clipper2Lib::FillRule::NonZero, closed, open);
        double outside = 0.0;
        for (const Clipper2Lib::Path64& o : open)
            for (std::size_t k = 1; k < o.size(); ++k)
                outside += std::hypot(to_mm(o[k].x - o[k - 1].x), to_mm(o[k].y - o[k - 1].y));
        return outside < 1e-3;
    };
    // The loop's points from index `from`, round to one spacing before it closes.
    const auto walk = [&](const std::vector<Point2d>& pts, std::size_t from, std::vector<ToolpathPoint>& to) {
        const std::size_t n = pts.size();
        double total = 0.0;
        for (std::size_t k = 0; k < n; ++k)
            total += std::hypot(pts[(k + 1) % n].x - pts[k].x, pts[(k + 1) % n].y - pts[k].y);
        const double stop = std::max(0.0, total - spacing);
        double run = 0.0;
        to.push_back({pts[from], width});
        for (std::size_t k = 0; k < n; ++k) {
            const Point2d a = pts[(from + k) % n], b = pts[(from + k + 1) % n];
            const double len = std::hypot(b.x - a.x, b.y - a.y);
            if (run + len >= stop) {
                const double t = len > 0.0 ? (stop - run) / len : 0.0;
                to.push_back({{a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t}, width});
                return;
            }
            run += len;
            to.push_back({b, width});
        }
    };
    // The nearest point of a loop to p: the index of the segment and the point on it.
    const auto nearest = [](const std::vector<Point2d>& pts, Point2d p) {
        std::size_t best = 0;
        Point2d at = pts[0];
        double d = std::numeric_limits<double>::max();
        for (std::size_t k = 0; k < pts.size(); ++k) {
            const Point2d a = pts[k], b = pts[(k + 1) % pts.size()];
            const double dx = b.x - a.x, dy = b.y - a.y, l2 = dx * dx + dy * dy;
            const double t = l2 > 0.0 ? std::clamp(((p.x - a.x) * dx + (p.y - a.y) * dy) / l2, 0.0, 1.0) : 0.0;
            const Point2d q{a.x + dx * t, a.y + dy * t};
            const double e = std::hypot(q.x - p.x, q.y - p.y);
            if (e < d)
                d = e, best = k, at = q;
        }
        return std::tuple{best, at, d};
    };
    // A loop opened at a point on its segment k.
    const auto opened = [](std::vector<Point2d> pts, std::size_t k, Point2d at) {
        pts.insert(pts.begin() + std::ptrdiff_t(k + 1), at);
        std::rotate(pts.begin(), pts.begin() + std::ptrdiff_t(k + 1), pts.end());
        return pts;
    };
    for (std::size_t first = 0; first < loops.size(); ++first) {
        if (loops[first].used)
            continue;
        // The start: the convex corner nearest to a right angle.
        const std::vector<Point2d>& pts = loops[first].points;
        std::size_t start = 0;
        double best = std::numeric_limits<double>::max();
        for (std::size_t k = 0; k < pts.size(); ++k) {
            const Point2d a = pts[(k + pts.size() - 1) % pts.size()], b = pts[k], c = pts[(k + 1) % pts.size()];
            const double ux = b.x - a.x, uy = b.y - a.y, vx = c.x - b.x, vy = c.y - b.y;
            const double cross = ux * vy - uy * vx, lu = std::hypot(ux, uy), lv = std::hypot(vx, vy);
            if (!(lu > 0.0) || !(lv > 0.0) || (cross > 0.0) != loops[first].positive)
                continue;
            const double turn = std::acos(std::clamp((ux * vx + uy * vy) / (lu * lv), -1.0, 1.0));
            const double off = std::abs(turn - std::numbers::pi / 2.0);
            if (off < best - 1e-9)
                best = off, start = k;
        }
        Toolpath spiral;
        std::size_t current = first;
        std::vector<Point2d> ring = loops[first].points;
        std::rotate(ring.begin(), ring.begin() + std::ptrdiff_t(start), ring.end());
        for (;;) {
            loops[current].used = true;
            walk(ring, 0, spiral.points);
            const Point2d end = spiral.points.back().p;
            // The next loop in: the nearest unused one of the next level.
            std::size_t next = loops.size();
            std::size_t segment = 0;
            Point2d at{};
            double d = 4.0 * spacing;
            for (std::size_t k = 0; k < loops.size(); ++k) {
                if (loops[k].used || loops[k].level != loops[current].level + 1)
                    continue;
                const auto [s, p, e] = nearest(loops[k].points, end);
                if (e <= d && step_inside(end, p))
                    d = e, next = k, segment = s, at = p;
            }
            if (next == loops.size())
                break;
            current = next;
            ring = opened(loops[next].points, segment, at);
        }
        if (spiral.points.size() >= 2)
            out.push_back(std::move(spiral));
    }
    return out;
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
