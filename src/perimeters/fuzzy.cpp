// Fuzzy skin. Own design from docs/spec/features/fuzzy_skin.md; see
// PROVENANCE.md.

#include "perimeters/fuzzy.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace tisma {

namespace {

// splitmix64 (its public-domain constants, as the random seams use): the same
// numbers on every compiler, unlike the standard distributions.
std::uint64_t next(std::uint64_t& state) {
    std::uint64_t z = (state += 0x9e3779b97f4a7c15ull);
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ull;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebull;
    return z ^ (z >> 31);
}

double uniform(std::uint64_t& state) { return double(next(state) >> 11) * 0x1.0p-53; } // [0, 1)

double segment_distance(const Point2d& p, Clipper2Lib::Point64 a, Clipper2Lib::Point64 b) {
    const double ax = to_mm(a.x), ay = to_mm(a.y), bx = to_mm(b.x), by = to_mm(b.y);
    const double dx = bx - ax, dy = by - ay, length2 = dx * dx + dy * dy;
    const double t = length2 > 0.0 ? std::clamp(((p.x - ax) * dx + (p.y - ay) * dy) / length2, 0.0, 1.0) : 0.0;
    return std::hypot(p.x - (ax + t * dx), p.y - (ay + t * dy));
}

// Whether the loop goes round a hole: the region's boundary nearest to it is
// a hole's (clockwise).
bool around_hole(const Toolpath& loop, const Polygons& region) {
    const Point2d& p = loop.points.front().p;
    double nearest = std::numeric_limits<double>::max();
    bool hole = false;
    for (const Polygon& poly : region)
        for (std::size_t i = 0; i < poly.size(); ++i)
            if (const double d = segment_distance(p, poly[i], poly[(i + 1) % poly.size()]); d < nearest) {
                nearest = d;
                hole = !Clipper2Lib::IsPositive(poly);
            }
    return hole;
}

// The loop through new points spaced from half to one and a half times the
// mean distance apart, each moved off the line by up to the thickness either
// way. With `painted`, only inside it: elsewhere the loop keeps its points.
void fuzz_loop(Toolpath& loop, const FuzzyParams& params, const Polygons* painted, std::uint64_t& state) {
    const std::size_t n = loop.points.size();
    const double d = params.point_distance;
    const auto gap = [&] { return (0.5 + uniform(state)) * d; };
    std::vector<ToolpathPoint> out;
    double ahead = gap(); // from the current segment's start to the next new point
    for (std::size_t i = 0; i < n; ++i) {
        const ToolpathPoint& a = loop.points[i];
        const ToolpathPoint& b = loop.points[(i + 1) % n];
        const double dx = b.p.x - a.p.x, dy = b.p.y - a.p.y, length = std::hypot(dx, dy);
        if (painted && !region_contains(*painted, a.p.x, a.p.y))
            out.push_back(a);
        for (; ahead < length; ahead += gap()) {
            const double t = ahead / length;
            ToolpathPoint q{{a.p.x + t * dx, a.p.y + t * dy}, a.width + t * (b.width - a.width), a.dz + t * (b.dz - a.dz),
                            a.below + t * (b.below - a.below)};
            if (painted && !region_contains(*painted, q.p.x, q.p.y))
                continue;
            const double off = (2.0 * uniform(state) - 1.0) * params.thickness;
            q.p.x += -dy / length * off;
            q.p.y += dx / length * off;
            out.push_back(q);
        }
        ahead -= length;
    }
    if (out.size() >= 3) // a loop shorter than three points' spacing stays as it was
        loop.points = std::move(out);
}

} // namespace

void fuzz_walls(std::vector<Toolpath>& walls, const Polygons& region, const FuzzyParams& params,
                const Polygons* painted, std::uint64_t seed) {
    if (!(params.point_distance > 0.0) || (params.mode == FuzzyMode::None && (!painted || painted->empty())))
        return;
    std::uint64_t k = 0;
    for (Toolpath& t : walls) {
        if (t.index != 0 || !t.closed || t.points.size() < 3)
            continue;
        std::uint64_t state = seed * 0x9e3779b97f4a7c15ull + (k++);
        const bool whole = params.mode == FuzzyMode::All || (params.mode == FuzzyMode::External && !around_hole(t, region));
        if (whole)
            fuzz_loop(t, params, nullptr, state);
        else if (painted && !painted->empty())
            fuzz_loop(t, params, painted, state);
    }
}

} // namespace tisma
