// Gyroid infill: marching squares on the gyroid's section, clipped with
// Clipper2 (BSL-1.0). Own design; the surface is the one in
// docs/spec/SOURCES.md. See PROVENANCE.md.

#include "infill/gyroid.hpp"

#include <cmath>
#include <cstdint>
#include <numbers>

namespace tisma {

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
    std::vector<double> sx(nx), cx(nx), sy(ny), cy(ny);
    for (std::size_t i = 0; i < nx; ++i) {
        const double x = k * h * double(i0 + std::int64_t(i));
        sx[i] = std::sin(x), cx[i] = std::cos(x);
    }
    for (std::size_t j = 0; j < ny; ++j) {
        const double y = k * h * double(j0 + std::int64_t(j));
        sy[j] = std::sin(y), cy[j] = std::cos(y);
    }
    const double sz = std::sin(k * z), cz = std::cos(k * z);
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
        const double y = h * (double(j0 + std::int64_t(j)) + (vertical ? t : 0.0));
        return Clipper2Lib::Point64{to_units(x), to_units(y)};
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
    // between their neighbours go before clipping.
    curves = Clipper2Lib::SimplifyPaths(curves, kGyroidTolerance * kUnitsPerMm, false);
    Clipper2Lib::Clipper64 clipper;
    clipper.AddOpenSubject(curves);
    clipper.AddClip(inner);
    Polygons closed, open;
    clipper.Execute(Clipper2Lib::ClipType::Intersection, Clipper2Lib::FillRule::NonZero, closed, open);
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
