// Beading propagation and toolpath sites, from Kuipers et al. 2020
// (arXiv 2004.13497), Sections 3.5 and 3.6. Own structure; see PROVENANCE.md.

#include "perimeters/sites.hpp"

#include <algorithm>
#include <cmath>
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

    // Beading conflicts (Section 3.5, Fig. 9): up an unmarked bone that starts at
    // a marked node, the beading turns from that node's into the one from the
    // top of the ramp over t_beading = w* of path length. A node reached from
    // several marked nodes follows the nearest.
    const double reach = params.preferred_width;
    std::vector<std::vector<std::pair<std::uint32_t, double>>> up(g.nodes.size()); // neighbour above, length
    for (const MedialGraph::Edge& e : g.edges) {
        if (e.central)
            continue;
        const double length = std::hypot(g.nodes[e.b].p.x - g.nodes[e.a].p.x, g.nodes[e.b].p.y - g.nodes[e.a].p.y);
        if (g.nodes[e.b].radius > g.nodes[e.a].radius)
            up[e.a].push_back({e.b, length});
        else if (g.nodes[e.a].radius > g.nodes[e.b].radius)
            up[e.b].push_back({e.a, length});
    }
    constexpr std::uint32_t kNone = 0xFFFFFFFFu;
    std::vector<double> travelled(g.nodes.size(), reach);
    std::vector<std::uint32_t> source(g.nodes.size(), kNone);
    std::vector<std::pair<std::uint32_t, double>> stack;
    for (std::uint32_t m = 0; m < g.nodes.size(); ++m) {
        if (counts[m] < 0.0)
            continue;
        stack.assign(1, {m, 0.0});
        while (!stack.empty()) {
            const auto [n, d] = stack.back();
            stack.pop_back();
            for (const auto& [k, length] : up[n]) {
                const double total = d + length;
                if (counts[k] >= 0.0 || total >= travelled[k])
                    continue; // a marked node, or already reached closer to its source
                travelled[k] = total;
                source[k] = m;
                stack.push_back({k, total});
            }
        }
    }
    for (std::uint32_t n = 0; n < g.nodes.size(); ++n)
        if (source[n] != kNone)
            out[n] = blend_beadings(out[source[n]], out[n], travelled[n] / reach);
    return out;
}

namespace {

// The beads of a node: indices [0, reach) reach it short of the node and sit on
// its ribs (node to support); on_node is the index of the bead at distance R,
// which sits on the node itself, or -1.
struct NodeBeads {
    std::uint32_t reach = 0;
    int on_node = -1;
};

NodeBeads node_beads(const MedialGraph::Node& node, const HalfBeading& b) {
    NodeBeads out;
    if (!(node.radius > 0.0))
        return out;
    for (std::size_t i = 0; i < b.distances.size(); ++i) {
        const double l = b.distances[i];
        if (l > node.radius + kOnNode)
            break; // this bead and the next ones do not reach the node
        if (l >= node.radius - kOnNode) {
            out.on_node = static_cast<int>(i);
            break;
        }
        ++out.reach;
    }
    return out;
}

// Bead at distance l on the rib from support s to the node: the distance to the
// outline grows linearly from 0 at s to R at the node.
Point2d rib_point(const MedialGraph::Node& node, const Point2d& s, double l) {
    const double f = l / node.radius;
    return {s.x + f * (node.p.x - s.x), s.y + f * (node.p.y - s.y)};
}

// The beads that cross a non-central edge, [first, end) of the upper node's
// beading, and that node. Empty for central edges and level ones.
struct MedialBeads {
    std::uint32_t first = 0, end = 0;
    std::uint32_t low = 0, high = 0;
};

MedialBeads medial_beads(const MedialGraph& g, const MedialGraph::Edge& e, const std::vector<HalfBeading>& beadings) {
    MedialBeads out;
    if (e.central || g.nodes[e.a].radius == g.nodes[e.b].radius)
        return out;
    const bool a_low = g.nodes[e.a].radius < g.nodes[e.b].radius;
    out.low = a_low ? e.a : e.b;
    out.high = a_low ? e.b : e.a;
    const double r_low = g.nodes[out.low].radius, r_high = g.nodes[out.high].radius;
    const HalfBeading& b = beadings[out.high];
    // A bead at R_low or R_high is a site of that node.
    std::uint32_t i = 0;
    while (i < b.distances.size() && !(b.distances[i] > r_low + kOnNode))
        ++i;
    out.first = out.end = i;
    while (out.end < b.distances.size() && b.distances[out.end] < r_high - kOnNode)
        ++out.end;
    return out;
}

Point2d medial_point(const MedialGraph& g, const MedialBeads& m, double l) {
    const MedialGraph::Node& low = g.nodes[m.low];
    const MedialGraph::Node& high = g.nodes[m.high];
    const double t = (l - low.radius) / (high.radius - low.radius);
    return {low.p.x + t * (high.p.x - low.p.x), low.p.y + t * (high.p.y - low.p.y)};
}

} // namespace

std::vector<Site> support_sites(const MedialGraph& g, const std::vector<HalfBeading>& beadings) {
    std::vector<Site> out;
    for (std::uint32_t ei = 0; ei < g.edges.size(); ++ei) {
        const MedialGraph::Edge& e = g.edges[ei];
        for (int end = 0; end < 2; ++end) {
            const std::uint32_t n = end == 0 ? e.a : e.b;
            const MedialGraph::Node& node = g.nodes[n];
            const HalfBeading& b = beadings[n];
            const NodeBeads nb = node_beads(node, b);
            const Site::Where where = end == 0 ? Site::Where::SideA : Site::Where::SideB;
            for (int side = 0; side < 2; ++side) {
                const Point2d& s = end == 0 ? e.support_a[side] : e.support_b[side];
                for (std::uint32_t i = 0; i < nb.reach; ++i)
                    out.push_back({rib_point(node, s, b.distances[i]), b.widths[i], int(i), ei, side, where, false});
                if (nb.on_node >= 0)
                    out.push_back({node.p, b.widths[std::size_t(nb.on_node)], nb.on_node, ei, side, where, true});
            }
        }
    }
    return out;
}

std::vector<Site> medial_sites(const MedialGraph& g, const std::vector<HalfBeading>& beadings) {
    std::vector<Site> out;
    for (std::uint32_t ei = 0; ei < g.edges.size(); ++ei) {
        const MedialBeads m = medial_beads(g, g.edges[ei], beadings);
        const HalfBeading& b = beadings[m.high];
        for (std::uint32_t i = m.first; i < m.end; ++i)
            out.push_back({medial_point(g, m, b.distances[i]), b.widths[i], int(i), ei, -1, Site::Where::Medial, false});
    }
    return out;
}

std::vector<Toolpath> extract_toolpaths(const MedialGraph& g, const std::vector<HalfBeading>& beadings) {
    // Every site gets a slot, its identity: the trapezoids that share a site
    // compute the same slot, so segments are chained through slots and never by
    // comparing positions. Slots, in one contiguous range:
    //  - rib r (a node and one of its support points), bead i: rib_base[r] + i;
    //  - the bead on node n: node_base + n;
    //  - edge e, bead i: medial_base[e] + i - first bead of e.
    const std::size_t edge_count = g.edges.size();
    std::vector<NodeBeads> nodes(g.nodes.size());
    for (std::size_t n = 0; n < g.nodes.size(); ++n)
        nodes[n] = node_beads(g.nodes[n], beadings[n]);

    // Ribs: each edge end has one per side. Ends of different edges at the same
    // node with the same support point (exactly: both read the same value) are
    // the same rib.
    struct RibEnd {
        std::uint32_t node;
        double sx, sy;
        std::uint32_t slot; // 4 * edge + 2 * end + side
    };
    std::vector<RibEnd> rib_ends;
    rib_ends.reserve(4 * edge_count);
    for (std::uint32_t ei = 0; ei < edge_count; ++ei) {
        const MedialGraph::Edge& e = g.edges[ei];
        for (std::uint32_t side = 0; side < 2; ++side) {
            rib_ends.push_back({e.a, e.support_a[side].x, e.support_a[side].y, 4 * ei + side});
            rib_ends.push_back({e.b, e.support_b[side].x, e.support_b[side].y, 4 * ei + 2 + side});
        }
    }
    std::sort(rib_ends.begin(), rib_ends.end(), [](const RibEnd& l, const RibEnd& r) {
        return std::tie(l.node, l.sx, l.sy, l.slot) < std::tie(r.node, r.sx, r.sy, r.slot);
    });
    std::vector<std::uint32_t> rib_base(4 * edge_count); // by 4 * edge + 2 * end + side
    std::uint32_t slots = 0;
    for (std::size_t i = 0; i < rib_ends.size(); ++i) {
        const RibEnd& r = rib_ends[i];
        const bool same = i > 0 && rib_ends[i - 1].node == r.node && rib_ends[i - 1].sx == r.sx && rib_ends[i - 1].sy == r.sy;
        if (!same)
            rib_base[r.slot] = slots, slots += nodes[r.node].reach;
        else
            rib_base[r.slot] = rib_base[rib_ends[i - 1].slot];
    }
    const std::uint32_t node_base = slots;
    slots += static_cast<std::uint32_t>(g.nodes.size());
    std::vector<MedialBeads> medial(edge_count);
    std::vector<std::uint32_t> medial_base(edge_count);
    for (std::size_t ei = 0; ei < edge_count; ++ei) {
        medial[ei] = medial_beads(g, g.edges[ei], beadings);
        medial_base[ei] = slots;
        slots += medial[ei].end - medial[ei].first;
    }

    // Segments: inside each trapezoid, the sites of one bead index on its two
    // node-to-support sides (A at node a, B at node b) and on the medial edge (M).
    struct Segment {
        ToolpathPoint a, b;
        std::uint32_t slot_a, slot_b;
        int index;
    };
    const auto top = [](const NodeBeads& n) { return n.reach + (n.on_node >= 0 ? 1u : 0u); };
    std::size_t most = 0; // at most one segment per bead index and trapezoid
    for (std::size_t ei = 0; ei < edge_count; ++ei)
        most += 2 * std::max({top(nodes[g.edges[ei].a]), top(nodes[g.edges[ei].b]), medial[ei].end});
    std::vector<Segment> segments;
    segments.reserve(most);
    struct Found {
        ToolpathPoint point;
        std::uint32_t slot;
        bool on_node;
    };
    for (std::uint32_t ei = 0; ei < edge_count; ++ei) {
        const MedialGraph::Edge& e = g.edges[ei];
        const NodeBeads& na = nodes[e.a];
        const NodeBeads& nb = nodes[e.b];
        const MedialBeads& m = medial[ei];
        const std::uint32_t count = std::max({top(na), top(nb), m.end});
        for (int side = 0; side < 2; ++side) {
            // Site of bead i on the side at node n with support s, if any.
            auto at_node = [&](std::uint32_t n, const NodeBeads& beads, const Point2d& s, std::uint32_t rib,
                               std::uint32_t i, Found& f) {
                const HalfBeading& b = beadings[n];
                if (i < beads.reach) {
                    f = {{rib_point(g.nodes[n], s, b.distances[i]), b.widths[i]}, rib_base[rib] + i, false};
                    return true;
                }
                if (int(i) == beads.on_node) {
                    f = {{g.nodes[n].p, b.widths[i]}, node_base + n, true};
                    return true;
                }
                return false;
            };
            for (std::uint32_t i = 0; i < count; ++i) {
                Found found[3];
                int k = 0;
                k += at_node(e.a, na, e.support_a[side], 4 * ei + std::uint32_t(side), i, found[k]);
                k += at_node(e.b, nb, e.support_b[side], 4 * ei + 2 + std::uint32_t(side), i, found[k]);
                if (k < 2 && i >= m.first && i < m.end) {
                    const HalfBeading& b = beadings[m.high];
                    found[k++] = {{medial_point(g, m, b.distances[i]), b.widths[i]}, medial_base[ei] + i - m.first, false};
                }
                if (k < 2)
                    continue; // a transition: this site stays unconnected
                // The middle bead runs along the medial edge: both trapezoids would emit it.
                if (side == 1 && found[0].on_node && found[1].on_node)
                    continue;
                if (found[0].slot == found[1].slot)
                    continue;
                segments.push_back({found[0].point, found[1].point, found[0].slot, found[1].slot, int(i)});
            }
        }
    }

    // A slot held by exactly two segment ends links them; more make a junction,
    // where polylines end. Ends are numbered 2 * segment + (0 for a, 1 for b).
    constexpr std::uint32_t kNone = 0xFFFFFFFFu;
    std::vector<std::uint32_t> holder(slots, kNone), link(2 * segments.size(), kNone);
    std::vector<std::uint8_t> holders(slots, 0);
    for (std::uint32_t end = 0; end < link.size(); ++end) {
        const std::uint32_t slot = end % 2 == 0 ? segments[end / 2].slot_a : segments[end / 2].slot_b;
        if (holders[slot] == 0) {
            holder[slot] = end;
        } else if (holders[slot] == 1) {
            link[end] = holder[slot];
            link[holder[slot]] = end;
        } else if (holders[slot] == 2) {
            link[link[holder[slot]]] = kNone;
            link[holder[slot]] = kNone;
        }
        holders[slot] = std::uint8_t(std::min(3, holders[slot] + 1));
    }

    std::vector<bool> used(segments.size(), false);
    auto point = [&](std::uint32_t end) -> const ToolpathPoint& {
        return end % 2 == 0 ? segments[end / 2].a : segments[end / 2].b;
    };
    auto append = [](std::vector<ToolpathPoint>& pts, const ToolpathPoint& p) {
        if (pts.empty() || pts.back().p.x != p.p.x || pts.back().p.y != p.p.y)
            pts.push_back(p);
    };
    std::vector<Toolpath> out;
    // Follows the links from `start`, a free end of a chain or any end of a loop.
    auto emit = [&](std::uint32_t start) {
        used[start / 2] = true;
        Toolpath path;
        path.index = segments[start / 2].index;
        append(path.points, point(start));
        append(path.points, point(start ^ 1u));
        for (std::uint32_t end = start;;) {
            const std::uint32_t next = link[end ^ 1u];
            if (next == start) {
                path.closed = true;
                break;
            }
            if (next == kNone || used[next / 2])
                break;
            used[next / 2] = true;
            append(path.points, point(next ^ 1u));
            end = next;
        }
        if (path.closed && path.points.size() > 1 && path.points.back().p.x == path.points.front().p.x &&
            path.points.back().p.y == path.points.front().p.y)
            path.points.pop_back();
        if (path.points.size() >= 2)
            out.push_back(std::move(path));
    };
    // Open chains from their free ends first; what is left lies on loops.
    for (std::uint32_t end = 0; end < link.size(); ++end)
        if (!used[end / 2] && link[end] == kNone)
            emit(end);
    for (std::uint32_t s = 0; s < segments.size(); ++s)
        if (!used[s])
            emit(2 * s);

    // A bead that reaches only one node, exactly (the middle bead of an odd
    // count at an isolated maximum, such as the centre of a regular polygon),
    // is in no segment: it becomes a toolpath of one point, a dot.
    for (std::uint32_t n = 0; n < g.nodes.size(); ++n) {
        if (nodes[n].on_node < 0 || holders[node_base + n] != 0)
            continue;
        const std::size_t i = std::size_t(nodes[n].on_node);
        Toolpath dot;
        dot.index = nodes[n].on_node;
        dot.points.push_back({g.nodes[n].p, beadings[n].widths[i]});
        out.push_back(std::move(dot));
    }
    return out;
}

} // namespace tisma
