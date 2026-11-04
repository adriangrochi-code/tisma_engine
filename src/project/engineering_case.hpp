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
// «Nervios» (structural_optimization.md, rule 4): from an analysis, the
// cells (as in rule 3) whose infill works mostly one way, grouped by that
// way's angle on the bed in steps of 15°; each group a zone of rectilinear
// infill in one direction at its angle, in every layer, its density unchanged.
struct InfillRibs {
    std::vector<double> angles; // degrees from X, 0 to 165, one per group
    std::vector<std::size_t> group_cells; // the cells of each group
    std::size_t cells = 0;
    std::vector<MeshModifier> modifiers; // "rib <angle>", in the analysis' coordinates
};
InfillRibs infill_ribs(const AnalysisReport& report);
double infill_strength_exponent(InfillPattern pattern);
InfillZones infill_zones(const AnalysisReport& report, double density, double asked, InfillPattern pattern,
                         double scale = 1.0);

LightestInfill lightest_infill(const IndexedMesh& mesh, const TriangleMesh& placed, const Settings& settings,
                               const AnalysisCase& c, const ProjectEngineering& e,
                               const std::function<void(double)>& progress = {},
                               const std::vector<MeshModifier>* modifiers = nullptr);

// «Refuerzos» (structural_optimization.md, rule 6): the cells of 2 mm whose
// centre is within `radius` of a fixed face or a load, as box modifiers (a
// row along X one box) with 2 walls more than `settings` and an infill of
// 40 %, never less than the part's. radius 0: twice the largest load disc's
// radius, at least 5 mm.
struct Reinforcement {
    double radius = 0.0;
    std::size_t cells = 0;
    std::vector<MeshModifier> modifiers; // "reinforcement", in the analysis' coordinates
};
Reinforcement reinforcement_zone(const AnalysisReport& report, const AnalysisCase& c, const Settings& settings,
                                 double radius = 0.0);

// «Lattice» (structural_optimization.md, rule 7): the support-free lattice of
// cells of side `cell` over the part, its bars of `diameter` that touch the
// part, as a solid zone (a modifier at 100 % infill); the part itself goes to
// 0 % infill. Empty when no bar touches it.
MeshModifier lattice_modifier(const TriangleMesh& part, double cell, double diameter);

// «Orientación recomendada» (structural_optimization.md, rule 8): the part
// (`placed`, on the bed) analysed with its layers stacked along Z (as it
// is), X and Y, its conditions turned with it; the best is recommended when
// its safety factor is more than 5 % above Z's. The overhangs of the new
// orientation are not checked.
struct OrientationChoice {
    std::array<double, 3> safety{}; // Z, X, Y; 0 when that analysis failed
    int best = 0;                   // 0 Z (as it is), 1 X, 2 Y
    bool recommend = false;
    int analyses = 0;
};
// The turn that stacks the layers along an axis (0 Z, 1 X, 2 Y): that axis
// becomes Z.
std::array<double, 9> layers_along(int axis);
OrientationChoice recommended_orientation(const TriangleMesh& placed, const Settings& settings, const ProjectEngineering& e,
                                          std::string_view filament_type,
                                          const std::function<void(double)>& progress = {});

// Rule 6's search: from the analysis at the lowest uniform `density` that
// holds, the reinforcement zone; then the lightest uniform infill of the rest
// with the zone in place, and its mass against the part's at `density`
// without it. Used only when lighter.
struct ReinforcedInfill {
    bool found = false;   // the part holds with the zone at some density
    bool lighter = false; // and weighs less than at `density` without it
    Reinforcement zone;
    LightestInfill best; // the rest's density with the zone
    double mass_g = 0.0, plain_mass_g = 0.0;
    int analyses = 0;
};
ReinforcedInfill reinforced_infill(const IndexedMesh& mesh, const TriangleMesh& placed, const Settings& settings,
                                   const AnalysisCase& c, const ProjectEngineering& e, int density,
                                   const std::function<void(double)>& progress = {});

// Ribs (structural_optimization.md, rule 4): from the analysis at the
// lowest uniform `density` that holds, the ribs; then the lowest density that
// holds with them, searched again (at most that density). Used only when
// lighter: a lower density.
struct RibbedInfill {
    bool found = false;   // there are ribs and the part holds with them at `density` or less
    bool lighter = false; // below `density`
    InfillRibs ribs;
    int density = 100;
    double min_safety = 0.0;
    int analyses = 0;
};
RibbedInfill ribbed_infill(const IndexedMesh& mesh, const TriangleMesh& placed, const Settings& settings,
                           const AnalysisCase& c, const ProjectEngineering& e, int density,
                           const std::function<void(double)>& progress = {});

// The zones' three levels scaled together, by bisection, until the part
// holds (structural_optimization.md, rule 3): from the analysis at the
// uniform `density` (the lightest that holds), the object at the lowest level
// and the other two as box modifiers; each try is analysed with those zones as
// printed. Proposed (`lighter`) only when it weighs less than the uniform.
struct ZonedInfill {
    bool found = false;   // some scale holds
    bool lighter = false; // and the part weighs less than at the uniform density
    InfillZones zones;    // the object at density[0], modifiers for the denser levels
    double mass_g = 0.0, uniform_mass_g = 0.0;
    double min_safety = 0.0;
    int analyses = 0;
};
ZonedInfill zoned_infill(const IndexedMesh& mesh, const TriangleMesh& placed, const Settings& settings,
                         const AnalysisCase& c, const ProjectEngineering& e, int density,
                         const std::function<void(double)>& progress = {});

// "fixed_zone 1", "force_point 2"...: a condition's name in the case.
std::string condition_name(const EngineeringCondition& condition);

} // namespace tisma
