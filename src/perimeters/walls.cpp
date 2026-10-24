// The wall chain of one region (Kuipers et al. 2020, Sections 3 and 5.1) and
// the region it leaves for infill. Own design; see PROVENANCE.md.

#include "perimeters/walls.hpp"

#include "perimeters/transitions.hpp"

namespace tisma {

LayerWalls layer_walls(const Polygons& region, const BeadingParams& params) {
    LayerWalls out;
    if (region.empty())
        return out;
    MedialGraph graph = medial_graph(region);
    mark_central(graph);
    filter_marking(graph, params.preferred_width);
    const TransitionResult transitions = apply_transitions(graph, params);
    out.toolpaths = extract_toolpaths(graph, node_beadings(graph, transitions.bead_counts, params));
    if (params.wall_count > 0) {
        const double w = params.preferred_width;
        const Polygons inner = offset(region, -params.wall_count * w);
        out.infill = offset(offset(inner, -w / 4.0), w / 4.0); // drop what is narrower than w*/2
    }
    return out;
}

} // namespace tisma
