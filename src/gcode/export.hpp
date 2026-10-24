#pragma once

#include "gcode/check.hpp"
#include "report/warnings.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace tisma {

struct SlicedModel;
struct Settings;

struct GcodeExport {
    bool written = false; // the file exists under the requested name and passed the check
    std::string error;    // why not, in plain words
    GcodeCheck check;     // what the check found (empty when the file could not even be written)
    std::size_t bytes = 0;
    double write_seconds = 0.0, check_seconds = 0.0;
};

// Writes the G-code so that a file under the requested name is always whole
// and correct (docs/GCODE_CHECK.md):
// 1. the G-code goes to "<path>.part";
// 2. that file is read back from the disk and checked (check_gcode, with
//    gcode_limits of the model and the settings);
// 3. if it passes, it is renamed to the path; if not, to "<path>.rejected",
//    for inspection, and nothing is left under the path.
// warnings, when given, gets the writer's warnings (as write_gcode).
GcodeExport export_gcode(const std::filesystem::path& path, const SlicedModel& model, const Settings& settings,
                         std::vector<Warning>* warnings = nullptr);

} // namespace tisma
