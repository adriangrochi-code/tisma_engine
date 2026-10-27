// The engine's work behind the interface. Own design; see PROVENANCE.md.

#include "ui/slicing.hpp"

#include "fea/analysis.hpp"
#include "gcode/writer.hpp"
#include "gpu/compute.hpp"
#include "mesh/indexed_mesh.hpp"
#include "report/stability.hpp"
#include "slice/printable.hpp"

#include <oneapi/tbb/parallel_invoke.h>

#include <chrono>
#include <sstream>
#include <utility>

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
    // A printer made from nothing: a stock Ender-3's speeds (owner's
    // decision, 2026-10-26; docs/CONFIG.md, "Velocidades de respaldo").
    fill_missing_speeds(s);
    return s;
}

Settings without_gcode_settings(Settings s) {
    const Settings d;
    s.wall_speed = d.wall_speed, s.outer_wall_speed = d.outer_wall_speed, s.infill_speed = d.infill_speed;
    s.solid_infill_speed = d.solid_infill_speed, s.top_solid_infill_speed = d.top_solid_infill_speed;
    s.support_speed = d.support_speed, s.travel_speed = d.travel_speed, s.first_layer_speed = d.first_layer_speed;
    s.max_speed = d.max_speed, s.corner_speed = d.corner_speed;
    s.bridge_speed = d.bridge_speed, s.gap_fill_speed = d.gap_fill_speed, s.over_bridges_speed = d.over_bridges_speed;
    s.first_layer_infill_speed = d.first_layer_infill_speed, s.travel_z_speed = d.travel_z_speed;
    s.infill_max_speed = d.infill_max_speed, s.infill_max_crossing_speed = d.infill_max_crossing_speed;
    s.accel_default = d.accel_default, s.accel_walls = d.accel_walls, s.accel_outer_wall = d.accel_outer_wall;
    s.accel_infill = d.accel_infill, s.accel_solid_infill = d.accel_solid_infill;
    s.accel_top_solid_infill = d.accel_top_solid_infill, s.accel_bridges = d.accel_bridges;
    s.accel_first_layer = d.accel_first_layer, s.accel_travel = d.accel_travel;
    s.accel_short_travel = d.accel_short_travel;
    s.dynamic_overhangs = d.dynamic_overhangs, s.overhang_speeds = d.overhang_speeds;
    s.flow_slope_up = d.flow_slope_up, s.flow_slope_down = d.flow_slope_down;
    s.acceleration = d.acceleration, s.travel_acceleration = d.travel_acceleration;
    s.max_feed_x = d.max_feed_x, s.max_feed_y = d.max_feed_y, s.max_feed_z = d.max_feed_z, s.max_feed_e = d.max_feed_e;
    s.retract_length = d.retract_length, s.retract_speed = d.retract_speed, s.retract_lift = d.retract_lift;
    s.retract_min_travel = d.retract_min_travel;
    s.min_layer_time = d.min_layer_time, s.min_print_speed = d.min_print_speed, s.keep_outer_speed = d.keep_outer_speed;
    s.fan_min = d.fan_min, s.fan_max = d.fan_max, s.fan_below_time = d.fan_below_time, s.fan_always = d.fan_always;
    s.fan_bridges = d.fan_bridges, s.fan_off_layers = d.fan_off_layers;
    s.start_gcode = d.start_gcode, s.end_gcode = d.end_gcode, s.variables = d.variables;
    s.flow = d.flow, s.pressure_advance = d.pressure_advance, s.filament_max_flow = d.filament_max_flow;
    s.machine_max_flow = d.machine_max_flow, s.chamber_temperature = d.chamber_temperature;
    s.max_nozzle_temp = d.max_nozzle_temp, s.max_bed_temp = d.max_bed_temp, s.max_chamber_temp = d.max_chamber_temp;
    s.gcode_resolution = d.gcode_resolution, s.relative_extrusion = d.relative_extrusion, s.flavor = d.flavor;
    s.scarf = d.scarf, s.scarf_only_smooth = d.scarf_only_smooth, s.scarf_start_height = d.scarf_start_height;
    s.scarf_length = d.scarf_length, s.scarf_entire_loop = d.scarf_entire_loop;
    s.scarf_max_segment = d.scarf_max_segment, s.scarf_inner = d.scarf_inner;
    s.detect_overhangs = d.detect_overhangs;
    return s;
}

// A run's cut: the model and the slicing's warnings, with what it was made
// from.
struct Slicer::Cut {
    TriangleMesh mesh;
    std::optional<Painting> painting;
    std::string key; // settings_json(without_gcode_settings(settings))
    bool stressed = false; // cut with a load case's stress field
    std::shared_ptr<const SlicedModel> model;
    std::vector<Warning> warnings;
    std::string gpu;
};

Slicer::Slicer() = default;
Slicer::~Slicer() = default;

SliceResult Slicer::run(const TriangleMesh& mesh, const Settings& settings, const AnalysisCase* load,
                        const Painting* painting) {
    SliceResult out;
    out.settings = settings;
    try {
        auto start = std::chrono::steady_clock::now();
        // The same cut as last time: only the G-code again. A load case
        // cannot be compared, so stress-guided infill always cuts anew, and
        // its cut serves no other run.
        std::string key = settings_json(without_gcode_settings(settings));
        const bool stressed = load && settings.infill_stress_density > 0.0;
        const std::shared_ptr<const Cut> last = std::exchange(last_, nullptr);
        if (last && last->key == key && !stressed && !last->stressed &&
            (painting ? last->painting && *last->painting == *painting : !last->painting) &&
            last->mesh.corners() == mesh.corners()) {
            out.model = last->model;
            out.warnings = last->warnings;
            out.gpu = last->gpu;
            out.staged = true;
            last_ = last;
            if (std::string reason = unprintable_reason(*out.model, settings); !reason.empty()) {
                out.error = std::move(reason);
                return out;
            }
        } else {
            out.model = cut(mesh, settings, load, painting, out);
            if (!out.error.empty())
                return out;
            last_ = std::make_shared<const Cut>(
                Cut{mesh, painting ? std::optional<Painting>(*painting) : std::nullopt, std::move(key), stressed,
                    out.model, out.warnings, out.gpu});
        }
        out.slice_seconds = seconds_since(start);

        start = std::chrono::steady_clock::now();
        std::ostringstream text;
        write_gcode(text, *out.model, settings, &out.warnings);
        out.gcode = std::move(text).str();
        out.gcode_seconds = seconds_since(start);

        // The check and the preview both only read the text: at the same time.
        tbb::parallel_invoke(
            [&] {
                const auto begin = std::chrono::steady_clock::now();
                out.check = check_gcode(out.gcode, gcode_limits(*out.model, settings));
                out.check_seconds = seconds_since(begin);
            },
            [&] {
                const auto begin = std::chrono::steady_clock::now();
                out.preview = read_gcode_preview(out.gcode, settings.filament_diameter);
                out.preview_seconds = seconds_since(begin);
            });
        out.ok = true;
    } catch (const std::exception& e) {
        out.error = e.what();
    }
    return out;
}

std::shared_ptr<const SlicedModel> Slicer::cut(const TriangleMesh& mesh, const Settings& settings,
                                               const AnalysisCase* load, const Painting* painting, SliceResult& out) {
    const IndexedMesh welded = weld_exact(mesh);
    if (std::string reason = unprintable_reason(welded, settings); !reason.empty()) {
        out.error = std::move(reason);
        return nullptr;
    }
    gpu::Context* gpu = nullptr;
    if (settings.gpu == GpuMode::Auto) {
        if (!gpu_tried_) {
            gpu_ = gpu::Context::create();
            gpu_tried_ = true;
        }
        gpu = gpu_.get();
    }
    // Stress-guided infill: the part analysed under the load case first.
    StressField stress;
    if (settings.infill_stress_density > 0.0 && load) {
        try {
            stress = infill_stress_field(welded, settings, *load, gpu);
        } catch (const std::exception& e) {
            out.warnings.push_back({"stress_infill", -1, 0.0,
                                    std::string("the analysis for stress-guided infill failed (") + e.what() +
                                        "): the infill stays even",
                                    0.0});
        }
    }
    auto model = std::make_shared<SlicedModel>();
    try {
        *model = slice_model(welded, settings, gpu, &stress, painting);
        if (gpu)
            out.gpu = gpu->device_name();
    } catch (const std::runtime_error& e) {
        if (!gpu)
            throw;
        out.warnings.push_back(
            {"gpu_fallback", -1, 0.0, std::string("the GPU failed (") + e.what() + "), sliced on the CPU", 0.0});
        gpu_.reset(); // do not try a failing device again
        *model = slice_model(welded, settings, nullptr, &stress, painting);
    }
    if (std::string reason = unprintable_reason(*model, settings); !reason.empty()) {
        out.error = std::move(reason);
        return nullptr;
    }
    std::vector<Warning> warnings = model_warnings(*model);
    out.warnings.insert(out.warnings.end(), warnings.begin(), warnings.end());
    warnings = stability_warnings(*model, settings);
    out.warnings.insert(out.warnings.end(), warnings.begin(), warnings.end());
    return model;
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
