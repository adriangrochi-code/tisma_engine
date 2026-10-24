// Rectilinear infill, with Clipper2 (BSL-1.0) open-path clipping. Own design;
// see PROVENANCE.md.

#include "infill/rectilinear.hpp"

#include "infill/gyroid.hpp"

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

// Joins line pieces into zig-zag polylines (Section "anchoring" in PROVENANCE.md):
// two piece ends that follow each other along a boundary of the region, on
// adjacent lines and at most max_arc apart along it, are joined by the
// boundary between them. Joins never close a cycle. Works in the frame where
// the lines are horizontal and `step` apart.
Polygons connect_lines(const Polygons& pieces, const Polygons& boundary, double step, double max_arc) {
    struct End {
        std::size_t poly = 0, edge = 0;
        double s = 0.0; // arc length from the polygon's first vertex
    };
    std::vector<std::vector<double>> prefix(boundary.size());
    std::vector<double> perimeter(boundary.size(), 0.0);
    auto length = [](const Point64& a, const Point64& b) { return std::hypot(double(b.x - a.x), double(b.y - a.y)); };
    for (std::size_t b = 0; b < boundary.size(); ++b) {
        const Polygon& poly = boundary[b];
        prefix[b].assign(poly.size() + 1, 0.0);
        for (std::size_t i = 0; i < poly.size(); ++i)
            prefix[b][i + 1] = prefix[b][i] + length(poly[i], poly[(i + 1) % poly.size()]);
        perimeter[b] = prefix[b].back();
    }
    auto locate = [&](const Point64& p) {
        End best;
        double nearest = std::numeric_limits<double>::max();
        for (std::size_t b = 0; b < boundary.size(); ++b)
            for (std::size_t i = 0; i < boundary[b].size(); ++i) {
                const Point64& a = boundary[b][i];
                const Point64& c = boundary[b][(i + 1) % boundary[b].size()];
                const double dx = double(c.x - a.x), dy = double(c.y - a.y);
                const double len2 = dx * dx + dy * dy;
                const double t =
                    len2 > 0.0 ? std::clamp((double(p.x - a.x) * dx + double(p.y - a.y) * dy) / len2, 0.0, 1.0) : 0.0;
                const double d = std::hypot(double(p.x) - (double(a.x) + t * dx), double(p.y) - (double(a.y) + t * dy));
                if (d < nearest) {
                    nearest = d;
                    best = {b, i, prefix[b][i] + t * std::sqrt(len2)};
                }
            }
        return best;
    };
    // End 2i is the first point of piece i, end 2i + 1 its last.
    std::vector<End> ends(2 * pieces.size());
    for (std::size_t i = 0; i < pieces.size(); ++i) {
        ends[2 * i] = locate(pieces[i].front());
        ends[2 * i + 1] = locate(pieces[i].back());
    }
    struct Join {
        double arc;
        std::size_t from, to; // forward along the boundary from `from` to `to`
    };
    std::vector<Join> joins;
    std::vector<std::vector<std::size_t>> on_poly(boundary.size());
    for (std::size_t e = 0; e < ends.size(); ++e)
        on_poly[ends[e].poly].push_back(e);
    for (std::size_t b = 0; b < boundary.size(); ++b) {
        std::vector<std::size_t>& ids = on_poly[b];
        if (ids.size() < 2)
            continue;
        std::sort(ids.begin(), ids.end(), [&](std::size_t l, std::size_t r) { return ends[l].s < ends[r].s; });
        for (std::size_t j = 0; j < ids.size(); ++j) {
            const std::size_t a = ids[j], c = ids[(j + 1) % ids.size()];
            if (ids.size() == 2 && j == 1)
                break; // the same pair the other way round
            double arc = ends[c].s - ends[a].s;
            if (arc < 0.0)
                arc += perimeter[b];
            const double dy = std::fabs(double(pieces[a / 2].front().y - pieces[c / 2].front().y));
            if (a / 2 != c / 2 && std::fabs(dy - step) < 0.25 * step && arc <= max_arc)
                joins.push_back({arc, a, c});
        }
    }
    std::sort(joins.begin(), joins.end(), [](const Join& l, const Join& r) {
        return l.arc < r.arc || (l.arc == r.arc && (l.from < r.from || (l.from == r.from && l.to < r.to)));
    });
    constexpr std::size_t kNone = static_cast<std::size_t>(-1);
    std::vector<std::size_t> parent(pieces.size()), link(ends.size(), kNone);
    std::vector<bool> forward(ends.size(), false);
    for (std::size_t i = 0; i < parent.size(); ++i)
        parent[i] = i;
    auto root = [&](std::size_t i) {
        while (parent[i] != i)
            i = parent[i] = parent[parent[i]];
        return i;
    };
    for (const Join& j : joins) {
        if (link[j.from] != kNone || link[j.to] != kNone || root(j.from / 2) == root(j.to / 2))
            continue;
        link[j.from] = j.to;
        link[j.to] = j.from;
        forward[j.from] = true;
        parent[root(j.from / 2)] = root(j.to / 2);
    }
    // The boundary vertices strictly between end x and end y, going forward or back.
    auto between = [&](std::size_t x, std::size_t y, Polygon& out) {
        const End& ex = ends[x];
        const Polygon& poly = boundary[ex.poly];
        const std::size_t n = poly.size();
        const double per = perimeter[ex.poly];
        double arc = forward[x] ? ends[y].s - ex.s : ex.s - ends[y].s;
        if (arc < 0.0)
            arc += per;
        std::size_t k = forward[x] ? (ex.edge + 1) % n : ex.edge;
        for (std::size_t steps = 0; steps < n; ++steps) {
            double d = forward[x] ? prefix[ex.poly][k] - ex.s : ex.s - prefix[ex.poly][k];
            if (d < 0.0)
                d += per;
            if (d >= arc - 1.0)
                break;
            if (d > 1.0)
                out.push_back(poly[k]);
            k = forward[x] ? (k + 1) % n : (k + n - 1) % n;
        }
    };
    Polygons out;
    std::vector<bool> used(pieces.size(), false);
    auto walk = [&](std::size_t start) {
        Polygon line;
        for (std::size_t at = start;;) {
            const Polygon& piece = pieces[at / 2];
            used[at / 2] = true;
            if (at % 2 == 0)
                line.insert(line.end(), piece.begin(), piece.end());
            else
                line.insert(line.end(), piece.rbegin(), piece.rend());
            const std::size_t other = at ^ 1u;
            const std::size_t next = link[other];
            if (next == kNone)
                break;
            between(other, next, line);
            at = next;
        }
        out.push_back(std::move(line));
    };
    for (std::size_t e = 0; e < ends.size(); ++e)
        if (!used[e / 2] && link[e] == kNone)
            walk(e);
    return out;
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
                                       bool connect, bool fit, bool monotonic) {
    std::vector<Toolpath> out;
    if (!(spacing > 0.0) || region.empty())
        return out;
    // A hair (10 nm) of slack keeps lines that run exactly along the shrunk
    // boundary, as fitted lines do.
    const Polygons inner = offset(region, -width / 2.0 + 1e-5);
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
        for (auto k = static_cast<std::int64_t>(std::ceil(double(ymin) / step)); double(k) * step <= double(ymax); ++k) {
            const std::int64_t y = std::llround(double(k) * step);
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
        open = connect_lines(open, turned, step, kMaxJoin * step);
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

LayerFill fill_layer(const LayerSurfaces& surfaces, std::size_t layer, const FillParams& params, double z) {
    LayerFill out;
    const double angle = params.angle + (layer % 2 == 1 ? 90.0 : 0.0);
    if (params.monotonic_top) {
        out.solid = rectilinear_fill(surfaces.top, params.width, angle, params.width, false, true, true);
        for (Toolpath& t : rectilinear_fill(surfaces.bottom, params.width, angle, params.width, true, true))
            out.solid.push_back(std::move(t));
    } else {
        const Polygons solid = Clipper2Lib::Union(surfaces.top, surfaces.bottom, Clipper2Lib::FillRule::NonZero);
        out.solid = rectilinear_fill(solid, params.width, angle, params.width, true, true);
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
    }
    return out;
}

} // namespace tisma
