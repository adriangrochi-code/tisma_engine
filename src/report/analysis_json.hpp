#pragma once

#include "fea/analysis.hpp"

#include <string>

namespace tisma {

// The analysis report as JSON (docs/ANALYSIS.md, "Informe").
std::string analysis_json(const AnalysisReport& report, const AnalysisCase& c);

// An estimate as JSON (docs/ANALYSIS.md, "Estimación").
std::string analysis_estimate_json(const AnalysisEstimate& estimate, const AnalysisCase& c);

} // namespace tisma
