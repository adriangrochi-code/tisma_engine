#pragma once

#include "config/settings.hpp"
#include "fea/adhesion.hpp"
#include "fea/analysis.hpp"
#include "fea/verdict.hpp"
#include "mesh/mesh.hpp"
#include "project/engineering.hpp"
#include "slice/placement.hpp"

#include <array>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

// A part's Engineering conditions as an analysis case (formats.md, rule 11
// bis: `analyze` on the project uses exactly them). Own design; see
// PROVENANCE.md.
namespace tisma {

// Adds to `c` a fixture for each fixed point or zone and a load for each force
// of `e` (hidden ones left out), on `mesh` (the part's own coordinates, as
// the conditions) moved by `shift` (to where the analysis has it):
// - a zone: its triangles; a point: a disc of its radius on the surface;
// - a force: its newtons along the inward normal of what it pushes (the
//   triangles' mean, weighted by area), a signed axis or its vector.
// Returns notes on what was left out: displacement limits (checked after the
// solve, not yet), and zones with no triangle of the mesh.
std::vector<std::string> add_conditions(AnalysisCase& c, const ProjectEngineering& e, const TriangleMesh& mesh,
                                        const std::array<double, 3>& shift = {});

// The whole case of a part (formats.md, rule 11 bis; FM25): the material
// changed by hand, else the filament type's (engineering_panel.md, rule 3),
// else the case's default, with the adhesion between layers and lines of
// `print` applied (fea/adhesion.hpp; kept in the case), the Structural tab's quality ("high"
// is high; "fast" the whole part solid in about 20 000 voxels a row per
// layer of `layer_height`, with the resolution check; the rest normal), then add_conditions. Notes, as add_conditions,
// also name an unknown material (the default is used).
AnalysisCase engineering_analysis_case(const ProjectEngineering& e, const TriangleMesh& mesh,
                                       const std::array<double, 3>& shift, std::vector<std::string>& notes,
                                       std::string_view filament_type = {}, double layer_height = 0.2,
                                       const PrintConditions& print = {});

// How the part prints, for the adhesion between layers and lines
// (fea/adhesion.hpp): the extrusion temperature (changed by hand, else the
// profile's), the layer height and line width (changed by hand, else the
// profile's), the fan at its maximum and the chamber's temperature when the
// profile heats one. The layer time is not known before slicing.
PrintConditions print_conditions(const Settings& settings, const ProjectEngineering& e);

// The values changed by hand that the analysis reads: the layer heights and
// the line width (as walls.width). The extrusion temperature reaches the
// material through print_conditions.
void apply_engineering_overrides(Settings& settings, const ProjectEngineering& e, std::vector<std::string>& notes);

// What gives way at the weakest voxel (None when unknown).
FailureMode weakest_mode(const AnalysisReport& report);

// The report of a Structural analysis (engineering_panel.md, rule 12):
// analysis_json with a "verdict" object added: the verdict
// (structural_analysis.md, «Resultados»), the safety factor asked for, the
// working temperature, the material's family, the mass in grams and the
// families to try when the part does not hold. Displacement limits are not
// checked yet ("limits_checked": false).
// With `mesh` (the part as analysed, the conditions in its frame), the
// displacement limits are checked (check_limits) and "limits" lists them.
std::string structural_report_json(const AnalysisReport& report, const AnalysisCase& c, const ProjectEngineering& e,
                                   const std::array<double, 12>& to_part = {1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0},
                                   const TriangleMesh* mesh = nullptr);

// A displacement limit checked after the solve (engineering_panel.md, rule
// 5; structural_analysis.md, «Demasiado flexible»): the largest movement of
// the nodes of its zone against its limit, mm (or % of the part's largest
// size).
struct LimitCheck {
    std::string name;
    double limit_mm = 0.0, displacement_mm = 0.0;
    bool exceeded = false;
};
std::vector<LimitCheck> check_limits(const ProjectEngineering& e, const TriangleMesh& mesh, const AnalysisReport& report);

// The verdict of an analysis as the report gives it (structural_report_json):
// the asked factor, the working temperature, the material's family and the
// displacement limits.
Verdict report_verdict(const AnalysisReport& report, const AnalysisCase& c, const ProjectEngineering& e,
                       const TriangleMesh& mesh);

// «Relleno mínimo uniforme» (structural_optimization.md, rule 2): the lowest
// whole infill percentage whose verdict is «Aguanta», by bisection (more
// infill never makes it worse). found is false when not even 100 % holds
// (more walls, another material or another shape are needed); solid when the
// case is the solid model (quality Rápida), which the infill cannot change. Each analysis
// reports its share of `progress` (done from 0 to 1 over the whole search);
// throwing from it stops the search.
struct LightestInfill {
    bool solid = false;      // the case analyses the part solid (Rápida): the infill does not count, nothing searched
    bool found = false;
    int density = 100;      // percent
    double min_safety = 0.0; // at that density
    int analyses = 0;
};
// «Relleno por zonas» (structural_optimization.md, rule 3), from an analysis
// with uniform infill at `density` percent: each voxel wants the density that
// brings its stress to the allowed, density · (asked / safety)^(1/nS) (nS the
// pattern's strength exponent, structural_analysis.md); the part is cut into
// cells of 3 voxels and at least 3 mm, each wanting its voxels' most; the
// cells fall in 3 levels by thirds, each level at its cells' most, times
// `scale` (at most 100 %). The zones are box modifiers, a level's cells next
// to each other along X joined into one box, in the analysis' coordinates.
struct InfillZones {
    std::array<double, 3> density{}; // percent, lowest level first
    std::array<std::size_t, 3> cells{};
    std::vector<MeshModifier> modifiers;
};
double infill_strength_exponent(InfillPattern pattern);
InfillZones infill_zones(const AnalysisReport& report, double density, double asked, InfillPattern pattern,
                         double scale = 1.0);

LightestInfill lightest_infill(const IndexedMesh& mesh, const TriangleMesh& placed, const Settings& settings,
                               const AnalysisCase& c, const ProjectEngineering& e,
                               const std::function<void(double)>& progress = {});

// "fixed_zone 1", "force_point 2"...: a condition's name in the case.
std::string condition_name(const EngineeringCondition& condition);

} // namespace tisma
