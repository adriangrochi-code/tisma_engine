#pragma once

#include "config/settings.hpp"
#include "slice/layers.hpp"

#include <array>
#include <cstddef>
#include <vector>

// Combining the sparse infill of several layers into one thicker layer
// (docs/INFILL.md; docs/spec/features/infill_patterns.md, rules 18 to 20):
// the same infill in fewer, thicker passes, for speed. Own design; see
// PROVENANCE.md.
namespace tisma {

// The tallest combined infill layer: infill.combine_max_height (mm, or a
// percentage of the nozzle), never past the nozzle diameter (rule 19). The
// nozzle is the one laying the sparse infill (0: machine.nozzle_diameter).
double combine_max_height(const Settings& settings, double nozzle = 0.0);

// Runs of layers [first, last] whose sparse infill prints once, at the last
// one, with their height together: from layer 1 up, `count` at a time (with
// combine_auto, as many as fit in max_height), each run no taller than
// max_height. Runs of one layer are left out.
std::vector<std::array<std::size_t, 2>> combine_runs(const std::vector<Layer>& layers, const Settings& settings,
                                                     double nozzle = 0.0);

} // namespace tisma
