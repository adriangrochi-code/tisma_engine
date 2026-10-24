// Rectilinear infill, with Clipper2 (BSL-1.0) open-path clipping. Own design;
// see PROVENANCE.md.

#include "infill/rectilinear.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace tisma {
namespace {

using Clipper2Lib::Point64;

Point64 rotate(const Point64& p, double c, double s) {
    return {std::llround(double(p.x) * c - double(p.y) * s), std::llround(double(p.x) * s + double(p.y) * c)};
}

} // namespace

std::vector<Toolpath> rectilinear_fill(const Polygons& region, double spacing, double angle_degrees, double width) {
    std::vector<Toolpath> out;
    if (!(spacing > 0.0) || region.empty())
        return out;
    const Polygons inner = offset(region, -width / 2.0);
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
    for (auto k = static_cast<std::int64_t>(std::ceil(double(ymin) / step)); double(k) * step <= double(ymax); ++k) {
        const std::int64_t y = std::llround(double(k) * step);
        lines.push_back({{xmin - 1, y}, {xmax + 1, y}});
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

LayerFill fill_layer(const LayerSurfaces& surfaces, std::size_t layer, const FillParams& params) {
    LayerFill out;
    const double angle = params.angle + (layer % 2 == 1 ? 90.0 : 0.0);
    const Polygons solid = Clipper2Lib::Union(surfaces.top, surfaces.bottom, Clipper2Lib::FillRule::NonZero);
    out.solid = rectilinear_fill(solid, params.width, angle, params.width);
    if (params.density > 0.0)
        out.sparse = rectilinear_fill(surfaces.sparse, params.width / params.density, angle, params.width);
    return out;
}

} // namespace tisma
