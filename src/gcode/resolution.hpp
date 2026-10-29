#pragma once

#include "perimeters/sites.hpp"

namespace tisma {

// The bead's width, its Z off the layer's top and the bead below it are kept
// to this, in mm: half the last digit the G-code writes.
inline constexpr double kResolutionAttribute = 0.0005;

// The toolpath with fewer, longer moves (gcode.resolution,
// docs/spec/features/gcode_extras.md): a point goes when the move that
// replaces it passes within `tolerance` mm of it, with the width, Z and bead
// below interpolated along that move within kResolutionAttribute of the
// point's own. Each move reaches as far as it can (found by doubling, then
// halving, so a path costs n log n). The first and last points stay (a closed
// path keeps its first point, the seam), and a closed path keeps three points
// at least. A tolerance of 0 or less returns the path as it is.
Toolpath simplify_toolpath(const Toolpath& path, double tolerance);

} // namespace tisma
