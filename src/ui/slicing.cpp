// The engine's work behind the interface. Own design; see PROVENANCE.md.

#include "ui/slicing.hpp"

#include "fea/analysis.hpp"
#include "gcode/writer.hpp"
#include "gpu/compute.hpp"
#include "mesh/indexed_mesh.hpp"
#include "slice/printable.hpp"

#include <chrono>
#include <sstream>

namespace tisma::ui {
namespace {

double seconds_since(std::chrono::steady_clock::time_point start) {
    return std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
}

constexpr const char* kStartGcode = "; Tisma: generic printer\n"
                                    "M140 S[bed_temperature] ; heat the bed\n"
                                    "M104 S[temperature] ; heat the nozzle\n"
                                    "G28 ; home\n"
                                    "G90\n"
                                    "M190 S[bed_temperature] ; wait for the bed\n"
                                    "M109 S[temperature] ; wait for the nozzle\n"
                                    "G92 E0\n";
constexpr const char* kEndGcode = "M104 S0 ; nozzle off\n"
                                  "M140 S0 ; bed off\n"
                                  "M107 ; fan off\n"
                                  "G91\n"
                                  "G1 Z5 F600 ; lift\n"
                                  "G90\n"
                                  "M84 ; motors off\n";

constexpr std::size_t kEstimateReach = 4; // counts go up to this many times the analysis's limit

VoxelCount count(const IndexedMesh& mesh, const Settings& settings, AnalysisQuality quality) {
    VoxelCount out;
    AnalysisCase c;
    c.quality = quality;
    out.limit = c.max_voxels;
    c.max_voxels *= kEstimateReach;
    try {
        out.voxel = analysis_resolution(c, settings).first;
        out.grid_voxels = analysis_grid_voxels(mesh, settings, c);
        out.over_limit = out.grid_voxels > out.limit;
        if (out.grid_voxels > c.max_voxels) {
            // Too many even to count them one by one: the box is the number.
            out.ok = true;
            return out;
        }
        const AnalysisEstimate e = estimate_analysis(mesh, settings, c);
        out.rows_per_layer = e.rows_per_layer;
        out.grid_voxels = e.grid.solid.size();
        out.part_voxels = e.part_voxels;
        out.printed_voxels = e.printed_voxels;
        out.memory_mb = e.memory_mb;
        out.seconds = e.seconds;
        out.over_limit = out.grid_voxels > out.limit;
        out.ok = true;
    } catch (const std::exception& e) {
        out.error = e.what();
    }
    return out;
}

} // namespace

Settings default_settings() {
    Settings s;
    s.flavor = GcodeFlavor::Marlin;
    s.bed = {{0.0, 0.0}, {256.0, 0.0}, {256.0, 256.0}, {0.0, 256.0}};
    s.max_height = 256.0;
    s.start_gcode = kStartGcode;
    s.end_gcode = kEndGcode;
    s.variables["temperature"] = {"210"};
    s.variables["bed_temperature"] = {"60"};
    return s;
}

Slicer::Slicer() = default;
Slicer::~Slicer() = default;

SliceResult Slicer::run(const TriangleMesh& mesh, const Settings& settings) {
    SliceResult out;
    out.settings = settings;
    try {
        auto start = std::chrono::steady_clock::now();
        const IndexedMesh welded = weld_exact(mesh);
        if (std::string reason = unprintable_reason(welded, settings); !reason.empty()) {
            out.error = std::move(reason);
            return out;
        }
        gpu::Context* gpu = nullptr;
        if (settings.gpu == GpuMode::Auto) {
            if (!gpu_tried_) {
                gpu_ = gpu::Context::create();
                gpu_tried_ = true;
            }
            gpu = gpu_.get();
        }
        try {
            out.model = slice_model(welded, settings, gpu);
            if (gpu)
                out.gpu = gpu->device_name();
        } catch (const std::runtime_error& e) {
            if (!gpu)
                throw;
            out.warnings.push_back(
                {"gpu_fallback", -1, 0.0, std::string("the GPU failed (") + e.what() + "), sliced on the CPU", 0.0});
            gpu_.reset(); // do not try a failing device again
            out.model = slice_model(welded, settings, nullptr);
        }
        if (std::string reason = unprintable_reason(out.model, settings); !reason.empty()) {
            out.error = std::move(reason);
            return out;
        }
        std::vector<Warning> model = model_warnings(out.model);
        out.warnings.insert(out.warnings.end(), model.begin(), model.end());
        out.slice_seconds = seconds_since(start);

        start = std::chrono::steady_clock::now();
        std::ostringstream text;
        write_gcode(text, out.model, settings, &out.warnings);
        out.gcode = std::move(text).str();
        out.gcode_seconds = seconds_since(start);

        start = std::chrono::steady_clock::now();
        out.check = check_gcode(out.gcode, gcode_limits(out.model, settings));
        out.check_seconds = seconds_since(start);

        start = std::chrono::steady_clock::now();
        out.preview = read_gcode_preview(out.gcode, settings.filament_diameter);
        out.preview_seconds = seconds_since(start);
        out.ok = true;
    } catch (const std::exception& e) {
        out.error = e.what();
    }
    return out;
}

VoxelEstimate estimate_voxels(const TriangleMesh& mesh, const Settings& settings) {
    VoxelEstimate out;
    try {
        const IndexedMesh welded = weld_exact(mesh);
        if (std::string reason = unprintable_reason(welded, settings); !reason.empty()) {
            out.error = std::move(reason);
            return out;
        }
        out.normal = count(welded, settings, AnalysisQuality::Normal);
        out.high = count(welded, settings, AnalysisQuality::High);
        out.ok = true;
    } catch (const std::exception& e) {
        out.error = e.what();
    }
    return out;
}

} // namespace tisma::ui
