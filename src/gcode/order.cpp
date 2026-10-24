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
        for (std::size_t i = 0; i < n; ++i) {
            const Point2d& a = loop.points[(i + n - 1) % n].p;
            const Point2d& b = loop.points[i].p;
            const Point2d& c = loop.points[(i + 1) % n].p;
            const double ux = b.x - a.x, uy = b.y - a.y, vx = c.x - b.x, vy = c.y - b.y;
            const double lu = std::hypot(ux, uy), lv = std::hypot(vx, vy);
            if (lu == 0.0 || lv == 0.0)
                continue;
            const double turn = (ux * vy - uy * vx) / (lu * lv); // sine of the turn, + to the left
            if (turn * (area > 0.0 ? -1.0 : 1.0) > limit)
                corners.push_back(i); // turns against the loop's direction: a concave corner
        }
        const std::vector<std::size_t>& candidates = corners.empty() ? all : corners;
        const auto it = memory.last.find(loop.index);
        if (it == memory.last.end())
            return rear_point(loop, candidates);
        std::size_t best = candidates.front();
        for (std::size_t i : candidates)
            if (distance(loop.points[i].p, it->second) < distance(loop.points[best].p, it->second))
                best = i;
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
            memory.last[t.index] = t.points.front().p;
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

} // namespace tisma
