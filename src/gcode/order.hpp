#pragma once

#include "config/settings.hpp"
#include "perimeters/sites.hpp"

#include <cstdint>
#include <map>
#include <vector>

namespace tisma {

// Where seams were in the layer below, per wall index, for aligned seams.
struct SeamMemory {
    std::map<int, Point2d> last;
};

// The seam of a closed loop (docs/spec/BEHAVIOR.md, "Costura"): the index of
// the point it starts at.
//  - nearest: the point nearest to `from`;
//  - rear: the point of largest Y (then smallest X);
//  - random: a point picked from `seed` (the same input gives the same G-code);
//  - aligned: among the concave corners (turns against the loop's direction
//    sharper than kSeamCorner), or all points if there are none, the one nearest
//    to the seam of the same wall index in the layer below; the rear one if there
//    was none.
inline constexpr double kSeamCorner = 30.0; // degrees
std::size_t seam_point(const Toolpath& loop, SeamMode mode, const Point2d& from, std::uint64_t seed,
                       const SeamMemory& memory);

// Orders paths greedily: from `cursor`, the path whose start is nearest next.
// An open path may run either way; a closed loop starts at its seam (rotated
// so). Moves `cursor` to the end of the last path and records the seams.
std::vector<Toolpath> order_paths(std::vector<Toolpath> paths, Point2d& cursor, SeamMode mode, std::uint64_t seed,
                                  SeamMemory& memory);

} // namespace tisma
