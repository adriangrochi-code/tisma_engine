// Own design; see PROVENANCE.md.

#include "project/engineering_values.hpp"

#include "fea/materials.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdlib>
#include <cstdio>
#include <cmath>
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

EngineeringValues engineering_values(const Settings& profile, const ProjectEngineering& e,
                                     std::string_view filament_profile) {
    const EngineeringOverrides& o = e.overrides;
    EngineeringValues v;
    v.filament_type = profile.filament_type;
    if (o.material && find_material(*o.material)) {
        v.material_id = *o.material;
        v.material_by_hand = true;
    } else {
        v.material_id = material_for_filament(profile.filament_type);
        if (v.material_id.empty() && profile.filament_type.empty() && !filament_profile.empty()) {
            v.material_id = material_for_filament(filament_profile);
            if (v.material_id.empty())
                v.material_id = material_for_filament(filament_profile.substr(0, filament_profile.find(' ')));
        }
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

EngineeringCondition& add_condition(ProjectEngineering& e, ConditionKind kind, const std::array<double, 3>& point,
                                    const std::array<double, 3>& normal, std::vector<std::uint32_t> triangles) {
    int number = 1;
    for (const EngineeringCondition& c : e.conditions)
        if (c.kind == kind)
            number = std::max(number, c.number + 1);
    EngineeringCondition c;
    c.kind = kind;
    c.number = number;
    c.point = point;
    c.normal = normal;
    c.triangles = std::move(triangles);
    if (c.is_force()) {
        c.newtons = 10.0;
        c.direction = ForceDirection::Normal;
    }
    e.conditions.push_back(std::move(c));
    return e.conditions.back();
}

std::string condition_label(const EngineeringCondition& c, const TriangleMesh& mesh, std::string_view language) {
    const bool es = language != "en";
    const char* name = nullptr;
    switch (c.kind) {
    case ConditionKind::FixedPoint: name = es ? "Punto fijo" : "Fixed point"; break;
    case ConditionKind::FixedZone: name = es ? "Zona fija" : "Fixed zone"; break;
    case ConditionKind::ForcePoint: name = es ? "Punto de fuerza" : "Force point"; break;
    case ConditionKind::ForceZone: name = es ? "Zona de fuerza" : "Force zone"; break;
    case ConditionKind::Limit: name = es ? "Límite" : "Limit"; break;
    }
    std::string out = std::string(name) + " " + std::to_string(c.number);
    const auto number = [&](double v, int decimals) {
        char text[32];
        std::snprintf(text, sizeof text, "%.*f", decimals, v);
        std::string t = text;
        if (t.find('.') != std::string::npos) {
            while (t.back() == '0')
                t.pop_back();
            if (t.back() == '.')
                t.pop_back();
        }
        if (es)
            std::replace(t.begin(), t.end(), '.', ',');
        return t;
    };
    if (!c.is_point()) {
        // The faces: groups of its triangles that face alike (within 1 degree).
        std::vector<std::array<double, 3>> normals;
        for (const std::uint32_t t : c.triangles) {
            if (t >= mesh.triangle_count())
                continue;
            const Vec3f a = mesh.corner(t, 0), b = mesh.corner(t, 1), d = mesh.corner(t, 2);
            const double u[3] = {double(b.x) - a.x, double(b.y) - a.y, double(b.z) - a.z};
            const double v[3] = {double(d.x) - a.x, double(d.y) - a.y, double(d.z) - a.z};
            std::array<double, 3> n{u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2], u[0] * v[1] - u[1] * v[0]};
            const double len = std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
            if (len <= 0.0)
                continue;
            for (double& x : n)
                x /= len;
            if (std::none_of(normals.begin(), normals.end(), [&](const std::array<double, 3>& m) {
                    return n[0] * m[0] + n[1] * m[1] + n[2] * m[2] > 0.99985;
                }))
                normals.push_back(n);
        }
        const std::size_t faces = normals.size();
        out += " · " + std::to_string(faces) + (es ? (faces == 1 ? " cara" : " caras") : (faces == 1 ? " face" : " faces"));
    } else {
        out += " · r " + number(c.radius, 1) + " mm";
    }
    if (c.is_force()) {
        out += " · " + number(c.newtons, 1) + " N";
        if (c.direction == ForceDirection::Normal) {
            out += es ? " hacia adentro" : " inward";
        } else if (c.direction == ForceDirection::Axis && c.axis != 0) {
            static constexpr const char* kAxes[] = {"X", "Y", "Z"};
            const int a = std::clamp(std::abs(c.axis), 1, 3) - 1;
            out += std::string(es ? " hacia " : " toward ") + (c.axis < 0 ? "−" : "+") + kAxes[a];
        } else {
            out += " (" + number(c.vector[0], 2) + "; " + number(c.vector[1], 2) + "; " + number(c.vector[2], 2) + ")";
        }
    } else if (c.kind == ConditionKind::Limit) {
        out += " · " + number(c.limit, 2) + (c.limit_percent ? " %" : " mm");
    }
    return out;
}

} // namespace tisma
