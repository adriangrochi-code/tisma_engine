// The printers' own G-code variables. Own code; see macro_variables.hpp and
// PROVENANCE.md.

#include "gcode/macro_variables.hpp"

#include "config/settings.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <vector>

namespace tisma {
namespace {

constexpr MacroVariable kVariables[] = {
#include "gcode/macro_variables.inc"
};

TemplateValue number(double v) { return TemplateValue::of(std::round(v * 1000.0) / 1000.0); }
TemplateValue whole(double v) { return TemplateValue::of(static_cast<long long>(std::llround(v))); }
TemplateValue list_of(TemplateValue v) { return TemplateValue::of(std::vector<TemplateValue>{std::move(v)}); }

// The value a variable the engine cannot fill yet gets: nothing printed
// happens from it, and it reads as its type.
TemplateValue neutral(char type) {
    switch (type) {
    case 'b': return TemplateValue::of(false);
    case 't': return TemplateValue::of(std::string());
    case 'N': case 'I': return list_of(TemplateValue::of(0LL));
    case 'B': return list_of(TemplateValue::of(false));
    case 'T': return list_of(TemplateValue::of(std::string()));
    default: return TemplateValue::of(0LL);
    }
}

} // namespace

std::span<const MacroVariable> macro_variables() { return kVariables; }

void add_macro_variables(TemplateContext& c, const Settings& s) {
    auto& v = c.variables;
    const auto set = [&](const char* name, TemplateValue value) {
        if (!v.contains(name))
            v[name] = std::move(value);
    };
    // What the engine knows, by the public names of gcode_variables.md.
    set("max_print_height", number(s.max_height));
    set("z_offset", number(s.z_offset));
    if (!s.bed.empty()) {
        std::array<double, 2> lo = s.bed.front(), hi = s.bed.front();
        for (const auto& p : s.bed)
            for (int a = 0; a < 2; ++a)
                lo[a] = std::min(lo[a], p[a]), hi[a] = std::max(hi[a], p[a]);
        const auto pair = [](double x, double y) { return TemplateValue::of(std::vector<TemplateValue>{number(x), number(y)}); };
        set("print_bed_min", pair(lo[0], lo[1]));
        set("print_bed_max", pair(hi[0], hi[1]));
        set("print_bed_size", pair(hi[0] - lo[0], hi[1] - lo[1]));
    }
    const std::pair<const char*, double> limits[] = {
        {"machine_max_feedrate_x", s.max_feed_x},      {"machine_max_feedrate_y", s.max_feed_y},
        {"machine_max_feedrate_z", s.max_feed_z},      {"machine_max_feedrate_e", s.max_feed_e},
        {"machine_max_acceleration_x", s.max_accel_x}, {"machine_max_acceleration_y", s.max_accel_y},
        {"machine_max_acceleration_z", s.max_accel_z}, {"machine_max_acceleration_e", s.max_accel_e},
        {"machine_max_acceleration_extruding", s.acceleration},
        {"machine_max_acceleration_retracting", s.retract_acceleration},
        {"machine_max_acceleration_travel", s.travel_acceleration},
        {"machine_max_jerk_x", s.jerk_x},              {"machine_max_jerk_y", s.jerk_y},
        {"machine_max_jerk_z", s.jerk_z},              {"machine_max_jerk_e", s.jerk_e},
        {"retract_length", s.retract_length},          {"retract_speed", s.retract_speed},
        {"retract_lift", s.retract_lift},              {"pressure_advance", s.pressure_advance},
    };
    for (const auto& [name, value] : limits)
        set(name, list_of(number(value)));
    set("default_acceleration", number(s.acceleration));
    set("travel_acceleration", number(s.travel_acceleration));
    set("enable_pressure_advance", list_of(TemplateValue::of(s.pressure_advance > 0.0)));
    set("filament_type", list_of(TemplateValue::of(s.filament_type)));
    set("spiral_vase", TemplateValue::of(s.vase));
    // The plate chosen to send to a Bambu Lab printer (gcode_macros.md, rule 12).
    if (!s.bambu_plate.empty())
        set("curr_bed_type", TemplateValue::of(s.bambu_plate));
    set("has_wipe_tower", TemplateValue::of(s.wipe_tower));
    set("standby_temperature_delta", whole(s.standby_delta));
    set("max_fan_speed", list_of(whole(s.fan_max)));
    set("min_fan_speed", list_of(whole(s.fan_min)));
    set("disable_fan_first_layers", list_of(whole(s.fan_off_layers)));
    for (const char* name : {"current_extruder", "previous_extruder", "next_extruder"})
        set(name, TemplateValue::of(0LL));
    if (const auto z = v.find("layer_z"); z != v.end())
        set("toolchange_z", z->second);

    // The rest of the list.
    for (const MacroVariable& m : kVariables) {
        const std::string name(m.name);
        if (v.contains(name))
            continue;
        if (m.type == 'v') {
            c.verbatim.insert(name);
            continue;
        }
        if (!m.equivalent.empty() && m.equivalent != m.name && v.contains(m.equivalent)) {
            c.aliases[name] = {std::string(m.equivalent), m.type};
            continue;
        }
        // first_layer_print_min_0: an item of a list the engine has.
        if (const std::size_t under = name.find_last_of('_');
            m.equivalent.empty() && under != std::string::npos && under + 1 < name.size() &&
            name.find_first_not_of("0123456789", under + 1) == std::string::npos) {
            const auto base = v.find(name.substr(0, under));
            const std::size_t at = std::stoul(name.substr(under + 1));
            if (base != v.end() && base->second.kind == TemplateValue::Kind::List && at < base->second.list.size()) {
                v[name] = base->second.list[at];
                continue;
            }
        }
        v[name] = neutral(m.type);
        c.unfilled.insert(name);
    }
}

} // namespace tisma
