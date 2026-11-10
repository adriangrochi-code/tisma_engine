#pragma once

// What the Structural tab says of an analysis (docs/spec/features/
// structural_analysis.md, «Resultados», and engineering_panel.md, rule 12):
// the material families of the spec's table (service temperature, density,
// strength), the temperature factor, the verdict, the mass and the materials
// to try when the part does not hold. Own design; see PROVENANCE.md.

#include "fea/material.hpp"
#include "fea/voxels.hpp"
#include "mesh/mesh.hpp"

#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace tisma {

// A row of the spec's «Materiales» table: typical printed values.
struct MaterialFamily {
    std::string_view name; // "PLA", "PETG", "PA-CF"...
    double e_xy = 0.0, e_z = 0.0, nu = 0.35; // MPa
    double s_xy = 0.0, s_z = 0.0;            // MPa
    double tg = 0.0, max_service = 0.0;      // °C
    double density = 0.0;                    // g/cm³
    double nozzle_min = 0.0, nozzle_max = 0.0;
    bool elastomer = false; // a linear analysis does not describe it (TPU)
};

std::span<const MaterialFamily> material_families();

// The family of a material record's id ("bambu-pa6-cf"), a filament type
// ("PETG-CF", "PLA+") or a family name; null when none matches. Carbon
// fibre turns PA and PC into their CF rows; PLA-CF and PETG-CF stay PLA and
// PETG (the table has no row for them).
const MaterialFamily* family_of(std::string_view name);

// f(T) of the spec's method, point 5: 1 up to 23 °C, straight down to 0.5 at
// the maximum service temperature, then down to 0.02 over the next 15 °C,
// and 0.02 above.
double temperature_factor(const MaterialFamily& family, double celsius);

// A family as a material of its own, with the table's typical values
// ("Probá con", engineering_panel.md, rule 12): its id "family:PEI", the
// family an id of that form names (null for other ids), and its constants:
// E_xy in the layer, E_z across, G = E / 2(1 + nu), the strengths S_xy and
// S_z in tension and compression, and S / sqrt(3) in shear (von Mises).
std::string family_material_id(const MaterialFamily& family);
const MaterialFamily* family_of_material_id(std::string_view id);
OrthotropicMaterial family_material(const MaterialFamily& family);

enum class Verdict { Holds, LowMargin, Overloaded, TooFlexible, OverTemperature, NotApplicable };
// "holds", "low_margin", "overloaded", "too_flexible", "over_temperature",
// "not_applicable".
const char* verdict_name(Verdict verdict);

struct VerdictInput {
    double min_safety = 0.0;      // the analysis' smallest safety factor
    double required_safety = 2.0; // asked for in the panel
    double working_temperature = 23.0;
    const MaterialFamily* family = nullptr; // unknown: no temperature or elastomer check
    bool limit_exceeded = false;  // a displacement limit was passed
};
// In the order the spec's table is read: an elastomer is not applicable; too
// hot is over temperature; below 1, overloaded; below the asked factor, low
// margin; past a displacement limit, too flexible; else it holds.
Verdict structural_verdict(const VerdictInput& in);

// The families (no elastomer) that would hold where this one does not: hot
// enough to serve at the temperature, and with the safety factor scaled by
// their weaker strength ratio and their temperature factor at or above the
// asked one. Strongest margin first.
struct MaterialSuggestion {
    const MaterialFamily* family = nullptr;
    double safety = 0.0; // the estimated smallest safety factor
};
std::vector<MaterialSuggestion> suggest_materials(const VerdictInput& in);

// The mass of the analysed voxels (as printed: without the gaps of sparse
// infill) in grams, at the density in g/cm³.
double voxel_mass_grams(const VoxelGrid& grid, double density);

// The outside of the analysed voxels as triangles (two per face of a solid
// voxel with no solid neighbour behind it, facing out), each with the value
// of its voxel: what the Structural tab paints its map on (rule 12).
// With the nodes' displacement (3 per node of the grid, as ElasticityResult
// keeps it), each corner also gets its own: the deformed shape is corner +
// scale * displacement (rule 12, «la deformación exagerada»).
struct VoxelSurface {
    std::vector<Vec3f> corners;      // 3 per triangle
    std::vector<double> value;       // per triangle
    std::vector<Vec3f> displacement; // per corner, when the nodes' displacement was given
};
VoxelSurface voxel_surface(const VoxelGrid& grid, const std::vector<double>& per_voxel,
                           const std::vector<double>* node_displacement = nullptr);

} // namespace tisma
