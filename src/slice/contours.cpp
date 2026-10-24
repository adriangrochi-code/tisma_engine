// Contour assembly by edge identity. Own design; see PROVENANCE.md.

#include "slice/contours.hpp"

#include <algorithm>
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

std::vector<Contour> join_segments(std::span<const LevelSegment> segments) {
    const EdgeIndex by_from(segments, &LevelSegment::edge_from);
    const EdgeIndex by_to(segments, &LevelSegment::edge_to);
    std::vector<bool> used(segments.size(), false);
    std::vector<Contour> out;

    auto walk = [&](std::uint32_t first) {
        Contour c;
        std::uint32_t s = first;
        while (true) {
            used[s] = true;
            c.points.push_back(segments[s].from);
            const std::uint32_t next = by_from.find(segments[s].edge_to);
            if (next == first) {
                c.closed = true;
                break;
            }
            if (next == kNone || used[next]) {
                c.points.push_back(segments[s].to);
                break;
            }
            s = next;
        }
        out.push_back(std::move(c));
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

} // namespace tisma
