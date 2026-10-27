// The output file's name and the post-processing scripts
// (docs/spec/features/gcode_templates.md, rules 15 to 17). Own design; see
// PROVENANCE.md.

#include "gcode/output.hpp"

#include "config/settings.hpp"
#include "gcode/preview.hpp"
#include "gcode/template.hpp"
#include "gcode/writer.hpp"
#include "slice/pipeline.hpp"

#include <cctype>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <sstream>

namespace tisma {

namespace {

// "11m", "1h7m", "2d3h0m": the time as a file name takes it.
std::string print_time_text(double seconds) {
    const long s = std::lround(std::max(seconds, 0.0));
    const long d = s / 86400, h = s / 3600 % 24, m = s / 60 % 60;
    if (d > 0)
        return std::to_string(d) + "d" + std::to_string(h) + "h" + std::to_string(m) + "m";
    if (h > 0)
        return std::to_string(h) + "h" + std::to_string(m) + "m";
    return std::to_string(m) + "m";
}

// A script's path or the G-code's as one argument of the shell.
std::string quoted(const std::string& text) {
#ifdef _WIN32
    return "\"" + text + "\"";
#else
    std::string out = "'";
    for (const char c : text)
        out += c == '\'' ? std::string("'\\''") : std::string(1, c);
    return out + "'";
#endif
}

void set_environment(const std::string& name, const std::string& value) {
#ifdef _WIN32
    _putenv_s(name.c_str(), value.c_str());
#else
    setenv(name.c_str(), value.c_str(), 1);
#endif
}

} // namespace

std::optional<GcodeTotals> read_gcode_totals(const std::filesystem::path& gcode) {
    std::ifstream in(gcode, std::ios::binary);
    if (!in)
        return std::nullopt;
    // The summary closes the file, before the settings: the last megabyte holds it.
    in.seekg(0, std::ios::end);
    const std::streamoff size = in.tellg();
    const std::streamoff tail = std::min<std::streamoff>(size, 1 << 20);
    in.seekg(size - tail);
    std::string text(static_cast<std::size_t>(tail), '\0');
    in.read(text.data(), tail);
    std::optional<GcodeTotals> out;
    std::istringstream lines(text);
    double grams = 0.0;
    for (std::string line; std::getline(lines, line);) {
        constexpr std::string_view time = "; estimated printing time = ", weight = "; total filament used [g] = ";
        if (line.starts_with(time))
            out = GcodeTotals{parse_duration(std::string_view(line).substr(time.size())), 0.0};
        else if (line.starts_with(weight))
            grams = std::atof(line.c_str() + weight.size());
    }
    if (out)
        out->grams = grams;
    return out;
}

std::string output_filename(const Settings& s, const SlicedModel& model, const std::optional<GcodeTotals>& totals) {
    TemplateContext c = template_context(s, model);
    c.variables["fill_density"] = TemplateValue::of(template_text(TemplateValue::of(s.infill_density)) + "%");
    c.variables["default_output_extension"] = TemplateValue::of(std::string(".gcode"));
    c.variables["print_time"] = TemplateValue::of(totals ? print_time_text(totals->print_seconds) : std::string("{print_time}"));
    c.variables["total_weight"] =
        totals ? TemplateValue::of(std::round(totals->grams * 100.0) / 100.0) : TemplateValue::of(std::string("{total_weight}"));
    return render_template(s.filename_format, c, "output.filename_format");
}

std::string run_post_process(const Settings& s, const SlicedModel& model, const std::filesystem::path& gcode) {
    if (s.post_process.empty())
        return {};
    // What each script reads (rule 16): the G-code's name, where it goes, and
    // every variable of the templates.
    set_environment("SLIC3R_PP_OUTPUT_NAME", gcode.string());
    set_environment("SLIC3R_PP_HOST", "File");
    for (const auto& [name, value] : template_context(s, model).variables) {
        std::string variable = "SLIC3R_";
        for (const char c : name)
            variable += c == '.' ? '_' : char(std::toupper(static_cast<unsigned char>(c)));
        set_environment(variable, template_text(value));
    }
    std::string errors;
    for (const std::string& script : s.post_process) {
        std::string command = quoted(script) + " " + quoted(gcode.string());
#ifdef _WIN32
        command = "\"" + command + "\""; // cmd.exe drops the outer quotes
#endif
        if (const int status = std::system(command.c_str()); status != 0)
            errors += script + ": exited with " + std::to_string(status) + "\n";
    }
    return errors;
}

} // namespace tisma
