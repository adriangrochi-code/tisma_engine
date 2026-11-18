#pragma once

#include "perimeters/sites.hpp"

#include <vector>

namespace tisma {

// Smooths the sharp turns of the sparse infill (orca_2_5.md, rule 11): each
// corner of a path becomes a curve tangent to its two stretches. `factor`
// from 0 (the path as it is) to 1 (the largest curve): a curve starts at
// most half the shorter stretch beside it from the corner, times `factor`,
// so two curves never overlap. Hairpins, turning more than kHairpinDegrees,
// stay pointed, and so do nearly straight joins.
inline constexpr double kHairpinDegrees = 154.0;
void smooth_corners(std::vector<Toolpath>& paths, double factor);

} // namespace tisma
