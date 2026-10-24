#pragma once

#include "geom/polygons.hpp"

#include <vector>

namespace tisma {

struct Point2d {
    double x = 0.0;
    double y = 0.0;
};

// An edge of the medial axis: a piece of the Voronoi diagram of the region's
// boundary segments that lies inside the region. radius_a and radius_b are the
// distances from its ends to the boundary (the radius of the largest inscribed
// circle there); in Kuipers et al. 2020 they decide how many beads fit.
// Curved edges (between a corner and a segment, a parabola) are given by
// their two ends for now.
struct MedialEdge {
    Point2d a, b;     // mm
    double radius_a;  // mm
    double radius_b;  // mm
    bool curved;
};

// Medial axis of a region (outer contours counter-clockwise, holes clockwise),
// from Boost.Polygon's segment Voronoi diagram. Each edge appears once.
// Coordinates must fit in 32 bits of the 1 nm unit (about +-2 m).
std::vector<MedialEdge> medial_axis(const Polygons& region);

} // namespace tisma
