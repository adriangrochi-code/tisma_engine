// Own design; see PROVENANCE.md.

#include "cli/node.hpp"

#include "cli/cli.hpp"
#include "calibration/calibration.hpp"
#include "aero/aero.hpp"
#include "config/settings.hpp"
#include "fea/analysis.hpp"
#include "fea/verdict.hpp"
#include "mesh/indexed_mesh.hpp"
#include "project/engineering_case.hpp"
#include "gcode/binary.hpp"
#include "gcode/output.hpp"
#include "io/paths.hpp"
#include "project/project.hpp"
#include "ui/project_slice.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <charconv>
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
        if (binary_output(settings))
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
    nlohmann::json summary{{"print_time", duration_text(seconds)},
                           {"print_time_s", std::llround(seconds)},
                           {"filament_mm", std::round(r.check.stats.filament * 100.0) / 100.0},
                           {"layers", r.model ? r.model->layers.size() : r.check.stats.layers}};
    // The grams only with the filament's density: an unknown one (0) gives
    // no weight rather than a wrong 0 g (the spec team's note, 2026-11-02).
    if (settings.filament_density > 0.0)
        summary["filament_g"] = std::round((totals ? totals->grams : 0.0) * 100.0) / 100.0;
    if (!write_summary(summary))
        return kExitInput;
    progress(100);
    return kExitOk;
}

bool node_analyze_wanted(std::span<const std::string_view> args) {
    for (const std::string_view a : args)
        if (a == "--summary" || a == "--sandbox" || a == "--mode" || a == "--object" || a == "--instance")
            return true;
    return false;
}

int cmd_analyze_project(std::span<const std::string_view> args, std::ostream& out, std::ostream& err) {
    NodeArgs a;
    std::string usage, mode = "analyze", quality;
    long object = -1, instance = -1;
    bool solid = false, no_zones = false, ribs = false;
    const auto whole = [&](std::string_view text, long& into) {
        long v = -1;
        const auto [end, ec] = std::from_chars(text.data(), text.data() + text.size(), v);
        if (ec != std::errc() || end != text.data() + text.size() || v < 0)
            return false;
        into = v;
        return true;
    };
    for (std::size_t i = 0; i < args.size(); ++i) {
        const std::string_view arg = args[i];
        const bool has_value = i + 1 < args.size();
        if ((arg == "-o" || arg == "--output") && has_value) {
            a.output = std::string(args[++i]);
        } else if (arg == "--summary" && has_value) {
            a.summary = std::string(args[++i]);
        } else if (arg == "--mode" && has_value) {
            mode = std::string(args[++i]);
        } else if (arg == "--quality" && has_value) {
            quality = std::string(args[++i]);
        } else if ((arg == "--object" || arg == "--instance") && has_value) {
            if (!whole(args[++i], arg == "--object" ? object : instance) && usage.empty())
                usage = "error: analyze: " + std::string(arg) + " needs a whole number from 0";
        } else if (arg == "--sandbox") {
            a.sandbox = true;
        } else if (arg == "--no-gpu") {
            a.no_gpu = true;
        } else if (arg == "--solid") {
            solid = true;
        } else if (arg == "--no-zones") {
            no_zones = true;
        } else if (arg == "--ribs") {
            ribs = true;
        } else if (!arg.starts_with("-") && a.project.empty()) {
            a.project = std::string(arg);
        } else if (usage.empty()) {
            usage = "error: analyze of a project: unexpected argument '" + std::string(arg) + "'";
        }
    }
    if (usage.empty() && (a.project.empty() || a.output.empty() || a.summary.empty()))
        usage = "error: analyze of a project needs the project, -o and --summary";
    if (usage.empty() && mode != "analyze" && mode != "lightest-infill" && mode != "resin-structure")
        usage = "error: analyze: --mode is analyze, lightest-infill or resin-structure";
    if (usage.empty() && !quality.empty() && quality != "fast" && quality != "normal" && quality != "high" && quality != "ultra")
        usage = "error: analyze: --quality is fast, normal, high or ultra";

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
    if (same_file(a.output, a.project) || same_file(a.summary, a.project))
        return fail(kExitInput, "an output would overwrite the project");
    if (a.output == a.summary || same_file(a.output, a.summary))
        return fail(kExitInput, "-o and --summary name the same file");
    for (const std::string* path : {&a.output, &a.summary})
        if (std::string problem = output_problem(write_target(*path)); !problem.empty())
            return fail(kExitInput, problem);
    if (mode != "analyze")
        return fail(kExitNotImplemented, "this analysis (" + mode + ") does not run in the cloud yet");

    Progress progress(out);
    progress(0);
    const std::optional<std::string> data = read_whole(a.project);
    if (!data)
        return fail(kExitInput, "cannot read the project '" + a.project + "'");
    if (!looks_like_project(*data))
        return fail(kExitInput, "'" + a.project + "' is not a project saved by Tisma");
    Project project;
    try {
        project = read_project(*data);
    } catch (const std::exception& e) {
        return fail(kExitInput, a.project + ": " + e.what());
    }
    if (project.models.empty())
        return fail(kExitModel, "the project has no models");
    const std::size_t index = object < 0 ? 0 : std::size_t(object);
    if (index >= project.models.size())
        return fail(kExitInput, "the project has no object " + std::to_string(index) + " (it has " +
                                    std::to_string(project.models.size()) + ", counted from 0)");
    if (instance > 0)
        return fail(kExitInput, "object " + std::to_string(index) + " has no instance " + std::to_string(instance) +
                                    " (each object has one, 0)");
    const ProjectModel& model = project.models[index];
    Settings settings = std::move(project.settings);
    for (const std::string& key : fill_missing_speeds(settings))
        err << "warning: the printer's profile gives no speed for " << key << ": a stock Ender-3's is used\n";
    if (a.no_gpu)
        settings.gpu = GpuMode::Off;
    const std::string fingerprint = engineering_fingerprint(model.mesh, model.engineering, settings_json(settings));
    std::vector<std::string> notes;
    apply_engineering_overrides(settings, model.engineering, notes);
    try {
        check_settings(settings);
    } catch (const SettingsError& e) {
        return fail(kExitInput, e.what());
    }
    progress(5);

    // The part where it is on the bed, its conditions moved with it, and
    // the way back for the report's points.
    const PlacedPart part = placed_part(model);
    ProjectEngineering placed = part.engineering;
    if (!quality.empty())
        placed.structural.quality = quality == "ultra" ? "high" : quality;
    if (quality == "ultra")
        notes.push_back("the quality 'ultra' runs as 'high' (the finest the engine has)");
    AnalysisCase c = engineering_analysis_case(placed, part.mesh, {}, notes, settings.filament_type, settings.layer_height);
    if (solid)
        c.model = AnalysisModel::Solid;
    if (no_zones || ribs)
        notes.push_back("--no-zones and --ribs change the optimizing modes, not 'analyze'");
    for (const std::string& note : notes)
        err << "warning: " << note << "\n";
    if (c.fixtures.empty() || c.loads.empty())
        return fail(kExitInput, model.name + ": the analysis needs a fixed point or zone and a force (mark them in Engineering)");
    const IndexedMesh mesh = weld_exact(part.mesh);
    progress(10);
    AnalysisReport report;
    try {
        report = run_analysis(mesh, settings, c, nullptr, [&progress](std::string_view, double done) {
            progress(10 + int(std::clamp(done, 0.0, 1.0) * 80.0));
        });
    } catch (const AnalysisError& e) {
        return fail(kExitAnalysis, e.what());
    }
    if (!report.valid)
        return fail(kExitAnalysis, "the analysis gave no valid result");
    progress(92);
    const std::string report_json = structural_report_json(report, c, placed, part.to_part, &part.mesh);
    const nlohmann::json result{{"format", "tisma-engineering-result"},
                                {"version", 1},
                                {"kind", "structural"},
                                {"object", index},
                                {"instance", 0},
                                {"part", model.name},
                                {"fingerprint", fingerprint},
                                {"report", nlohmann::json::parse(report_json)}};
    if (!write_file(a.output, [&](std::ostream& f) { f << result.dump() << "\n"; }))
        return fail(kExitInput, "cannot write the result '" + a.output + "'");
    const nlohmann::json& v = result["report"]["verdict"];
    const auto finite = [](double x) { return std::isfinite(x) ? nlohmann::json(x) : nlohmann::json(nullptr); };
    nlohmann::json summary{{"verdict", v.value("name", "")},
                           {"min_safety", finite(report.result.min_safety)},
                           {"required_safety", v.value("required_safety", 0.0)},
                           {"max_displacement_mm", finite(report.result.elasticity.max_displacement)},
                           {"mass_g", std::round(v.value("mass_g", 0.0) * 100.0) / 100.0},
                           {"family", v.value("family", "")},
                           {"voxels", report.grid.count()}};
    if (!write_summary(summary))
        return kExitInput;
    progress(100);
    return kExitOk;
}

int cmd_aero_project(std::span<const std::string_view> args, std::ostream& out, std::ostream& err) {
    NodeArgs a;
    std::string usage, quality = "normal", direction_text = "x";
    long object = -1, instance = -1;
    double speed = 10.0;
    bool speed_given = false;
    for (std::size_t i = 0; i < args.size(); ++i) {
        const std::string_view arg = args[i];
        const bool has_value = i + 1 < args.size();
        if ((arg == "-o" || arg == "--output") && has_value) {
            a.output = std::string(args[++i]);
        } else if (arg == "--summary" && has_value) {
            a.summary = std::string(args[++i]);
        } else if (arg == "--quality" && has_value) {
            quality = std::string(args[++i]);
        } else if (arg == "--direction" && has_value) {
            direction_text = std::string(args[++i]);
        } else if (arg == "--speed" && has_value) {
            const std::string_view v = args[++i];
            const auto [end, ec] = std::from_chars(v.data(), v.data() + v.size(), speed);
            speed_given = ec == std::errc() && end == v.data() + v.size();
            if (!speed_given && usage.empty())
                usage = "error: aero: --speed needs a number of m/s";
        } else if ((arg == "--object" || arg == "--instance") && has_value) {
            const std::string_view v = args[++i];
            long n = -1;
            const auto [end, ec] = std::from_chars(v.data(), v.data() + v.size(), n);
            if (ec != std::errc() || end != v.data() + v.size() || n < 0) {
                if (usage.empty())
                    usage = "error: aero: " + std::string(arg) + " needs a whole number from 0";
            } else {
                (arg == "--object" ? object : instance) = n;
            }
        } else if (arg == "--sandbox") {
            a.sandbox = true;
        } else if (arg == "--no-gpu") {
            a.no_gpu = true;
        } else if (!arg.starts_with("-") && a.project.empty()) {
            a.project = std::string(arg);
        } else if (usage.empty()) {
            usage = "error: aero: unexpected argument '" + std::string(arg) + "'";
        }
    }
    if (usage.empty() && (a.project.empty() || a.output.empty() || a.summary.empty()))
        usage = "error: aero needs the project, -o and --summary";
    aero::AeroQuality q = aero::AeroQuality::Normal;
    if (usage.empty() && !aero::quality_named(quality, q))
        usage = "error: aero: --quality is friction, fast, normal or high";
    if (usage.empty() && (!(speed > 0.0) || speed > 100.0))
        usage = "error: aero: --speed is more than 0 and up to 100 m/s";
    // The direction: an axis with its sign, or three numbers.
    std::array<double, 3> direction{1, 0, 0};
    if (usage.empty()) {
        static const std::pair<const char*, std::array<double, 3>> axes[] = {
            {"x", {1, 0, 0}}, {"+x", {1, 0, 0}}, {"-x", {-1, 0, 0}}, {"y", {0, 1, 0}},  {"+y", {0, 1, 0}},
            {"-y", {0, -1, 0}}, {"z", {0, 0, 1}}, {"+z", {0, 0, 1}}, {"-z", {0, 0, -1}}};
        bool found = false;
        for (const auto& [name, v] : axes)
            if (direction_text == name)
                direction = v, found = true;
        if (!found) {
            double v[3];
            const char* p = direction_text.c_str();
            const char* end = p + direction_text.size();
            int k = 0;
            for (; k < 3; ++k) {
                const auto [next, ec] = std::from_chars(p, end, v[k]);
                if (ec != std::errc())
                    break;
                p = next;
                if (k < 2) {
                    if (p == end || *p != ',')
                        break;
                    ++p;
                }
            }
            if (k == 3 && p == end && std::isfinite(v[0]) && std::isfinite(v[1]) && std::isfinite(v[2]) &&
                std::hypot(v[0], v[1], v[2]) > 0.0)
                direction = {v[0], v[1], v[2]};
            else
                usage = "error: aero: --direction is x, -x, y, -y, z, -z or three numbers x,y,z";
        }
    }

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
    if (same_file(a.output, a.project) || same_file(a.summary, a.project))
        return fail(kExitInput, "an output would overwrite the project");
    if (a.output == a.summary || same_file(a.output, a.summary))
        return fail(kExitInput, "-o and --summary name the same file");
    for (const std::string* path : {&a.output, &a.summary})
        if (std::string problem = output_problem(write_target(*path)); !problem.empty())
            return fail(kExitInput, problem);

    Progress progress(out);
    progress(0);
    const std::optional<std::string> data = read_whole(a.project);
    if (!data)
        return fail(kExitInput, "cannot read the project '" + a.project + "'");
    if (!looks_like_project(*data))
        return fail(kExitInput, "'" + a.project + "' is not a project saved by Tisma");
    Project project;
    try {
        project = read_project(*data);
    } catch (const std::exception& e) {
        return fail(kExitInput, a.project + ": " + e.what());
    }
    if (project.models.empty())
        return fail(kExitModel, "the project has no models");
    const std::size_t index = object < 0 ? 0 : std::size_t(object);
    if (index >= project.models.size())
        return fail(kExitInput, "the project has no object " + std::to_string(index) + " (it has " +
                                    std::to_string(project.models.size()) + ", counted from 0)");
    if (instance > 0)
        return fail(kExitInput, "object " + std::to_string(index) + " has no instance " + std::to_string(instance) +
                                    " (each object has one, 0)");
    const ProjectModel& model = project.models[index];
    Settings settings = std::move(project.settings);
    std::vector<std::string> notes;
    apply_engineering_overrides(settings, model.engineering, notes);
    progress(5);

    aero::AeroInput in;
    in.mesh = placed_part(model).mesh;
    in.direction = direction;
    in.speed = speed;
    in.quality = q;
    in.layer_height = settings.layer_height > 0.0 ? settings.layer_height : 0.2;
    const aero::AeroResult r = aero::analyze(in, [&progress](double done) {
        progress(5 + int(std::clamp(done, 0.0, 1.0) * 85.0));
        return true;
    });
    if (!r.ok)
        return fail(kExitAnalysis, r.error);
    progress(92);
    nlohmann::json result = nlohmann::json::parse(aero::result_json(r, in));
    result["object"] = index;
    result["instance"] = 0;
    result["part"] = model.name;
    if (!write_file(a.output, [&](std::ostream& f) { f << result.dump() << "\n"; }))
        return fail(kExitInput, "cannot write the result '" + a.output + "'");
    const auto finite = [](double x) { return std::isfinite(x) ? nlohmann::json(x) : nlohmann::json(nullptr); };
    const nlohmann::json summary{{"drag_n", finite(r.drag_n)},
                                 {"cd", finite(r.cd)},
                                 {"cd_pressure", finite(r.cd_pressure)},
                                 {"cd_friction", finite(r.cd_friction)},
                                 {"reynolds", finite(r.reynolds)},
                                 {"reynolds_simulated", finite(r.reynolds_simulated)}};
    if (!write_summary(summary))
        return kExitInput;
    progress(100);
    return kExitOk;
}

PlacedPart placed_part(const ProjectModel& m) {
    PlacedPart p;
    const auto& t = m.turn;
    const auto place = [&](const Vec3f& v) {
        return Vec3f{float(t[0] * v.x + t[1] * v.y + t[2] * v.z + m.offset[0]),
                     float(t[3] * v.x + t[4] * v.y + t[5] * v.z + m.offset[1]),
                     float(t[6] * v.x + t[7] * v.y + t[8] * v.z + m.offset[2])};
    };
    const double det = t[0] * (t[4] * t[8] - t[5] * t[7]) - t[1] * (t[3] * t[8] - t[5] * t[6]) + t[2] * (t[3] * t[7] - t[4] * t[6]);
    for (std::size_t k = 0; k < m.mesh.triangle_count(); ++k) // a mirror keeps its faces outward
        if (det < 0.0)
            p.mesh.add_triangle(place(m.mesh.corner(k, 0)), place(m.mesh.corner(k, 2)), place(m.mesh.corner(k, 1)));
        else
            p.mesh.add_triangle(place(m.mesh.corner(k, 0)), place(m.mesh.corner(k, 1)), place(m.mesh.corner(k, 2)));
    p.engineering = m.engineering;
    move_engineering(p.engineering, t, m.offset);
    const std::array<double, 9> inv{(t[4] * t[8] - t[5] * t[7]) / det, (t[2] * t[7] - t[1] * t[8]) / det,
                                    (t[1] * t[5] - t[2] * t[4]) / det, (t[5] * t[6] - t[3] * t[8]) / det,
                                    (t[0] * t[8] - t[2] * t[6]) / det, (t[2] * t[3] - t[0] * t[5]) / det,
                                    (t[3] * t[7] - t[4] * t[6]) / det, (t[1] * t[6] - t[0] * t[7]) / det,
                                    (t[0] * t[4] - t[1] * t[3]) / det};
    for (std::size_t i = 0; i < 9; ++i)
        p.to_part[i] = inv[i];
    for (std::size_t i = 0; i < 3; ++i)
        p.to_part[9 + i] = -(inv[3 * i] * m.offset[0] + inv[3 * i + 1] * m.offset[1] + inv[3 * i + 2] * m.offset[2]);
    return p;
}

} // namespace tisma
