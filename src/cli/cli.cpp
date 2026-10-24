#include "cli/cli.hpp"

#include "io/stl.hpp"
#include "io/vtk.hpp"
#include "config/analysis_case.hpp"
#include "config/settings.hpp"
#include "fea/analysis.hpp"
#include "report/analysis_json.hpp"
#include "gcode/writer.hpp"
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

#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <ostream>
#include <string>

namespace tisma {
namespace {

constexpr std::string_view kUsage =
    "usage:\n"
    "  tisma-engine slice <model.stl> --config <settings.json> -o <out.gcode> [--warnings <out.json>]\n"
    "  tisma-engine analyze <model.stl> --config <settings.json> --case <case.json> -o <report.json>\n"
    "                       [--vtk <map.vtk>]\n"
    "  tisma-engine info <model.stl>\n"
    "  tisma-engine --help | --version\n";

struct SliceArgs {
    std::string model;
    std::string config;
    std::string output;
    std::string warnings; // empty: not written
    std::string analysis_case; // analyze only
    std::string vtk;           // analyze only; empty: not written
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
        } else if (a == "--case" && analyze) {
            if (!value(out.analysis_case))
                return std::nullopt;
        } else if (a == "--vtk" && analyze) {
            if (!value(out.vtk))
                return std::nullopt;
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

void print_mesh_summary(const StlFile& stl, double seconds, std::ostream& out) {
    const BoundingBox box = stl.mesh.bounding_box();
    out << "format: " << (stl.format == StlFormat::Binary ? "binary" : "ascii") << "\n";
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

int read_model(const std::string& path, StlFile& stl, double& seconds, std::ostream& err) {
    const auto start = std::chrono::steady_clock::now();
    try {
        stl = read_stl(path);
    } catch (const StlError& e) {
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
    StlFile stl;
    double seconds = 0.0;
    if (int rc = read_model(std::string(args[0]), stl, seconds, err); rc != kExitOk)
        return rc;
    print_mesh_summary(stl, seconds, out);
    const auto start = std::chrono::steady_clock::now();
    const IndexedMesh indexed = weld_exact(stl.mesh);
    const double weld_ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    out << "vertices: " << indexed.vertices.size() << "\n";
    out << "weld_ms: " << weld_ms << "\n";

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

    StlFile stl;
    double seconds = 0.0;
    if (int rc = read_model(parsed->model, stl, seconds, err); rc != kExitOk)
        return rc;
    out << "read " << stl.mesh.triangle_count() << " triangles in " << seconds * 1000.0 << " ms\n";

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
    SlicedModel sliced;
    std::vector<Warning> warnings;
    try {
        sliced = slice_model(mesh, settings, gpu.get());
    } catch (const std::runtime_error& e) {
        if (!gpu)
            throw;
        err << "warning: GPU failed (" << e.what() << "), slicing on the CPU\n";
        warnings.push_back({"gpu_fallback", -1, 0.0, std::string("the GPU failed (") + e.what() + "), sliced on the CPU", 0.0});
        sliced = slice_model(mesh, settings, nullptr);
    }
    const double slice_ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    if (const std::string reason = unprintable_reason(sliced); !reason.empty()) {
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
    std::ofstream gcode(std::string(parsed->output), std::ios::binary);
    if (!gcode) {
        err << "error: cannot write '" << parsed->output << "'\n";
        return kExitInput;
    }
    const auto write_start = std::chrono::steady_clock::now();
    std::vector<Warning> model = model_warnings(sliced);
    warnings.insert(warnings.end(), model.begin(), model.end());
    write_gcode(gcode, sliced, settings, &warnings);
    gcode.close();
    if (!gcode) {
        err << "error: writing '" << parsed->output << "' failed\n";
        return kExitInput;
    }
    const double write_ms =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - write_start).count();
    out << "write_ms: " << write_ms << "\n";
    out << "wrote " << parsed->output << "\n";
    out << "warnings: " << warnings.size() << "\n";
    if (!parsed->warnings.empty()) {
        std::ofstream report(parsed->warnings, std::ios::binary);
        report << warnings_json(warnings);
        report.close();
        if (!report) {
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
    StlFile stl;
    double seconds = 0.0;
    if (int rc = read_model(parsed->model, stl, seconds, err); rc != kExitOk)
        return rc;
    out << "read " << stl.mesh.triangle_count() << " triangles in " << seconds * 1000.0 << " ms\n";
    const IndexedMesh mesh = weld_exact(stl.mesh);
    if (const std::string reason = unprintable_reason(mesh, settings); !reason.empty()) {
        err << "error: " << reason << "\n";
        return kExitModel;
    }
    std::unique_ptr<gpu::Context> gpu;
    if (settings.gpu == GpuMode::Auto && analysis.model == AnalysisModel::AsPrinted)
        gpu = gpu::Context::create();
    AnalysisReport report;
    try {
        report = run_analysis(mesh, settings, analysis, gpu.get());
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
    for (const AnalysisWarning& w : report.warnings)
        err << "warning: " << w.message << "\n";

    std::ofstream json(parsed->output, std::ios::binary);
    json << analysis_json(report, analysis);
    json.close();
    if (!json) {
        err << "error: cannot write '" << parsed->output << "'\n";
        return kExitInput;
    }
    out << "wrote " << parsed->output << "\n";
    if (!parsed->vtk.empty()) {
        std::ofstream vtk(parsed->vtk, std::ios::binary);
        write_analysis_vtk(vtk, report);
        vtk.close();
        if (!vtk) {
            err << "error: cannot write '" << parsed->vtk << "'\n";
            return kExitInput;
        }
        out << "wrote " << parsed->vtk << "\n";
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
    if (command == "slice")
        return cmd_slice(rest, out, err);
    if (command == "info")
        return cmd_info(rest, out, err);
    if (command == "analyze")
        return cmd_analyze(rest, out, err);
    err << "error: unknown command '" << command << "'\n" << kUsage;
    return kExitUsage;
}

} // namespace tisma
