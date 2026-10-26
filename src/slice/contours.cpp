// Contour assembly by edge identity. Own design; see PROVENANCE.md.

#include "slice/contours.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <utility>

namespace tisma {
namespace {

constexpr std::uint32_t kNone = 0xFFFFFFFFu;

// Sorted (edge, segment) pairs, searched by binary search: contiguous and
// cheaper than a hash map for the few hundred segments of a typical layer.
class EdgeIndex {
public:
    EdgeIndex(std::span<const LevelSegment> segments, EdgeKey LevelSegment::*field) {
        entries_.reserve(segments.size());
        for (std::size_t i = 0; i < segments.size(); ++i)
            entries_.emplace_back(segments[i].*field, static_cast<std::uint32_t>(i));
        std::sort(entries_.begin(), entries_.end());
    }

    // First segment with this key, or kNone.
    std::uint32_t find(EdgeKey key) const {
        const auto it = std::lower_bound(entries_.begin(), entries_.end(), std::make_pair(key, std::uint32_t(0)));
        return (it != entries_.end() && it->first == key) ? it->second : kNone;
    }

private:
    std::vector<std::pair<EdgeKey, std::uint32_t>> entries_;
};

} // namespace

std::vector<Contour> join_segments(std::span<const LevelSegment> segments, std::vector<std::vector<EdgeKey>>* edges) {
    const EdgeIndex by_from(segments, &LevelSegment::edge_from);
    const EdgeIndex by_to(segments, &LevelSegment::edge_to);
    std::vector<bool> used(segments.size(), false);
    std::vector<Contour> out;

    if (edges)
        edges->clear();
    auto walk = [&](std::uint32_t first) {
        Contour c;
        std::vector<EdgeKey> on;
        std::uint32_t s = first;
        while (true) {
            used[s] = true;
            c.points.push_back(segments[s].from);
            if (edges)
                on.push_back(segments[s].edge_from);
            const std::uint32_t next = by_from.find(segments[s].edge_to);
            if (next == first) {
                c.closed = true;
                break;
            }
            if (next == kNone || used[next]) {
                c.points.push_back(segments[s].to);
                if (edges)
                    on.push_back(segments[s].edge_to);
                break;
            }
            s = next;
        }
        out.push_back(std::move(c));
        if (edges)
            edges->push_back(std::move(on));
    };

    // Open chains first, from their heads: segments that no segment leads into.
    for (std::uint32_t i = 0; i < segments.size(); ++i)
        if (!used[i] && by_to.find(segments[i].edge_from) == kNone)
            walk(i);
    // Whatever is left lies on closed loops.
    for (std::uint32_t i = 0; i < segments.size(); ++i)
        if (!used[i])
            walk(i);
    return out;
}

void close_gaps(std::vector<Contour>& contours, double max_gap) {
    std::vector<std::size_t> open;
    for (std::size_t i = 0; i < contours.size(); ++i)
        if (!contours[i].closed && !contours[i].points.empty())
            open.push_back(i);
    if (open.empty())
        return;

    // Candidate joins: end of open[a] to start of open[b], at most max_gap apart.
    struct Join {
        double distance;
        std::size_t from, to; // positions in open
    };
    std::vector<Join> joins;
    for (std::size_t a = 0; a < open.size(); ++a) {
        const Vec3f& end = contours[open[a]].points.back();
        for (std::size_t b = 0; b < open.size(); ++b) {
            const Vec3f& start = contours[open[b]].points.front();
            const double d = std::hypot(double(end.x) - start.x, double(end.y) - start.y, double(end.z) - start.z);
            if (d <= max_gap)
                joins.push_back({d, a, b});
        }
    }
    std::sort(joins.begin(), joins.end(), [](const Join& l, const Join& r) {
        return l.distance < r.distance || (l.distance == r.distance && (l.from < r.from || (l.from == r.from && l.to < r.to)));
    });

    // Greedy: take the nearest joins whose end and start are both still free.
    constexpr std::size_t kNoLink = static_cast<std::size_t>(-1);
    std::vector<std::size_t> next(open.size(), kNoLink), prev(open.size(), kNoLink);
    for (const Join& j : joins)
        if (next[j.from] == kNoLink && prev[j.to] == kNoLink) {
            next[j.from] = j.to;
            prev[j.to] = j.from;
        }

    // Rebuild the open contours by following the links.
    std::vector<Contour> merged;
    std::vector<bool> done(open.size(), false);
    auto follow = [&](std::size_t first) {
        Contour c;
        std::size_t k = first;
        while (true) {
            done[k] = true;
            const std::vector<Vec3f>& pts = contours[open[k]].points;
            // A gap of zero length would repeat the point.
            const bool repeat = !c.points.empty() && c.points.back() == pts.front();
            c.points.insert(c.points.end(), pts.begin() + (repeat ? 1 : 0), pts.end());
            if (next[k] == first) {
                c.closed = true;
                if (c.points.size() > 1 && c.points.back() == c.points.front())
                    c.points.pop_back();
                break;
            }
            if (next[k] == kNoLink)
                break;
            k = next[k];
        }
        merged.push_back(std::move(c));
    };
    for (std::size_t k = 0; k < open.size(); ++k)
        if (!done[k] && prev[k] == kNoLink)
            follow(k); // chains with a free start
    for (std::size_t k = 0; k < open.size(); ++k)
        if (!done[k])
            follow(k); // what is left forms cycles

    std::vector<Contour> result;
    result.reserve(contours.size() - open.size() + merged.size());
    for (Contour& c : contours)
        if (c.closed || c.points.empty())
            result.push_back(std::move(c));
    for (Contour& c : merged)
        result.push_back(std::move(c));
    contours = std::move(result);
}

} // namespace tisma
