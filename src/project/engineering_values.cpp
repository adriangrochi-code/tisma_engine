// Own design; see PROVENANCE.md.

#include "project/engineering_values.hpp"

#include "fea/materials.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdlib>
#include <utility>

namespace tisma {

namespace {

// The first number of a machine variable ("temperature"), or `fallback`.
double variable_number(const Settings& s, const char* name, double fallback) {
    const auto it = s.variables.find(name);
    if (it == s.variables.end() || it->second.empty())
        return fallback;
    const char* text = it->second.front().c_str();
    char* end = nullptr;
    const double v = std::strtod(text, &end);
    return end == text ? fallback : v;
}

std::string upper(std::string_view text) {
    std::string out;
    for (const char c : text)
        if (c != ' ' && c != '_')
            out.push_back(char(std::toupper(static_cast<unsigned char>(c))));
    return out;
}

} // namespace

std::string material_for_filament(std::string_view filament_type) {
    // One record a type: the one measured on the most common printing
    // conditions (docs/MATERIALS.md).
    static constexpr std::array<std::pair<std::string_view, std::string_view>, 10> kByType{{
        {"PLA", "bambu-pla-basic"},
        {"PLA+", "esun-pla-plus"},
        {"PLA-CF", "bambu-pla-cf"},
        {"PETG", "bambu-petg-basic"},
        {"PETG-CF", "bambu-petg-cf"},
        {"ABS", "bambu-abs"},
        {"ASA", "bambu-asa"},
        {"PA6-CF", "bambu-pa6-cf"},
        {"PA-CF", "bambu-pa6-cf"},
        {"PAHT-CF", "bambu-paht-cf"},
    }};
    const std::string type = upper(filament_type);
    for (const auto& [name, id] : kByType)
        if (type == name && find_material(id))
            return std::string(id);
    return {};
}

EngineeringValues engineering_values(const Settings& profile, const ProjectEngineering& e) {
    const EngineeringOverrides& o = e.overrides;
    EngineeringValues v;
    v.filament_type = profile.filament_type;
    if (o.material && find_material(*o.material)) {
        v.material_id = *o.material;
        v.material_by_hand = true;
    } else {
        v.material_id = material_for_filament(profile.filament_type);
    }
    const auto pick = [](const std::optional<double>& hand, double from_profile) {
        return hand ? PanelValue{*hand, true} : PanelValue{from_profile, false};
    };
    v.extrusion_temperature = pick(o.extrusion_temperature, variable_number(profile, "temperature", 0.0));
    v.bed_temperature = variable_number(profile, "bed_temperature", 0.0);
    v.layer_height = pick(o.layer_height, profile.layer_height);
    v.first_layer_height = pick(o.first_layer_height, profile.first_layer_height);
    v.line_width = pick(o.line_width, profile.wall_width);
    v.working_temperature = e.working_temperature;
    return v;
}

AnalyzeBlock analyze_block(const ProjectEngineering& e, const EngineeringValues& values) {
    if (values.material_id.empty())
        return AnalyzeBlock::Material;
    bool fixed = false, force = false;
    for (const EngineeringCondition& c : e.conditions) {
        if (c.hidden)
            continue;
        fixed |= c.kind == ConditionKind::FixedPoint || c.kind == ConditionKind::FixedZone;
        force |= c.is_force();
    }
    if (!fixed)
        return AnalyzeBlock::Fixed;
    if (!force)
        return AnalyzeBlock::Force;
    return AnalyzeBlock::None;
}

} // namespace tisma
