// Beading propagation and toolpath sites, from Kuipers et al. 2020
// (arXiv 2004.13497), Sections 3.5 and 3.6. Own structure; see PROVENANCE.md.

#include "perimeters/sites.hpp"

#include <cmath>

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
                    if (b.distances[i] > node.radius + 1e-9)
                        break; // this bead and the next ones do not reach the node
                    const double f = std::min(1.0, b.distances[i] / node.radius);
                    out.push_back({{s.x + f * (node.p.x - s.x), s.y + f * (node.p.y - s.y)},
                                   b.widths[i],
                                   static_cast<int>(i),
                                   ei,
                                   side,
                                   end == 1});
                }
            }
        }
    }
    return out;
}

} // namespace tisma
