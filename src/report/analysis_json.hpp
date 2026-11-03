#pragma once

#include "fea/analysis.hpp"

#include <string>
#include <array>

namespace tisma {

// The analysis report as JSON (docs/ANALYSIS.md, "Informe"). Its points are
// in the part's own coordinates (structural_analysis.md; the frame a project
// keeps its conditions in): `to_part` takes a point of the analysis (on the
// bed) there, its linear part by rows then its translation. The critical
// point is also given on the bed, as "weakest_on_bed".
std::string analysis_json(const AnalysisReport& report, const AnalysisCase& c,
                          const std::array<double, 12>& to_part = {1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0});

// An estimate as JSON (docs/ANALYSIS.md, "Estimación").
std::string analysis_estimate_json(const AnalysisEstimate& estimate, const AnalysisCase& c);

} // namespace tisma
