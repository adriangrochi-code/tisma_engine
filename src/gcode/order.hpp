#pragma once

#include "config/settings.hpp"
#include "geom/polygons.hpp"
#include "perimeters/sites.hpp"

#include <cstdint>
#include <map>
#include <vector>

namespace tisma {

// Where seams were, per wall index, for aligned seams: every seam of the layer
// below and those already laid in this one. next_layer() moves this layer's
// seams below (an index the new layer did not use keeps its older seams).
struct SeamMemory {
    std::map<int, std::vector<Point2d>> below;
    std::map<int, std::vector<Point2d>> current;
    void next_layer();
};

// The seam of a closed loop (docs/spec/BEHAVIOR.md, "Costura"): the index of
// the point it starts at.
//  - nearest: the point nearest to `from`;
//  - rear: the point of largest Y (then smallest X);
//  - random: a point picked from `seed` (the same input gives the same G-code);
//  - aligned: among the concave corners (turns against the loop's direction
//    sharper than kSeamCorner, measured to the points kSeamSpan away on each
//    side), or all points if there are none, the one nearest
//    to a seam of the same wall index in the layer below; the rear one if there
//    was none.
inline constexpr double kSeamCorner = 30.0; // degrees
inline constexpr double kSeamSpan = 0.1;    // mm: a corner's turn is measured to points this far away
std::size_t seam_point(const Toolpath& loop, SeamMode mode, const Point2d& from, std::uint64_t seed,
                       const SeamMemory& memory);

// Orders paths greedily: from `cursor`, the path whose start is nearest next.
// An open path may run either way; a closed loop starts at its seam (rotated
// so). Moves `cursor` to the end of the last path and records the seams.
std::vector<Toolpath> order_paths(std::vector<Toolpath> paths, Point2d& cursor, SeamMode mode, std::uint64_t seed,
                                  SeamMemory& memory);

// The islands of a layer region: its connected pieces, each an outer contour
// (counter-clockwise) with its holes (clockwise). An island inside another's
// hole is an island of its own.
std::vector<Polygons> split_islands(const Polygons& region);

// The island the point is in (on its boundary counts as in), or -1.
int island_of(const std::vector<Polygons>& islands, const Point2d& p);

} // namespace tisma
