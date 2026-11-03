#pragma once

#include "config/settings.hpp"
#include "fea/analysis.hpp"
#include "mesh/mesh.hpp"
#include "project/engineering.hpp"

#include <array>
#include <string>
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
// changed by hand or the case's default, the Structural tab's quality ("high"
// is high, the rest normal), then add_conditions. Notes, as add_conditions,
// also name an unknown material (the default is used).
AnalysisCase engineering_analysis_case(const ProjectEngineering& e, const TriangleMesh& mesh,
                                       const std::array<double, 3>& shift, std::vector<std::string>& notes);

// The values changed by hand that the analysis reads: the layer heights and
// the line width (as walls.width). The extrusion temperature does not change
// the material's constants yet; a note says so.
void apply_engineering_overrides(Settings& settings, const ProjectEngineering& e, std::vector<std::string>& notes);

// "fixed_zone 1", "force_point 2"...: a condition's name in the case.
std::string condition_name(const EngineeringCondition& condition);

} // namespace tisma
