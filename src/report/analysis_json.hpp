#pragma once

#include "fea/analysis.hpp"

#include <string>
#include <array>

namespace tisma {

// The analysis report as JSON (docs/ANALYSIS.md, "Informe"). Its points are
// in the part's own coordinates (structural_analysis.md; the frame a project
// keeps its conditions in): the analysis' bed coordinates less `part_shift`,
// where the part's own box corner went on the bed (its lowest point on it).
// The critical point is also given on the bed, as "weakest_on_bed".
std::string analysis_json(const AnalysisReport& report, const AnalysisCase& c,
                          const std::array<double, 3>& part_shift = {});

// An estimate as JSON (docs/ANALYSIS.md, "Estimación").
std::string analysis_estimate_json(const AnalysisEstimate& estimate, const AnalysisCase& c);

} // namespace tisma
