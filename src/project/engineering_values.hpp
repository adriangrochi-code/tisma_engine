#pragma once

#include "config/settings.hpp"
#include "project/engineering.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <vector>
#include <string_view>

// The values the Structural panel shows (engineering_panel.md, rules 3, 4
// and 9): each read from the chosen profiles unless changed by hand in the
// panel, and what keeps «Analizar» off. Own design; see PROVENANCE.md.
namespace tisma {

// A value of the panel and whether it was changed by hand (the violet dot).
struct PanelValue {
    double value = 0.0;
    bool by_hand = false;
};

struct EngineeringValues {
    std::string filament_type; // the filament profile's, as written there
    std::string material_id;   // a record of fea/materials.hpp; empty: none fits («Elegí un material»)
    bool material_by_hand = false;
    PanelValue extrusion_temperature; // C, the layers after the first
    double bed_temperature = 0.0;     // C, shown only
    PanelValue layer_height, first_layer_height, line_width; // mm
    double working_temperature = 23.0; // C (rule 4)
};

// The material record for a filament type ("PLA", "petg", "PA6-CF"...), or
// empty when the table has none for it. The type is read without case and
// with spaces, dashes and plus signs as written.
std::string material_for_filament(std::string_view filament_type);

// The panel's values for `profile` (the settings the profiles make) and a
// part's Engineering data. A filament profile that names no type is read by
// its name: the whole name, then its first word («PLA Genérico» is PLA).
EngineeringValues engineering_values(const Settings& profile, const ProjectEngineering& e,
                                     std::string_view filament_profile = {});

// A new condition of this kind, numbered after the others of its kind
// (rule 8: «Zona fija 1», «Zona fija 2»), added to the list: a point at
// `point` with its outward `normal`, or a zone of `triangles`. A force
// starts at 10 N along the normal, into the part; a point's disc at 5 mm.
EngineeringCondition& add_condition(ProjectEngineering& e, ConditionKind kind, const std::array<double, 3>& point,
                                    const std::array<double, 3>& normal, std::vector<std::uint32_t> triangles);

// Its line in the list (rule 8), in the interface's language ("es" or
// "en"): «Zona fija 1 · 3 caras», «Punto de fuerza 1 · 50 N hacia −Z». A
// zone counts its faces: the groups of its triangles that face alike.
std::string condition_label(const EngineeringCondition& c, const TriangleMesh& mesh, std::string_view language);

// What keeps «Analizar» off, in the order the panel says it.
enum class AnalyzeBlock { None, Material, Fixed, Force };
AnalyzeBlock analyze_block(const ProjectEngineering& e, const EngineeringValues& values);

} // namespace tisma
