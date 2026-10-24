// A structural analysis of a part as printed: slicing, voxels, line
// directions, the solve and its summary. Own design; see PROVENANCE.md.

#include "fea/analysis.hpp"

#include "config/settings.hpp"
#include "fea/as_printed.hpp"
#include "fea/hex8.hpp"
#include "mesh/indexed_mesh.hpp"
#include "slice/pipeline.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <sstream>
#include <tuple>

namespace tisma {
namespace {

constexpr double kOnFace = 1e-6; // mm: how far from a face's plane its box reaches

// Shares of the progress bar: slicing, then the voxels, then the solve.
constexpr double kSlicingShare = 0.10, kPrepareShare = 0.15, kSolveEnd = 0.97;

// For estimates, fitted to the peak memory of the 10 mm cube at both
// qualities and both models (docs/ANALYSIS.md, "Recursos"): nearly all of it
// follows the whole grid, empty voxels too (the solver's vectors span every
// node); slicing as printed adds its own.
constexpr double kBytesPerGridVoxelSolid = 540.0, kBytesPerGridVoxelAsPrinted = 590.0;
constexpr double kBaseMemoryMbSolid = 10.0, kBaseMemoryMbAsPrinted = 50.0;
constexpr double kSecondsPerVoxelIteration = 0.95e-6; // per analysed voxel and iteration: 4 cores, multigrid
constexpr double kTypicalIterationsAsPrinted = 60.0, kTypicalIterationsSolid = 15.0;

double seconds_since(std::chrono::steady_clock::time_point start) {
    return std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
}

std::string format(double v) {
    std::ostringstream s;
    s.precision(4);
    s << v;
    return s.str();
}

} // namespace

const char* failure_mode_name(FailureMode mode) {
    switch (mode) {
    case FailureMode::Line:
        return "line";
    case FailureMode::BetweenLines:
        return "between_lines";
    case FailureMode::BetweenLayers:
        return "between_layers";
    case FailureMode::None:
        break;
    }
    return "none";
}

std::pair<double, int> analysis_resolution(const AnalysisCase& c, const Settings& settings) {
    const double voxel =
        c.voxel > 0.0 ? c.voxel : settings.wall_width / (c.quality == AnalysisQuality::High ? 4.0 : 2.0);
    const int rows = c.rows_per_layer > 0 ? c.rows_per_layer
                                          : std::max(1, int(std::lround(settings.layer_height / voxel)));
    return {voxel, rows};
}

Box resolve_region(const Region& region, const VoxelGrid& g) {
    if (region.face == PartFace::None)
        return region.box;
    int lo[3] = {g.nx, g.ny, g.nz}, hi[3] = {-1, -1, -1};
    for (int k = 0; k < g.nz; ++k)
        for (int j = 0; j < g.ny; ++j)
            for (int i = 0; i < g.nx; ++i)
                if (g.at(i, j, k)) {
                    lo[0] = std::min(lo[0], i), hi[0] = std::max(hi[0], i);
                    lo[1] = std::min(lo[1], j), hi[1] = std::max(hi[1], j);
                    lo[2] = std::min(lo[2], k), hi[2] = std::max(hi[2], k);
                }
    Box box;
    if (hi[0] < 0) { // no voxels: a box that holds nothing
        box.min[0] = 1.0, box.max[0] = -1.0;
        return box;
    }
    switch (region.face) {
    case PartFace::Bottom:
        box.max[2] = g.z[std::size_t(lo[2])] + kOnFace;
        break;
    case PartFace::Top:
        box.min[2] = g.z[std::size_t(hi[2]) + 1] - kOnFace;
        break;
    case PartFace::XMin:
        box.max[0] = g.x0 + lo[0] * g.hx + kOnFace;
        break;
    case PartFace::XMax:
        box.min[0] = g.x0 + (hi[0] + 1) * g.hx - kOnFace;
        break;
    case PartFace::YMin:
        box.max[1] = g.y0 + lo[1] * g.hy + kOnFace;
        break;
    case PartFace::YMax:
        box.min[1] = g.y0 + (hi[1] + 1) * g.hy - kOnFace;
        break;
    case PartFace::None:
        break;
    }
    return box;
}

namespace {

// The voxels an analysis works on, and what it needs to know about them.
struct Prepared {
    VoxelGrid grid;
    std::vector<float> direction;
    std::vector<Layer> layers;
    int rows = 1;
    double voxel = 0.0;
    std::size_t part = 0, printed = 0;
    std::vector<AnalysisWarning> warnings;
};

Prepared prepare(const IndexedMesh& mesh, const Settings& settings, const AnalysisCase& c, gpu::Context* gpu,
                 const AnalysisProgress& progress) {
    Prepared p;
    if (mesh.vertices.empty())
        throw AnalysisError("the model is empty");
    std::tie(p.voxel, p.rows) = analysis_resolution(c, settings);

    // Layers: the slicing's own plan, through the whole slicing when the
    // toolpaths give the directions.
    if (progress)
        progress("slicing", 0.0);
    SlicedModel sliced;
    if (c.model == AnalysisModel::AsPrinted) {
        sliced = slice_model(mesh, settings, gpu);
        p.layers = sliced.layers;
    } else {
        double min_z = std::numeric_limits<double>::infinity(), max_z = -min_z;
        for (const Vec3f& v : mesh.vertices)
            min_z = std::min(min_z, double(v.z)), max_z = std::max(max_z, double(v.z));
        p.layers = plan_layers(min_z, max_z, {settings.first_layer_height, settings.layer_height});
    }
    if (p.layers.empty())
        throw AnalysisError("the model has no layers to analyse");

    // Check the size before allocating the voxels.
    if (progress)
        progress("voxels", kSlicingShare);
    double min_x = std::numeric_limits<double>::infinity(), min_y = min_x, max_x = -min_x, max_y = -min_x;
    for (const Vec3f& v : mesh.vertices) {
        min_x = std::min(min_x, double(v.x)), max_x = std::max(max_x, double(v.x));
        min_y = std::min(min_y, double(v.y)), max_y = std::max(max_y, double(v.y));
    }
    const double count = std::max(1.0, std::ceil((max_x - min_x) / p.voxel)) *
                         std::max(1.0, std::ceil((max_y - min_y) / p.voxel)) * double(p.layers.size()) * p.rows;
    if (count > double(c.max_voxels))
        throw AnalysisError("the analysis would need " + format(count) + " voxels, more than max_voxels (" +
                            std::to_string(c.max_voxels) + "); use normal quality or larger voxels");
    p.grid = voxelize(mesh, p.voxel, p.voxel, layer_planes(p.layers, p.rows));
    p.part = p.grid.count();
    if (c.model == AnalysisModel::AsPrinted) {
        p.direction = line_directions(p.grid, sliced);
        if (p.voxel <= 0.5 * settings.wall_width + 1e-9)
            p.grid = printed_voxels(std::move(p.grid), p.direction);
        else
            p.warnings.push_back({"coarse_voxels", "voxels of " + format(p.voxel) +
                                                       " mm are wider than half a line: the gaps of sparse infill "
                                                       "are not modelled, the part is analysed solid"});
    }
    p.printed = p.grid.count();
    return p;
}

} // namespace

AnalysisEstimate estimate_analysis(const IndexedMesh& mesh, const Settings& settings, const AnalysisCase& c,
                                   gpu::Context* gpu) {
    Prepared p = prepare(mesh, settings, c, gpu, {});
    AnalysisEstimate out;
    out.rows_per_layer = p.rows;
    out.part_voxels = p.part;
    out.printed_voxels = p.printed;
    const bool printed = c.model == AnalysisModel::AsPrinted;
    out.memory_mb = (printed ? kBaseMemoryMbAsPrinted : kBaseMemoryMbSolid) +
                    (printed ? kBytesPerGridVoxelAsPrinted : kBytesPerGridVoxelSolid) * double(p.grid.solid.size()) / 1e6;
    out.seconds = kSecondsPerVoxelIteration * double(p.printed) *
                  (printed ? kTypicalIterationsAsPrinted : kTypicalIterationsSolid);
    out.grid = std::move(p.grid);
    return out;
}

AnalysisReport run_analysis(const IndexedMesh& mesh, const Settings& settings, const AnalysisCase& c,
                            gpu::Context* gpu, const AnalysisProgress& progress) {
    AnalysisReport out;
    const auto start = std::chrono::steady_clock::now();
    Prepared p = prepare(mesh, settings, c, gpu, progress);
    const int rows = p.rows;
    out.rows_per_layer = rows;
    out.layers = std::move(p.layers);
    out.part_voxels = p.part;
    out.direction = std::move(p.direction);
    out.warnings = std::move(p.warnings);
    out.grid = std::move(p.grid);
    const VoxelGrid& g = out.grid;

    std::vector<Fixture> fixtures;
    for (const CaseFixture& f : c.fixtures) {
        const Fixture fixture{resolve_region(f.region, g), f.x, f.y, f.z};
        bool used = false;
        for (int k = 0; k < g.nz && !used; ++k)
            for (int j = 0; j < g.ny && !used; ++j)
                for (int i = 0; i < g.nx && !used; ++i) {
                    if (!g.at(i, j, k))
                        continue;
                    for (int a = 0; a < 8 && !used; ++a)
                        used = fixture.box.contains(g.x0 + (i + kHexCorner[a][0]) * g.hx, g.y0 + (j + kHexCorner[a][1]) * g.hy,
                                                    g.z[std::size_t(k + kHexCorner[a][2])]);
                }
        if (!used)
            out.warnings.push_back({"fixture_unused", "fixture '" + f.name + "' touches no part of the model"});
        fixtures.push_back(fixture);
    }
    std::vector<SurfaceLoad> loads;
    for (const CaseLoad& l : c.loads) {
        SurfaceLoad load;
        load.box = resolve_region(l.region, g);
        for (int a = 0; a < 3; ++a)
            load.force[a] = l.force[std::size_t(a)];
        loads.push_back(load);
    }
    out.prepare_seconds = seconds_since(start);

    const auto solve_start = std::chrono::steady_clock::now();
    SolverOptions solver = c.solver;
    if (progress) {
        // Progress from the residual on a log scale between 1 and the
        // tolerance, never going back.
        progress("solving", kPrepareShare);
        solver.on_iteration = [&, done = 0.0](int iteration, double residual) mutable {
            if (c.solver.on_iteration)
                c.solver.on_iteration(iteration, residual);
            const double f = std::clamp(std::log(residual) / std::log(c.solver.tolerance), 0.0, 1.0);
            if (f > done + 0.01) {
                done = f;
                progress("solving", kPrepareShare + (kSolveEnd - kPrepareShare) * done);
            }
        };
    }
    out.result = solve_as_printed(g, c.material, out.direction, fixtures, loads, solver, {},
                                  StressWindow{settings.wall_width, settings.wall_width, rows});
    if (progress)
        progress("summary", kSolveEnd);
    out.solve_seconds = seconds_since(solve_start);
    const ElasticityResult& e = out.result.elasticity;
    out.printed_voxels = g.count();
    std::size_t loose = 0;
    out.grid = held_voxels(out.grid, fixtures, loose); // the voxels the solve used

    // Summary: the weakest voxel, each layer's weakest, how much is unsafe.
    const std::size_t plane = std::size_t(g.nx) * std::size_t(g.ny);
    out.layer_safety.resize(out.layers.size());
    for (std::size_t l = 0; l < out.layers.size(); ++l)
        out.layer_safety[l] = {out.layers[l].bottom, out.layers[l].top, std::numeric_limits<double>::infinity(), FailureMode::None};
    static constexpr double kThresholds[3] = {1.0, 1.5, 2.0};
    for (std::size_t v = 0; v < out.result.safety.size(); ++v) {
        const double s = out.result.safety[v];
        if (!std::isfinite(s))
            continue;
        for (int t = 0; t < 3; ++t)
            out.below[std::size_t(t)] += s < kThresholds[t];
        const std::size_t layer = v / plane / std::size_t(rows);
        if (layer < out.layer_safety.size() && s < out.layer_safety[layer].min_safety)
            out.layer_safety[layer].min_safety = s, out.layer_safety[layer].mode = out.result.mode[v];
    }
    if (std::isfinite(out.result.min_safety)) {
        const std::size_t v = out.result.weakest;
        const int i = int(v % std::size_t(g.nx)), j = int(v / std::size_t(g.nx) % std::size_t(g.ny)), k = int(v / plane);
        out.weakest = {g.x0 + (i + 0.5) * g.hx, g.y0 + (j + 0.5) * g.hy,
                       0.5 * (g.z[std::size_t(k)] + g.z[std::size_t(k) + 1])};
        out.weakest_layer = k / rows;
    }

    // What makes the result doubtful or the part unsafe.
    const bool held = g.count() > 0;
    if (e.loose_voxels > 0)
        out.warnings.push_back({"loose_voxels", std::to_string(e.loose_voxels) +
                                                    " voxels are not joined to any fixture and were left out"});
    if (e.unapplied_loads > 0)
        out.warnings.push_back({"load_not_applied", std::to_string(e.unapplied_loads) +
                                                        " loads found no face of the held part in their region"});
    if (!e.converged)
        out.warnings.push_back({"not_converged", "the solver stopped after " + std::to_string(e.iterations) +
                                                     " iterations at a residual of " + format(e.residual)});
    if (out.below[0] > 0)
        out.warnings.push_back({"unsafe", std::to_string(out.below[0]) + " voxels would fail (safety below 1), the "
                                                                          "weakest at layer " +
                                              std::to_string(out.weakest_layer) + " by " +
                                              failure_mode_name(out.result.mode[out.result.weakest])});
    out.valid = e.converged && held && e.unapplied_loads < int(c.loads.size()) && !c.loads.empty();
    if (!held)
        out.warnings.push_back({"nothing_held", "no fixture holds any part of the model"});
    if (c.loads.empty())
        out.warnings.push_back({"no_loads", "the case has no loads"});
    if (progress)
        progress("done", 1.0);
    return out;
}

} // namespace tisma
