#pragma once

#include "geom/polygons.hpp"
#include "perimeters/sites.hpp"

#include <vector>

namespace tisma {

// Arcs over an overhang held from one side only
// (docs/spec/features/arc_overhangs.md): concentric arcs around one centre,
// 1.2 mm inside what holds the zone and facing the middle of the edge it hangs
// from, each a little wider than the one before and lying on it, clipped by
// the zone; they grow until they reach its farthest point, cut into pieces by
// its holes and its sides. One way, then the other; every other one split in
// two on the line through the centre across that edge. A zone held from more
// than one side is a bridge, and one held all round (a ring round a column)
// keeps its bridge too. Own design on the published idea of the «arc
// overhang» (Steven McCulloch, 2023), without its code.
struct ArcParams {
    double width = 0.4;        // mm, the beads'
    double overlap = 10.0;     // percent of the width each arc lies on the one before
    double max_distance = 0.0; // mm from the centre the arcs reach; 0: no limit
};

// mm the centre lies inside what holds the zone (rule 7: 98.8 with the edge at 100).
inline constexpr double kArcCentre = 1.2;

struct ArcZone {
    Point2d centre;
    Polygons area;              // what the arcs cover: the zone, a bite of what holds it, within the outline
    std::vector<Toolpath> arcs; // in printing order, from the centre out
};

// The arc zones of a layer: its region (`region`) where it is past the region
// below (`below`), in pieces held from one side.
std::vector<ArcZone> overhang_arcs(const Polygons& region, const Polygons& below, const ArcParams& params);

} // namespace tisma
