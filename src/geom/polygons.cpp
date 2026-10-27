// Thin layer over Clipper2 (BSL-1.0). Own design; see PROVENANCE.md.

#include "geom/polygons.hpp"

#include <algorithm>

#include <oneapi/tbb/parallel_for.h>

namespace tisma {

Polygons to_polygons(const std::vector<Contour>& contours) {
    Polygons out;
    out.reserve(contours.size());
    for (const Contour& c : contours) {
        if (!c.closed || c.points.size() < 3)
            continue;
        Polygon p;
        p.reserve(c.points.size());
        for (const Vec3f& v : c.points)
            p.emplace_back(to_units(v.x), to_units(v.y));
        out.push_back(std::move(p));
    }
    return out;
}

Polygons layer_region(const Polygons& polygons) {
    return Clipper2Lib::Union(polygons, Clipper2Lib::FillRule::NonZero);
}

Polygons layer_region(const Polygons& polygons, FillMode mode) {
    switch (mode) {
    case FillMode::Union: return layer_region(polygons);
    case FillMode::EvenOdd: return Clipper2Lib::Union(polygons, Clipper2Lib::FillRule::EvenOdd);
    case FillMode::FillHoles: {
        Polygons outer;
        for (Polygon& p : layer_region(polygons))
            if (Clipper2Lib::Area(p) > 0)
                outer.push_back(std::move(p)); // holes are clockwise: negative area
        return Clipper2Lib::Union(outer, Clipper2Lib::FillRule::NonZero);
    }
    }
    return layer_region(polygons);
}

Polygons simplify(const Polygons& polygons, double tolerance_mm) {
    if (!(tolerance_mm > 0.0))
        return polygons;
    return Clipper2Lib::SimplifyPaths(polygons, tolerance_mm * kUnitsPerMm, true);
}

Polygons offset(const Polygons& region, double delta_mm, double miter_limit) {
    if (delta_mm == 0.0)
        return region;
    return Clipper2Lib::InflatePaths(region, delta_mm * kUnitsPerMm, Clipper2Lib::JoinType::Miter,
                                     Clipper2Lib::EndType::Polygon, miter_limit);
}

namespace {

void pieces_without_offset_holes(const Clipper2Lib::PolyPath64& outer, double delta_mm, Polygons& out) {
    Polygons holes;
    for (const auto& hole : outer) {
        Polygon filled = hole->Polygon();
        if (Clipper2Lib::Area(filled) < 0.0)
            std::reverse(filled.begin(), filled.end());
        holes.push_back(std::move(filled));
    }
    Polygons piece = {outer.Polygon()};
    if (!holes.empty()) {
        const Polygons grown = offset(holes, delta_mm);
        piece = Clipper2Lib::Difference(piece, grown, Clipper2Lib::FillRule::NonZero);
    }
    out.insert(out.end(), piece.begin(), piece.end());
    for (const auto& hole : outer)
        for (const auto& island : *hole)
            pieces_without_offset_holes(*island, delta_mm, out);
}

} // namespace

Polygons offset_holes(const Polygons& region, double delta_mm) {
    if (delta_mm == 0.0 || region.empty())
        return region;
    Clipper2Lib::Clipper64 clipper;
    clipper.AddSubject(region);
    Clipper2Lib::PolyTree64 tree;
    clipper.Execute(Clipper2Lib::ClipType::Union, Clipper2Lib::FillRule::NonZero, tree);
    Polygons pieces;
    for (const auto& outer : tree)
        pieces_without_offset_holes(*outer, delta_mm, pieces);
    return Clipper2Lib::Union(pieces, Clipper2Lib::FillRule::NonZero);
}

std::vector<Polygons> layer_regions(const std::vector<std::vector<Contour>>& layers) {
    std::vector<Polygons> out(layers.size());
    tbb::parallel_for(std::size_t(0), layers.size(), [&](std::size_t l) { out[l] = layer_region(to_polygons(layers[l])); });
    return out;
}

double area_mm2(const Polygons& region) {
    return Clipper2Lib::Area(region) / (kUnitsPerMm * kUnitsPerMm);
}

bool region_contains(const Polygons& region, double x, double y) {
    const Clipper2Lib::Point64 q{to_units(x), to_units(y)};
    int winding = 0;
    for (const Polygon& poly : region) {
        const Clipper2Lib::PointInPolygonResult r = Clipper2Lib::PointInPolygon(q, poly);
        if (r == Clipper2Lib::PointInPolygonResult::IsOn)
            return true;
        if (r == Clipper2Lib::PointInPolygonResult::IsInside)
            winding += Clipper2Lib::IsPositive(poly) ? 1 : -1;
    }
    return winding != 0;
}

Polygons cell_region(const std::vector<char>& mask, int nx, int ny, double x0, double y0, double hx, double hy) {
    Polygons boxes;
    for (int j = 0; j < ny; ++j) {
        int start = -1;
        for (int i = 0; i <= nx; ++i) {
            const bool set = i < nx && mask[std::size_t(j) * std::size_t(nx) + std::size_t(i)] != 0;
            if (set && start < 0)
                start = i;
            if (!set && start >= 0) {
                const double xa = x0 + start * hx, xb = x0 + i * hx;
                const double ya = y0 + j * hy, yb = ya + hy;
                boxes.push_back({{to_units(xa), to_units(ya)}, {to_units(xb), to_units(ya)},
                                 {to_units(xb), to_units(yb)}, {to_units(xa), to_units(yb)}});
                start = -1;
            }
        }
    }
    if (boxes.empty())
        return {};
    const Polygons joined = Clipper2Lib::Union(boxes, Clipper2Lib::FillRule::NonZero);
    const double half = 0.5 * std::max(hx, hy);
    return offset(offset(joined, half), -half);
}

// The convex hull of points, counter-clockwise (Andrew's monotone chain).
Polygon convex_hull(Polygon points) {
    std::sort(points.begin(), points.end(), [](const Clipper2Lib::Point64& a, const Clipper2Lib::Point64& b) {
        return a.x < b.x || (a.x == b.x && a.y < b.y);
    });
    points.erase(std::unique(points.begin(), points.end()), points.end());
    if (points.size() < 3)
        return points;
    const auto cross = [](const Clipper2Lib::Point64& o, const Clipper2Lib::Point64& a, const Clipper2Lib::Point64& b) {
        return double(a.x - o.x) * double(b.y - o.y) - double(a.y - o.y) * double(b.x - o.x);
    };
    Polygon hull(2 * points.size());
    std::size_t k = 0;
    for (std::size_t i = 0; i < points.size(); ++i) {
        while (k >= 2 && cross(hull[k - 2], hull[k - 1], points[i]) <= 0.0)
            --k;
        hull[k++] = points[i];
    }
    for (std::size_t i = points.size() - 1, t = k + 1; i-- > 0;) {
        while (k >= t && cross(hull[k - 2], hull[k - 1], points[i]) <= 0.0)
            --k;
        hull[k++] = points[i];
    }
    hull.resize(k - 1);
    return hull;
}

} // namespace tisma
