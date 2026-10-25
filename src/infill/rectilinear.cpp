// Rectilinear infill, with Clipper2 (BSL-1.0) open-path clipping. Own design;
// see PROVENANCE.md.

#include "infill/rectilinear.hpp"

#include "infill/adaptive.hpp"
#include "infill/connect.hpp"
#include "infill/gyroid.hpp"
#include "perimeters/coverage.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>

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
// nearest to where the last one ended.
Polygons order_monotonic(Polygons pieces, double step, double half_width) {
    const std::size_t n = pieces.size();
    std::vector<std::vector<std::size_t>> after(n); // pieces that wait for this one
    std::vector<std::size_t> waiting(n, 0);
    for (std::size_t p = 0; p < n; ++p)
        for (std::size_t q = 0; q < n; ++q) {
            const double dy = double(pieces[p].front().y - pieces[q].front().y);
            if (std::fabs(dy - step) > 0.25 * step)
                continue; // q is not on the line just below p
            const double p0 = double(pieces[p].front().x) - half_width, p1 = double(pieces[p].back().x) + half_width;
            const double q0 = double(pieces[q].front().x) - half_width, q1 = double(pieces[q].back().x) + half_width;
            if (p0 <= q1 && q0 <= p1) {
                after[q].push_back(p);
                ++waiting[p];
            }
        }
    std::vector<bool> done(n, false);
    Polygons out;
    out.reserve(n);
    for (std::size_t k = 0; k < n; ++k) {
        std::size_t best = n;
        double nearest = std::numeric_limits<double>::max();
        for (std::size_t p = 0; p < n; ++p) {
            if (done[p] || waiting[p] != 0)
                continue;
            const double d = out.empty() ? 0.0
                                         : std::hypot(double(pieces[p].front().x - out.back().back().x),
                                                      double(pieces[p].front().y - out.back().back().y));
            if (d < nearest) // pieces come sorted across the lines and along them, so ties keep that order
                nearest = d, best = p;
        }
        if (best == n)
            break; // cannot happen: the line below always comes first
        done[best] = true;
        for (std::size_t p : after[best])
            --waiting[p];
        out.push_back(pieces[best]);
    }
    return out;
}

} // namespace

std::vector<Toolpath> rectilinear_fill(const Polygons& region, double spacing, double angle_degrees, double width,
                                       bool connect, bool fit, bool monotonic, double overlap, double shift) {
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
    Polygons lines;
    if (fit) {
        // As many lines as fit across, centred: the outer ones run along the shrunk boundary.
        const double across = double(ymax - ymin) - 20.0; // without the slack
        const auto count = static_cast<std::int64_t>(std::floor(across / step + 1e-9)) + 1;
        const double first = double(ymin) + 10.0 + (across - double(count - 1) * step) / 2.0;
        for (std::int64_t k = 0; k < count; ++k) {
            const std::int64_t y = std::llround(first + double(k) * step);
            lines.push_back({{xmin - 1, y}, {xmax + 1, y}});
        }
    } else {
        const double base = shift * kUnitsPerMm;
        for (auto k = static_cast<std::int64_t>(std::ceil((double(ymin) - base) / step));
             double(k) * step + base <= double(ymax); ++k) {
            const std::int64_t y = std::llround(double(k) * step + base);
            lines.push_back({{xmin - 1, y}, {xmax + 1, y}});
        }
    }
    Clipper2Lib::Clipper64 clipper;
    clipper.AddOpenSubject(lines);
    clipper.AddClip(turned);
    Polygons closed, open;
    clipper.Execute(Clipper2Lib::ClipType::Intersection, Clipper2Lib::FillRule::NonZero, closed, open);
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
        open = join_along_boundary(open, turned, kMaxJoin * step, step);
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

std::vector<Toolpath> gap_fill(const Polygons& region, const std::vector<Toolpath>& lines, double width) {
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
    return layer_walls(left, beads).toolpaths;
}

LayerFill fill_layer(const LayerSurfaces& surfaces, std::size_t layer, const FillParams& params, double z) {
    LayerFill out;
    const double angle = params.angle + (layer % 2 == 1 ? 90.0 : 0.0);
    if (params.monotonic_top)
        out.top = rectilinear_fill(surfaces.top, params.width, angle, params.width, false, true, true,
                                   params.wall_overlap * params.width);
    else
        out.top = rectilinear_fill(surfaces.top, params.width, angle, params.width, true, true);
    out.solid = rectilinear_fill(surfaces.bottom, params.width, angle, params.width, true, true);
    if (params.gap_fill && !(out.top.empty() && out.solid.empty())) {
        std::vector<Toolpath> lines = out.top;
        lines.insert(lines.end(), out.solid.begin(), out.solid.end());
        out.gaps = gap_fill(Clipper2Lib::Union(surfaces.top, surfaces.bottom, Clipper2Lib::FillRule::NonZero), lines,
                            params.width);
    }
    // Bridge lines stay straight: a join along the edge would hang in the air.
    out.bridge = rectilinear_fill(surfaces.bridge, params.width, surfaces.bridge_angle, params.width, false, true);
    if (params.density <= 0.0)
        return out;
    switch (params.pattern) {
    case InfillPattern::Rectilinear:
        out.sparse = rectilinear_fill(surfaces.sparse, params.width / params.density, angle, params.width, true);
        break;
    case InfillPattern::Grid: {
        const double spacing = 2.0 * params.width / params.density;
        out.sparse = rectilinear_fill(surfaces.sparse, spacing, params.angle, params.width);
        for (Toolpath& t : rectilinear_fill(surfaces.sparse, spacing, params.angle + 90.0, params.width))
            out.sparse.push_back(std::move(t));
        break;
    }
    case InfillPattern::Gyroid:
        out.sparse = gyroid_fill(surfaces.sparse, gyroid_period(params.width, params.density), z, params.width);
        break;
    case InfillPattern::Triangles:
    case InfillPattern::Cubic: {
        // Three families of lines 60 degrees apart, each a third of the
        // material. Triangles: the same in every layer, crossing at common
        // points. Cubic: the faces of cubes standing on a vertex, so each family
        // moves across its lines by z / sqrt(2) with the height (infill_patterns.md,
        // rules 4 and 6).
        const double spacing = 3.0 * params.width / params.density;
        const double shift = params.pattern == InfillPattern::Cubic ? -z / std::numbers::sqrt2 : 0.0;
        for (int k = 0; k < 3; ++k)
            for (Toolpath& t : rectilinear_fill(surfaces.sparse, spacing, params.angle + 60.0 * k, params.width, false,
                                                false, false, 0.0, std::fmod(shift, spacing)))
                out.sparse.push_back(std::move(t));
        break;
    }
    case InfillPattern::AdaptiveCubic:
        out.sparse = adaptive_cubic_fill(surfaces.sparse, params.depth, 3.0 * params.width / params.density,
                                         params.width, z);
        break;
    }
    return out;
}

} // namespace tisma
