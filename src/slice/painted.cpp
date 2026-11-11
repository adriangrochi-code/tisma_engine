// Painted areas by layer. Own design; see PROVENANCE.md.

#include "slice/painted.hpp"

#include <oneapi/tbb/parallel_for.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

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

std::vector<std::vector<ColorZone>> color_zones(const IndexedMesh& mesh, const Painting& painting,
                                                const std::vector<Layer>& layers, const std::vector<Polygons>& regions,
                                                double step, double depth, int top_layers, int bottom_layers) {
    std::vector<std::vector<ColorZone>> out(layers.size());
    if (painting.empty(PaintChannel::Color) || !(step > 0.0))
        return out;
    // Each mark's band at the outline, by layer: its side faces' band, and its
    // tops' and bottoms' areas carried down and up into the part.
    std::vector<std::pair<std::uint8_t, std::vector<Polygons>>> bands;
    for (int m = 1; m < Painting::kSplit; ++m) {
        if (!painting.has(PaintChannel::Color, std::uint8_t(m)))
            continue;
        std::vector<PaintedPiece> sides, tops, bottoms;
        for (const PaintedPiece& p : painted_pieces(mesh, painting, PaintChannel::Color, std::uint8_t(m))) {
            const double ux = p[1].x - p[0].x, uy = p[1].y - p[0].y, uz = p[1].z - p[0].z;
            const double vx = p[2].x - p[0].x, vy = p[2].y - p[0].y, vz = p[2].z - p[0].z;
            const double nx = uy * vz - uz * vy, ny = uz * vx - ux * vz, nz = ux * vy - uy * vx;
            const double len = std::sqrt(nx * nx + ny * ny + nz * nz);
            if (len > 0.0 && nz > 0.7 * len)
                tops.push_back(p);
            else if (len > 0.0 && nz < -0.7 * len)
                bottoms.push_back(p);
            else
                sides.push_back(p);
        }
        std::vector<Polygons> band = painted_areas(sides, layers, step);
        const auto carry = [&](const std::vector<PaintedPiece>& pieces, bool down, int count) {
            const std::vector<Polygons> at = painted_areas(pieces, layers, step / 2.0);
            for (std::size_t l = 0; l < at.size(); ++l) {
                if (at[l].empty())
                    continue;
                const double from = down ? layers[l].top : layers[l].bottom;
                for (std::size_t k = 0; k < layers.size(); ++k) {
                    const std::size_t j = down ? l - std::min(l, k) : l + k;
                    if (j >= layers.size() || (down && k > l))
                        break;
                    const double reach = down ? from - layers[j].top : layers[j].bottom - from;
                    if (depth > 0.0 ? reach > depth - 1e-9 : int(k) >= std::max(count, 1))
                        break;
                    band[j] = Clipper2Lib::Union(band[j], at[l], Clipper2Lib::FillRule::NonZero);
                }
            }
        };
        carry(tops, true, top_layers);
        carry(bottoms, false, bottom_layers);
        bands.push_back({std::uint8_t(m), std::move(band)});
    }
    if (bands.empty())
        return out;
    const std::size_t steps =
        depth > 0.0 ? std::size_t(std::ceil(depth / step)) : std::numeric_limits<std::size_t>::max();
    tbb::parallel_for(std::size_t(0), std::min(layers.size(), regions.size()), [&](std::size_t l) {
        const Polygons& region = regions[l];
        if (region.empty())
            return;
        // The labels: 0 (the part's own, unpainted faces) and each mark.
        std::vector<std::uint8_t> marks{0};
        std::vector<Polygons> zone{};
        Polygons painted;
        for (const auto& [m, by_layer] : bands)
            if (l < by_layer.size() && !by_layer[l].empty()) {
                Polygons z = Clipper2Lib::Intersect(by_layer[l], region, Clipper2Lib::FillRule::NonZero);
                if (z.empty())
                    continue;
                marks.push_back(m);
                painted = Clipper2Lib::Union(painted, z, Clipper2Lib::FillRule::NonZero);
                zone.push_back(std::move(z));
            }
        if (marks.size() == 1)
            return;
        // The unpainted band at the outline.
        const Polygons ring = Clipper2Lib::Difference(region, offset(region, -step), Clipper2Lib::FillRule::NonZero);
        zone.insert(zone.begin(), Clipper2Lib::Difference(ring, painted, Clipper2Lib::FillRule::NonZero));
        Polygons taken = Clipper2Lib::Union(zone.front(), painted, Clipper2Lib::FillRule::NonZero);
        // Grown all together, a step at a time, each over what is not taken yet.
        std::vector<Polygons> front = zone;
        for (std::size_t s = 0; s < steps; ++s) {
            bool grew = false;
            Polygons now = taken;
            for (std::size_t k = 0; k < zone.size(); ++k) {
                if (front[k].empty())
                    continue;
                Polygons more = Clipper2Lib::Intersect(offset(front[k], step), region, Clipper2Lib::FillRule::NonZero);
                more = Clipper2Lib::Difference(more, taken, Clipper2Lib::FillRule::NonZero);
                more = Clipper2Lib::Difference(more, now, Clipper2Lib::FillRule::NonZero); // first label this step wins
                if (more.empty() || std::abs(Clipper2Lib::Area(more)) < 1.0) {
                    front[k].clear();
                    continue;
                }
                zone[k] = Clipper2Lib::Union(zone[k], more, Clipper2Lib::FillRule::NonZero);
                now = Clipper2Lib::Union(now, more, Clipper2Lib::FillRule::NonZero);
                front[k] = std::move(more);
                grew = true;
            }
            taken = std::move(now);
            if (!grew)
                break;
        }
        // Two steps thin or thinner there is no room for two filaments: a painted
        // side's colour goes through (multi_extruder.md, rule 18, ME17); two
        // colours keep the halves they grew.
        // Two widths count (ME17: a 0.8 mm wall with 0.4 mm lines).
        const Polygons thin = Clipper2Lib::Difference(region, offset(offset(region, -1.05 * step), 1.05 * step),
                                                      Clipper2Lib::FillRule::NonZero);
        if (!thin.empty()) {
            const std::vector<Polygons> grown = zone;
            for (std::size_t k = 1; k < zone.size(); ++k) {
                Polygons more = Clipper2Lib::Intersect(thin, offset(grown[k], 2.0 * step), Clipper2Lib::FillRule::NonZero);
                for (std::size_t o = 1; o < zone.size(); ++o)
                    if (o != k)
                        more = Clipper2Lib::Difference(more, grown[o], Clipper2Lib::FillRule::NonZero);
                if (!more.empty())
                    zone[k] = Clipper2Lib::Union(zone[k], more, Clipper2Lib::FillRule::NonZero);
            }
        }
        for (std::size_t k = 1; k < zone.size(); ++k)
            if (!zone[k].empty())
                out[l].push_back({marks[k], std::move(zone[k])});
    });
    return out;
}

std::vector<std::pair<std::uint8_t, Toolpath>> split_by_zones(const Toolpath& path, const std::vector<ColorZone>& zones,
                                                              double step) {
    std::vector<std::pair<std::uint8_t, Toolpath>> out;
    if (path.points.empty())
        return out;
    // Inside a zone: its contours' winding (outer ones counter-clockwise,
    // holes clockwise) is not zero there.
    const auto mark_at = [&](double x, double y) {
        const Clipper2Lib::Point64 q{to_units(x), to_units(y)};
        for (const ColorZone& z : zones) {
            int winding = 0;
            for (const auto& poly : z.area)
                if (Clipper2Lib::PointInPolygon(q, poly) != Clipper2Lib::PointInPolygonResult::IsOutside)
                    winding += Clipper2Lib::Area(poly) > 0 ? 1 : -1;
            if (winding != 0)
                return z.mark;
        }
        return std::uint8_t(0);
    };
    std::vector<ToolpathPoint> points = path.points;
    if (path.closed && points.size() > 1)
        points.push_back(points.front());
    const auto lerp = [](const ToolpathPoint& a, const ToolpathPoint& b, double t) {
        ToolpathPoint q = a;
        q.p.x = a.p.x + (b.p.x - a.p.x) * t, q.p.y = a.p.y + (b.p.y - a.p.y) * t;
        q.width = a.width + (b.width - a.width) * t;
        q.dz = a.dz + (b.dz - a.dz) * t;
        q.below = a.below + (b.below - a.below) * t;
        return q;
    };
    Toolpath piece = path;
    piece.closed = false;
    piece.points = {points.front()};
    int now = -1;
    for (std::size_t i = 1; i < points.size(); ++i) {
        const ToolpathPoint& a = points[i - 1];
        const ToolpathPoint& b = points[i];
        const double length = std::hypot(b.p.x - a.p.x, b.p.y - a.p.y);
        const int parts = std::max(1, int(std::ceil(length / step)));
        for (int k = 1; k <= parts; ++k) {
            const ToolpathPoint end = lerp(a, b, double(k) / parts);
            const ToolpathPoint mid = lerp(a, b, (k - 0.5) / parts);
            const int m = mark_at(mid.p.x, mid.p.y);
            if (now >= 0 && m != now) {
                out.push_back({std::uint8_t(now), piece});
                piece.points = {piece.points.back()};
            }
            now = m;
            piece.points.push_back(end);
        }
    }
    if (piece.points.size() >= 2 && now >= 0)
        out.push_back({std::uint8_t(now), piece});
    // Only one colour: the path as it was. A loop cut open: its last piece
    // and its first, of one colour, are one.
    if (out.size() == 1) {
        out.front().second = path;
    } else if (path.closed && out.size() > 1 && out.front().first == out.back().first) {
        Toolpath& last = out.back().second;
        last.points.insert(last.points.end(), out.front().second.points.begin() + 1, out.front().second.points.end());
        out.erase(out.begin());
    }
    return out;
}

} // namespace tisma
