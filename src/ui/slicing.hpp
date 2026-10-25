#pragma once

#include "config/settings.hpp"
#include "gcode/check.hpp"
#include "gcode/preview.hpp"
#include "mesh/mesh.hpp"
#include "report/warnings.hpp"
#include "slice/pipeline.hpp"

#include <atomic>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace tisma::gpu {
class Context;
}

// The engine's work behind the interface's buttons, off the drawing thread.
// Own design; see PROVENANCE.md.
namespace tisma::ui {

// The settings the interface starts with: the engine's defaults on a generic
// 256 x 256 x 256 mm Marlin printer, whose start G-code heats the bed and the
// nozzle to the [bed_temperature] and [temperature] variables (60 and 210 C).
Settings default_settings();

struct SliceResult {
    bool ok = false;
    std::string error;  // why not, from the engine (in English, as the command line says it)
    Settings settings;  // what it was sliced with
    SlicedModel model;
    std::string gcode;  // the G-code, as export_gcode would write it
    GcodeCheck check;   // the check of that text
    GcodePreview preview;
    std::vector<Warning> warnings;
    std::string gpu;    // the device used, or empty for the CPU
    double slice_seconds = 0.0, gcode_seconds = 0.0, check_seconds = 0.0, preview_seconds = 0.0;
};

// Runs `tisma-engine slice` without writing a file: the printability checks of
// the mesh, the slicing (on the GPU when settings.gpu allows one, on the CPU if
// it fails), the checks of the sliced model, the G-code in memory, its check
// and the preview. Never throws: a failure comes back in `error`. One run at a
// time per Slicer; the GPU context is created on the first run and kept.
class Slicer {
public:
    Slicer();
    ~Slicer();
    // With a load case and infill.stress_density, the infill guided by the
    // stresses under it (docs/INFILL.md).
    SliceResult run(const TriangleMesh& mesh, const Settings& settings, const AnalysisCase* load = nullptr);

private:
    std::unique_ptr<gpu::Context> gpu_;
    bool gpu_tried_ = false;
};

struct VoxelCount {
    bool ok = false;
    std::string error;  // why there is no count (from the engine, in English)
    double voxel = 0.0; // mm across x and y
    int rows_per_layer = 1;
    std::size_t grid_voxels = 0;          // the box of voxels around the part
    std::size_t part_voxels = 0, printed_voxels = 0; // 0 when there were too many to count
    double memory_mb = 0.0, seconds = 0.0;
    std::size_t limit = 0;   // the analysis's limit on grid_voxels (AnalysisCase::max_voxels)
    bool over_limit = false; // the analysis would refuse to run at this quality
};

struct VoxelEstimate {
    bool ok = false;
    std::string error;       // the model cannot be analysed at all
    VoxelCount normal, high; // the two analysis qualities (docs/ANALYSIS.md)
};

// How many voxels the structural analysis would use for the model, at normal
// and high quality, with its memory and time (estimate_analysis). A quality
// over the analysis's voxel limit is still counted, up to four times the
// limit, and marked over_limit; beyond that only the grid's box is given.
VoxelEstimate estimate_voxels(const TriangleMesh& mesh, const Settings& settings);

// What a result made in the background was made from: the model and the
// settings, each counted up when it changes. The interface keeps working
// while a job runs, so by the time the result arrives it may belong to a
// model that is no longer open (it is dropped) or to settings that changed
// since (it is shown as out of date).
struct Revision {
    std::uint64_t model = 0, settings = 0;
};
enum class Fit { Current, OutOfDate, OtherModel };
inline Fit fit(const Revision& made, const Revision& now) {
    if (made.model != now.model)
        return Fit::OtherModel;
    return made.settings != now.settings ? Fit::OutOfDate : Fit::Current;
}

// A function run on its own thread, whose result the drawing thread picks up
// when it is ready. Starting again waits for the run before.
template <class Result>
class Background {
public:
    ~Background() { wait(); }

    template <class Function>
    void start(Function function) {
        wait();
        done_ = false;
        thread_ = std::thread([this, function = std::move(function)]() mutable {
            result_ = function();
            done_ = true;
        });
    }
    bool running() const { return thread_.joinable() && !done_; }
    // The result once, when the run is over (also after wait()).
    std::optional<Result> take() {
        if (!done_)
            return std::nullopt;
        if (thread_.joinable())
            thread_.join();
        done_ = false;
        return std::move(result_);
    }
    void wait() {
        if (thread_.joinable())
            thread_.join();
    }

private:
    std::thread thread_;
    std::atomic<bool> done_{false};
    Result result_{};
};

} // namespace tisma::ui
