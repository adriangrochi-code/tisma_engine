#pragma once

#include <filesystem>
#include <optional>
#include <string>

namespace tisma {

struct SlicedModel;
struct Settings;

// What a finished G-code says about itself in its closing summary.
struct GcodeTotals {
    double print_seconds = 0.0; // "; estimated printing time = ..."
    double grams = 0.0;         // "; total filament used [g] = ..."
};

// Reads the closing summary at the end of a G-code file; none when the file
// cannot be read or has no estimated time.
std::optional<GcodeTotals> read_gcode_totals(const std::filesystem::path& gcode);

// The output file's name (docs/spec/features/gcode_templates.md, rule 15):
// output.filename_format in the template language, with the print's
// variables (template_context), fill_density ("15%") and
// default_output_extension (".gcode", or ".bgcode" with gcode.binary, which
// also turns a name ending in .gcode into .bgcode). print_time ("11m",
// "1h7m") and total_weight (grams) are known once the G-code is written:
// without `totals` they stay as written, "{print_time}" and "{total_weight}".
// Throws TemplateError.
std::string output_filename(const Settings& settings, const SlicedModel& model, const std::optional<GcodeTotals>& totals);

// The post-processing scripts (rules 16 and 17): each one run with the
// G-code's path as its only argument and, in its environment,
// SLIC3R_PP_OUTPUT_NAME (that path), SLIC3R_PP_HOST (File) and every
// template variable as SLIC3R_<NAME> (upper case, dots as _). A script may
// change the file. Returns what went wrong, empty when every script ran and
// returned 0. Whoever calls this asks first: a script from a settings file
// runs anything.
std::string run_post_process(const Settings& settings, const SlicedModel& model, const std::filesystem::path& gcode);

} // namespace tisma
