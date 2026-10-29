#pragma once

#include "perimeters/walls.hpp"
#include "surfaces/surfaces.hpp"

#include <vector>

namespace tisma {

// Bridges (docs/spec/BEHAVIOR.md, "Puentes"): the part of a layer's bottom
// surface with nothing printed under it. What is printed in the layer below is
// measured from what is really laid there: the area of its walls' beads, its
// solid surfaces, and its sparse infill region when sparse infill is printed
// (a surface resting on the infill below is not a bridge). Parts narrower than
// a bead are left out. A bridge is grown by two beads into the rest of the
// bottom surface to anchor its lines, and moved from `bottom` to `bridge`.
// Its angle is the one, in steps of kBridgeAngleStep, whose lines have the most
// length with both ends anchored on what is printed below (within a bead);
// among equals, the one with the shortest lines. The first layer has none.
// With sparse infill printed, what is printed below covers its region but for
// slits narrower than a bead, so a bottom surface inside the region below
// (regions, one per layer) is no bridge and the bead area is not computed.
inline constexpr double kBridgeAngleStep = 5.0; // degrees
// With `others` (one per layer: what other extruders' parts lay there,
// multi_extruder.md), that counts as printed below too.
void find_bridges(std::vector<LayerSurfaces>& surfaces, const std::vector<Polygons>& regions,
                  const std::vector<LayerWalls>& walls, bool sparse_printed, double width,
                  const std::vector<Polygons>* others = nullptr);

} // namespace tisma
