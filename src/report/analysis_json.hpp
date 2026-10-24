#pragma once

#include "fea/analysis.hpp"

#include <string>

namespace tisma {

// The analysis report as JSON (docs/ANALYSIS.md, "Informe").
std::string analysis_json(const AnalysisReport& report, const AnalysisCase& c);

} // namespace tisma
