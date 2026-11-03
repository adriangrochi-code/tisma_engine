#pragma once

#include "config/settings.hpp"
#include "project/engineering.hpp"

#include <string>
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

// What keeps «Analizar» off, in the order the panel says it.
enum class AnalyzeBlock { None, Material, Fixed, Force };
AnalyzeBlock analyze_block(const ProjectEngineering& e, const EngineeringValues& values);

} // namespace tisma
