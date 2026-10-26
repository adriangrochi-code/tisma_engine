#pragma once

#include "config/settings.hpp"
#include "geom/polygons.hpp"
#include "perimeters/sites.hpp"

#include <cstdint>
#include <vector>

// Fuzzy skin (docs/spec/features/fuzzy_skin.md): the outer wall redrawn
// through points at random distances along it, each moved at random off the
// line, so the surface comes out rough. Own design; see PROVENANCE.md.
namespace tisma {

struct FuzzyParams {
    FuzzyMode mode = FuzzyMode::None;
    double thickness = 0.3;      // mm: how far a point may move off the line, either way
    double point_distance = 0.8; // mm: the mean distance between the new points
};

// Roughens the closed outer walls (bead index 0) of one layer: in External
// mode those around the region's outer contours, in All mode also those
// around its holes, and where `painted` is given, the stretches of any outer
// wall inside it, whatever the mode (rule 4). Inner walls never change. The
// points are spread from `seed`: the same layer gives the same G-code.
void fuzz_walls(std::vector<Toolpath>& walls, const Polygons& region, const FuzzyParams& params,
                const Polygons* painted, std::uint64_t seed);

} // namespace tisma
