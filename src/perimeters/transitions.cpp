// Bead counts on the centre and transition anchors, from Kuipers et al. 2020
// (arXiv 2004.13497), Section 3.4, Eqs. 4-6. Own structure; see PROVENANCE.md.

#include "perimeters/transitions.hpp"

#include <algorithm>

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

} // namespace tisma
