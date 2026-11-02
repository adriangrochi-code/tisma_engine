// Own design; see PROVENANCE.md.

#include "cli/node.hpp"

#include "cli/cli.hpp"
#include "calibration/calibration.hpp"
#include "config/settings.hpp"
#include "gcode/binary.hpp"
#include "gcode/output.hpp"
#include "io/paths.hpp"
#include "project/project.hpp"
#include "ui/project_slice.hpp"

#include <nlohmann/json.hpp>

#include <cmath>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <optional>
#include <ostream>
#include <string>
#include <vector>

namespace tisma {

namespace {

struct NodeArgs {
    std::string project, output, summary;
    bool sandbox = false, no_gpu = false, yes = false;
};

// The first argument that is not an option or an option's value.
std::string first_input(std::span<const std::string_view> args) {
    for (std::size_t i = 0; i < args.size(); ++i) {
        const std::string_view a = args[i];
        if (a == "-o" || a == "--output" || a == "--summary" || a == "--config" || a == "-c" || a == "--warnings" ||
            a == "--case" || a == "--copies") {
            ++i;
            continue;
        }
        if (!a.starts_with("-"))
            return std::string(a);
    }
    return {};
}

std::optional<std::string> read_whole(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in)
        return std::nullopt;
    std::string data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (in.bad())
        return std::nullopt;
    return data;
}


// "1h 2m 3s", "4m 5s", "6s".
std::string duration_text(double seconds) {
    const long long total = std::llround(std::max(0.0, seconds));
    const long long h = total / 3600, m = total / 60 % 60, s = total % 60;
    std::string out;
    if (h > 0)
        out += std::to_string(h) + "h ";
    if (h > 0 || m > 0)
        out += std::to_string(m) + "m ";
    return out + std::to_string(s) + "s";
}

// The progress, one line each time it changes.
class Progress {
public:
    explicit Progress(std::ostream& out) : out_(out) {}
    void operator()(int percent) {
        if (percent <= last_)
            return;
        last_ = percent;
        out_ << "progress " << percent << std::endl;
    }

private:
    std::ostream& out_;
    int last_ = -1;
};

} // namespace

bool node_slice_wanted(std::span<const std::string_view> args) {
    bool config = false;
    for (const std::string_view a : args) {
        if (a == "--summary" || a == "--sandbox" || a == "--no-gpu")
            return true;
        config |= a == "--config" || a == "-c";
    }
    const std::string input = first_input(args);
    return !config && !input.empty() && is_project_file(input);
}

int cmd_slice_project(std::span<const std::string_view> args, std::ostream& out, std::ostream& err) {
    NodeArgs a;
    std::string usage;
    for (std::size_t i = 0; i < args.size(); ++i) {
        const std::string_view arg = args[i];
        const bool has_value = i + 1 < args.size();
        if ((arg == "-o" || arg == "--output") && has_value) {
            a.output = std::string(args[++i]);
        } else if (arg == "--summary" && has_value) {
            a.summary = std::string(args[++i]);
        } else if (arg == "--sandbox") {
            a.sandbox = true;
        } else if (arg == "--no-gpu") {
            a.no_gpu = true;
        } else if (arg == "--yes") {
            a.yes = true;
        } else if (!arg.starts_with("-") && a.project.empty()) {
            a.project = std::string(arg);
        } else if (usage.empty()) {
            usage = "error: slice of a project: unexpected argument '" + std::string(arg) + "'";
        }
    }
    if (usage.empty() && (a.project.empty() || a.output.empty() || a.summary.empty()))
        usage = "error: slice of a project needs the project, -o and --summary";

    // The summary: the result, or what went wrong. When it cannot be
    // written, the error output says so and the exit code is not 0.
    const auto write_summary = [&](const nlohmann::json& j) {
        if (a.summary.empty())
            return false;
        if (!write_file(a.summary, [&](std::ostream& f) { f << j.dump(2) << "\n"; })) {
            err << "error: cannot write the summary '" << a.summary << "'\n";
            return false;
        }
        return true;
    };
    const auto fail = [&](int code, const std::string& message) {
        err << "error: " << message << "\n";
        write_summary(nlohmann::json{{"error", message}});
        return code;
    };
    if (!usage.empty()) {
        err << usage << "\n";
        write_summary(nlohmann::json{{"error", usage.substr(7)}});
        return kExitUsage;
    }
    // Nothing is written over the project, nor the summary over the G-code.
    if (same_file(a.output, a.project) || same_file(a.summary, a.project))
        return fail(kExitInput, "an output would overwrite the project");
    if (a.output == a.summary || same_file(a.output, a.summary))
        return fail(kExitInput, "-o and --summary name the same file");
    for (const std::string* path : {&a.output, &a.summary})
        if (std::string problem = output_problem(write_target(*path)); !problem.empty())
            return fail(kExitInput, problem);

    Progress progress(out);
    progress(0);
    // The project, and nothing else of this machine: no profiles, no
    // preferences, no catalog (rule 18).
    const std::optional<std::string> data = read_whole(a.project);
    if (!data)
        return fail(kExitInput, "cannot read the project '" + a.project + "'");
    if (!looks_like_project(*data))
        return fail(kExitInput, "'" + a.project + "' is not a project saved by Tisma (its own file, or a 3MF with " +
                                    "Metadata/tisma_project.json): projects of other slicers are not read yet");
    Project project;
    try {
        project = read_project(*data);
    } catch (const std::exception& e) {
        return fail(kExitInput, a.project + ": " + e.what());
    }
    if (project.models.empty())
        return fail(kExitModel, "the project has no models");
    progress(5);

    Settings settings = std::move(project.settings);
    for (const std::string& key : fill_missing_speeds(settings))
        err << "warning: the printer's profile gives no speed for " << key << ": a stock Ender-3's is used\n";
    if (a.no_gpu)
        settings.gpu = GpuMode::Off;
    try {
        check_settings(settings);
    } catch (const SettingsError& e) {
        return fail(kExitInput, e.what());
    }
    if (settings.machine_technology == MachineTechnology::Msla)
        return fail(kExitNotImplemented, "the nodes do not slice for resin printers yet");
    // A calibration prints its own model alone; the resin tests have no
    // supports, pad or hollowing (calibration_models.md, "Resina").
    if (project.calibration) {
        project.models.resize(1);
        const std::optional<CalibrationKind> kind = calibration_kind_from_name(project.calibration->kind);
        if (kind && is_resin_calibration(*kind))
            settings.resin_supports = settings.resin_pad = settings.resin_pad_around_object = settings.resin_hollow = false;
    }
    const std::vector<ui::PlateObject> objects = ui::project_objects(project.models);
    project.models.clear();
    if (settings.vase && objects.size() > 1 && !settings.sequential)
        return fail(kExitModel, "vase mode prints one object, or several one after another (sequential.enabled)");
    std::vector<TriangleMesh> placed;
    placed.reserve(objects.size());
    for (const ui::PlateObject& o : objects)
        placed.push_back(ui::placed_mesh(o));
    ui::PlateSlice plate = ui::plate_slice(objects, placed, settings);
    const TriangleMesh mesh = ui::plate_mesh(placed);
    placed.clear();
    progress(10);

    ui::Slicer slicer;
    const ui::SliceResult r =
        slicer.run(mesh, settings, nullptr, plate.painting.empty() ? nullptr : &plate.painting,
                   plate.by_object ? &plate.objects : nullptr, project.mixes.empty() ? nullptr : &project.mixes,
                   plate.volumes.empty() ? nullptr : &plate.volumes);
    if (!r.ok)
        return fail(kExitModel, r.error);
    if (!r.check.ok()) {
        std::string message = "the G-code failed its own check";
        if (!r.check.problems.empty())
            message += ": line " + std::to_string(r.check.problems.front().line) + ": " + r.check.problems.front().message;
        return fail(kExitGcode, message);
    }
    for (const Warning& w : r.warnings)
        err << "warning: " << w.message << "\n";
    progress(85);

    // Text or binary, as the profile says (gcode.binary).
    const bool written = write_file(a.output, [&](std::ostream& f) {
        if (settings.gcode_binary)
            f << binary_gcode(r.gcode, settings);
        else
            f << r.gcode;
    });
    if (!written)
        return fail(kExitInput, "cannot write the G-code '" + a.output + "'");
    progress(95);

    // The settings' post-processing scripts run anything: never in the
    // sandbox (rule 20), and otherwise only once confirmed.
    const std::filesystem::path target = write_target(a.output);
    if (!settings.post_process.empty()) {
        if (a.sandbox) {
            err << "warning: --sandbox: the settings' post-processing scripts are not run:";
            for (const std::string& script : settings.post_process)
                err << " " << script;
            err << "\n";
        } else {
            std::string answer = a.yes ? "Y" : "";
            if (!a.yes) {
                err << "A post-processing script has been detected in the settings. Continue (Y/N)? ";
                std::getline(std::cin, answer);
            }
            if (answer != "Y" && answer != "y") {
                std::error_code ignored;
                std::filesystem::remove(target, ignored);
                return fail(kExitInput, "the post-processing was not confirmed: no G-code is kept");
            }
            if (const std::string errors = run_post_process(settings, *r.model, target); !errors.empty())
                return fail(kExitInput, "post-processing: " + errors);
        }
    }

    const std::optional<GcodeTotals> totals = read_gcode_totals(target);
    const double seconds = totals ? totals->print_seconds : 0.0;
    const nlohmann::json summary{{"print_time", duration_text(seconds)},
                                 {"print_time_s", std::llround(seconds)},
                                 {"filament_mm", std::round(r.check.stats.filament * 100.0) / 100.0},
                                 {"filament_g", std::round((totals ? totals->grams : 0.0) * 100.0) / 100.0},
                                 {"layers", r.model ? r.model->layers.size() : r.check.stats.layers}};
    if (!write_summary(summary))
        return kExitInput;
    progress(100);
    return kExitOk;
}

} // namespace tisma
