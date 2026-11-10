// Polyholes. Own design from orca_2_5.md rule 5; see PROVENANCE.md.

#include "geom/polyholes.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <optional>

namespace tisma {
namespace {

struct Hole {
    std::size_t path = 0; // index in its layer's region
    double x = 0.0, y = 0.0, r = 0.0, margin = 0.0;
};

// The hole's circle when the path is a round hole (clockwise, convex, more than
// 8 points, within the margin of its circle).
std::optional<Hole> round_hole(const Polygon& path, const PolyholeOptions& o) {
    if (path.size() <= 8 || Clipper2Lib::Area(path) >= 0.0)
        return std::nullopt;
    const std::size_t n = path.size();
    // Convex: every turn of a clockwise path goes the same way (collinear
    // points allowed).
    for (std::size_t i = 0; i < n; ++i) {
        const auto& a = path[i];
        const auto& b = path[(i + 1) % n];
        const auto& c = path[(i + 2) % n];
        const double cross = double(b.x - a.x) * double(c.y - b.y) - double(b.y - a.y) * double(c.x - b.x);
        const double scale = std::hypot(double(b.x - a.x), double(b.y - a.y)) *
                             std::hypot(double(c.x - b.x), double(c.y - b.y));
        if (cross > 1e-9 * scale)
            return std::nullopt;
    }
    Hole h;
    for (const auto& p : path)
        h.x += to_mm(p.x), h.y += to_mm(p.y);
    h.x /= double(n), h.y /= double(n);
    for (const auto& p : path)
        h.r += std::hypot(to_mm(p.x) - h.x, to_mm(p.y) - h.y);
    h.r /= double(n);
    if (!(h.r > 0.0))
        return std::nullopt;
    h.margin = o.margin_percent ? h.r * o.margin / 100.0 : o.margin;
    for (const auto& p : path)
        if (std::abs(std::hypot(to_mm(p.x) - h.x, to_mm(p.y) - h.y) - h.r) > h.margin)
            return std::nullopt;
    return h;
}

bool same_hole(const Hole& a, const Hole& b) {
    const double m = std::max(a.margin, b.margin);
    return std::hypot(a.x - b.x, a.y - b.y) <= m && std::abs(a.r - b.r) <= m;
}

// Whether `layer` holds a hole matching h (holes sorted by x).
bool has_match(const std::vector<Hole>& layer, const Hole& h) {
    const auto from = std::lower_bound(layer.begin(), layer.end(), h.x - h.margin * 2.0 - 1e-9,
                                       [](const Hole& a, double x) { return a.x < x; });
    for (auto it = from; it != layer.end() && it->x <= h.x + h.margin * 2.0 + 1e-9; ++it)
        if (same_hole(*it, h))
            return true;
    return false;
}

} // namespace

int polyhole_sides(double diameter, double nozzle, int max_edges) {
    const double per_mm = 2.0 * 0.4 / std::max(nozzle, 1e-3);
    const long sides = std::lround(diameter * per_mm);
    return int(std::clamp<long>(sides, 3, std::max(3, max_edges)));
}

std::vector<char> make_polyholes(std::vector<Polygons>& regions, const PolyholeOptions& o) {
    std::vector<std::vector<Hole>> holes(regions.size());
    for (std::size_t l = 0; l < regions.size(); ++l) {
        for (std::size_t i = 0; i < regions[l].size(); ++i)
            if (auto h = round_hole(regions[l][i], o)) {
                h->path = i;
                holes[l].push_back(*h);
            }
        std::sort(holes[l].begin(), holes[l].end(), [](const Hole& a, const Hole& b) { return a.x < b.x; });
    }
    std::vector<char> changed(regions.size(), 0);
    for (std::size_t l = 0; l < regions.size(); ++l) {
        std::vector<char> turned(regions[l].size(), 0);
        Polygons cutters;
        for (const Hole& h : holes[l]) {
            const bool lasts = l == 0 || (l > 0 && has_match(holes[l - 1], h)) ||
                               (l + 1 < regions.size() && has_match(holes[l + 1], h));
            if (!lasts)
                continue;
            const int n = polyhole_sides(2.0 * h.r, o.nozzle, o.max_edges);
            const double step = 2.0 * std::numbers::pi / double(n);
            const double start = o.twist ? double(l % 5) * step / 5.0 : 0.0;
            const double corner = h.r / std::cos(std::numbers::pi / double(n)); // the flats touch the circle
            Polygon polygon;
            for (int k = 0; k < n; ++k) {
                const double a = start + step * double(k);
                polygon.push_back({to_units(h.x + corner * std::cos(a)), to_units(h.y + corner * std::sin(a))});
            }
            cutters.push_back(std::move(polygon));
            turned[h.path] = 1;
        }
        if (cutters.empty())
            continue;
        Polygons kept;
        for (std::size_t i = 0; i < regions[l].size(); ++i)
            if (!turned[i])
                kept.push_back(regions[l][i]);
        regions[l] = Clipper2Lib::Difference(kept, cutters, Clipper2Lib::FillRule::NonZero);
        changed[l] = 1;
    }
    return changed;
}

} // namespace tisma
