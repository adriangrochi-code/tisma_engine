// Gyroid infill: marching squares on the gyroid's section, clipped with
// Clipper2 (BSL-1.0). Own design; the surface is the one in
// docs/spec/SOURCES.md. See PROVENANCE.md.

#include "infill/gyroid.hpp"

#include "infill/connect.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <map>
#include <numbers>
#include <utility>

#include <oneapi/tbb/parallel_for.h>

namespace tisma {

namespace {

using Clipper2Lib::Point64;

// The gyroid's section on rows jb to je of the grid (cells between them),
// chained into polylines and simplified. A curve that leaves the band ends
// on a crossing of its first or last row, which the next band computes the
// same way: from the same values at the same nodes.
Polygons band_curves(std::int64_t i0, std::size_t nx, std::int64_t j0, std::size_t jb, std::size_t je, double h,
                     const std::vector<double>& sx, const std::vector<double>& cx, double k, double sz, double cz) {
    const std::size_t ny = je - jb + 1;
    std::vector<double> sy(ny), cy(ny);
    for (std::size_t j = 0; j < ny; ++j) {
        const double y = k * h * double(j0 + std::int64_t(jb + j));
        sy[j] = std::sin(y), cy[j] = std::cos(y);
    }
    std::vector<double> v(nx * ny);
    for (std::size_t j = 0; j < ny; ++j)
        for (std::size_t i = 0; i < nx; ++i)
            v[j * nx + i] = sx[i] * cy[j] + sy[j] * cz + sz * cx[i];

    // Crossings on grid edges: horizontal edge (i, j)-(i + 1, j) has id j nx + i,
    // vertical edge (i, j)-(i, j + 1) has id nx ny + j nx + i.
    auto at = [&](std::size_t i, std::size_t j) { return v[j * nx + i]; };
    auto point = [&](std::size_t id) {
        const bool vertical = id >= nx * ny;
        const std::size_t e = vertical ? id - nx * ny : id;
        const std::size_t i = e % nx, j = e / nx;
        const double a = at(i, j), b = vertical ? at(i, j + 1) : at(i + 1, j);
        const double t = a / (a - b);
        const double x = h * (double(i0 + std::int64_t(i)) + (vertical ? 0.0 : t));
        const double y = h * (double(j0 + std::int64_t(jb + j)) + (vertical ? t : 0.0));
        return Point64{to_units(x), to_units(y)};
    };
    // Segments of every cell; each crossing belongs to two cells.
    constexpr std::uint32_t kNone = 0xFFFFFFFFu;
    std::vector<std::uint32_t> first(2 * nx * ny, kNone), second(2 * nx * ny, kNone);
    std::vector<std::pair<std::size_t, std::size_t>> segments;
    auto add = [&](std::size_t a, std::size_t b) {
        const auto s = static_cast<std::uint32_t>(segments.size());
        segments.push_back({a, b});
        for (std::size_t c : {a, b})
            (first[c] == kNone ? first[c] : second[c]) = s;
    };
    for (std::size_t j = 0; j + 1 < ny; ++j)
        for (std::size_t i = 0; i + 1 < nx; ++i) {
            const double c00 = at(i, j), c10 = at(i + 1, j), c11 = at(i + 1, j + 1), c01 = at(i, j + 1);
            const int index = (c00 > 0) | (c10 > 0) << 1 | (c11 > 0) << 2 | (c01 > 0) << 3;
            const std::size_t bottom = j * nx + i, top = (j + 1) * nx + i;
            const std::size_t left = nx * ny + j * nx + i, right = nx * ny + j * nx + i + 1;
            const bool centre = c00 + c10 + c11 + c01 > 0;
            switch (index) {
            case 1: case 14: add(left, bottom); break;
            case 2: case 13: add(bottom, right); break;
            case 3: case 12: add(left, right); break;
            case 4: case 11: add(right, top); break;
            case 6: case 9: add(bottom, top); break;
            case 7: case 8: add(left, top); break;
            case 5: // c00 and c11 above zero
                if (centre)
                    add(bottom, right), add(top, left);
                else
                    add(left, bottom), add(right, top);
                break;
            case 10: // c10 and c01 above zero
                if (centre)
                    add(left, bottom), add(right, top);
                else
                    add(bottom, right), add(top, left);
                break;
            default:
                break;
            }
        }
    // Chain segments through shared crossings into polylines.
    std::vector<bool> used(segments.size(), false);
    Polygons curves;
    auto other = [&](std::size_t s, std::size_t c) { return segments[s].first == c ? segments[s].second : segments[s].first; };
    auto next = [&](std::size_t c, std::size_t s) -> std::size_t {
        const std::uint32_t n = first[c] == s ? second[c] : first[c];
        return n == kNone ? segments.size() : n;
    };
    auto walk = [&](std::size_t s, std::size_t from) {
        Polygon line{point(from)};
        std::size_t c = from;
        while (s < segments.size() && !used[s]) {
            used[s] = true;
            c = other(s, c);
            line.push_back(point(c));
            s = next(c, s);
        }
        curves.push_back(std::move(line));
    };
    for (std::size_t s = 0; s < segments.size(); ++s) // open curves from their free ends
        for (std::size_t c : {segments[s].first, segments[s].second})
            if (!used[s] && second[c] == kNone)
                walk(s, c);
    for (std::size_t s = 0; s < segments.size(); ++s) // closed loops
        if (!used[s])
            walk(s, segments[s].first);
    // The curves are smooth: points closer than kGyroidTolerance to the line
    // between their neighbours go before clipping (the ends stay).
    return Clipper2Lib::SimplifyPaths(curves, kGyroidTolerance * kUnitsPerMm, false);
}

Polygons clip_open(const Polygons& paths, const Polygons& region) {
    if (paths.empty() || region.empty())
        return {};
    Clipper2Lib::Clipper64 clipper;
    clipper.AddOpenSubject(paths);
    clipper.AddClip(region);
    Polygons closed, open;
    clipper.Execute(Clipper2Lib::ClipType::Intersection, Clipper2Lib::FillRule::NonZero, closed, open);
    return open;
}

// Joins the pieces whose ends meet on an inner band edge (y one of `cuts`):
// the same crossing, computed the same way in both bands.
Polygons join_at_rows(Polygons all, const std::vector<std::int64_t>& cuts) {
    std::erase_if(all, [](const Polygon& p) { return p.size() < 2; });
    std::map<std::pair<std::int64_t, std::int64_t>, std::vector<std::size_t>> at; // end = 2 * piece + (0 front, 1 back)
    for (std::size_t k = 0; k < all.size(); ++k)
        for (int e = 0; e < 2; ++e) {
            const Point64& q = e ? all[k].back() : all[k].front();
            if (std::binary_search(cuts.begin(), cuts.end(), q.y))
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
            if (free_end == 1)
                std::reverse(p.begin(), p.end());
            chain.insert(chain.end(), chain.empty() ? p.begin() : p.begin() + 1, p.end());
            const std::size_t next = mate[2 * k + std::size_t(1 - free_end)];
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
    for (std::size_t k = 0; k < all.size(); ++k) // loops cut by the bands
        if (!used[k])
            walk(k, 0);
    return out;
}

} // namespace

std::vector<Toolpath> gyroid_fill(const Polygons& region, double period, double z, double width) {
    std::vector<Toolpath> out;
    if (!(period > 0.0) || region.empty())
        return out;
    const Polygons inner = offset(region, -width / 2.0 + 1e-5);
    if (inner.empty())
        return out;
    const Clipper2Lib::Rect64 box = Clipper2Lib::GetBounds(inner);
    const double h = period / kGyroidSamples;
    const auto i0 = static_cast<std::int64_t>(std::floor(to_mm(box.left) / h)) - 1;
    const auto i1 = static_cast<std::int64_t>(std::ceil(to_mm(box.right) / h)) + 1;
    const auto j0 = static_cast<std::int64_t>(std::floor(to_mm(box.top) / h)) - 1;
    const auto j1 = static_cast<std::int64_t>(std::ceil(to_mm(box.bottom) / h)) + 1;
    const std::size_t nx = std::size_t(i1 - i0 + 1), ny = std::size_t(j1 - j0 + 1);
    const double k = 2.0 * std::numbers::pi / period;
    std::vector<double> sx(nx), cx(nx);
    for (std::size_t i = 0; i < nx; ++i) {
        const double x = k * h * double(i0 + std::int64_t(i));
        sx[i] = std::sin(x), cx[i] = std::cos(x);
    }
    const double sz = std::sin(k * z), cz = std::cos(k * z);
    // Bands of rows of about kGyroidBandNodes nodes each (at 100 % a 300 mm
    // part had 144 million nodes, gigabytes at once): each marched, clipped
    // and simplified apart, in parallel, and the pieces joined again where
    // they cross a band's edge. The bands depend on the region only.
    const std::size_t rows = std::max<std::size_t>(16, kGyroidBandNodes / nx);
    const std::size_t bands = (ny - 1 + rows - 1) / rows;
    Polygons open;
    if (bands <= 1) {
        open = clip_open(band_curves(i0, nx, j0, 0, ny - 1, h, sx, cx, k, sz, cz), inner);
    } else {
        std::vector<Polygons> pieces(bands);
        std::vector<std::int64_t> cuts;
        for (std::size_t b = 1; b < bands; ++b)
            cuts.push_back(to_units(h * double(j0 + std::int64_t(b * rows))));
        const std::int64_t margin = to_units(std::max(1.0, 4.0 * h));
        tbb::parallel_for(std::size_t(0), bands, [&](std::size_t b) {
            const std::size_t jb = b * rows, je = std::min(ny - 1, (b + 1) * rows);
            const Clipper2Lib::Rect64 wide(box.left - margin, to_units(h * double(j0 + std::int64_t(jb))) - margin,
                                           box.right + margin, to_units(h * double(j0 + std::int64_t(je))) + margin);
            pieces[b] = clip_open(band_curves(i0, nx, j0, jb, je, h, sx, cx, k, sz, cz), Clipper2Lib::RectClip(wide, inner));
        });
        Polygons all;
        for (Polygons& p : pieces)
            for (Polygon& q : p)
                all.push_back(std::move(q));
        open = join_at_rows(std::move(all), cuts);
    }
    open.erase(std::remove_if(open.begin(), open.end(), [](const Polygon& p) { return p.size() < 2; }), open.end());
    // Anchor the curve ends: join them along the boundary, up to half a period apart.
    open = join_along_boundary(open, inner, period / 2.0 * kUnitsPerMm);
    for (const Polygon& piece : open) {
        if (piece.size() < 2)
            continue;
        Toolpath t;
        for (const auto& p : piece)
            t.points.push_back({{to_mm(p.x), to_mm(p.y)}, width});
        out.push_back(std::move(t));
    }
    return out;
}

} // namespace tisma
