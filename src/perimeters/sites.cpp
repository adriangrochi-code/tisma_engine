// Beading propagation and toolpath sites, from Kuipers et al. 2020
// (arXiv 2004.13497), Sections 3.5 and 3.6. Own structure; see PROVENANCE.md.

#include "perimeters/sites.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <tuple>
#include <utility>

namespace tisma {

std::vector<HalfBeading> node_beadings(const MedialGraph& g, const std::vector<double>& counts,
                                       const BeadingParams& params) {
    std::vector<std::vector<std::uint32_t>> neighbours(g.nodes.size());
    for (const MedialGraph::Edge& e : g.edges) {
        neighbours[e.a].push_back(e.b);
        neighbours[e.b].push_back(e.a);
    }
    auto beading_of_central = [&](std::uint32_t n) {
        return interpolate_beading(params, counts[n], 2.0 * g.nodes[n].radius);
    };
    std::vector<HalfBeading> out(g.nodes.size());
    for (std::uint32_t n = 0; n < g.nodes.size(); ++n) {
        if (counts[n] >= 0.0) {
            out[n] = beading_of_central(n);
            continue;
        }
        // Climb to the neighbour with the largest radius until a central node or a peak.
        std::uint32_t at = n;
        for (std::size_t steps = 0; steps < g.nodes.size(); ++steps) {
            std::uint32_t best = at;
            for (std::uint32_t m : neighbours[at])
                if (g.nodes[m].radius > g.nodes[best].radius)
                    best = m;
            if (best == at || counts[best] >= 0.0) {
                at = best;
                break;
            }
            at = best;
        }
        if (counts[at] >= 0.0) {
            out[n] = beading_of_central(at);
        } else {
            const double thickness = 2.0 * g.nodes[at].radius;
            out[n] = half_beading(compute_beading(params, bead_count(params, thickness), thickness));
        }
    }
    return out;
}

std::vector<Site> support_sites(const MedialGraph& g, const std::vector<HalfBeading>& beadings) {
    std::vector<Site> out;
    for (std::uint32_t ei = 0; ei < g.edges.size(); ++ei) {
        const MedialGraph::Edge& e = g.edges[ei];
        for (int end = 0; end < 2; ++end) {
            const std::uint32_t n = end == 0 ? e.a : e.b;
            const MedialGraph::Node& node = g.nodes[n];
            if (!(node.radius > 0.0))
                continue;
            const HalfBeading& b = beadings[n];
            for (int side = 0; side < 2; ++side) {
                const Point2d& s = end == 0 ? e.support_a[side] : e.support_b[side];
                for (std::size_t i = 0; i < b.distances.size(); ++i) {
                    const double l = b.distances[i];
                    if (l > node.radius + kOnNode)
                        break; // this bead and the next ones do not reach the node
                    const bool on_node = l >= node.radius - kOnNode;
                    const double f = l / node.radius;
                    out.push_back({on_node ? node.p : Point2d{s.x + f * (node.p.x - s.x), s.y + f * (node.p.y - s.y)},
                                   b.widths[i],
                                   static_cast<int>(i),
                                   ei,
                                   side,
                                   end == 0 ? Site::Where::SideA : Site::Where::SideB,
                                   on_node});
                }
            }
        }
    }
    return out;
}

std::vector<Site> medial_sites(const MedialGraph& g, const std::vector<HalfBeading>& beadings) {
    std::vector<Site> out;
    for (std::uint32_t ei = 0; ei < g.edges.size(); ++ei) {
        const MedialGraph::Edge& e = g.edges[ei];
        if (e.central)
            continue;
        const MedialGraph::Node& a = g.nodes[e.a];
        const MedialGraph::Node& b = g.nodes[e.b];
        if (a.radius == b.radius)
            continue;
        const MedialGraph::Node& low = a.radius < b.radius ? a : b;
        const MedialGraph::Node& high = a.radius < b.radius ? b : a;
        const HalfBeading& beading = beadings[a.radius < b.radius ? e.b : e.a];
        for (std::size_t i = 0; i < beading.distances.size(); ++i) {
            const double l = beading.distances[i];
            // A bead at R_low or R_high is a site of that node (support_sites).
            if (!(l > low.radius + kOnNode) || !(l < high.radius - kOnNode))
                continue;
            const double t = (l - low.radius) / (high.radius - low.radius);
            out.push_back({{low.p.x + t * (high.p.x - low.p.x), low.p.y + t * (high.p.y - low.p.y)},
                           beading.widths[i],
                           static_cast<int>(i),
                           ei,
                           -1,
                           Site::Where::Medial,
                           false});
        }
    }
    return out;
}

namespace {

// What a site is, independent of the trapezoid that computes it: the trapezoids
// that share a site produce the same key, so segments are chained by key and
// never by comparing positions. A site on a rib (node to support) is named by
// the node, the support point (exact: both trapezoids read the same value) and
// the bead; a site on a node by the node and the bead; a site on a medial edge
// by the edge and the bead.
struct SiteKey {
    enum class Kind : std::uint8_t { Rib, Node, Medial };
    Kind kind;
    std::uint32_t id; // node, or edge for Medial
    double sx, sy;    // support point of a Rib
    int index;

    auto tie() const { return std::tie(kind, id, sx, sy, index); }
    bool operator<(const SiteKey& o) const { return tie() < o.tie(); }
    bool operator==(const SiteKey& o) const { return tie() == o.tie(); }
};

SiteKey key_of(const MedialGraph& g, const Site& s) {
    const MedialGraph::Edge& e = g.edges[s.edge];
    if (s.where == Site::Where::Medial)
        return {SiteKey::Kind::Medial, s.edge, 0.0, 0.0, s.index};
    const bool a = s.where == Site::Where::SideA;
    const std::uint32_t node = a ? e.a : e.b;
    if (s.on_node)
        return {SiteKey::Kind::Node, node, 0.0, 0.0, s.index};
    const Point2d& support = a ? e.support_a[s.side] : e.support_b[s.side];
    return {SiteKey::Kind::Rib, node, support.x, support.y, s.index};
}

struct Segment {
    ToolpathPoint a, b;
    SiteKey ka, kb;
};

} // namespace

std::vector<Toolpath> extract_toolpaths(const MedialGraph& g, const std::vector<HalfBeading>& beadings) {
    // Sites of each trapezoid, by (edge, side).
    std::vector<std::vector<Site>> by_trapezoid(2 * g.edges.size());
    for (const Site& s : support_sites(g, beadings))
        by_trapezoid[2 * s.edge + std::size_t(s.side)].push_back(s);
    for (const Site& s : medial_sites(g, beadings)) {
        by_trapezoid[2 * s.edge].push_back(s);
        by_trapezoid[2 * s.edge + 1].push_back(s);
    }

    std::vector<Segment> segments;
    for (std::size_t t = 0; t < by_trapezoid.size(); ++t) {
        const int side = int(t % 2);
        std::map<int, std::vector<const Site*>> by_index;
        for (const Site& s : by_trapezoid[t])
            by_index[s.index].push_back(&s);
        for (const auto& [index, sites] : by_index) {
            if (sites.size() < 2)
                continue; // a transition: this site stays unconnected
            // Prefer the two node-to-support sides; a medial site pairs with the one side present.
            const Site* first = sites[0];
            const Site* second = sites[1];
            if (sites.size() > 2) {
                for (const Site* s : sites)
                    if (s->where == Site::Where::SideA)
                        first = s;
                    else if (s->where == Site::Where::SideB)
                        second = s;
            }
            // The middle bead runs along the medial edge: both trapezoids would emit it.
            if (side == 1 && first->on_node && second->on_node)
                continue;
            const SiteKey ka = key_of(g, *first), kb = key_of(g, *second);
            if (ka == kb)
                continue;
            segments.push_back({{first->p, first->width}, {second->p, second->width}, ka, kb});
        }
    }

    // Ends of segments sorted by key: a key held by exactly two ends links them.
    struct End {
        SiteKey key;
        std::uint32_t segment;
        int which; // 0: a, 1: b
    };
    std::vector<End> ends;
    ends.reserve(2 * segments.size());
    for (std::uint32_t i = 0; i < segments.size(); ++i) {
        ends.push_back({segments[i].ka, i, 0});
        ends.push_back({segments[i].kb, i, 1});
    }
    std::sort(ends.begin(), ends.end(), [](const End& l, const End& r) {
        return l.key < r.key || (l.key == r.key && (l.segment < r.segment || (l.segment == r.segment && l.which < r.which)));
    });
    // link[2 * segment + which]: the end it continues into, or kNoLink.
    constexpr std::size_t kNoLink = static_cast<std::size_t>(-1);
    std::vector<std::size_t> link(2 * segments.size(), kNoLink);
    for (std::size_t i = 0; i < ends.size();) {
        std::size_t j = i + 1;
        while (j < ends.size() && ends[j].key == ends[i].key)
            ++j;
        if (j - i == 2) { // more than two make a junction, where polylines end
            const std::size_t x = 2 * ends[i].segment + std::size_t(ends[i].which);
            const std::size_t y = 2 * ends[i + 1].segment + std::size_t(ends[i + 1].which);
            link[x] = y;
            link[y] = x;
        }
        i = j;
    }

    std::vector<bool> used(segments.size(), false);
    auto point = [&](std::size_t end) -> const ToolpathPoint& {
        return end % 2 == 0 ? segments[end / 2].a : segments[end / 2].b;
    };
    auto append = [](std::vector<ToolpathPoint>& pts, const ToolpathPoint& p) {
        if (pts.empty() || pts.back().p.x != p.p.x || pts.back().p.y != p.p.y)
            pts.push_back(p);
    };
    // Walks from the far end of the segment of `end` along the links; returns
    // true when it came back to `stop` (a loop).
    auto walk = [&](std::size_t end, std::size_t stop, std::vector<ToolpathPoint>& pts) {
        while (true) {
            const std::size_t far = end ^ 1u;
            const std::size_t next = link[far];
            if (next == kNoLink)
                return false;
            if (next == stop)
                return true;
            if (used[next / 2])
                return false;
            used[next / 2] = true;
            append(pts, point(next ^ 1u));
            end = next;
        }
    };
    std::vector<Toolpath> out;
    auto emit = [&](std::size_t start) { // start: the free end of a chain, or any end of a loop
        used[start / 2] = true;
        Toolpath path;
        path.index = segments[start / 2].ka.index;
        append(path.points, point(start));
        append(path.points, point(start ^ 1u));
        path.closed = walk(start, start, path.points);
        if (path.closed && path.points.size() > 1 && path.points.back().p.x == path.points.front().p.x &&
            path.points.back().p.y == path.points.front().p.y)
            path.points.pop_back();
        if (path.points.size() >= 2)
            out.push_back(std::move(path));
    };
    // Open chains from their free ends first, then what is left lies on loops.
    for (std::size_t end = 0; end < link.size(); ++end)
        if (!used[end / 2] && link[end] == kNoLink)
            emit(end);
    for (std::size_t s = 0; s < segments.size(); ++s)
        if (!used[s])
            emit(2 * s);
    return out;
}

} // namespace tisma
