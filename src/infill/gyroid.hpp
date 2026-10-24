#pragma once

#include "geom/polygons.hpp"
#include "perimeters/sites.hpp"

#include <vector>

namespace tisma {

// Gyroid infill (docs/spec/SOURCES.md): the curves where
// sin X cos Y + sin Y cos Z + sin Z cos X = 0, with X, Y, Z = 2 pi (x, y, z) /
// period, at height z, clipped to the region shrunk by width / 2, as open
// toolpaths of constant width. The curves come from marching squares on a
// grid fixed to the origin, kGyroidSamples steps per period; points within
// kGyroidTolerance of the line between their neighbours are left out.
inline constexpr int kGyroidSamples = 24;
inline constexpr double kGyroidTolerance = 0.005; // mm
std::vector<Toolpath> gyroid_fill(const Polygons& region, double period, double z, double width);

// The period for a density (0 to 1) with beads of this width: at fixed z the
// curves are about 2.61 / period long per unit area (measured numerically,
// averaged over z), and the material is that length times the width.
inline constexpr double kGyroidLengthPerArea = 2.61;
inline double gyroid_period(double width, double density) { return kGyroidLengthPerArea * width / density; }

} // namespace tisma
