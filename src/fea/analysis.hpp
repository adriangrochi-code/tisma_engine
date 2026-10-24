#pragma once

#include "fea/elasticity.hpp"
#include "fea/material.hpp"
#include "fea/voxels.hpp"
#include "slice/layers.hpp"

#include <array>
#include <cstdint>
#include <functional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace tisma {

struct IndexedMesh;
struct Settings;
namespace gpu {
class Context;
}

// A structural analysis of a part as the settings would print it (PLAN.md,
// A1; the case file is described in docs/ANALYSIS.md). Own design; see
// PROVENANCE.md.

// How fine the voxels are: normal, 2 per line width in the layer and one row
// per layer; high, 4 per line width and rows about as tall as the voxels are
// wide (2 per 0.2 mm layer with 0.4 mm lines).
enum class AnalysisQuality : std::uint8_t { Normal, High };
// As printed: the sliced toolpaths give every voxel its line direction, and
// the gaps of sparse infill stay empty (with voxels at most half a line
// wide). Solid: the whole part, every direction unknown (the layer's average
// stiffness and its weakest direction for failure), without slicing.
enum class AnalysisModel : std::uint8_t { AsPrinted, Solid };

// Where a fixture or a load goes: a box in model coordinates (mm), or one face
// of the part's voxels (its lowest, highest, leftmost... extent).
enum class PartFace : std::uint8_t { None, Bottom, Top, XMin, XMax, YMin, YMax };
struct Region {
    Box box;
    PartFace face = PartFace::None;
};

struct CaseFixture {
    std::string name;
    Region region;
    bool x = true, y = true, z = true;
};

struct CaseLoad {
    std::string name;
    Region region;
    std::array<double, 3> force{}; // N, spread over the boundary faces in the region
};

struct AnalysisCase {
    std::string material_id = "bambu-pla-basic"; // a record of fea/materials.hpp
    OrthotropicMaterial material;                 // the record's constants with the case's changes
    AnalysisModel model = AnalysisModel::AsPrinted;
    AnalysisQuality quality = AnalysisQuality::Normal;
    double voxel = 0.0;      // mm across x and y; 0: from the quality and the line width
    int rows_per_layer = 0;  // 0: from the quality
    std::size_t max_voxels = 8'000'000; // above this the analysis does not run (memory)
    std::vector<CaseFixture> fixtures;
    std::vector<CaseLoad> loads;
    SolverOptions solver;
};

struct LayerSafety {
    double bottom = 0.0, top = 0.0;
    double min_safety = 0.0; // +infinity for a layer without stress
    FailureMode mode = FailureMode::None;
};

struct AnalysisWarning {
    std::string code;    // e.g. "loose_voxels", "load_not_applied", "not_converged", "unsafe"
    std::string message; // in plain words
};

struct AnalysisReport {
    VoxelGrid grid; // the voxels analysed: as printed without the gaps, without loose pieces
    std::vector<float> direction; // per voxel, the line's angle (radians); NaN unknown; empty for Solid
    AsPrintedResult result;
    std::vector<Layer> layers;
    int rows_per_layer = 1;
    std::size_t part_voxels = 0;    // the part's voxels
    std::size_t printed_voxels = 0; // those a line passes through (as printed), or all
    std::array<double, 3> weakest{}; // the centre of the weakest voxel
    int weakest_layer = -1;
    std::vector<LayerSafety> layer_safety; // one per layer
    std::array<std::size_t, 3> below{};    // voxels with safety below 1, 1.5 and 2
    double prepare_seconds = 0.0, solve_seconds = 0.0;
    std::vector<AnalysisWarning> warnings;
    // The result means something: the solver converged, a fixture holds the
    // part and at least one load acts on it.
    bool valid = false;
};

// Thrown when the analysis cannot start (too many voxels, an empty part).
class AnalysisError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

// The voxel size and rows per layer the case asks for with these settings.
std::pair<double, int> analysis_resolution(const AnalysisCase& c, const Settings& settings);

// The region's box on these voxels: a face becomes a thin box on the plane of
// the solid voxels' extent along that axis.
Box resolve_region(const Region& region, const VoxelGrid& grid);

// Progress for a user interface: the stage ("slicing", "voxels", "solving",
// "summary") and how much of the whole analysis is done, from 0 to 1.
using AnalysisProgress = std::function<void(std::string_view stage, double done)>;

// Slices (as printed) and voxelizes the part, solves, and summarizes.
AnalysisReport run_analysis(const IndexedMesh& mesh, const Settings& settings, const AnalysisCase& c,
                            gpu::Context* gpu = nullptr, const AnalysisProgress& progress = {});

// What an analysis would take, without solving: the voxels (slicing as
// printed to find the gaps), and the memory and time they need.
struct AnalysisEstimate {
    VoxelGrid grid; // the voxels that would be analysed (before leaving out loose pieces)
    int rows_per_layer = 1;
    std::size_t part_voxels = 0, printed_voxels = 0;
    double memory_mb = 0.0; // peak, from a fit to measured runs (docs/ANALYSIS.md, "Recursos")
    double seconds = 0.0;   // rough: typical iterations on the 4-core reference machine
};
AnalysisEstimate estimate_analysis(const IndexedMesh& mesh, const Settings& settings, const AnalysisCase& c,
                                   gpu::Context* gpu = nullptr);

const char* failure_mode_name(FailureMode mode);

} // namespace tisma
