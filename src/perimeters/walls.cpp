// The wall chain of one region (Kuipers et al. 2020, Sections 3 and 5.1) and
// the region it leaves for infill. Own design; see PROVENANCE.md.

#include "perimeters/walls.hpp"

#include "perimeters/transitions.hpp"

#include <algorithm>

namespace tisma {

LayerWalls layer_walls(const Polygons& input, const BeadingParams& params) {
    LayerWalls out;
    // Rings of no area, as a boolean operation may leave, break the medial
    // axis (a node 30 m from a 16 mm part): out.
    Polygons region;
    region.reserve(input.size());
    for (const Polygon& ring : input)
        if (std::abs(Clipper2Lib::Area(ring)) > 1e6) // nm2: a thousandth of a mm squared
            region.push_back(ring);
    if (region.empty())
        return out;
    MedialGraph graph = medial_graph(region);
    mark_central(graph);
    filter_marking(graph, params.preferred_width);
    const TransitionResult transitions = apply_transitions(graph, params);
    out.toolpaths = extract_toolpaths(graph, node_beadings(graph, transitions.bead_counts, params));
    // Every closed loop runs counter-clockwise (own choice), whatever way the
    // chaining of sites happened to walk it: the G-code does not depend on
    // the order of the Voronoi diagram's edges.
    for (Toolpath& t : out.toolpaths) {
        if (!t.closed || t.points.size() < 3)
            continue;
        double area = 0.0;
        for (std::size_t i = 0; i < t.points.size(); ++i) {
            const Point2d& a = t.points[i].p;
            const Point2d& b = t.points[(i + 1) % t.points.size()].p;
            area += a.x * b.y - b.x * a.y;
        }
        if (area < 0.0)
            std::reverse(t.points.begin(), t.points.end());
    }
    if (params.wall_count > 0) {
        const double w = params.preferred_width;
        const Polygons inner = offset(region, -params.wall_count * w);
        out.infill = offset(offset(inner, -w / 4.0), w / 4.0); // drop what is narrower than w*/2
    }
    return out;
}

} // namespace tisma
