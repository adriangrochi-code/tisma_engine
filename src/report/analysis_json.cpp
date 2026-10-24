// The analysis report as JSON, on nlohmann/json (MIT). Own design; see
// PROVENANCE.md.

#include "report/analysis_json.hpp"

#include "fea/materials.hpp"

#include <nlohmann/json.hpp>

#include <cmath>

namespace tisma {
namespace {

using json = nlohmann::ordered_json;

// A safety factor, or null when nothing stresses that part.
json safety(double s) { return std::isfinite(s) ? json(s) : json(nullptr); }

} // namespace

std::string analysis_json(const AnalysisReport& r, const AnalysisCase& c) {
    const OrthotropicMaterial& m = c.material;
    const MaterialRecord* record = find_material(c.material_id);
    json material = {{"id", c.material_id},
                     {"name", record ? std::string(record->name) : std::string()},
                     {"source", record ? std::string(record->source) : std::string()},
                     {"constants",
                      {{"e1", m.e1},     {"e2", m.e2},     {"e3", m.e3},     {"g12", m.g12},   {"g13", m.g13},
                       {"g23", m.g23},   {"nu12", m.nu12}, {"nu13", m.nu13}, {"nu23", m.nu23}, {"xt", m.xt},
                       {"xc", m.xc},     {"yt", m.yt},     {"yc", m.yc},     {"zt", m.zt},     {"zc", m.zc},
                       {"s12", m.s12},   {"s13", m.s13},   {"s23", m.s23}}}};
    const ElasticityResult& e = r.result.elasticity;
    json result = {{"max_displacement_mm", e.max_displacement},
                   {"max_von_mises_mpa", e.max_von_mises},
                   {"min_safety", safety(r.result.min_safety)},
                   {"weakest", nullptr},
                   {"min_peak_safety", safety(r.result.min_peak_safety)},
                   {"voxels_below_1", r.below[0]},
                   {"voxels_below_1_5", r.below[1]},
                   {"voxels_below_2", r.below[2]}};
    if (r.weakest_layer >= 0)
        result["weakest"] = {{"x", r.weakest[0]},
                             {"y", r.weakest[1]},
                             {"z", r.weakest[2]},
                             {"layer", r.weakest_layer},
                             {"mode", failure_mode_name(r.result.mode[r.result.weakest])}};
    json layers = json::array();
    for (std::size_t l = 0; l < r.layer_safety.size(); ++l) {
        const LayerSafety& s = r.layer_safety[l];
        layers.push_back({{"index", l},
                          {"bottom", s.bottom},
                          {"top", s.top},
                          {"min_safety", safety(s.min_safety)},
                          {"mode", failure_mode_name(s.mode)}});
    }
    json warnings = json::array();
    for (const AnalysisWarning& w : r.warnings)
        warnings.push_back({{"code", w.code}, {"message", w.message}});
    const json root = {
        {"version", 1},
        {"valid", r.valid},
        {"model", c.model == AnalysisModel::AsPrinted ? "as_printed" : "solid"},
        {"material", material},
        {"voxels",
         {{"size", r.grid.hx},
          {"rows_per_layer", r.rows_per_layer},
          {"grid", {r.grid.nx, r.grid.ny, r.grid.nz}},
          {"part", r.part_voxels},
          {"printed", r.printed_voxels},
          {"loose", e.loose_voxels},
          {"analysed", r.grid.count()}}},
        {"solver",
         {{"preconditioner", c.solver.preconditioner == Preconditioner::Multigrid ? "multigrid" : "jacobi"},
          {"levels", e.levels},
          {"iterations", e.iterations},
          {"residual", e.residual},
          {"tolerance", c.solver.tolerance},
          {"converged", e.converged},
          {"prepare_seconds", r.prepare_seconds},
          {"solve_seconds", r.solve_seconds}}},
        {"result", result},
        {"layers", layers},
        {"warnings", warnings}};
    return root.dump(2) + "\n";
}

std::string analysis_estimate_json(const AnalysisEstimate& e, const AnalysisCase& c) {
    const json root = {{"version", 1},
                       {"estimate", true},
                       {"model", c.model == AnalysisModel::AsPrinted ? "as_printed" : "solid"},
                       {"voxels",
                        {{"size", e.grid.hx},
                         {"rows_per_layer", e.rows_per_layer},
                         {"grid", {e.grid.nx, e.grid.ny, e.grid.nz}},
                         {"part", e.part_voxels},
                         {"printed", e.printed_voxels}}},
                       {"estimated_memory_mb", e.memory_mb},
                       {"estimated_seconds", e.seconds}};
    return root.dump(2) + "\n";
}

} // namespace tisma
