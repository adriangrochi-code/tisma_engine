#pragma once

#include "fea/analysis.hpp"

#include <string>
#include <string_view>

namespace tisma {

// The analysis report as JSON (docs/ANALYSIS.md, "Informe"). `frame` says
// where its points are: "model" (the model file's coordinates) or "bed".
std::string analysis_json(const AnalysisReport& report, const AnalysisCase& c, std::string_view frame = "model");

// An estimate as JSON (docs/ANALYSIS.md, "Estimación").
std::string analysis_estimate_json(const AnalysisEstimate& estimate, const AnalysisCase& c);

} // namespace tisma
