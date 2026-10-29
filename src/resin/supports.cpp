#include "resin/supports.hpp"

#include "resin/resin.hpp"

#include <tbb/parallel_for.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <map>
#include <numbers>
#include <utility>

namespace tisma {
namespace {

// A disc of radius r as a 24-gon of the same area.
Polygon disc(double x, double y, double r) {
    constexpr int kSides = 24;
    const double scale = std::sqrt(2.0 * std::numbers::pi / (kSides * std::sin(2.0 * std::numbers::pi / kSides)));
    Polygon out;
    out.reserve(kSides);
    for (int i = 0; i < kSides; ++i) {
        const double a = 2.0 * std::numbers::pi * i / kSides;
        out.push_back({to_units(x + r * scale * std::cos(a)), to_units(y + r * scale * std::sin(a))});
    }
    return out;
}

// The pieces of a region, each with its holes.
std::vector<Polygons> islands_of(const Polygons& region) {
    Clipper2Lib::Clipper64 clipper;
    clipper.AddSubject(region);
    Clipper2Lib::PolyTree64 tree;
    clipper.Execute(Clipper2Lib::ClipType::Union, Clipper2Lib::FillRule::NonZero, tree);
    std::vector<Polygons> out;
    std::vector<const Clipper2Lib::PolyPath64*> pieces;
    for (const auto& piece : tree)
        pieces.push_back(piece.get());
    for (std::size_t k = 0; k < pieces.size(); ++k) {
        Polygons island{pieces[k]->Polygon()};
        for (const auto& hole : *pieces[k]) {
            island.push_back(hole->Polygon());
            for (const auto& inner : *hole)
                pieces.push_back(inner.get());
        }
        out.push_back(std::move(island));
    }
    return out;
}

// A point inside the island: its outline's centroid if that is in it, else
// the middle of the widest stretch inside along the centroid's row.
std::array<double, 2> inside_point(const Polygons& island) {
    const Polygon& outer = island.front();
    double area = 0.0, cx = 0.0, cy = 0.0;
    for (std::size_t i = 0, n = outer.size(); i < n; ++i) {
        const double x0 = to_mm(outer[i].x), y0 = to_mm(outer[i].y);
        const double x1 = to_mm(outer[(i + 1) % n].x), y1 = to_mm(outer[(i + 1) % n].y);
        const double c = x0 * y1 - x1 * y0;
        area += c;
        cx += (x0 + x1) * c;
        cy += (y0 + y1) * c;
    }
    if (std::abs(area) < 1e-12)
        return {to_mm(outer.front().x), to_mm(outer.front().y)};
    cx /= 3.0 * area;
    cy /= 3.0 * area;
    if (region_contains(island, cx, cy))
        return {cx, cy};
    std::vector<double> xs;
    for (const Polygon& p : island)
        for (std::size_t i = 0, n = p.size(); i < n; ++i) {
            const double x0 = to_mm(p[i].x), y0 = to_mm(p[i].y);
            const double x1 = to_mm(p[(i + 1) % n].x), y1 = to_mm(p[(i + 1) % n].y);
            if ((y0 <= cy) != (y1 <= cy))
                xs.push_back(x0 + (cy - y0) / (y1 - y0) * (x1 - x0));
        }
    std::sort(xs.begin(), xs.end());
    double best = -1.0, mx = cx;
    for (std::size_t i = 0; i + 1 < xs.size(); i += 2)
        if (xs[i + 1] - xs[i] > best)
            best = xs[i + 1] - xs[i], mx = 0.5 * (xs[i] + xs[i + 1]);
    return {mx, cy};
}

double tip_length(const Settings& s) {
    return std::max(1.0, s.resin_support_pillar_diameter);
}

} // namespace

std::vector<std::array<double, 3>> support_points(const IndexedMesh& part, const std::vector<Polygons>& sections,
                                                  const std::vector<Layer>& layers, const Settings& s, double floor) {
    const double spacing = 4.0 / std::sqrt(s.resin_support_density / 100.0);
    const double limit = -std::cos(kResinSupportFaceAngle * std::numbers::pi / 180.0);
    std::vector<std::array<double, 3>> points;
    std::map<std::pair<long, long>, std::vector<double>> taken;
    for (const auto& t : part.triangles) {
        const Vec3f& va = part.vertices[t[0]];
        const Vec3f& vb = part.vertices[t[1]];
        const Vec3f& vc = part.vertices[t[2]];
        const double a[3] = {va.x, va.y, va.z}, b[3] = {vb.x, vb.y, vb.z}, c[3] = {vc.x, vc.y, vc.z};
        const double u[3] = {b[0] - a[0], b[1] - a[1], b[2] - a[2]}, v[3] = {c[0] - a[0], c[1] - a[1], c[2] - a[2]};
        const double n[3] = {u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2], u[0] * v[1] - u[1] * v[0]};
        const double len = std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
        if (!(len > 0.0) || n[2] / len >= limit)
            continue;
        const double d = (b[1] - c[1]) * (a[0] - c[0]) + (c[0] - b[0]) * (a[1] - c[1]);
        if (std::abs(d) < 1e-12)
            continue;
        const auto i0 = long(std::ceil(std::min({a[0], b[0], c[0]}) / spacing - 0.5));
        const auto i1 = long(std::floor(std::max({a[0], b[0], c[0]}) / spacing - 0.5));
        const auto j0 = long(std::ceil(std::min({a[1], b[1], c[1]}) / spacing - 0.5));
        const auto j1 = long(std::floor(std::max({a[1], b[1], c[1]}) / spacing - 0.5));
        for (long i = i0; i <= i1; ++i)
            for (long j = j0; j <= j1; ++j) {
                const double x = (double(i) + 0.5) * spacing, y = (double(j) + 0.5) * spacing;
                const double w1 = ((b[1] - c[1]) * (x - c[0]) + (c[0] - b[0]) * (y - c[1])) / d;
                const double w2 = ((c[1] - a[1]) * (x - c[0]) + (a[0] - c[0]) * (y - c[1])) / d;
                const double w3 = 1.0 - w1 - w2;
                if (w1 < -1e-9 || w2 < -1e-9 || w3 < -1e-9)
                    continue;
                const double z = w1 * a[2] + w2 * b[2] + w3 * c[2];
                if (z < floor + 0.5)
                    continue;
                std::vector<double>& zs = taken[{i, j}];
                if (std::any_of(zs.begin(), zs.end(), [&](double o) { return std::abs(o - z) < 0.2; }))
                    continue;
                zs.push_back(z);
                points.push_back({x, y, z});
            }
    }
    // Islands that start over nothing: each layer against the one below, all at once.
    std::vector<std::vector<Polygons>> starts(sections.size());
    tbb::parallel_for(std::size_t(0), std::min(sections.size(), layers.size()), [&](std::size_t l) {
        if (sections[l].empty() || layers[l].bottom < floor + 0.5)
            return;
        for (Polygons& island : islands_of(sections[l]))
            if (l == 0 || sections[l - 1].empty() ||
                area_mm2(Clipper2Lib::Intersect(island, sections[l - 1], Clipper2Lib::FillRule::NonZero)) < 1e-6)
                starts[l].push_back(std::move(island));
    });
    for (std::size_t l = 0; l < starts.size(); ++l)
        for (const Polygons& island : starts[l]) {
            const double z = layers[l].bottom;
            const bool held = std::any_of(points.begin(), points.end(), [&](const std::array<double, 3>& p) {
                return std::abs(p[2] - z) <= 1.0 && region_contains(island, p[0], p[1]);
            });
            if (!held) {
                const std::array<double, 2> c = inside_point(island);
                points.push_back({c[0], c[1], z});
            }
        }
    return points;
}

std::vector<ResinPillar> support_pillars(const std::vector<std::array<double, 3>>& points,
                                         const std::vector<Polygons>& sections, const std::vector<Layer>& layers,
                                         const Settings& s, double floor) {
    std::vector<ResinPillar> out;
    if (points.empty())
        return out;
    // Where the part is, layer by layer, on cells of a quarter millimetre.
    double box[4] = {1e300, 1e300, -1e300, -1e300};
    for (const Polygons& p : sections)
        if (!p.empty()) {
            const Clipper2Lib::Rect64 b = Clipper2Lib::GetBounds(p);
            box[0] = std::min(box[0], to_mm(b.left)), box[1] = std::min(box[1], to_mm(b.top));
            box[2] = std::max(box[2], to_mm(b.right)), box[3] = std::max(box[3], to_mm(b.bottom));
        }
    for (const auto& p : points)
        box[0] = std::min(box[0], p[0]), box[1] = std::min(box[1], p[1]), box[2] = std::max(box[2], p[0]),
        box[3] = std::max(box[3], p[1]);
    constexpr double q = 0.25;
    const double margin = s.resin_support_pillar_diameter + 1.0;
    PixelGrid grid;
    grid.left = box[0] - margin;
    grid.top = box[3] + margin;
    grid.pixel_x = grid.pixel_y = q;
    grid.width = int(std::ceil((box[2] - box[0] + 2 * margin) / q));
    grid.height = int(std::ceil((box[3] - box[1] + 2 * margin) / q));
    const std::size_t count = std::min(sections.size(), layers.size());
    std::vector<std::vector<std::uint8_t>> occupied(count);
    tbb::parallel_for(std::size_t(0), count, [&](std::size_t l) {
        if (sections[l].empty())
            return;
        std::vector<std::uint8_t>& cells = occupied[l];
        cells.assign(std::size_t(grid.width) * std::size_t(grid.height), 0);
        coverage_rows(sections[l], grid, [&](int row, int x0, std::span<const double> share) {
            for (std::size_t i = 0; i < share.size(); ++i)
                if (share[i] > 0.01)
                    cells[std::size_t(row) * std::size_t(grid.width) + std::size_t(x0) + i] = 1;
        });
    });
    const double clearance = 0.5 * s.resin_support_pillar_diameter + 0.3;
    const int reach = int(std::ceil(clearance / q));
    for (const auto& p : points) {
        const int cx = int(std::floor((p[0] - grid.left) / q)), cy = int(std::floor((grid.top - p[1]) / q));
        auto blocked = [&](std::size_t l) {
            if (occupied[l].empty())
                return false;
            for (int dy = -reach; dy <= reach; ++dy)
                for (int dx = -reach; dx <= reach; ++dx) {
                    const int x = cx + dx, y = cy + dy;
                    if (x < 0 || y < 0 || x >= grid.width || y >= grid.height)
                        continue;
                    if ((dx * dx + dy * dy) * q * q > (clearance + q) * (clearance + q))
                        continue;
                    if (occupied[l][std::size_t(y) * std::size_t(grid.width) + std::size_t(x)])
                        return true;
                }
            return false;
        };
        // Down from the first layer wholly under the point.
        ResinPillar pillar{p[0], p[1], p[2], floor, false};
        for (std::size_t l = count; l-- > 0;) {
            if (layers[l].top > p[2] - 0.02)
                continue;
            if (layers[l].top <= floor)
                break;
            if (blocked(l)) {
                pillar.land = layers[l].top;
                pillar.on_part = true;
                break;
            }
        }
        if (pillar.on_part && pillar.point - pillar.land < 1.5 * tip_length(s))
            continue; // no room for the tips
        out.push_back(pillar);
    }
    return out;
}

double pillar_radius(const ResinPillar& p, const Settings& s, double z) {
    const double tip_r = 0.5 * s.resin_support_tip_diameter, pillar_r = 0.5 * s.resin_support_pillar_diameter;
    const double tip = tip_length(s), pen = s.resin_support_tip_penetration;
    constexpr double kFree = 1e300; // the pillar's own radius
    // The top tip, from inside the part down to the pillar.
    if (z > p.point + pen)
        return 0.0;
    const double top = z >= p.point         ? tip_r
                       : z >= p.point - tip ? tip_r + (pillar_r - tip_r) * (p.point - z) / tip
                                            : kFree;
    // The bottom: a tip into the part under it, or the foot, twice the
    // pillar's diameter where it stands, over one diameter.
    double bottom;
    if (p.on_part) {
        if (z < p.land - pen)
            return 0.0;
        bottom = z <= p.land ? tip_r : z <= p.land + tip ? tip_r + (pillar_r - tip_r) * (z - p.land) / tip : kFree;
    } else {
        if (z < p.land)
            return 0.0;
        const double foot = s.resin_support_pillar_diameter;
        bottom = z <= p.land + foot ? pillar_r + pillar_r * (1.0 - (z - p.land) / foot) : kFree;
    }
    const double r = std::min(top, bottom);
    return r == kFree ? pillar_r : r;
}

Polygons pad_outline(const std::vector<ResinPillar>& pillars, const std::vector<Polygons>& sections,
                     const std::vector<Layer>& layers, const Settings& s) {
    Polygons base;
    for (const ResinPillar& p : pillars)
        if (!p.on_part)
            base.push_back(disc(p.x, p.y, s.resin_support_pillar_diameter));
    if (s.resin_pad_around_object)
        for (std::size_t l = 0; l < sections.size() && l < layers.size() && layers[l].bottom < s.resin_pad_thickness; ++l)
            base.insert(base.end(), sections[l].begin(), sections[l].end());
    if (base.empty())
        return {};
    // Grown by the margin, and closed over the gaps between the feet.
    constexpr double kClose = 2.5;
    return offset(offset(offset(Clipper2Lib::Union(base, Clipper2Lib::FillRule::NonZero), s.resin_pad_margin), kClose),
                  -kClose);
}

Polygons support_section(const std::vector<ResinPillar>& pillars, const Polygons& pad, const Settings& s, double z) {
    Polygons out;
    for (const ResinPillar& p : pillars)
        if (const double r = pillar_radius(p, s, z); r > 0.0)
            out.push_back(disc(p.x, p.y, r));
    if (!pad.empty() && z < s.resin_pad_thickness) {
        const double slope = s.resin_pad_wall_slope * std::numbers::pi / 180.0;
        const double grow = s.resin_pad_wall_slope < 90.0 ? (s.resin_pad_thickness - z) / std::tan(slope) : 0.0;
        const Polygons layer = grow > 0.0 ? offset(pad, grow) : pad;
        out.insert(out.end(), layer.begin(), layer.end());
    }
    return out.empty() ? out : Clipper2Lib::Union(out, Clipper2Lib::FillRule::NonZero);
}

} // namespace tisma
