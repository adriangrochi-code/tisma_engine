#pragma once

#include "report/warnings.hpp"

#include <vector>

namespace tisma {

struct SlicedModel;
struct Settings;

// The stability review of a part printed without supports
// (docs/spec/features/stability.md, the «Estabilidad» of print_review.md):
// - a part that starts in the air (critical);
// - what is printed tipping over its footing by its weight (a warning, or
//   critical with review.bed_adhesion);
// - a slender column bending under the nozzle's push (a warning with
//   review.nozzle_force; without it, the three most flexible columns as
//   information);
// - the part peeling off the bed (a warning, with both measured values).
// Statics and Euler-Bernoulli beams, as in any text on the mechanics of
// materials; own design otherwise.
std::vector<Warning> stability_warnings(const SlicedModel& model, const Settings& settings);

// E across the layers (MPa) for the columns: PLA's, as the spec's tests use,
// until the filament's profile carries its material.
inline constexpr double kStabilityModulusZ = 2600.0;
// g/mm3 for the weight: PLA's.
inline constexpr double kStabilityDensity = 1.24e-3;

} // namespace tisma
