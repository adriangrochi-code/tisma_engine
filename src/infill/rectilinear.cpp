// Rectilinear infill, with Clipper2 (BSL-1.0) open-path clipping. Own design;
// see PROVENANCE.md.

#include "infill/rectilinear.hpp"

#include "infill/adaptive.hpp"
#include "infill/connect.hpp"
#include "infill/gyroid.hpp"
#include "infill/patterns.hpp"
#include "perimeters/coverage.hpp"

#include <algorithm>
#include <set>
#include <map>
#include <cmath>
#include <limits>
#include <numbers>
#include <optional>

namespace tisma {
namespace {

using Clipper2Lib::Point64;

Point64 rotate(const Point64& p, double c, double s) {
    return {std::llround(double(p.x) * c - double(p.y) * s), std::llround(double(p.x) * s + double(p.y) * c)};
}

// Monotonic order (docs/spec/BEHAVIOR.md, "Superficies monotónicas") of
// horizontal pieces, each running left to right: a piece comes after every
// piece of the line below that touches it (their spans, widened by half a
// bead, overlap). Among the pieces that may come next, the one starting
// nearest to where the last one ended (ties: the first in the input, which
// comes sorted across the lines and along them).
// Without comparing every pair: the line below is found among the lines'
// heights, its touching pieces by their sorted starts, and the nearest ready
// piece in a grid of their starts (a large top surface has thousands).
Polygons order_monotonic(Polygons pieces, double step, double half_width) {
    const std::size_t n = pieces.size();
    if (n == 0)
        return pieces;
    std::vector<std::vector<std::size_t>> after(n); // pieces that wait for this one
    std::vector<std::size_t> waiting(n, 0);
    // The pieces of each line height, by their widened start.
    std::map<std::int64_t, std::vector<std::size_t>> rows;
    for (std::size_t q = 0; q < n; ++q)
        rows[pieces[q].front().y].push_back(q);
    const auto start = [&](std::size_t q) { return double(pieces[q].front().x) - half_width; };
    const auto end = [&](std::size_t q) { return double(pieces[q].back().x) + half_width; };
    // Each row sorted by start, with the greatest end so far: the pieces
    // touching [p0, p1] all lie before the last start <= p1, and none lies
    // before the place where that greatest end falls below p0.
    std::map<std::int64_t, std::vector<double>> reach;
    for (auto& [y, row] : rows) {
        std::stable_sort(row.begin(), row.end(), [&](std::size_t a, std::size_t b) { return start(a) < start(b); });
        std::vector<double>& m = reach[y];
        m.reserve(row.size());
        for (std::size_t q : row)
            m.push_back(m.empty() ? end(q) : std::max(m.back(), end(q)));
    }
    for (std::size_t p = 0; p < n; ++p) {
        const double py = double(pieces[p].front().y);
        const double p0 = start(p), p1 = end(p);
        // Heights within a quarter step of one step below.
        auto it = rows.lower_bound(std::int64_t(std::ceil(py - 1.25 * step)));
        for (; it != rows.end() && double(it->first) <= py - 0.75 * step; ++it) {
            if (std::fabs(py - double(it->first) - step) > 0.25 * step)
                continue;
            const std::vector<std::size_t>& row = it->second;
            const std::vector<double>& most = reach.at(it->first);
            std::size_t k = std::size_t(std::upper_bound(row.begin(), row.end(), p1,
                                                         [&](double v, std::size_t q) { return v < start(q); }) -
                                        row.begin());
            while (k > 0 && most[k - 1] >= p0) {
                --k;
                if (end(row[k]) >= p0) {
                    after[row[k]].push_back(p);
                    ++waiting[p];
                }
            }
        }
    }
    // The ready pieces' starts in a grid of square cells.
    std::int64_t x0 = INT64_MAX, y0 = INT64_MAX, x1 = INT64_MIN, y1 = INT64_MIN;
    for (const Polygon& piece : pieces) {
        x0 = std::min(x0, piece.front().x), x1 = std::max(x1, piece.front().x);
        y0 = std::min(y0, piece.front().y), y1 = std::max(y1, piece.front().y);
    }
    const double cell = std::max(4.0 * step, 1.0);
    const auto nx = std::size_t(double(x1 - x0) / cell) + 1, ny = std::size_t(double(y1 - y0) / cell) + 1;
    std::vector<std::vector<std::size_t>> grid(nx * ny);
    const auto cell_of = [&](double x, double y) {
        const auto cx = std::size_t(std::clamp((x - double(x0)) / cell, 0.0, double(nx - 1)));
        const auto cy = std::size_t(std::clamp((y - double(y0)) / cell, 0.0, double(ny - 1)));
        return std::pair{cx, cy};
    };
    std::set<std::size_t> ready;
    const auto add = [&](std::size_t p) {
        ready.insert(p);
        const auto [cx, cy] = cell_of(double(pieces[p].front().x), double(pieces[p].front().y));
        grid[cy * nx + cx].push_back(p);
    };
    const auto remove = [&](std::size_t p) {
        ready.erase(p);
        const auto [cx, cy] = cell_of(double(pieces[p].front().x), double(pieces[p].front().y));
        std::vector<std::size_t>& c = grid[cy * nx + cx];
        c.erase(std::find(c.begin(), c.end(), p));
    };
    for (std::size_t p = 0; p < n; ++p)
        if (waiting[p] == 0)
            add(p);
    Polygons out;
    out.reserve(n);
    while (!ready.empty()) {
        std::size_t best = *ready.begin(); // the first: nothing printed yet
        if (!out.empty()) {
            const double ex = double(out.back().back().x), ey = double(out.back().back().y);
            const auto [cx, cy] = cell_of(ex, ey);
            double nearest = std::numeric_limits<double>::max();
            best = n;
            const std::size_t reach = std::max(nx, ny);
            for (std::size_t r = 0; r <= reach; ++r) {
                // A start r cells away lies at least (r - 1) cells away.
                if (best != n && double(r) - 1.0 > 0.0 && (double(r) - 1.0) * cell > nearest)
                    break;
                const std::int64_t lo_x = std::int64_t(cx) - std::int64_t(r), hi_x = std::int64_t(cx) + std::int64_t(r);
                const std::int64_t lo_y = std::int64_t(cy) - std::int64_t(r), hi_y = std::int64_t(cy) + std::int64_t(r);
                for (std::int64_t gy = lo_y; gy <= hi_y; ++gy)
                    for (std::int64_t gx = lo_x; gx <= hi_x; ++gx) {
                        if (gx < 0 || gy < 0 || gx >= std::int64_t(nx) || gy >= std::int64_t(ny))
                            continue;
                        if (gx != lo_x && gx != hi_x && gy != lo_y && gy != hi_y)
                            continue; // inside: an earlier ring
                        for (std::size_t p : grid[std::size_t(gy) * nx + std::size_t(gx)]) {
                            const double d = std::hypot(double(pieces[p].front().x) - ex, double(pieces[p].front().y) - ey);
                            if (d < nearest || (d == nearest && p < best))
                                nearest = d, best = p;
                        }
                    }
            }
        }
        remove(best);
        for (std::size_t p : after[best])
            if (--waiting[p] == 0)
                add(p);
        out.push_back(std::move(pieces[best]));
    }
    return out;
}

} // namespace

std::vector<Toolpath> rectilinear_fill(const Polygons& region, double spacing, double angle_degrees, double width,
                                       bool connect, bool fit, bool monotonic, double overlap, double shift,
                                       double max_join, const Polygons* join_inside, bool whole) {
    std::vector<Toolpath> out;
    if (!(spacing > 0.0) || region.empty())
        return out;
    // A hair (10 nm) of slack keeps lines that run exactly along the shrunk
    // boundary, as fitted lines do.
    const Polygons inner = offset(region, -width / 2.0 + overlap + 1e-5);
    if (inner.empty())
        return out;
    // Work in a frame where the lines are horizontal: rotate by -angle.
    const double a = angle_degrees * std::numbers::pi / 180.0;
    const double c = std::cos(a), s = std::sin(a);
    Polygons turned;
    std::int64_t xmin = INT64_MAX, xmax = INT64_MIN, ymin = INT64_MAX, ymax = INT64_MIN;
    for (const Polygon& path : inner) {
        Polygon t;
        t.reserve(path.size());
        for (const Point64& p : path) {
            const Point64 q = rotate(p, c, -s);
            xmin = std::min(xmin, q.x), xmax = std::max(xmax, q.x);
            ymin = std::min(ymin, q.y), ymax = std::max(ymax, q.y);
            t.push_back(q);
        }
        turned.push_back(std::move(t));
    }
    const double step = spacing * kUnitsPerMm;
    const auto clip = [](const Polygons& lines, const Polygons& to, Polygons& open) {
        Clipper2Lib::Clipper64 clipper;
        clipper.AddOpenSubject(lines);
        clipper.AddClip(to);
        Polygons closed, pieces;
        clipper.Execute(Clipper2Lib::ClipType::Intersection, Clipper2Lib::FillRule::NonZero, closed, pieces);
        open.insert(open.end(), pieces.begin(), pieces.end());
    };
    Polygons open;
    if (fit) {
        // Each connected piece on its own (an outer contour and its holes): as
        // many lines as fit across it, centred, so its outer ones run along
        // its shrunk boundary. Fitted to the whole layer, a second island off
        // the first's spacing kept a strip of its edge bare (the review of
        // 2026-10-25).
        Clipper2Lib::Clipper64 joined;
        joined.AddSubject(turned);
        Clipper2Lib::PolyTree64 tree;
        joined.Execute(Clipper2Lib::ClipType::Union, Clipper2Lib::FillRule::NonZero, tree);
        std::vector<const Clipper2Lib::PolyPath64*> outers;
        for (const auto& outer : tree)
            outers.push_back(outer.get());
        for (std::size_t o = 0; o < outers.size(); ++o) {
            Polygons piece{outers[o]->Polygon()};
            for (const auto& hole : *outers[o]) {
                piece.push_back(hole->Polygon());
                for (const auto& island : *hole)
                    outers.push_back(island.get());
            }
            std::int64_t lo = INT64_MAX, hi = INT64_MIN, left = INT64_MAX, right = INT64_MIN;
            for (const Point64& q : piece.front()) {
                lo = std::min(lo, q.y), hi = std::max(hi, q.y);
                left = std::min(left, q.x), right = std::max(right, q.x);
            }
            const double across = double(hi - lo) - 20.0; // without the slack
            if (across < 0.0)
                continue;
            // Whole: enough lines to span it with at most `step` between them,
            // the outer ones on its shrunk edges; one line in the middle when
            // two would overlap by more than a quarter (narrower than 3/4 of
            // a step).
            const auto count = whole ? (across < 0.75 * step ? 1 : static_cast<std::int64_t>(std::ceil(across / step - 1e-9)) + 1)
                                     : static_cast<std::int64_t>(std::floor(across / step + 1e-9)) + 1;
            const double gap = whole && count > 1 ? across / double(count - 1) : step;
            const double first = double(lo) + 10.0 + (across - double(count - 1) * gap) / 2.0;
            Polygons lines;
            for (std::int64_t k = 0; k < count; ++k) {
                const std::int64_t y = std::llround(first + double(k) * gap);
                lines.push_back({{left - 1, y}, {right + 1, y}});
            }
            clip(lines, piece, open);
        }
    } else {
        Polygons lines;
        const double base = shift * kUnitsPerMm;
        for (auto k = static_cast<std::int64_t>(std::ceil((double(ymin) - base) / step));
             double(k) * step + base <= double(ymax); ++k) {
            const std::int64_t y = std::llround(double(k) * step + base);
            lines.push_back({{xmin - 1, y}, {xmax + 1, y}});
        }
        clip(lines, turned, open);
    }
    // Order across the lines, then along them, left to right.
    for (Polygon& piece : open)
        if (piece.size() >= 2 && piece.front().x > piece.back().x)
            std::reverse(piece.begin(), piece.end());
    std::sort(open.begin(), open.end(), [](const Polygon& l, const Polygon& r) {
        return l.front().y < r.front().y || (l.front().y == r.front().y && l.front().x < r.front().x);
    });
    open.erase(std::remove_if(open.begin(), open.end(), [](const Polygon& p) { return p.size() < 2; }), open.end());
    if (monotonic)
        open = order_monotonic(std::move(open), step, width / 2.0 * kUnitsPerMm);
    else if (connect)
    {
        // The region that holds the joins, in the same turned frame.
        Polygons inside_turned;
        if (join_inside)
            for (const Polygon& path : *join_inside) {
                Polygon t;
                t.reserve(path.size());
                for (const Point64& p : path)
                    t.push_back(rotate(p, c, -s));
                inside_turned.push_back(std::move(t));
            }
        open = join_along_boundary(open, turned, max_join > 0.0 ? max_join * kUnitsPerMm : kMaxJoin * step, step,
                                   join_inside ? &inside_turned : nullptr);
    }
    for (const Polygon& piece : open) {
        if (piece.size() < 2)
            continue;
        Toolpath t;
        for (const Point64& q : piece) {
            const Point64 p = rotate(q, c, s);
            t.points.push_back({{to_mm(p.x), to_mm(p.y)}, width});
        }
        out.push_back(std::move(t));
    }
    return out;
}

std::vector<Toolpath> gap_fill(const Polygons& region, const std::vector<Toolpath>& lines, double width, double overlap) {
    if (region.empty())
        return {};
    Polygons left = Clipper2Lib::Difference(region, bead_area(lines), Clipper2Lib::FillRule::NonZero);
    // Parts thinner than half a bead get no bead anyway; dropping them also drops
    // the hair-thin channels where two bead areas nearly touch.
    left = offset(offset(left, -width / 4.0), width / 4.0);
    if (left.empty())
        return {};
    BeadingParams beads;
    beads.preferred_width = width;
    beads.overlap = overlap;
    return layer_walls(left, beads).toolpaths;
}

std::size_t fill_period(const FillParams& params) {
    // The solid lines take turns every other layer.
    if (params.density <= 0.0)
        return 2;
    switch (params.pattern) {
    case InfillPattern::Gyroid:
    case InfillPattern::TpmsD:
    case InfillPattern::TpmsFk:
    case InfillPattern::Cubic:
    case InfillPattern::AdaptiveCubic:
    case InfillPattern::SupportCubic:
        return 0;
    case InfillPattern::Honeycomb:
        return 6;
    case InfillPattern::Honeycomb3d:
        return 0; // it moves with the height
    case InfillPattern::Rectilinear:
    case InfillPattern::Grid:
    case InfillPattern::Triangles:
    case InfillPattern::Lightning: // none here: lightning_layers draws it
    case InfillPattern::Stars:
    case InfillPattern::AlignedRectilinear:
    case InfillPattern::Zigzag:
    case InfillPattern::Line:
    case InfillPattern::Concentric:
    case InfillPattern::Hilbert:
    case InfillPattern::Archimedean:
    case InfillPattern::Octagram:
        return 2;
    }
    return 0;
}

double layer_angle(double angle, const std::vector<double>& angles, std::size_t layer) {
    if (!angles.empty())
        return angles[layer % angles.size()];
    return angle + (layer % 2 == 1 ? 90.0 : 0.0);
}

std::vector<Toolpath> multiline_paths(const std::vector<Toolpath>& paths, int lines, double spacing, double width,
                                      const Polygons& region) {
    std::vector<Toolpath> out;
    Clipper2Lib::Paths64 copies;
    for (const Toolpath& path : paths) {
        // Steps shorter than the copies' half width (a join's jog along a
        // wall) left out: their normals would twist the copies across each other.
        Toolpath t;
        t.closed = path.closed;
        for (const ToolpathPoint& q : path.points) {
            if (!t.points.empty() &&
                std::hypot(q.p.x - t.points.back().p.x, q.p.y - t.points.back().p.y) < 0.5 * lines * spacing) {
                if (&q == &path.points.back() && t.points.size() > 1)
                    t.points.back() = q; // the end stays where it is
                continue;
            }
            t.points.push_back(q);
        }
        const std::size_t n = t.points.size();
        if (n < 2)
            continue;
        // Each point's normal: the bisector of its segments' normals, longer
        // at a corner so the copy keeps its distance (at most twice).
        std::vector<Point2d> normal(n);
        const auto seg_normal = [&](std::size_t a, std::size_t b) {
            const double dx = t.points[b].p.x - t.points[a].p.x, dy = t.points[b].p.y - t.points[a].p.y;
            const double l = std::hypot(dx, dy);
            return l > 0.0 ? Point2d{-dy / l, dx / l} : Point2d{0.0, 0.0};
        };
        for (std::size_t i = 0; i < n; ++i) {
            const bool has_prev = t.closed || i > 0, has_next = t.closed || i + 1 < n;
            const Point2d a = has_prev ? seg_normal((i + n - 1) % n, i) : Point2d{0.0, 0.0};
            const Point2d b = has_next ? seg_normal(i, (i + 1) % n) : Point2d{0.0, 0.0};
            Point2d m{a.x + b.x, a.y + b.y};
            const double l = std::hypot(m.x, m.y);
            if (!(l > 1e-12)) {
                normal[i] = has_prev ? a : b;
                continue;
            }
            m = {m.x / l, m.y / l};
            const Point2d ref = has_prev ? a : b;
            const double c = std::max(0.5, m.x * ref.x + m.y * ref.y);
            normal[i] = {m.x / c, m.y / c};
        }
        for (int k = 0; k < lines; ++k) {
            const double shift = (double(k) - 0.5 * double(lines - 1)) * spacing;
            Clipper2Lib::Path64 copy;
            for (std::size_t i = 0; i <= n; ++i) {
                if (i == n && !t.closed)
                    break;
                const std::size_t j = i % n;
                copy.push_back({to_units(t.points[j].p.x + normal[j].x * shift), to_units(t.points[j].p.y + normal[j].y * shift)});
            }
            copies.push_back(std::move(copy));
        }
    }
    if (copies.empty())
        return out;
    Clipper2Lib::Clipper64 clipper;
    clipper.AddOpenSubject(copies);
    clipper.AddClip(region);
    Clipper2Lib::Paths64 closed, open;
    clipper.Execute(Clipper2Lib::ClipType::Intersection, Clipper2Lib::FillRule::NonZero, closed, open);
    for (const Clipper2Lib::Path64& p : open) {
        if (p.size() < 2)
            continue;
        Toolpath t;
        for (const auto& q : p)
            t.points.push_back({{to_mm(q.x), to_mm(q.y)}, width});
        out.push_back(std::move(t));
    }
    return out;
}

double trapezoid_row(double spacing, double density, double slant_degrees) {
    // A period of 2 (flat + run) holds 2 flats and 2 slants of row / sin a:
    // the length per area is (2 flat + 2 row / sin a) / (2 (flat + run) row).
    const double a = slant_degrees * std::numbers::pi / 180.0;
    const double run = 1.0 / std::tan(a), flat = std::max(1.0, 2.0 * run); // per row
    const double per_area = (2.0 * flat + 2.0 / std::sin(a)) / (2.0 * (flat + run)); // times 1 / row
    return per_area * spacing / density;
}

std::vector<Toolpath> trapezoid_fill(const Polygons& region, double row, double angle_degrees, double slant_degrees,
                                     double shift, double width) {
    std::vector<Toolpath> out;
    if (region.empty() || !(row > 0.0))
        return out;
    const double a = slant_degrees * std::numbers::pi / 180.0, t = angle_degrees * std::numbers::pi / 180.0;
    const double run = row / std::tan(a), flat = std::max(row, 2.0 * run), period = 2.0 * (flat + run);
    const double c = std::cos(t), s = std::sin(t);
    // The region's box in the rotated frame (u along the rows, v across).
    const Clipper2Lib::Rect64 box = Clipper2Lib::GetBounds(region);
    double u0 = 1e300, u1 = -1e300, v0 = 1e300, v1 = -1e300;
    for (const double x : {to_mm(box.left), to_mm(box.right)})
        for (const double y : {to_mm(box.top), to_mm(box.bottom)}) {
            const double u = x * c + y * s, v = -x * s + y * c;
            u0 = std::min(u0, u), u1 = std::max(u1, u), v0 = std::min(v0, v), v1 = std::max(v1, v);
        }
    const long k0 = long(std::floor((v0 - shift) / row)) - 1, k1 = long(std::ceil((v1 - shift) / row));
    const long p0 = long(std::floor(u0 / period)) - 1, p1 = long(std::ceil(u1 / period)) + 1;
    Clipper2Lib::Paths64 waves;
    const auto put = [&](Clipper2Lib::Path64& path, double u, double v) {
        path.push_back({to_units(u * c - v * s), to_units(u * s + v * c)});
    };
    for (long k = k0; k <= k1; ++k) {
        const double lo = shift + double(k) * row, hi = lo + row;
        Clipper2Lib::Path64 path;
        for (long p = p0; p <= p1; ++p) {
            const double u = double(p) * period;
            put(path, u, lo);
            put(path, u + flat, lo);
            put(path, u + flat + run, hi);
            put(path, u + 2.0 * flat + run, hi);
        }
        put(path, double(p1 + 1) * period, lo);
        waves.push_back(std::move(path));
    }
    Clipper2Lib::Clipper64 clipper;
    clipper.AddOpenSubject(waves);
    clipper.AddClip(region);
    Clipper2Lib::Paths64 closed, open;
    clipper.Execute(Clipper2Lib::ClipType::Intersection, Clipper2Lib::FillRule::NonZero, closed, open);
    for (const Clipper2Lib::Path64& p : open) {
        if (p.size() < 2)
            continue;
        Toolpath tp;
        for (const auto& q : p)
            tp.points.push_back({{to_mm(q.x), to_mm(q.y)}, width});
        out.push_back(std::move(tp));
    }
    return out;
}

LayerFill fill_layer(const LayerSurfaces& surfaces, std::size_t layer, const FillParams& params, double z) {
    LayerFill out;
    const double angle = layer_angle(params, layer);
    // The top surface itself, and under it the rest of the top layers, solid
    // like the bottom ones (surfaces_infill.md, rule 2).
    const Polygons surface = surfaces.top_under.empty()
                                 ? surfaces.top
                                 : Clipper2Lib::Difference(surfaces.top, surfaces.top_under, Clipper2Lib::FillRule::NonZero);
    const Polygons solid = surfaces.top_under.empty()
                               ? surfaces.bottom
                               : Clipper2Lib::Union(surfaces.bottom, surfaces.top_under, Clipper2Lib::FillRule::NonZero);
    const double top = params.top_width > 0.0 ? params.top_width : params.width;
    const double inner = params.solid_width > 0.0 ? params.solid_width : params.width;
    // Neighbouring lines overlap as the walls' beads (perimeters.md, rule 1):
    // a bead's spacing apart, the outer ones a spacing's half from the edge.
    const double d = std::clamp(params.overlap, 0.0, 0.5 * std::min(top, inner));
    // A surface in a curve pattern (infill_patterns.md, rule 16): the sparse
    // curves, dense, a spacing apart. Null for the line patterns.
    const auto curves = [&](SurfacePattern pattern, const Polygons& region, double width) -> std::optional<std::vector<Toolpath>> {
        const double spacing = width - d;
        switch (pattern) {
        case SurfacePattern::Concentric:
            return concentric_fill(region, spacing, width);
        case SurfacePattern::Hilbert:
            return hilbert_fill(region, spacing, params.angle, width);
        case SurfacePattern::Archimedean:
            return archimedean_fill(region, spacing, params.centre, width);
        case SurfacePattern::Octagram:
            return octagram_fill(region, spacing, params.centre, width);
        case SurfacePattern::SpiralInset: // orca_2_5.md, rule 13 bis
            return spiral_inset_fill(region, spacing, width);
        default:
            return std::nullopt;
        }
    };
    // A centre-out pattern in a set order (orca_2_5.md, rule 8): the concentric
    // loops come from the edge in, the spirals' pieces are sorted by how near
    // the centre they start and turned to run away from it; inward is the
    // reverse. Each is then printed in that order.
    const auto in_order = [&](std::vector<Toolpath> paths, SurfacePattern pattern, FillOrder order) {
        if (order == FillOrder::Default ||
            (pattern != SurfacePattern::Concentric && pattern != SurfacePattern::Archimedean &&
             pattern != SurfacePattern::Octagram && pattern != SurfacePattern::SpiralInset))
            return paths;
        bool outward = order == FillOrder::Outward;
        if (pattern == SurfacePattern::SpiralInset) {
            // Each spiral runs from the outside in: turned round for outward.
            for (Toolpath& t : paths) {
                if (outward)
                    std::reverse(t.points.begin(), t.points.end());
                t.in_order = t.one_way = true;
            }
            return paths;
        }
        if (pattern != SurfacePattern::Concentric) {
            const auto from_centre = [&](const ToolpathPoint& q) {
                return std::hypot(q.p.x - params.centre.x, q.p.y - params.centre.y);
            };
            std::vector<std::pair<double, std::size_t>> keys;
            for (std::size_t i = 0; i < paths.size(); ++i) {
                Toolpath& t = paths[i];
                if (!t.closed && t.points.size() >= 2 && from_centre(t.points.back()) < from_centre(t.points.front()))
                    std::reverse(t.points.begin(), t.points.end());
                keys.push_back({t.points.empty() ? 0.0 : from_centre(t.points.front()), i});
            }
            std::stable_sort(keys.begin(), keys.end());
            std::vector<Toolpath> sorted;
            for (const auto& [key, i] : keys)
                sorted.push_back(std::move(paths[i]));
            paths = std::move(sorted); // now from the centre out
            if (!outward)
                for (Toolpath& t : paths)
                    if (!t.closed)
                        std::reverse(t.points.begin(), t.points.end());
            outward = !outward; // the list runs from the centre: turned for inward
        }
        if (outward)
            std::reverse(paths.begin(), paths.end());
        for (Toolpath& t : paths) {
            t.in_order = true;
            t.one_way = !t.closed;
        }
        return paths;
    };
    // monotonic_lines (rule 14, P10): each line a piece of its own, and all
    // printed the same way, at the layer's angle: a piece that runs against
    // it is turned round.
    const auto one_way = [](std::vector<Toolpath> lines, double degrees) {
        const double a = degrees * std::numbers::pi / 180.0, ux = std::cos(a), uy = std::sin(a);
        for (Toolpath& t : lines)
            if (t.points.size() >= 2 && (t.points.back().p.x - t.points.front().p.x) * ux +
                                                (t.points.back().p.y - t.points.front().p.y) * uy <
                                            0.0)
                std::reverse(t.points.begin(), t.points.end());
        for (Toolpath& t : lines)
            t.one_way = true;
        return lines;
    };
    // A fixed angle for the top and the bottom surfaces (orca_2_5.md, rule 9),
    // or the layer's; the solid layers under a top surface keep the layer's.
    const bool fixed_top = params.top_angle >= 0.0, fixed_bottom = params.bottom_angle >= 0.0;
    const double top_angle = fixed_top ? params.top_angle : angle;
    const double bottom_angle = fixed_bottom ? params.bottom_angle : angle;
    // The top surface: monotonic lines (each a piece, in monotonic order),
    // the same all one way (monotonic_lines), zig-zags, zig-zags that do not
    // turn between layers (aligned_rectilinear, rule 15), or a curve.
    if (auto c = curves(params.top_pattern, surface, top))
        out.top = in_order(std::move(*c), params.top_pattern, params.top_order);
    else if (params.top_pattern == SurfacePattern::MonotonicLines)
        out.top = one_way(rectilinear_fill(surface, top - d, top_angle, top, false, true, true, params.wall_overlap * top + d / 2.0),
                          top_angle);
    else if (params.top_pattern == SurfacePattern::Monotonic)
        out.top = rectilinear_fill(surface, top - d, top_angle, top, false, true, true, params.wall_overlap * top + d / 2.0);
    else
        out.top = rectilinear_fill(surface, top - d,
                                   fixed_top                                                 ? params.top_angle
                                   : params.top_pattern == SurfacePattern::AlignedRectilinear ? params.angle
                                                                                              : angle,
                                   top, true, true, false, d / 2.0);
    // The bottom surfaces: monotonic like the top ones (surfaces.bottom_pattern),
    // reaching as far as the zig-zags, or zig-zags. The other patterns only
    // on the bottom surface itself (rule 17); the solid layers over it and
    // under a top surface zig-zag as always.
    const SurfacePattern bp = params.bottom_pattern;
    const bool lines_bottom = bp == SurfacePattern::Monotonic || bp == SurfacePattern::MonotonicLines;
    const auto zigzag = [&](const Polygons& region, double degrees) {
        return rectilinear_fill(region, inner - d, degrees, inner, true, true, false, d / 2.0);
    };
    // Zig-zags over the bottom part and the part under a top surface, each at its angle.
    const auto zigzag_solid = [&](const Polygons& bottom, std::vector<Toolpath>& to) {
        if (!fixed_bottom || surfaces.top_under.empty()) {
            const Polygons both = surfaces.top_under.empty()
                                      ? bottom
                                      : Clipper2Lib::Union(bottom, surfaces.top_under, Clipper2Lib::FillRule::NonZero);
            for (Toolpath& t : zigzag(both, fixed_bottom ? bottom_angle : angle))
                to.push_back(std::move(t));
            return;
        }
        const Polygons under = Clipper2Lib::Difference(surfaces.top_under, bottom, Clipper2Lib::FillRule::NonZero);
        for (Toolpath& t : zigzag(bottom, bottom_angle))
            to.push_back(std::move(t));
        for (Toolpath& t : zigzag(under, angle))
            to.push_back(std::move(t));
    };
    // A region's solid in a pattern: a curve, monotonic lines (all one way
    // with monotonic_lines), zig-zags fixed at the infill's angle
    // (aligned_rectilinear) or zig-zags at `degrees`.
    const auto solid_in = [&](SurfacePattern pattern, const Polygons& region, double degrees, FillOrder order) {
        if (auto c = curves(pattern, region, inner))
            return in_order(std::move(*c), pattern, order);
        if (pattern == SurfacePattern::Monotonic || pattern == SurfacePattern::MonotonicLines) {
            std::vector<Toolpath> lines = rectilinear_fill(region, inner - d, degrees, inner, false, true, true, d / 2.0);
            return pattern == SurfacePattern::MonotonicLines ? one_way(std::move(lines), degrees) : lines;
        }
        return zigzag(region, pattern == SurfacePattern::AlignedRectilinear ? params.angle : degrees);
    };
    if (params.internal_pattern != SurfacePattern::Rectilinear) {
        // The solid inside the part in its own pattern (rule 13 bis): the
        // bottom surface itself as the bottom pattern says, the rest (the
        // layers over it and under a top surface) in the internal one.
        const Polygons exposed = Clipper2Lib::Intersect(surfaces.bottom, surfaces.bottom_exposed, Clipper2Lib::FillRule::NonZero);
        const Polygons internal =
            Clipper2Lib::Union(Clipper2Lib::Difference(surfaces.bottom, exposed, Clipper2Lib::FillRule::NonZero),
                               surfaces.top_under, Clipper2Lib::FillRule::NonZero);
        if (!exposed.empty())
            out.solid = solid_in(bp, exposed, bottom_angle, params.bottom_order);
        if (!internal.empty())
            for (Toolpath& t : solid_in(params.internal_pattern, internal, angle, FillOrder::Default))
                out.solid.push_back(std::move(t));
    } else if (lines_bottom && !surfaces.bottom.empty()) {
        out.solid = rectilinear_fill(surfaces.bottom, inner - d, bottom_angle, inner, false, true, true, d / 2.0);
        if (bp == SurfacePattern::MonotonicLines)
            out.solid = one_way(std::move(out.solid), bottom_angle);
        for (Toolpath& t : zigzag(surfaces.top_under, angle))
            out.solid.push_back(std::move(t));
    } else if (bp != SurfacePattern::Rectilinear && !surfaces.bottom.empty()) {
        // The bottom surface itself in its pattern, the rest in zig-zags.
        const Polygons exposed = Clipper2Lib::Intersect(surfaces.bottom, surfaces.bottom_exposed, Clipper2Lib::FillRule::NonZero);
        if (auto c = curves(bp, exposed, inner))
            out.solid = in_order(std::move(*c), bp, params.bottom_order);
        else // aligned_rectilinear: the zig-zags at the infill's angle in every layer
            out.solid = zigzag(exposed, fixed_bottom ? bottom_angle : params.angle);
        if (fixed_bottom)
            zigzag_solid(Clipper2Lib::Difference(surfaces.bottom, exposed, Clipper2Lib::FillRule::NonZero), out.solid);
        else
            for (Toolpath& t : zigzag(Clipper2Lib::Difference(solid, exposed, Clipper2Lib::FillRule::NonZero), angle))
                out.solid.push_back(std::move(t));
    } else {
        zigzag_solid(surfaces.bottom, out.solid);
    }
    if (params.gap_fill && !(out.top.empty() && out.solid.empty())) {
        std::vector<Toolpath> lines = out.top;
        lines.insert(lines.end(), out.solid.begin(), out.solid.end());
        out.gaps = gap_fill(Clipper2Lib::Union(surfaces.top, surfaces.bottom, Clipper2Lib::FillRule::NonZero), lines,
                            params.width, d);
    }
    // Bridge lines are joined only where the join lies entirely over what
    // holds the bridge (toolpath_time.md, rule 1): a join along a free edge
    // would hang in the air, so those lines stay apart.
    const double bridge = params.bridge_width > 0.0 ? params.bridge_width : params.width;
    out.bridge = rectilinear_fill(surfaces.bridge, bridge, surfaces.bridge_angle, bridge, !surfaces.bridge_support.empty(),
                                  true, false, 0.0, 0.0, 2.0 * (bridge + bridge),
                                  surfaces.bridge_support.empty() ? nullptr : &surfaces.bridge_support, true);
    if (params.density <= 0.0)
        return out;
    const double w = params.sparse_width > 0.0 ? params.sparse_width : params.width;
    // The lines apart by the material they lay: a spacing each (w less the overlap).
    const double ws = w - std::clamp(params.overlap, 0.0, 0.5 * w);
    // Several lines per path (orca_2_5.md, rule 13): the pattern drawn N
    // times larger (density / N), each path as N touching lines; the same
    // material.
    // A pattern's length does not follow its density exactly (joins along
    // the walls, cells, clipping), so the density is corrected (up to six
    // times, the nearest kept) for the lines to add up to the single line's length.
    if (params.multiline > 1 && params.pattern != InfillPattern::Lightning && !surfaces.sparse.empty()) {
        const auto length = [](const std::vector<Toolpath>& paths) {
            double total = 0.0;
            for (const Toolpath& t : paths)
                for (std::size_t i = 1; i < t.points.size(); ++i)
                    total += std::hypot(t.points[i].p.x - t.points[i - 1].p.x, t.points[i].p.y - t.points[i - 1].p.y);
            return total;
        };
        LayerSurfaces only;
        only.sparse = surfaces.sparse;
        FillParams one = params;
        one.multiline = 1;
        const double target = length(fill_layer(only, layer, one, z).sparse);
        one.density = params.density / double(params.multiline);
        // The patterns whose lines cross (part 2): trapezoid waves that never
        // cross themselves, turning a family's angle each layer; a square
        // wave for the grid, on the triangles' 60 degrees for the others
        // (cubic's rows move with z like its lines).
        const bool grid = params.pattern == InfillPattern::Grid;
        const bool adaptive =
            params.pattern == InfillPattern::AdaptiveCubic || params.pattern == InfillPattern::SupportCubic;
        const bool waves = grid || adaptive || params.pattern == InfillPattern::Triangles ||
                           params.pattern == InfillPattern::Stars || params.pattern == InfillPattern::Cubic;
        const auto base = [&](const FillParams& f) {
            if (!waves)
                return fill_layer(only, layer, f, z).sparse;
            const double slant = grid ? 90.0 : 60.0;
            const double row = trapezoid_row(ws, f.density, slant);
            const double turn = params.angle + (grid ? 90.0 : 60.0) * double(layer % (grid ? 2 : 3));
            if (adaptive) // each cube size its own waves
                return adaptive_cubic_fill(surfaces.sparse, f.depth, 3.0 * ws / f.density, w, z, true, turn);
            const double shift = params.pattern == InfillPattern::Cubic ? std::fmod(-z / std::numbers::sqrt2, row) : 0.0;
            return trapezoid_fill(surfaces.sparse, row, turn, slant, shift, w);
        };
        out.sparse = multiline_paths(base(one), params.multiline, ws, w, surfaces.sparse);
        double got = length(out.sparse), best = std::abs(got - target);
        for (int round = 0; round < 6 && got > 0.0 && target > 0.0 && best > 0.01 * target; ++round) {
            one.density = std::min(1.0, one.density * target / got);
            std::vector<Toolpath> again = multiline_paths(base(one), params.multiline, ws, w, surfaces.sparse);
            got = length(again);
            if (std::abs(got - target) < best) {
                best = std::abs(got - target);
                out.sparse = std::move(again);
            }
        }
        return out;
    }
    switch (params.pattern) {
    case InfillPattern::Rectilinear:
    case InfillPattern::Zigzag: // the same in Tisma Slicer (infill_patterns.md, rule 2)
        out.sparse = rectilinear_fill(surfaces.sparse, ws / params.density, angle, w, true, false, false, 0.0, 0.0,
                                      params.max_link);
        break;
    case InfillPattern::AlignedRectilinear: // every layer at the infill angle (rule 1)
        out.sparse = rectilinear_fill(surfaces.sparse, ws / params.density, params.angle, w, true, false, false, 0.0, 0.0,
                                      params.max_link);
        break;
    case InfillPattern::Grid: {
        const double spacing = 2.0 * ws / params.density;
        out.sparse = rectilinear_fill(surfaces.sparse, spacing, params.angle, w);
        for (Toolpath& t : rectilinear_fill(surfaces.sparse, spacing, params.angle + 90.0, w))
            out.sparse.push_back(std::move(t));
        break;
    }
    case InfillPattern::Gyroid:
        out.sparse = gyroid_fill(surfaces.sparse, gyroid_period(ws, params.density), z, w);
        break;
    case InfillPattern::TpmsD:
    case InfillPattern::TpmsFk: {
        // Schwarz D and Fischer-Koch S (orca_2_5.md, rule 10), marched like
        // the gyroid with the period their own length per area gives.
        const Tpms kind = params.pattern == InfillPattern::TpmsD ? Tpms::SchwarzD : Tpms::FischerKochS;
        const double period = tpms_period(kind, ws, params.density);
        const GyroidField field = [&](double x, double y) { return tpms_value(kind, x, y, z, period); };
        // Fischer-Koch S has twice the frequency in each term: a finer grid.
        out.sparse = gyroid_fill(surfaces.sparse, kind == Tpms::FischerKochS ? 0.5 * period : period, z, w, &field);
        break;
    }
    case InfillPattern::Triangles:
    case InfillPattern::Stars:
    case InfillPattern::Cubic: {
        // Three families of lines 60 degrees apart, each a third of the
        // material. Triangles: the same in every layer, crossing at common
        // points. Stars: the second family half a spacing over, so the three
        // never meet at one point and draw six-pointed stars. Cubic: the faces
        // of cubes standing on a vertex, so each family moves across its lines
        // by z / sqrt(2) with the height (infill_patterns.md, rules 4 to 6).
        const double spacing = 3.0 * ws / params.density;
        const double shift = params.pattern == InfillPattern::Cubic ? -z / std::numbers::sqrt2 : 0.0;
        for (int k = 0; k < 3; ++k) {
            const double over = params.pattern == InfillPattern::Stars && k == 1 ? 0.5 * spacing : 0.0;
            for (Toolpath& t : rectilinear_fill(surfaces.sparse, spacing, params.angle + 60.0 * k, w, false,
                                                false, false, 0.0, std::fmod(shift, spacing) + over))
                out.sparse.push_back(std::move(t));
        }
        break;
    }
    case InfillPattern::AdaptiveCubic:
    case InfillPattern::SupportCubic:
        out.sparse = adaptive_cubic_fill(surfaces.sparse, params.depth, 3.0 * ws / params.density, w, z);
        break;
    case InfillPattern::Line: // every other line turned 6.7 degrees, the other way on odd layers (rule 3, P3)
        out.sparse = line_fill(surfaces.sparse, ws / params.density, angle, layer % 2 == 1 ? -6.7 : 6.7, w);
        break;
    case InfillPattern::Honeycomb: {
        // The principal direction goes through angle + 90, + 30 and - 30, a
        // layer each (rule 10, P8: 135, 75 and 15 degrees at 45 in layers 6, 7 and 5).
        const double principal = params.angle + 90.0 - 60.0 * double(layer % 3);
        out.sparse = honeycomb_fill(surfaces.sparse, honeycomb_side(ws, params.density), principal, w);
        break;
    }
    case InfillPattern::Honeycomb3d: // rows along 0 and 90 degrees, a layer each (rule 11, P8)
        out.sparse = honeycomb3d_fill(surfaces.sparse, ws / params.density, z, layer % 2 == 0 ? 0.0 : 90.0, w);
        break;
    case InfillPattern::Concentric:
        out.sparse = concentric_fill(surfaces.sparse, ws / params.density, w);
        break;
    case InfillPattern::Hilbert: // the same in every layer (rule 12)
        out.sparse = hilbert_fill(surfaces.sparse, ws / params.density, params.angle, w);
        break;
    case InfillPattern::Archimedean:
        out.sparse = archimedean_fill(surfaces.sparse, ws / params.density, params.centre, w);
        break;
    case InfillPattern::Octagram:
        out.sparse = octagram_fill(surfaces.sparse, ws / params.density, params.centre, w);
        break;
    case InfillPattern::Lightning:
        break; // all the layers at once, from the top down (lightning_layers)
    }
    return out;
}

} // namespace tisma
