// Path order and seams. Own design from the behaviour in
// docs/spec/BEHAVIOR.md; see PROVENANCE.md.

#include "gcode/order.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>

namespace tisma {
namespace {

double distance(const Point2d& a, const Point2d& b) { return std::hypot(a.x - b.x, a.y - b.y); }

std::size_t rear_point(const Toolpath& t, const std::vector<std::size_t>& candidates) {
    std::size_t best = candidates.front();
    for (std::size_t i : candidates) {
        const Point2d& p = t.points[i].p;
        const Point2d& b = t.points[best].p;
        if (p.y > b.y || (p.y == b.y && p.x < b.x))
            best = i;
    }
    return best;
}

// A small deterministic mix (splitmix64 finaliser, a public-domain constant set).
std::uint64_t mix(std::uint64_t z) {
    z += 0x9e3779b97f4a7c15ull;
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ull;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebull;
    return z ^ (z >> 31);
}

} // namespace

void SeamMemory::next_layer() {
    for (auto& [index, seams] : current)
        below[index] = std::move(seams);
    current.clear();
}

std::size_t seam_point(const Toolpath& loop, SeamMode mode, const Point2d& from, std::uint64_t seed,
                       const SeamMemory& memory) {
    const std::size_t n = loop.points.size();
    if (n == 0)
        return 0;
    std::vector<std::size_t> all(n);
    for (std::size_t i = 0; i < n; ++i)
        all[i] = i;
    switch (mode) {
    case SeamMode::Nearest: {
        std::size_t best = 0;
        for (std::size_t i = 1; i < n; ++i)
            if (distance(loop.points[i].p, from) < distance(loop.points[best].p, from))
                best = i;
        return best;
    }
    case SeamMode::Rear:
        return rear_point(loop, all);
    case SeamMode::Random:
        return static_cast<std::size_t>(mix(seed) % n);
    case SeamMode::Aligned: {
        double area = 0.0;
        for (std::size_t i = 0; i < n; ++i) {
            const Point2d& a = loop.points[i].p;
            const Point2d& b = loop.points[(i + 1) % n].p;
            area += a.x * b.y - b.x * a.y;
        }
        const double limit = std::sin(kSeamCorner * std::numbers::pi / 180.0);
        std::vector<std::size_t> corners;
        // The turn at a point, measured to the points at least kSeamSpan away
        // on each side: zig-zags much smaller than a bead are not corners.
        auto away = [&](std::size_t i, bool forward) {
            const Point2d& b = loop.points[i].p;
            std::size_t j = i;
            for (std::size_t k = 0; k + 1 < n; ++k) {
                j = forward ? (j + 1) % n : (j + n - 1) % n;
                if (distance(loop.points[j].p, b) >= kSeamSpan)
                    break;
            }
            return j;
        };
        for (std::size_t i = 0; i < n; ++i) {
            const Point2d& a = loop.points[away(i, false)].p;
            const Point2d& b = loop.points[i].p;
            const Point2d& c = loop.points[away(i, true)].p;
            const double ux = b.x - a.x, uy = b.y - a.y, vx = c.x - b.x, vy = c.y - b.y;
            const double lu = std::hypot(ux, uy), lv = std::hypot(vx, vy);
            if (lu == 0.0 || lv == 0.0)
                continue;
            const double turn = (ux * vy - uy * vx) / (lu * lv); // sine of the turn, + to the left
            if (turn * (area > 0.0 ? -1.0 : 1.0) > limit)
                corners.push_back(i); // turns against the loop's direction: a concave corner
        }
        const std::vector<std::size_t>& candidates = corners.empty() ? all : corners;
        const auto it = memory.below.find(loop.index);
        if (it == memory.below.end() || it->second.empty())
            return rear_point(loop, candidates);
        std::size_t best = candidates.front();
        double nearest = std::numeric_limits<double>::max();
        for (std::size_t i : candidates)
            for (const Point2d& seam : it->second)
                if (const double d = distance(loop.points[i].p, seam); d < nearest)
                    nearest = d, best = i;
        return best;
    }
    }
    return 0;
}

std::vector<Toolpath> order_paths(std::vector<Toolpath> paths, Point2d& cursor, SeamMode mode, std::uint64_t seed,
                                  SeamMemory& memory) {
    std::vector<Toolpath> out;
    out.reserve(paths.size());
    std::vector<bool> done(paths.size(), false);
    std::vector<std::size_t> seams(paths.size(), 0);
    // A loop's seam does not depend on the cursor except in nearest mode.
    for (std::size_t i = 0; i < paths.size(); ++i)
        if (paths[i].closed && mode != SeamMode::Nearest)
            seams[i] = seam_point(paths[i], mode, cursor, seed + i, memory);
    for (std::size_t k = 0; k < paths.size(); ++k) {
        std::size_t best = paths.size();
        bool reverse = false;
        double nearest = std::numeric_limits<double>::max();
        for (std::size_t i = 0; i < paths.size(); ++i) {
            if (done[i] || paths[i].points.empty())
                continue;
            const Toolpath& t = paths[i];
            if (t.closed) {
                if (mode == SeamMode::Nearest)
                    seams[i] = seam_point(t, mode, cursor, seed + i, memory);
                const double d = distance(cursor, t.points[seams[i]].p);
                if (d < nearest)
                    nearest = d, best = i, reverse = false;
            } else {
                const double front = distance(cursor, t.points.front().p);
                const double back = distance(cursor, t.points.back().p);
                if (front < nearest)
                    nearest = front, best = i, reverse = false;
                if (back < nearest)
                    nearest = back, best = i, reverse = true;
            }
        }
        if (best == paths.size())
            break;
        done[best] = true;
        Toolpath t = std::move(paths[best]);
        if (t.closed) {
            std::rotate(t.points.begin(), t.points.begin() + std::ptrdiff_t(seams[best]), t.points.end());
            memory.current[t.index].push_back(t.points.front().p);
            cursor = t.points.front().p; // a loop ends where it starts
        } else {
            if (reverse)
                std::reverse(t.points.begin(), t.points.end());
            cursor = t.points.back().p;
        }
        out.push_back(std::move(t));
    }
    return out;
}

std::vector<Polygons> split_islands(const Polygons& region) {
    Clipper2Lib::Clipper64 clipper;
    clipper.AddSubject(region);
    Clipper2Lib::PolyTree64 tree;
    clipper.Execute(Clipper2Lib::ClipType::Union, Clipper2Lib::FillRule::NonZero, tree);
    std::vector<Polygons> islands;
    std::vector<const Clipper2Lib::PolyPath64*> outers;
    for (const auto& outer : tree)
        outers.push_back(outer.get());
    for (std::size_t k = 0; k < outers.size(); ++k) {
        Polygons island{outers[k]->Polygon()};
        for (const auto& hole : *outers[k]) {
            island.push_back(hole->Polygon());
            for (const auto& inner : *hole)
                outers.push_back(inner.get());
        }
        islands.push_back(std::move(island));
    }
    return islands;
}

int island_of(const std::vector<Polygons>& islands, const Point2d& p) {
    const Clipper2Lib::Point64 q{to_units(p.x), to_units(p.y)};
    for (std::size_t k = 0; k < islands.size(); ++k) {
        const Polygons& island = islands[k];
        if (island.empty() || Clipper2Lib::PointInPolygon(q, island.front()) == Clipper2Lib::PointInPolygonResult::IsOutside)
            continue;
        bool in_hole = false;
        for (std::size_t h = 1; h < island.size() && !in_hole; ++h)
            in_hole = Clipper2Lib::PointInPolygon(q, island[h]) == Clipper2Lib::PointInPolygonResult::IsInside;
        if (!in_hole)
            return int(k);
    }
    return -1;
}

} // namespace tisma
