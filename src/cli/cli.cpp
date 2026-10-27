#include "cli/cli.hpp"

#include "io/model.hpp"
#include "io/stl.hpp"
#include "slice/placement.hpp"
#include "io/ply.hpp"
#include "io/vtk.hpp"
#include "config/analysis_case.hpp"
#include "config/settings.hpp"
#include "fea/analysis.hpp"
#include "report/analysis_json.hpp"
#include "gcode/export.hpp"
#include "gcode/output.hpp"
#include "gcode/template.hpp"
#include "gcode/writer.hpp"
#include "report/stability.hpp"
#include "report/warnings.hpp"
#include "geom/medial_axis.hpp"
#include "geom/polygons.hpp"
#include "gpu/compute.hpp"
#include "mesh/indexed_mesh.hpp"
#include "perimeters/sites.hpp"
#include "perimeters/transitions.hpp"
#include "slice/layers.hpp"
#include "slice/pipeline.hpp"
#include "slice/printable.hpp"
#include "resin/goo.hpp"
#include "resin/resin.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <chrono>
#include <exception>
#include <filesystem>
#include <iostream>
#include <fstream>
#include <functional>
#include <memory>
#include <optional>
#include <ostream>
#include <string>

namespace tisma {
namespace {

constexpr std::string_view kUsage =
    "usage:\n"
    "  tisma-engine slice <model> --config <settings.json> -o <out.gcode | out.goo | folder> [--warnings <out.json>]\n"
    "                     (a .goo file: for a resin printer, docs/spec/features/resin.md)\n"
    "                     [--case <case.json>]   (the load case of infill.stress_density)\n"
    "                     [--yes]   (run the settings' post-processing scripts without asking)\n"
    "                     [--copies <n>]   (n copies of the model, arranged; sequential.enabled prints them one by one)\n"
    "  tisma-engine analyze <model> --config <settings.json> --case <case.json> -o <report.json>\n"
    "                       [--vtk <map.vtk>] [--mesh <voxels.ply>] [--progress] [--estimate]\n"
    "  tisma-engine info <model>\n"
    "  tisma-engine --help | --version\n"
    "A model is an STL, OBJ or 3MF file.\n";

// Writes a file in one piece: into "<path>.part" first, renamed over the
// path once complete, so a process stopped half way never leaves a partial
// file under the real name. False (and no temporary left) on failure.
bool write_file(const std::string& path, const std::function<void(std::ostream&)>& write) {
    const std::filesystem::path target(path), part(path + ".part");
    std::error_code ignored;
    {
        std::ofstream f(part, std::ios::binary);
        if (!f)
            return false;
        write(f);
        f.close();
        if (!f) {
            std::filesystem::remove(part, ignored);
            return false;
        }
    }
    std::error_code ec;
    std::filesystem::rename(part, target, ec);
    if (ec) {
        std::filesystem::remove(part, ignored);
        return false;
    }
    return true;
}

struct SliceArgs {
    std::string model;
    std::string config;
    std::string output;
    std::string warnings; // empty: not written
    std::string analysis_case; // analyze; slice: the load case of stress-guided infill
    std::string vtk;           // analyze only; empty: not written
    std::string mesh;          // analyze only; empty: not written
    bool progress = false;     // analyze only: progress lines on the output
    bool estimate = false;     // analyze only: the voxels, memory and time, without solving
    bool yes = false;          // slice only: run the settings' post-processing scripts without asking
    std::size_t copies = 1;    // slice only: copies of the model on the bed
};

// Returns the arguments of 'slice' or 'analyze', or writes the problem to err.
std::optional<SliceArgs> parse_slice_args(std::span<const std::string_view> args, std::ostream& err,
                                          bool analyze = false) {
    SliceArgs out;
    for (std::size_t i = 0; i < args.size(); ++i) {
        const std::string_view a = args[i];
        auto value = [&](std::string& dst) {
            if (i + 1 >= args.size()) {
                err << "error: " << a << " needs a value\n";
                return false;
            }
            dst = std::string(args[++i]);
            return true;
        };
        if (a == "--config" || a == "-c") {
            if (!value(out.config))
                return std::nullopt;
        } else if (a == "-o" || a == "--output") {
            if (!value(out.output))
                return std::nullopt;
        } else if (a == "--warnings" && !analyze) {
            if (!value(out.warnings))
                return std::nullopt;
        } else if (a == "--case") {
            if (!value(out.analysis_case))
                return std::nullopt;
        } else if (a == "--vtk" && analyze) {
            if (!value(out.vtk))
                return std::nullopt;
        } else if (a == "--mesh" && analyze) {
            if (!value(out.mesh))
                return std::nullopt;
        } else if (a == "--progress" && analyze) {
            out.progress = true;
        } else if (a == "--estimate" && analyze) {
            out.estimate = true;
        } else if (a == "--yes" && !analyze) {
            out.yes = true;
        } else if (a == "--copies" && !analyze) {
            std::string n;
            if (!value(n))
                return std::nullopt;
            char* end = nullptr;
            const long v = std::strtol(n.c_str(), &end, 10);
            if (n.empty() || *end != '\0' || v < 1 || v > 1000) {
                err << "error: --copies needs a whole number from 1 to 1000\n";
                return std::nullopt;
            }
            out.copies = std::size_t(v);
        } else if (!a.empty() && a.front() == '-') {
            err << "error: unknown option " << a << "\n";
            return std::nullopt;
        } else if (out.model.empty()) {
            out.model = std::string(a);
        } else {
            err << "error: more than one model given\n";
            return std::nullopt;
        }
    }
    if (analyze && (out.model.empty() || out.config.empty() || out.analysis_case.empty() || out.output.empty())) {
        err << "error: analyze needs a model, --config, --case and -o\n";
        return std::nullopt;
    }
    if (out.model.empty() || out.config.empty() || out.output.empty()) {
        err << "error: slice needs a model, --config and -o\n";
        return std::nullopt;
    }
    return out;
}

void print_mesh_summary(const ModelFile& stl, double seconds, std::ostream& out) {
    const BoundingBox box = stl.mesh.bounding_box();
    out << "format: "
        << (stl.format == ModelFormat::StlBinary  ? "binary"
            : stl.format == ModelFormat::StlAscii ? "ascii"
            : stl.format == ModelFormat::Obj      ? "obj"
                                                  : "3mf")
        << "\n";
    if (stl.format == ModelFormat::ThreeMf)
        out << "objects: " << stl.objects << "\nparts: " << stl.parts << "\n";
    out << "triangles: " << stl.mesh.triangle_count() << "\n";
    if (!box.empty) {
        out << "size: " << (box.max.x - box.min.x) << " x " << (box.max.y - box.min.y) << " x "
            << (box.max.z - box.min.z) << "\n";
        out << "min: " << box.min.x << " " << box.min.y << " " << box.min.z << "\n";
        out << "max: " << box.max.x << " " << box.max.y << " " << box.max.z << "\n";
    }
    out << "volume: " << stl.mesh.signed_volume() << "\n";
    out << "read_ms: " << seconds * 1000.0 << "\n";
}

int load_model(const std::string& path, ModelFile& stl, double& seconds, std::ostream& err) {
    const auto start = std::chrono::steady_clock::now();
    try {
        stl = read_model(path);
    } catch (const ModelError& e) {
        err << "error: " << path << ": " << e.what() << "\n";
        return kExitInput;
    }
    seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    return kExitOk;
}

int cmd_info(std::span<const std::string_view> args, std::ostream& out, std::ostream& err) {
    if (args.size() != 1) {
        err << "error: info needs exactly one model\n" << kUsage;
        return kExitUsage;
    }
    ModelFile stl;
    double seconds = 0.0;
    if (int rc = load_model(std::string(args[0]), stl, seconds, err); rc != kExitOk)
        return rc;
    print_mesh_summary(stl, seconds, out);
    const auto start = std::chrono::steady_clock::now();
    const IndexedMesh indexed = weld_exact(stl.mesh);
    const double weld_ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    out << "vertices: " << indexed.vertices.size() << "\n";
    out << "weld_ms: " << weld_ms << "\n";

    // Models the engine refuses to slice are not sliced here either (one at
    // 1e30 mm used to grow memory without end).
    if (const std::string reason = unprintable_reason(indexed, Settings{}); !reason.empty()) {
        out << "slicing: skipped (" << reason << ")\n";
        return kExitOk;
    }

    // Planar slicing with 0.2 mm layers: level-set segments and contours.
    const BoundingBox box = stl.mesh.bounding_box();
    if (!box.empty) {
        const auto slice_start = std::chrono::steady_clock::now();
        std::vector<float> heights;
        for (const Layer& layer : plan_layers(box.min.z, box.max.z, {0.2, 0.2}))
            heights.push_back(float(layer.slice_z()));
        const auto layers = slice_levels(indexed, height_field(indexed), heights);
        const double slice_ms =
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - slice_start).count();
        std::size_t contours = 0;
        for (const auto& layer : layers)
            contours += layer.size();
        out << "layers: " << layers.size() << "\n";
        out << "contours: " << contours << "\n";
        out << "slice_ms: " << slice_ms << "\n";
        if (layers.empty())
            return kExitOk; // flat or thinner than a layer: no middle layer to look at

        // Regions: closed contours merged into polygons with holes (Clipper2).
        const auto region_start = std::chrono::steady_clock::now();
        const std::vector<Polygons> regions = layer_regions(layers);
        const double region_ms =
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - region_start).count();
        double volume = 0.0;
        for (std::size_t l = 0; l < regions.size(); ++l)
            volume += area_mm2(regions[l]) * 0.2;
        out << "region_ms: " << region_ms << "\n";

        // Medial axis of the middle layer (F2), with the centre marked, on the
        // region as the slicing builds it (simplified with the default tolerance).
        const std::size_t mid = layers.size() / 2;
        const Polygons mid_region = layer_region(simplify(to_polygons(layers[mid]), Settings{}.simplify_tolerance));
        const auto axis_start = std::chrono::steady_clock::now();
        MedialGraph axis = medial_graph(mid_region);
        mark_central(axis);
        const double axis_ms =
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - axis_start).count();
        out << "mid_layer_axis_edges: " << axis.edges.size() << "\n";
        out << "mid_layer_axis_ms: " << axis_ms << "\n";

        // Variable-width perimeters of the middle layer (F2, Kuipers et al. 2020).
        const auto walls_start = std::chrono::steady_clock::now();
        const BeadingParams beading;
        filter_marking(axis, beading.preferred_width);
        const TransitionResult transitions = apply_transitions(axis, beading);
        const std::vector<Toolpath> walls = extract_toolpaths(axis, node_beadings(axis, transitions.bead_counts, beading));
        const double walls_ms =
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - walls_start).count();
        out << "mid_layer_toolpaths: " << walls.size() << "\n";
        out << "mid_layer_toolpaths_ms: " << walls_ms << "\n";
        out << "sliced_volume: " << volume << "\n";
    }
    return kExitOk;
}

// Whether the output is a resin printer's GOO file.
bool goo_output(const std::string& path) {
    std::string ext = std::filesystem::path(path).extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return char(std::tolower(c)); });
    return ext == ".goo";
}

// Resin (resin.md): the part's images and the platform's moves in a GOO
// file, the part centred on the display unless the settings place it.
int slice_resin_file(const SliceArgs& args, Settings settings, ModelFile& model, std::ostream& out, std::ostream& err) {
    if (settings.bed.empty())
        settings.bed = {{0.0, 0.0},
                        {settings.display_width, 0.0},
                        {settings.display_width, settings.display_height},
                        {0.0, settings.display_height}};
    std::vector<MeshObject> objects;
    if (args.copies > 1)
        model.mesh = place_copies(model.mesh, args.copies, settings, std::filesystem::path(args.model).filename().string(),
                                  &objects);
    else
        place_on_bed(model.mesh, settings);
    const auto start = std::chrono::steady_clock::now();
    ResinPrint print;
    try {
        print = slice_resin(weld_exact(model.mesh), settings);
    } catch (const ResinError& e) {
        err << "error: " << e.what() << "\n";
        return kExitModel;
    }
    out << "layers: " << print.layers.size() << "\n";
    out << "volume_mm3: " << print.volume << "\n";
    out << "weight_g: " << print.weight << "\n";
    out << "print_seconds: " << std::llround(print.seconds) << "\n";
    out << "slice_ms: " << std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count()
        << "\n";
    if (!write_file(args.output, [&](std::ostream& f) { write_goo(f, print, settings, goo_time_now()); })) {
        err << "error: cannot write '" << args.output << "'\n";
        return kExitInput;
    }
    out << "wrote " << args.output << "\n";
    // The cups (resin.md, rules 13 to 15).
    const std::vector<Warning> warnings = resin_warnings(print, std::filesystem::path(args.model).filename().string());
    out << "warnings: " << warnings.size() << "\n";
    if (!args.warnings.empty()) {
        if (!write_file(args.warnings, [&](std::ostream& report) { report << warnings_json(warnings); })) {
            err << "error: cannot write '" << args.warnings << "'\n";
            return kExitInput;
        }
        out << "wrote " << args.warnings << "\n";
    }
    return kExitOk;
}

int cmd_slice(std::span<const std::string_view> args, std::ostream& out, std::ostream& err) {
    const std::optional<SliceArgs> parsed = parse_slice_args(args, err);
    if (!parsed) {
        err << kUsage;
        return kExitUsage;
    }
    Settings settings;
    try {
        settings = load_settings(parsed->config);
    } catch (const SettingsError& e) {
        err << "error: " << parsed->config << ": " << e.what() << "\n";
        return kExitInput;
    }
    for (const std::string& group : settings.ignored_groups)
        err << "warning: settings group '" << group << "' is not used yet\n";
    if (goo_output(parsed->output)) {
        ModelFile model;
        double seconds = 0.0;
        if (int rc = load_model(parsed->model, model, seconds, err); rc != kExitOk)
            return rc;
        out << "read " << model.mesh.triangle_count() << " triangles in " << seconds * 1000.0 << " ms\n";
        return slice_resin_file(*parsed, settings, model, out, err);
    }
    // The templates' input_filename_base (gcode_templates.md, rule 7).
    settings.input_name = std::filesystem::path(parsed->model).stem().string();
    // The speeds come from the printer's profile; those the file does not give
    // are a stock Ender-3's (docs/CONFIG.md, "Velocidades de respaldo").
    std::string fallback;
    for (const std::string& key : fill_missing_speeds(settings))
        fallback += (fallback.empty() ? "" : ", ") + key;
    if (!fallback.empty())
        err << "warning: no speed for " << fallback << ": using a stock Ender-3's\n";

    ModelFile stl;
    double seconds = 0.0;
    if (int rc = load_model(parsed->model, stl, seconds, err); rc != kExitOk)
        return rc;
    out << "read " << stl.mesh.triangle_count() << " triangles in " << seconds * 1000.0 << " ms\n";
    // The copies, each an object of the plate (placement.md, rule 2;
    // sequential.md, rule 2), or the model alone.
    std::vector<MeshObject> objects;
    const std::string file_name = std::filesystem::path(parsed->model).filename().string();
    if (parsed->copies > 1) {
        stl.mesh = place_copies(stl.mesh, parsed->copies, settings, file_name, &objects);
    } else {
        place_on_bed(stl.mesh, settings);
        objects.push_back({file_name, 0, stl.mesh.triangle_count(), 0, 0});
    }

    const auto start = std::chrono::steady_clock::now();
    const IndexedMesh mesh = weld_exact(stl.mesh);
    if (const std::string reason = unprintable_reason(mesh, settings); !reason.empty()) {
        err << "error: " << reason << "\n";
        return kExitModel;
    }
    std::unique_ptr<gpu::Context> gpu;
    if (settings.gpu == GpuMode::Auto)
        gpu = gpu::Context::create();
    out << "gpu: " << (gpu ? gpu->device_name() : std::string("none, using the CPU")) << "\n";
    // Stress-guided infill (docs/INFILL.md): the part analysed under the load
    // case first, to know where it works hardest.
    StressField stress;
    if (settings.infill_stress_density > 0.0) {
        if (parsed->analysis_case.empty()) {
            err << "warning: infill.stress_density needs a load case (--case): the infill stays even\n";
        } else {
            AnalysisCase c;
            try {
                c = load_analysis_case(parsed->analysis_case);
            } catch (const SettingsError& e) {
                err << "error: " << parsed->analysis_case << ": " << e.what() << "\n";
                return kExitInput;
            }
            const auto analysis_start = std::chrono::steady_clock::now();
            try {
                stress = infill_stress_field(mesh, settings, c, gpu.get());
            } catch (const AnalysisError& e) {
                err << "error: the analysis for stress-guided infill: " << e.what() << "\n";
                return kExitAnalysis;
            }
            out << "stress_analysis_ms: "
                << std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - analysis_start).count()
                << "\n";
            if (stress.empty())
                err << "warning: the analysis gave no valid result: the infill stays even\n";
        }
    }
    SlicedModel sliced;
    std::vector<Warning> warnings;
    if (!fallback.empty())
        warnings.push_back({"speed_fallback", -1, 0.0,
                            "the printer's profile gives no speed for " + fallback + ": a stock Ender-3's are used", 0.0});
    try {
        sliced = slice_model(mesh, settings, gpu.get(), &stress, nullptr, objects.empty() ? nullptr : &objects);
    } catch (const std::runtime_error& e) {
        if (!gpu)
            throw;
        err << "warning: GPU failed (" << e.what() << "), slicing on the CPU\n";
        warnings.push_back({"gpu_fallback", -1, 0.0, std::string("the GPU failed (") + e.what() + "), sliced on the CPU", 0.0});
        sliced = slice_model(mesh, settings, nullptr, &stress, nullptr, objects.empty() ? nullptr : &objects);
    }
    const double slice_ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    if (const std::string reason = unprintable_reason(sliced, settings); !reason.empty()) {
        err << "error: " << reason << "\n";
        return kExitModel;
    }

    double volume = 0.0;
    std::size_t polygons = 0, walls = 0, solid_lines = 0, sparse_lines = 0, bridge_lines = 0;
    double top = 0.0, bottom = 0.0, sparse = 0.0, support = 0.0;
    for (std::size_t l = 0; l < sliced.layers.size(); ++l) {
        volume += area_mm2(sliced.regions[l]) * (sliced.layers[l].top - sliced.layers[l].bottom);
        polygons += sliced.regions[l].size();
        walls += sliced.walls[l].toolpaths.size();
        top += area_mm2(sliced.surfaces[l].top);
        bottom += area_mm2(sliced.surfaces[l].bottom);
        sparse += area_mm2(sliced.surfaces[l].sparse);
        solid_lines += sliced.fills[l].top.size() + sliced.fills[l].solid.size();
        sparse_lines += sliced.fills[l].sparse.size();
        bridge_lines += sliced.fills[l].bridge.size();
        if (l < sliced.support_areas.size())
            support += area_mm2(sliced.support_areas[l]);
    }
    out << "layers: " << sliced.layers.size() << "\n";
    out << "polygons: " << polygons << "\n";
    out << "wall_toolpaths: " << walls << "\n";
    out << "top_area: " << top << "\n";
    out << "bottom_area: " << bottom << "\n";
    out << "sparse_area: " << sparse << "\n";
    out << "solid_infill_lines: " << solid_lines << "\n";
    out << "sparse_infill_lines: " << sparse_lines << "\n";
    out << "bridge_lines: " << bridge_lines << "\n";
    out << "support_area: " << support << "\n";
    out << "sliced_volume: " << volume << "\n";
    out << "slice_ms: " << slice_ms << "\n";
    if (sliced.open_contours > 0)
        err << "warning: " << sliced.open_contours
            << " open contours were dropped (holes in the mesh wider than slicing.gap_closing_radius)\n";
    std::vector<Warning> model = model_warnings(sliced);
    warnings.insert(warnings.end(), model.begin(), model.end());
    const std::vector<Warning> stability = stability_warnings(sliced, settings);
    warnings.insert(warnings.end(), stability.begin(), stability.end());
    // Into a folder: the name from output.filename_format, first without the
    // print's time and weight, then with them (gcode_templates.md, rule 15).
    std::filesystem::path target = parsed->output;
    const bool folder = std::filesystem::is_directory(target);
    try {
        if (folder)
            target /= output_filename(settings, sliced, std::nullopt);
    } catch (const TemplateError& e) {
        err << "error: " << e.what() << "\n";
        return kExitInput;
    }
    const GcodeExport exported = export_gcode(target, sliced, settings, &warnings);
    if (!exported.written) {
        err << "error: " << exported.error << "\n";
        for (const GcodeProblem& p : exported.check.problems)
            err << "  line " << p.line << ": " << p.message << "\n";
        return exported.check.problem_count > 0 ? kExitGcode : kExitInput;
    }
    if (folder) {
        const std::filesystem::path final_name = target.parent_path() / output_filename(settings, sliced, read_gcode_totals(target));
        std::error_code ec;
        if (final_name != target)
            std::filesystem::rename(target, final_name, ec);
        if (ec) {
            err << "error: cannot rename " << target.string() << " to " << final_name.string() << "\n";
            return kExitInput;
        }
        target = final_name;
    }
    // Post-processing scripts, once confirmed: they run anything (rules 16 and 17).
    if (!settings.post_process.empty()) {
        out << "A post-processing script has been detected in the settings:";
        for (const std::string& script : settings.post_process)
            out << " " << script;
        out << "\nContinue (Y/N)? ";
        std::string answer = parsed->yes ? "Y" : "";
        if (!parsed->yes)
            std::getline(std::cin, answer);
        out << answer << "\n";
        if (answer != "Y" && answer != "y") {
            std::error_code ignored;
            std::filesystem::remove(target, ignored);
            err << "error: the post-processing was not confirmed: no G-code is kept\n";
            return kExitInput;
        }
        if (const std::string errors = run_post_process(settings, sliced, target); !errors.empty()) {
            err << "error: post-processing: " << errors;
            return kExitInput;
        }
    }
    out << "write_ms: " << exported.write_seconds * 1000.0 << "\n";
    out << "check_ms: " << exported.check_seconds * 1000.0 << "\n";
    out << "gcode_check: ok (" << exported.check.stats.lines << " lines, " << exported.check.stats.layers
        << " layers, " << exported.check.stats.filament << " mm of filament)\n";
    out << "wrote " << target.string() << "\n";
    out << "warnings: " << warnings.size() << "\n";
    if (!parsed->warnings.empty()) {
        if (!write_file(parsed->warnings, [&](std::ostream& report) { report << warnings_json(warnings); })) {
            err << "error: cannot write '" << parsed->warnings << "'\n";
            return kExitInput;
        }
        out << "wrote " << parsed->warnings << "\n";
    }
    return kExitOk;
}

int cmd_analyze(std::span<const std::string_view> args, std::ostream& out, std::ostream& err) {
    const std::optional<SliceArgs> parsed = parse_slice_args(args, err, true);
    if (!parsed) {
        err << kUsage;
        return kExitUsage;
    }
    Settings settings;
    AnalysisCase analysis;
    try {
        settings = load_settings(parsed->config);
    } catch (const SettingsError& e) {
        err << "error: " << parsed->config << ": " << e.what() << "\n";
        return kExitInput;
    }
    try {
        analysis = load_analysis_case(parsed->analysis_case);
    } catch (const SettingsError& e) {
        err << "error: " << parsed->analysis_case << ": " << e.what() << "\n";
        return kExitInput;
    }
    ModelFile stl;
    double seconds = 0.0;
    if (int rc = load_model(parsed->model, stl, seconds, err); rc != kExitOk)
        return rc;
    out << "read " << stl.mesh.triangle_count() << " triangles in " << seconds * 1000.0 << " ms\n";
    place_on_bed(stl.mesh, settings);
    const IndexedMesh mesh = weld_exact(stl.mesh);
    if (const std::string reason = unprintable_reason(mesh, settings); !reason.empty()) {
        err << "error: " << reason << "\n";
        return kExitModel;
    }
    std::unique_ptr<gpu::Context> gpu;
    if (settings.gpu == GpuMode::Auto && analysis.model == AnalysisModel::AsPrinted)
        gpu = gpu::Context::create();
    if (parsed->estimate) {
        AnalysisEstimate estimate;
        try {
            estimate = estimate_analysis(mesh, settings, analysis, gpu.get());
        } catch (const AnalysisError& e) {
            err << "error: " << e.what() << "\n";
            return kExitAnalysis;
        }
        out << "voxels: " << estimate.printed_voxels << " of " << estimate.grid.solid.size() << " ("
            << estimate.grid.hx << " mm, " << estimate.rows_per_layer << " rows per layer)\n";
        out << "estimated_memory_mb: " << estimate.memory_mb << "\n";
        out << "estimated_seconds: " << estimate.seconds << "\n";
        if (!write_file(parsed->output, [&](std::ostream& json) { json << analysis_estimate_json(estimate, analysis); })) {
            err << "error: cannot write '" << parsed->output << "'\n";
            return kExitInput;
        }
        out << "wrote " << parsed->output << "\n";
        return kExitOk;
    }
    AnalysisProgress progress;
    if (parsed->progress)
        progress = [&out](std::string_view stage, double done) {
            out << "progress: " << stage << " " << done << std::endl;
        };
    AnalysisReport report;
    try {
        // Stress-guided infill: the same infill the slicing would print.
        const StressField infill = settings.infill_stress_density > 0.0
                                       ? infill_stress_field(mesh, settings, analysis, gpu.get())
                                       : StressField{};
        report = run_analysis(mesh, settings, analysis, gpu.get(), progress, &infill);
    } catch (const AnalysisError& e) {
        err << "error: " << e.what() << "\n";
        return kExitAnalysis;
    }
    const ElasticityResult& e = report.result.elasticity;
    out << "voxels: " << report.grid.count() << " of " << report.grid.solid.size() << " (" << report.grid.hx
        << " mm, " << report.rows_per_layer << " rows per layer)\n";
    out << "solver: " << e.levels << " levels, " << e.iterations << " iterations, residual " << e.residual
        << (e.converged ? "" : " (not converged)") << "\n";
    out << "prepare_ms: " << report.prepare_seconds * 1000.0 << "\n";
    out << "solve_ms: " << report.solve_seconds * 1000.0 << "\n";
    out << "max_displacement_mm: " << e.max_displacement << "\n";
    out << "max_von_mises_mpa: " << e.max_von_mises << "\n";
    if (report.weakest_layer >= 0)
        out << "min_safety: " << report.result.min_safety << " at layer " << report.weakest_layer << " ("
            << failure_mode_name(report.result.mode[report.result.weakest]) << "); peak in one voxel "
            << report.result.min_peak_safety << "\n";
    else
        out << "min_safety: none (nothing is stressed)\n";
    if (report.resolution.done)
        out << "resolution: with " << report.resolution.voxel << " mm voxels min_safety " << report.resolution.min_safety
            << ", a change of " << 100.0 * report.resolution.change << " %; extrapolated "
            << report.resolution.extrapolated_min_safety << "\n";
    else
        out << "resolution: not checked (" << report.resolution.reason << ")\n";
    for (const AnalysisWarning& w : report.warnings)
        err << "warning: " << w.message << "\n";

    if (!write_file(parsed->output, [&](std::ostream& json) { json << analysis_json(report, analysis); })) {
        err << "error: cannot write '" << parsed->output << "'\n";
        return kExitInput;
    }
    out << "wrote " << parsed->output << "\n";
    if (!parsed->vtk.empty()) {
        if (!write_file(parsed->vtk, [&](std::ostream& vtk) { write_analysis_vtk(vtk, report); })) {
            err << "error: cannot write '" << parsed->vtk << "'\n";
            return kExitInput;
        }
        out << "wrote " << parsed->vtk << "\n";
    }
    if (!parsed->mesh.empty()) {
        if (!write_file(parsed->mesh, [&](std::ostream& ply) { write_voxel_mesh_ply(ply, report); })) {
            err << "error: cannot write '" << parsed->mesh << "'\n";
            return kExitInput;
        }
        out << "wrote " << parsed->mesh << "\n";
    }
    return report.valid ? kExitOk : kExitAnalysis;
}

} // namespace

int run_cli(std::span<const std::string_view> args, std::ostream& out, std::ostream& err) {
    if (args.empty()) {
        err << kUsage;
        return kExitUsage;
    }
    const std::string_view command = args[0];
    const auto rest = args.subspan(1);
    if (command == "--help" || command == "-h" || command == "help") {
        out << kUsage;
        return kExitOk;
    }
    if (command == "--version") {
        out << "tisma-engine " << TISMA_VERSION << "\n";
        return kExitOk;
    }
    try {
        if (command == "slice")
            return cmd_slice(rest, out, err);
        if (command == "info")
            return cmd_info(rest, out, err);
        if (command == "analyze")
            return cmd_analyze(rest, out, err);
    } catch (const std::exception& e) {
        // What the commands do not catch is the engine's own fault: said, not a crash.
        err << "error: internal: " << e.what() << "\n";
        return kExitInternal;
    }
    err << "error: unknown command '" << command << "'\n" << kUsage;
    return kExitUsage;
}

} // namespace tisma
