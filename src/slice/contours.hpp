#pragma once

#include "slice/level_set.hpp"

#include <span>
#include <vector>

namespace tisma {

// A chain of level-set segments. Closed contours repeat no point: the last
// point connects back to the first. Open contours come from meshes with holes.
struct Contour {
    std::vector<Vec3f> points;
    bool closed = false;
};

// Joins segments into contours through their edge keys, without comparing
// coordinates: a segment follows another when its edge_from equals the other's
// edge_to. Open chains are returned from their first to their last point.
// Collinear points are kept; simplification is a later step. With edges, also
// the mesh edge of every point, contour by contour: another level that crosses
// the same edges has the same contours, found with edge_crossing alone.
std::vector<Contour> join_segments(std::span<const LevelSegment> segments,
                                   std::vector<std::vector<EdgeKey>>* edges = nullptr);

// Closes small gaps left by holes in the mesh: joins the end of an open contour
// to the start of an open contour (itself included) when they are at most
// max_gap apart, nearest pairs first. Joined chains that come back to their
// start become closed. Closed contours are not touched; open contours that
// cannot be joined stay open.
void close_gaps(std::vector<Contour>& contours, double max_gap);

} // namespace tisma
