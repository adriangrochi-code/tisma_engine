#include "cli/calibration_cli.hpp"

#include "calibration/calibration.hpp"
#include "cli/cli.hpp"
#include "config/settings.hpp"
#include "io/stl.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <charconv>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>

namespace tisma {

namespace {

constexpr std::string_view kCalibrationUsage =
    "usage:\n"
    "  tisma-engine calibration <kind> --config <settings.json> [--start <v> --end <v> --step <v> --segment <mm>]\n"
    "                           --model <out.stl> --settings <out.json>\n"
    "      the test part and the settings with its height changes, ready for slice; without a range, the one the\n"
    "      settings suggest. The segments (heights and values) as JSON on the output\n"
    "  tisma-engine calibration apply <kind> --value <v> --config <settings.json> -o <out.json>\n"
    "      the value that printed best written into the settings, the tower's changes removed\n"
    "kinds: dimensions, temperature, retraction, flow, speed, retraction_speed, pressure_advance, first_layer, fit,\n"
    "       max_flow, acceleration, elephant_foot, resin_exposure, resin_measure\n";

int usage(std::ostream& err, const std::string& problem) {
    err << "error: " << problem << "\n" << kCalibrationUsage;
    return kExitUsage;
}

bool number(const std::string& text, double& v) {
    const auto [end, ec] = std::from_chars(text.data(), text.data() + text.size(), v);
    return ec == std::errc() && end == text.data() + text.size();
}

bool write_text(const std::string& path, const std::string& text) {
    std::ofstream f(std::filesystem::path(path), std::ios::binary | std::ios::trunc);
    f.write(text.data(), std::streamsize(text.size()));
    return bool(f);
}

} // namespace

int cmd_calibration(std::span<const std::string_view> args, std::ostream& out, std::ostream& err) {
    if (args.empty())
        return usage(err, "calibration needs a kind");
    const bool apply = args[0] == "apply";
    if (apply)
        args = args.subspan(1);
    if (args.empty())
        return usage(err, "calibration needs a kind");
    const std::optional<CalibrationKind> kind = calibration_kind_from_name(args[0]);
    if (!kind)
        return usage(err, "unknown calibration '" + std::string(args[0]) + "'");
    std::map<std::string, std::string> o;
    for (std::size_t i = 1; i < args.size(); ++i) {
        const std::string a(args[i]);
        static const char* known[] = {"--config", "--start", "--end", "--step", "--segment", "--model", "--settings", "--value", "-o"};
        if (std::find(std::begin(known), std::end(known), a) == std::end(known) || i + 1 >= args.size())
            return usage(err, "calibration: unexpected argument '" + a + "'");
        o[a] = std::string(args[++i]);
    }
    if (!o.count("--config"))
        return usage(err, "calibration needs --config");
    Settings settings;
    try {
        settings = load_settings(o["--config"]);
    } catch (const std::exception& e) {
        err << "error: " << e.what() << "\n";
        return kExitInput;
    }

    if (apply) {
        double value = 0.0;
        if (!o.count("--value") || !number(o["--value"], value) || !o.count("-o"))
            return usage(err, "calibration apply needs --value <number> and -o");
        if (!apply_calibration(*kind, value, settings)) {
            err << "error: the " << calibration_kind_name(*kind) << " test sets no value of the settings\n";
            return kExitInput;
        }
        if (!write_text(o["-o"], settings_json(settings))) {
            err << "error: cannot write " << o["-o"] << "\n";
            return kExitInput;
        }
        return kExitOk;
    }

    if (!o.count("--model") || !o.count("--settings"))
        return usage(err, "calibration needs --model and --settings");
    CalibrationPrint print;
    const bool ranged = o.count("--start") || o.count("--end") || o.count("--step");
    if (ranged) {
        double start = 0, end = 0, step = 0, segment = *kind == CalibrationKind::ResinExposure ? 2.0 : 5.0;
        if (!is_tower(*kind))
            return usage(err, std::string("the ") + calibration_kind_name(*kind) + " test takes no range");
        if (!number(o["--start"], start) || !number(o["--end"], end) || !number(o["--step"], step) || !(step > 0) ||
            (o.count("--segment") && (!number(o["--segment"], segment) || !(segment > 0))))
            return usage(err, "calibration: --start, --end and --step need numbers (--step and --segment above 0)");
        print = calibration_tower(*kind, start, end, step, segment);
    } else {
        print = automatic_calibration(*kind, settings);
    }
    if (print.mesh.triangle_count() == 0) {
        err << "error: no " << calibration_kind_name(*kind) << " part for these settings\n";
        return kExitInput;
    }
    settings.changes = print.changes;
    if (!write_text(o["--model"], stl_binary(print.mesh, "tisma calibration " + print.kind)) ||
        !write_text(o["--settings"], settings_json(settings))) {
        err << "error: cannot write the part or the settings\n";
        return kExitInput;
    }
    nlohmann::json segments = nlohmann::json::array();
    for (const CalibrationSegment& s : print.segments)
        segments.push_back({{"z0", s.z0}, {"z1", s.z1}, {"value", s.value}});
    out << nlohmann::json({{"kind", print.kind}, {"unit", print.unit}, {"segments", segments},
                           {"triangles", print.mesh.triangle_count()}})
               .dump(1)
        << "\n";
    return kExitOk;
}

} // namespace tisma
