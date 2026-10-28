// Rectilinear infill, with Clipper2 (BSL-1.0) open-path clipping. Own design;
// see PROVENANCE.md.

#include "infill/rectilinear.hpp"

#include "infill/adaptive.hpp"
#include "infill/connect.hpp"
#include "infill/gyroid.hpp"
#include "infill/patterns.hpp"
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
            const auto count = static_cast<std::int64_t>(std::floor(across / step + 1e-9)) + 1;
            const double first = double(lo) + 10.0 + (across - double(count - 1) * step) / 2.0;
            Polygons lines;
            for (std::int64_t k = 0; k < count; ++k) {
                const std::int64_t y = std::llround(first + double(k) * step);
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
    case InfillPattern::Cubic:
    case InfillPattern::AdaptiveCubic:
    case InfillPattern::SupportCubic:
        return 0;
    case InfillPattern::Honeycomb:
        return 6;
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

LayerFill fill_layer(const LayerSurfaces& surfaces, std::size_t layer, const FillParams& params, double z) {
    LayerFill out;
    const double angle = params.angle + (layer % 2 == 1 ? 90.0 : 0.0);
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
    if (params.monotonic_top)
        out.top = rectilinear_fill(surface, top - d, angle, top, false, true, true, params.wall_overlap * top + d / 2.0);
    else
        out.top = rectilinear_fill(surface, top - d, angle, top, true, true, false, d / 2.0);
    out.solid = rectilinear_fill(solid, inner - d, angle, inner, true, true, false, d / 2.0);
    if (params.gap_fill && !(out.top.empty() && out.solid.empty())) {
        std::vector<Toolpath> lines = out.top;
        lines.insert(lines.end(), out.solid.begin(), out.solid.end());
        out.gaps = gap_fill(Clipper2Lib::Union(surfaces.top, surfaces.bottom, Clipper2Lib::FillRule::NonZero), lines,
                            params.width, d);
    }
    // Bridge lines stay straight: a join along the edge would hang in the air.
    const double bridge = params.bridge_width > 0.0 ? params.bridge_width : params.width;
    out.bridge = rectilinear_fill(surfaces.bridge, bridge, surfaces.bridge_angle, bridge, false, true);
    if (params.density <= 0.0)
        return out;
    const double w = params.sparse_width > 0.0 ? params.sparse_width : params.width;
    // The lines apart by the material they lay: a spacing each (w less the overlap).
    const double ws = w - std::clamp(params.overlap, 0.0, 0.5 * w);
    switch (params.pattern) {
    case InfillPattern::Rectilinear:
    case InfillPattern::Zigzag: // the same in Tisma Slicer (infill_patterns.md, rule 2)
        out.sparse = rectilinear_fill(surfaces.sparse, ws / params.density, angle, w, true);
        break;
    case InfillPattern::AlignedRectilinear: // every layer at the infill angle (rule 1)
        out.sparse = rectilinear_fill(surfaces.sparse, ws / params.density, params.angle, w, true);
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
