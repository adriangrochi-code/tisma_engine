// Joining infill pieces along their region's boundary. Own design; see
// PROVENANCE.md.

#include "infill/connect.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <vector>

namespace tisma {

using Clipper2Lib::Point64;

Polygons join_along_boundary(const Polygons& pieces, const Polygons& boundary, double max_arc, double row_step) {
    struct End {
        std::size_t poly = 0, edge = 0;
        double s = 0.0;        // arc length from the polygon's first vertex
        double away = 0.0;     // distance to the boundary (units)
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
    // The nearest boundary edge to a point: among equally near ones, the
    // first (by polygon, then edge).
    auto consider = [&](const Point64& p, std::size_t b, std::size_t i, End& best, double& nearest) {
        const Point64& a = boundary[b][i];
        const Point64& c = boundary[b][(i + 1) % boundary[b].size()];
        const double dx = double(c.x - a.x), dy = double(c.y - a.y);
        const double len2 = dx * dx + dy * dy;
        const double t = len2 > 0.0 ? std::clamp((double(p.x - a.x) * dx + double(p.y - a.y) * dy) / len2, 0.0, 1.0) : 0.0;
        const double d = std::hypot(double(p.x) - (double(a.x) + t * dx), double(p.y) - (double(a.y) + t * dy));
        if (d < nearest || (d == nearest && (b < best.poly || (b == best.poly && i < best.edge)))) {
            nearest = d;
            best = {b, i, prefix[b][i] + t * std::sqrt(len2), d};
        }
    };
    // Past a few dozen edges, a grid of them: every end looked at every edge
    // (a plate of many parts, a part with many holes: seconds per layer).
    std::size_t edge_count = 0;
    for (const Polygon& poly : boundary)
        edge_count += poly.size();
    struct EdgeRef {
        std::uint32_t poly, edge;
    };
    std::vector<std::vector<EdgeRef>> cells;
    double x0 = 0.0, y0 = 0.0, cell = 1.0;
    long nx = 0, ny = 0;
    if (edge_count > 64) {
        double xmin = std::numeric_limits<double>::max(), ymin = xmin, xmax = std::numeric_limits<double>::lowest(), ymax = xmax;
        for (const Polygon& poly : boundary)
            for (const Point64& q : poly)
                xmin = std::min(xmin, double(q.x)), xmax = std::max(xmax, double(q.x)), ymin = std::min(ymin, double(q.y)),
                ymax = std::max(ymax, double(q.y));
        const double w = xmax - xmin, h = ymax - ymin;
        cell = std::max({std::sqrt(w * h / double(edge_count)), std::max(w, h) / 1024.0, 1.0});
        x0 = xmin, y0 = ymin;
        nx = long(w / cell) + 1, ny = long(h / cell) + 1;
        cells.resize(std::size_t(nx) * std::size_t(ny));
        for (std::size_t b = 0; b < boundary.size(); ++b)
            for (std::size_t i = 0; i < boundary[b].size(); ++i) {
                const Point64& a = boundary[b][i];
                const Point64& c = boundary[b][(i + 1) % boundary[b].size()];
                const long cx0 = std::min(nx - 1, long((double(std::min(a.x, c.x)) - x0) / cell));
                const long cx1 = std::min(nx - 1, long((double(std::max(a.x, c.x)) - x0) / cell));
                const long cy0 = std::min(ny - 1, long((double(std::min(a.y, c.y)) - y0) / cell));
                const long cy1 = std::min(ny - 1, long((double(std::max(a.y, c.y)) - y0) / cell));
                for (long y = cy0; y <= cy1; ++y)
                    for (long x = cx0; x <= cx1; ++x)
                        cells[std::size_t(y) * std::size_t(nx) + std::size_t(x)].push_back({std::uint32_t(b), std::uint32_t(i)});
            }
    }
    auto locate = [&](const Point64& p) {
        End best;
        double nearest = std::numeric_limits<double>::max();
        if (cells.empty()) {
            for (std::size_t b = 0; b < boundary.size(); ++b)
                for (std::size_t i = 0; i < boundary[b].size(); ++i)
                    consider(p, b, i, best, nearest);
            return best;
        }
        // Rings of cells around p's cell: an edge first met in ring r + 1 or
        // beyond is at least r cells away.
        const long cx = long(std::floor((double(p.x) - x0) / cell)), cy = long(std::floor((double(p.y) - y0) / cell));
        const long far = std::max({std::labs(cx), std::labs(cx - nx), std::labs(cy), std::labs(cy - ny)}) + 1;
        const auto visit = [&](long x, long y) {
            if (x < 0 || y < 0 || x >= nx || y >= ny)
                return;
            for (const EdgeRef& e : cells[std::size_t(y) * std::size_t(nx) + std::size_t(x)])
                consider(p, e.poly, e.edge, best, nearest);
        };
        for (long r = 0; r <= far; ++r) {
            for (long x = cx - r; x <= cx + r; ++x) {
                visit(x, cy - r);
                if (r > 0)
                    visit(x, cy + r);
            }
            for (long y = cy - r + 1; y <= cy + r - 1; ++y) {
                visit(cx - r, y);
                if (r > 0)
                    visit(cx + r, y);
            }
            if (nearest < double(r) * cell)
                break;
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
            const bool rows = row_step <= 0.0 || std::fabs(dy - row_step) < 0.25 * row_step;
            // Only ends on the boundary join along it: a spiral's start in
            // the middle of the region, snapped to the nearest edge, made a
            // straight bead across its turns (the review of 2026-10-25).
            constexpr double kOnBoundary = 0.01 * kUnitsPerMm;
            const bool on_boundary = ends[a].away <= kOnBoundary && ends[c].away <= kOnBoundary;
            if (a / 2 != c / 2 && rows && arc <= max_arc && on_boundary)
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

} // namespace tisma
