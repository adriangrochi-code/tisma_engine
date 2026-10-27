// Painted areas by layer. Own design; see PROVENANCE.md.

#include "slice/painted.hpp"

#include <oneapi/tbb/parallel_for.h>

#include <algorithm>
#include <array>
#include <cmath>

namespace tisma {

namespace {

struct Q {
    double x, y, z;
};

// The part of a convex polygon on one side of a plane z = level.
std::vector<Q> cut(const std::vector<Q>& in, double level, bool keep_above) {
    std::vector<Q> out;
    const std::size_t n = in.size();
    for (std::size_t i = 0; i < n; ++i) {
        const Q& a = in[i];
        const Q& b = in[(i + 1) % n];
        const double da = keep_above ? a.z - level : level - a.z, db = keep_above ? b.z - level : level - b.z;
        if (da >= 0.0)
            out.push_back(a);
        if ((da >= 0.0) != (db >= 0.0)) {
            const double t = da / (da - db);
            out.push_back({a.x + t * (b.x - a.x), a.y + t * (b.y - a.y), level});
        }
    }
    return out;
}

} // namespace

std::vector<PaintedPiece> painted_pieces(const IndexedMesh& mesh, const Painting& painting, PaintChannel channel,
                                         std::uint8_t mark) {
    std::vector<PaintedPiece> pieces;
    if (!painting.has(channel, mark))
        return pieces;
    const std::size_t count = std::min(mesh.triangle_count(), painting.size(channel));
    for (std::size_t t = 0; t < count; ++t) {
        const auto& v = mesh.triangles[t];
        for_each_painted(painting, channel, t, mesh.vertices[v[0]], mesh.vertices[v[1]], mesh.vertices[v[2]],
                         [&](const Vec3f& a, const Vec3f& b, const Vec3f& c, std::uint8_t m) {
                             if (m == mark)
                                 pieces.push_back({a, b, c});
                         });
    }
    return pieces;
}

std::vector<Polygons> painted_areas(const std::vector<PaintedPiece>& pieces, const std::vector<Layer>& layers,
                                    double grow) {
    std::vector<Polygons> out(layers.size());
    if (layers.empty() || pieces.empty())
        return out;
    // Each layer's pieces: the layers are in order, so the ones a piece
    // reaches are a run found by bisection.
    std::vector<std::vector<std::uint32_t>> by_layer(layers.size());
    constexpr double kTouch = 1e-4; // mm: a face on a layer's boundary belongs to both layers
    for (std::size_t i = 0; i < pieces.size(); ++i) {
        const PaintedPiece& p = pieces[i];
        const double lo = std::min({p[0].z, p[1].z, p[2].z}), hi = std::max({p[0].z, p[1].z, p[2].z});
        const auto first = std::lower_bound(layers.begin(), layers.end(), lo - kTouch,
                                            [](const Layer& l, double z) { return l.top < z; });
        for (auto it = first; it != layers.end() && it->bottom <= hi + kTouch; ++it)
            by_layer[std::size_t(it - layers.begin())].push_back(std::uint32_t(i));
    }
    tbb::parallel_for(std::size_t(0), layers.size(), [&](std::size_t l) {
        if (by_layer[l].empty())
            return;
        Polygons faces, lines;
        for (const std::uint32_t i : by_layer[l]) {
            std::vector<Q> poly;
            for (const Vec3f& v : pieces[i])
                poly.push_back({v.x, v.y, v.z});
            poly = cut(cut(poly, layers[l].bottom - kTouch, true), layers[l].top + kTouch, false);
            if (poly.size() < 2)
                continue;
            Polygon p;
            for (const Q& q : poly)
                p.push_back({to_units(q.x), to_units(q.y)});
            const double area = Clipper2Lib::Area(p);
            if (std::fabs(area) > 1e-4 * kUnitsPerMm * kUnitsPerMm) { // a face seen from above or below
                if (area < 0.0)
                    std::reverse(p.begin(), p.end());
                faces.push_back(std::move(p));
            } else { // a wall: seen from above it is a line
                lines.push_back(std::move(p));
            }
        }
        Polygons area = Clipper2Lib::Union(faces, Clipper2Lib::FillRule::NonZero);
        if (grow > 0.0) {
            area = offset(area, grow);
            if (!lines.empty())
                area = Clipper2Lib::Union(area,
                                          Clipper2Lib::InflatePaths(lines, grow * kUnitsPerMm, Clipper2Lib::JoinType::Miter,
                                                                    Clipper2Lib::EndType::Square),
                                          Clipper2Lib::FillRule::NonZero);
        }
        out[l] = std::move(area);
    });
    return out;
}

} // namespace tisma
