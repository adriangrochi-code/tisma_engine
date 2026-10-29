#pragma once

#include "perimeters/sites.hpp"

#include <span>
#include <vector>

namespace tisma {

// A run of a path's points that follows a circle, written as one G2 or G3
// (docs/spec/features/gcode_extras.md, rule 10).
struct ArcRun {
    std::size_t first = 0, last = 0; // the arc goes from points[first] to points[last]
    Point2d center;
    double radius = 0.0;
    bool ccw = false;   // G3 (counter-clockwise) or G2 (clockwise)
    double sweep = 0.0; // radians it turns, > 0
    double length() const { return radius * sweep; }
};

// Fewest moves an arc replaces, and the most it sweeps: past half a turn the
// next arc takes over (the centre stays well defined, and no arc closes on
// itself).
inline constexpr std::size_t kArcMinSegments = 3;
inline constexpr double kArcMaxSweep = 3.141592653589793;
// Radii an arc may have, mm: tighter is a corner, wider is a straight line.
inline constexpr double kArcMinRadius = 0.05;
inline constexpr double kArcMaxRadius = 1000.0;

// The arcs of the polyline `points`, in order and not overlapping (one may
// start where the last ended). A run is an arc when its points all lie
// within `tolerance` mm of the circle through its first, middle and last
// points, and the middles of its moves within twice that (the path's points
// are the shape's, and its moves were already let stray `tolerance` from it
// by simplify_toolpath: the arc keeps as close to the shape as they did, and
// a polygon's corners never become a circle); it turns one way, by less than
// half a turn per move and kArcMaxSweep in all; and its points have one width
// and no Z or wavy offset (the arc is a flat bead of one section). Each arc
// reaches as far as it can from where it starts. Own design.
std::vector<ArcRun> fit_arcs(std::span<const ToolpathPoint> points, double tolerance);

} // namespace tisma
