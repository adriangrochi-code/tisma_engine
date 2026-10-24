// Thin layer over Clipper2 (BSL-1.0). Own design; see PROVENANCE.md.

#include "geom/polygons.hpp"

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

Polygons offset(const Polygons& region, double delta_mm, double miter_limit) {
    if (delta_mm == 0.0)
        return region;
    return Clipper2Lib::InflatePaths(region, delta_mm * kUnitsPerMm, Clipper2Lib::JoinType::Miter,
                                     Clipper2Lib::EndType::Polygon, miter_limit);
}

std::vector<Polygons> layer_regions(const std::vector<std::vector<Contour>>& layers) {
    std::vector<Polygons> out(layers.size());
    tbb::parallel_for(std::size_t(0), layers.size(), [&](std::size_t l) { out[l] = layer_region(to_polygons(layers[l])); });
    return out;
}

double area_mm2(const Polygons& region) {
    return Clipper2Lib::Area(region) / (kUnitsPerMm * kUnitsPerMm);
}

} // namespace tisma
