#pragma once

#include "config/settings.hpp"
#include "mesh/indexed_mesh.hpp"

#include <string>

namespace tisma {

struct SlicedModel;

// Why a model cannot be printed on the machine, or an empty string
// (docs/spec/BEHAVIOR.md, "Errores y avisos"). Own design:
// - no triangles;
// - a vertex outside the bed polygon in XY, when machine.bed is set (on its
//   edge counts as inside; this is exact for convex beds);
// - taller than machine.max_height, when that is set.
std::string unprintable_reason(const IndexedMesh& mesh, const Settings& settings);

// The same after slicing: no layer encloses any area (a flat or open mesh).
std::string unprintable_reason(const SlicedModel& model);

} // namespace tisma
