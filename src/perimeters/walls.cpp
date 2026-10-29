// The wall chain of one region (Kuipers et al. 2020, Sections 3 and 5.1) and
// the region it leaves for infill. Own design; see PROVENANCE.md.

#include "perimeters/walls.hpp"

#include "perimeters/transitions.hpp"

#include <algorithm>

namespace tisma {

LayerWalls layer_walls(const Polygons& input, const BeadingParams& given) {
    LayerWalls out;
    // In spacings (perimeters.md, rule 1): the outer bead's edge on the
    // outline puts its centre half a width in, half an overlap more than half
    // a spacing; between neighbours, the spacings laid edge to edge.
    BeadingParams params = given;
    const double overlap = std::clamp(given.overlap, 0.0, 0.5 * given.preferred_width);
    params.overlap = 0.0;
    params.preferred_width -= overlap;
    if (params.outer_width > 0.0)
        params.outer_width = std::max(params.outer_width - overlap, 0.1 * params.preferred_width);
    // Rings of no area, as a boolean operation may leave, break the medial
    // axis (a node 30 m from a 16 mm part): out.
    Polygons region;
    region.reserve(input.size());
    for (const Polygon& ring : overlap > 0.0 ? offset(input, -overlap / 2.0) : input)
        if (std::abs(Clipper2Lib::Area(ring)) > 1e6) // nm2: a thousandth of a mm squared
            region.push_back(ring);
    if (region.empty())
        return out;
    MedialGraph graph = medial_graph(region);
    mark_central(graph, 180.0 - std::clamp(params.transition_angle, 0.0, 179.0));
    filter_marking(graph, params.preferred_width);
    TransitionSettings ramps;
    if (params.transition_length > 0.0)
        ramps.transition_length = params.transition_length;
    const TransitionResult transitions = apply_transitions(graph, params, ramps);
    out.toolpaths = extract_toolpaths(graph, node_beadings(graph, transitions.bead_counts, params));
    if (overlap > 0.0)
        for (Toolpath& t : out.toolpaths)
            for (ToolpathPoint& p : t.points)
                if (p.width > 0.0) // a tip that tapers to nothing stays nothing
                    p.width += overlap;
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
        const Polygons inner = offset(region, -(outer_bead(params) + (params.wall_count - 1) * w));
        out.infill = offset(offset(inner, -w / 4.0), w / 4.0); // drop what is narrower than w*/2
    }
    return out;
}

} // namespace tisma
