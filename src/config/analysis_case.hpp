#pragma once

#include "fea/analysis.hpp"

#include <filesystem>
#include <string_view>

namespace tisma {

// Parses an analysis case (docs/ANALYSIS.md): the material, the model and its
// voxels, the fixtures, the loads and the solver. Throws SettingsError naming
// the key on bad input.
AnalysisCase parse_analysis_case(std::string_view json);

// Reads and parses a case file. Throws SettingsError.
AnalysisCase load_analysis_case(const std::filesystem::path& path);

} // namespace tisma
