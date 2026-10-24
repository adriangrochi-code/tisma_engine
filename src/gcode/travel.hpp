#pragma once

#include "geom/medial_axis.hpp"
#include "geom/polygons.hpp"

#include <vector>

namespace tisma {

// A travel move: the points the nozzle passes through after its start, the
// last one being the destination.
struct TravelPlan {
    std::vector<Point2d> points;
    bool leaves_part = false; // it crosses the part's walls or runs outside: retract whatever its length
    double length = 0.0;      // mm, along the points
};

// Plans the travels of one layer so that they stay inside the part
// (docs/spec/BEHAVIOR.md, "Viajes y retracción"). Own design:
// - a travel whose straight segment stays inside the layer's region goes straight;
// - otherwise it runs inside the travel region, the region shrunk by one bead
//   (inside the outer wall): an end outside it first hops to the closest point
//   of it, then the segment between the two ends is followed and, wherever it
//   leaves the travel region, replaced by the boundary it crossed, the shorter
//   way round; the route is then shortened where a straight cut stays inside;
// - when the two ends are in different connected pieces of the travel region
//   (or too far from it), the travel leaves the part and goes straight.
class TravelPlanner {
public:
    // region: the layer's region (outer contours counter-clockwise, holes
    // clockwise); bead: one wall bead width in mm.
    TravelPlanner(const Polygons& region, double bead);

    TravelPlan plan(Point2d from, Point2d to) const;

private:
    struct Ring {
        std::vector<Point2d> points;
        double min_x, min_y, max_x, max_y;
    };
    struct Located {
        int piece = -1;
        Point2d p;
        bool hopped = false;
    };

    static std::vector<Ring> rings_of(const Polygons& polygons);
    static bool visible(const std::vector<Ring>& rings, Point2d a, Point2d b);
    static bool inside(const std::vector<Ring>& rings, Point2d p);
    int piece_of(Point2d p) const;
    Located locate(Point2d p) const;
    bool route(int piece, Point2d a, Point2d b, std::vector<Point2d>& out) const;

    std::vector<Ring> region_;  // the layer's region
    std::vector<Ring> travel_;  // the region shrunk by one bead
    std::vector<std::vector<std::size_t>> pieces_; // rings of travel_ per connected piece, outer first
    std::vector<Ring> margin_;  // the travel region grown by a little: shortcut checks
    double snap_;               // farthest hop onto the travel region, mm
};

} // namespace tisma
