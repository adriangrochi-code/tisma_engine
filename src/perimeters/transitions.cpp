// Bead counts on the centre and transition anchors, from Kuipers et al. 2020
// (arXiv 2004.13497), Section 3.4, Eqs. 4-6. Own structure; see PROVENANCE.md.

#include "perimeters/transitions.hpp"

#include <algorithm>
#include <cmath>

namespace tisma {

std::vector<int> central_bead_counts(const MedialGraph& graph, const BeadingParams& params) {
    std::vector<int> counts(graph.nodes.size(), -1);
    for (std::size_t i = 0; i < graph.nodes.size(); ++i)
        if (graph.nodes[i].central)
            counts[i] = bead_count(params, 2.0 * graph.nodes[i].radius);
    return counts;
}

std::vector<TransitionAnchor> transition_anchors(const MedialGraph& graph, const std::vector<int>& counts,
                                                 const BeadingParams& params) {
    std::vector<TransitionAnchor> out;
    for (std::uint32_t ei = 0; ei < graph.edges.size(); ++ei) {
        const MedialGraph::Edge& e = graph.edges[ei];
        if (!e.central || counts[e.a] < 0 || counts[e.b] < 0 || counts[e.a] == counts[e.b])
            continue;
        const MedialGraph::Node& a = graph.nodes[e.a];
        const MedialGraph::Node& b = graph.nodes[e.b];
        const int low = std::min(counts[e.a], counts[e.b]);
        const int high = std::max(counts[e.a], counts[e.b]);
        std::vector<TransitionAnchor> here;
        // Step through every count the edge passes; the uniform scheme jumps by two.
        for (int n = low; n < high;) {
            const double thickness = transition_thickness(params, n);
            const int next = bead_count(params, thickness * (1.0 + 1e-12) + 1e-12);
            if (next <= n)
                break; // no further count: nothing more to anchor
            // Eq. 6: R(v_x) = q^-1(n) / 2, interpolated between the edge's ends.
            const double dr = b.radius - a.radius;
            const double t = dr != 0.0 ? std::clamp((thickness / 2.0 - a.radius) / dr, 0.0, 1.0) : 0.5;
            here.push_back({ei, t, {a.p.x + t * (b.p.x - a.p.x), a.p.y + t * (b.p.y - a.p.y)}, n, next});
            n = next;
        }
        std::sort(here.begin(), here.end(), [](const TransitionAnchor& l, const TransitionAnchor& r) { return l.t < r.t; });
        out.insert(out.end(), here.begin(), here.end());
    }
    return out;
}

namespace {

double distance(const Point2d& a, const Point2d& b) { return std::hypot(b.x - a.x, b.y - a.y); }

// A path of central edges, as node indices and the edge between consecutive nodes.
struct Chain {
    std::vector<std::uint32_t> nodes;
    std::vector<std::uint32_t> edges;
};

std::vector<Chain> central_chains(const MedialGraph& g) {
    std::vector<std::vector<std::uint32_t>> incident(g.nodes.size());
    for (std::uint32_t e = 0; e < g.edges.size(); ++e)
        if (g.edges[e].central) {
            incident[g.edges[e].a].push_back(e);
            incident[g.edges[e].b].push_back(e);
        }
    std::vector<bool> used(g.edges.size(), false);
    std::vector<Chain> chains;
    auto walk = [&](std::uint32_t start, std::uint32_t first_edge) {
        Chain c;
        c.nodes.push_back(start);
        std::uint32_t node = start, edge = first_edge;
        while (true) {
            used[edge] = true;
            c.edges.push_back(edge);
            node = g.edges[edge].a == node ? g.edges[edge].b : g.edges[edge].a;
            c.nodes.push_back(node);
            if (incident[node].size() != 2 || node == start)
                break;
            edge = incident[node][0] == edge ? incident[node][1] : incident[node][0];
            if (used[edge])
                break;
        }
        chains.push_back(std::move(c));
    };
    // Chains between branch points or ends first, then closed loops.
    for (std::uint32_t n = 0; n < g.nodes.size(); ++n)
        if (incident[n].size() != 2)
            for (std::uint32_t e : incident[n])
                if (!used[e])
                    walk(n, e);
    for (std::uint32_t n = 0; n < g.nodes.size(); ++n)
        for (std::uint32_t e : incident[n])
            if (!used[e])
                walk(n, e);
    return chains;
}

} // namespace

TransitionResult apply_transitions(MedialGraph& g, const BeadingParams& params, const TransitionSettings& settings) {
    TransitionResult result;
    const double ramp = settings.transition_length >= 0.0 ? settings.transition_length : params.preferred_width;
    const std::vector<int> counts = central_bead_counts(g, params);
    result.bead_counts.assign(g.nodes.size(), -1.0);
    for (std::size_t i = 0; i < counts.size(); ++i)
        if (counts[i] >= 0)
            result.bead_counts[i] = counts[i];

    struct Ramp {
        double start, end; // positions along the chain: start on the thinner side
        int n;             // bead count on the thinner side
        int next;          // bead count on the thicker side
    };
    for (Chain& chain : central_chains(g)) {
        // Positions of the nodes along the chain.
        std::vector<double> s(chain.nodes.size(), 0.0);
        for (std::size_t i = 1; i < s.size(); ++i)
            s[i] = s[i - 1] + distance(g.nodes[chain.nodes[i - 1]].p, g.nodes[chain.nodes[i]].p);
        const double length = s.back();

        // Anchors along the chain, with their direction (+1 when the count grows along the chain).
        struct Anchor {
            double s;
            int n, next, dir;
        };
        std::vector<Anchor> anchors;
        for (std::size_t i = 0; i + 1 < chain.nodes.size(); ++i) {
            const int c0 = counts[chain.nodes[i]], c1 = counts[chain.nodes[i + 1]];
            if (c0 < 0 || c1 < 0 || c0 == c1)
                continue;
            const double r0 = g.nodes[chain.nodes[i]].radius, r1 = g.nodes[chain.nodes[i + 1]].radius;
            const int dir = c1 > c0 ? 1 : -1;
            std::vector<Anchor> here;
            for (int n = std::min(c0, c1); n < std::max(c0, c1);) {
                const double thickness = transition_thickness(params, n);
                const int next = bead_count(params, thickness * (1.0 + 1e-12) + 1e-12);
                if (next <= n)
                    break;
                const double t = r1 != r0 ? std::clamp((thickness / 2.0 - r0) / (r1 - r0), 0.0, 1.0) : 0.5;
                here.push_back({s[i] + t * (s[i + 1] - s[i]), n, next, dir});
                n = next;
            }
            std::sort(here.begin(), here.end(), [](const Anchor& l, const Anchor& r) { return l.s < r.s; });
            anchors.insert(anchors.end(), here.begin(), here.end());
        }
        result.anchors += anchors.size();

        // Filter: consecutive anchors in opposite directions closer than the limit
        // cancel out; the counts between them become the surrounding count.
        std::vector<bool> keep(anchors.size(), true);
        for (std::size_t k = 0; k + 1 < anchors.size(); ++k) {
            if (!keep[k] || anchors[k].dir == anchors[k + 1].dir)
                continue;
            if (anchors[k + 1].s - anchors[k].s < settings.filter_distance) {
                keep[k] = keep[k + 1] = false;
                result.anchors_filtered += 2;
                const int surrounding = anchors[k].dir > 0 ? anchors[k].n : anchors[k].next;
                for (std::size_t i = 0; i < chain.nodes.size(); ++i)
                    if (s[i] > anchors[k].s && s[i] < anchors[k + 1].s)
                        result.bead_counts[chain.nodes[i]] = surrounding;
                ++k;
            }
        }

        // Ramps: length `ramp`, starting t(n) (q^-1(n)/w* - n) before the anchor on
        // the thinner side. A ramp that does not fit in the chain is dropped.
        std::vector<Ramp> ramps;
        for (std::size_t k = 0; k < anchors.size(); ++k) {
            if (!keep[k])
                continue;
            const Anchor& a = anchors[k];
            const double lead = ramp * (transition_thickness(params, a.n) / params.preferred_width - a.n);
            const double start = a.dir > 0 ? a.s - lead : a.s + lead;
            const double end = a.dir > 0 ? start + ramp : start - ramp;
            if (std::min(start, end) < 0.0 || std::max(start, end) > length) {
                ++result.anchors_dropped;
                continue;
            }
            ramps.push_back({start, end, a.n, a.next});
        }

        // Split the chain at the ramp ends, then give the nodes inside each ramp
        // their fractional count (Eq. 7).
        auto split_at = [&](double pos) -> std::uint32_t {
            for (std::size_t i = 0; i + 1 < chain.nodes.size(); ++i) {
                if (std::fabs(s[i] - pos) < 1e-9)
                    return chain.nodes[i];
                if (std::fabs(s[i + 1] - pos) < 1e-9)
                    return chain.nodes[i + 1];
                if (pos > s[i] && pos < s[i + 1]) {
                    const double t = (pos - s[i]) / (s[i + 1] - s[i]);
                    const MedialGraph::Node& a = g.nodes[chain.nodes[i]];
                    const MedialGraph::Node& b = g.nodes[chain.nodes[i + 1]];
                    const auto id = static_cast<std::uint32_t>(g.nodes.size());
                    g.nodes.push_back({{a.p.x + t * (b.p.x - a.p.x), a.p.y + t * (b.p.y - a.p.y)},
                                       a.radius + t * (b.radius - a.radius),
                                       true});
                    result.bead_counts.push_back(-1.0);
                    // Edge i now ends at the new node; a new edge continues to the old end.
                    // The new node's supports are interpolated along the edge
                    // (exact on straight edges; pieces of curved ones are at most 0.2 mm).
                    MedialGraph::Edge& old = g.edges[chain.edges[i]];
                    const bool forward = old.a == chain.nodes[i];
                    const std::uint32_t far = forward ? old.b : old.a;
                    const double u = forward ? t : 1.0 - t; // position from old.a
                    Point2d mid[2];
                    for (int k = 0; k < 2; ++k)
                        mid[k] = {old.support_a[k].x + u * (old.support_b[k].x - old.support_a[k].x),
                                  old.support_a[k].y + u * (old.support_b[k].y - old.support_a[k].y)};
                    MedialGraph::Edge rest{id, far, old.curved, true, {mid[0], mid[1]}, {}};
                    if (forward) {
                        rest.support_b[0] = old.support_b[0];
                        rest.support_b[1] = old.support_b[1];
                        old.b = id;
                        old.support_b[0] = mid[0];
                        old.support_b[1] = mid[1];
                    } else {
                        rest.support_b[0] = old.support_a[0];
                        rest.support_b[1] = old.support_a[1];
                        old.a = id;
                        old.support_a[0] = mid[0];
                        old.support_a[1] = mid[1];
                    }
                    g.edges.push_back(rest);
                    chain.nodes.insert(chain.nodes.begin() + std::ptrdiff_t(i + 1), id);
                    chain.edges.insert(chain.edges.begin() + std::ptrdiff_t(i + 1),
                                       static_cast<std::uint32_t>(g.edges.size() - 1));
                    s.insert(s.begin() + std::ptrdiff_t(i + 1), pos);
                    return id;
                }
            }
            return chain.nodes.back();
        };
        for (const Ramp& r : ramps) {
            const std::uint32_t a = split_at(r.start);
            const std::uint32_t b = split_at(r.end);
            result.bead_counts[a] = r.n;
            result.bead_counts[b] = r.next;
            for (std::size_t i = 0; i < chain.nodes.size(); ++i) {
                const double lo = std::min(r.start, r.end), hi = std::max(r.start, r.end);
                if (s[i] > lo + 1e-9 && s[i] < hi - 1e-9)
                    result.bead_counts[chain.nodes[i]] = r.n + std::fabs(s[i] - r.start) / ramp;
            }
        }

        // The shell's beading bends where its ramp starts and ends (Section 5.1:
        // extra nodes at the locations of the non-linearities). Nodes there keep
        // beadings interpolated between nodes on it; their counts are
        // interpolated along the edge.
        if (const std::optional<ShellRamp> shell = shell_ramp(params)) {
            for (const double thickness : {shell->start, shell->end}) {
                const double r = thickness / 2.0;
                for (std::size_t i = 0; i + 1 < chain.nodes.size(); ++i) {
                    const double r0 = g.nodes[chain.nodes[i]].radius, r1 = g.nodes[chain.nodes[i + 1]].radius;
                    if (!((r0 < r && r < r1) || (r1 < r && r < r0)))
                        continue;
                    const double t = (r - r0) / (r1 - r0);
                    const double c0 = result.bead_counts[chain.nodes[i]], c1 = result.bead_counts[chain.nodes[i + 1]];
                    const std::uint32_t id = split_at(s[i] + t * (s[i + 1] - s[i]));
                    result.bead_counts[id] = c0 + t * (c1 - c0);
                    ++i; // the rest of the split edge lies on one side of r
                }
            }
        }
    }
    return result;
}

} // namespace tisma
