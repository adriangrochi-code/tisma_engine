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
// Collinear points are kept; simplification is a later step.
std::vector<Contour> join_segments(std::span<const LevelSegment> segments);

} // namespace tisma
