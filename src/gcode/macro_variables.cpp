// The printers' own G-code variables. Own code; see macro_variables.hpp and
// PROVENANCE.md.

#include "gcode/macro_variables.hpp"

#include "config/settings.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <ctime>
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
        // cura's names for the bed (gcode_variables.md, «cura»).
        set("machine_width", number(hi[0] - lo[0]));
        set("machine_depth", number(hi[1] - lo[1]));
    }
    set("machine_height", number(s.max_height));
    set("infill_sparse_density", number(s.infill_density));
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
    // Without a plate chosen, Bambu Studio's «Cool Plate» (rule 10 quater).
    set("curr_bed_type", TemplateValue::of(s.bambu_plate.empty() ? std::string("Cool Plate") : s.bambu_plate));
    set("has_wipe_tower", TemplateValue::of(s.wipe_tower));
    set("standby_temperature_delta", whole(s.standby_delta));
    set("max_fan_speed", list_of(whole(s.fan_max)));
    set("min_fan_speed", list_of(whole(s.fan_min)));
    set("disable_fan_first_layers", list_of(whole(s.fan_off_layers)));
    for (const char* name : {"current_extruder", "previous_extruder", "next_extruder"})
        set(name, TemplateValue::of(0LL));
    if (const auto z = v.find("layer_z"); z != v.end())
        set("toolchange_z", z->second);

    // Speeds and temperatures a machine's G-code computes from (gcode_macros.md,
    // rule 10 quater, BH45): worked out from the settings, never 0. One the
    // settings do not give takes a safe value, named in the log.
    const auto first_number = [&](const char* name) {
        const auto it = v.find(name);
        if (it == v.end())
            return 0.0;
        const TemplateValue& x = it->second.kind == TemplateValue::Kind::List
                                     ? (it->second.list.empty() ? TemplateValue{} : it->second.list.front())
                                     : it->second;
        return x.kind == TemplateValue::Kind::Real ? x.real : x.kind == TemplateValue::Kind::Int ? double(x.integer) : 0.0;
    };
    const auto estimate = [&](const char* name, TemplateValue value) {
        if (v.contains(name))
            return;
        v[name] = std::move(value);
        c.estimated.insert(name);
    };
    // The filament's flow, mm3/s: its own cap, else the hot end's, else the
    // print's; none, a safe one for any common hot end and filament.
    constexpr double kSafeFlow = 8.0;
    const double flow = s.filament_max_flow > 0.0  ? s.filament_max_flow
                        : s.machine_max_flow > 0.0 ? s.machine_max_flow
                        : s.speed_max_flow > 0.0   ? s.speed_max_flow
                                                   : 0.0;
    for (const char* name : {"flush_volumetric_speeds", "filament_max_volumetric_speed"}) {
        if (flow > 0.0)
            set(name, list_of(number(flow)));
        else
            estimate(name, list_of(number(kSafeFlow)));
    }
    const double outer_speed = s.outer_wall_speed.of(s.wall_speed);
    const double outer_width = s.outer_wall_width > 0.0 ? s.outer_wall_width : auto_wall_width(s.nozzle_diameter);
    if (const double outer = outer_speed * outer_width * s.layer_height; outer > 0.0)
        set("outer_wall_volumetric_speed", number(flow > 0.0 ? std::min(outer, flow) : outer));
    else
        estimate("outer_wall_volumetric_speed", number(flow > 0.0 ? flow : kSafeFlow));
    // The filament's temperature, from machine.variables.
    double nozzle = first_number("temperature");
    if (nozzle <= 0.0)
        nozzle = first_number("first_layer_temperature");
    for (const char* name : {"flush_temperatures", "nozzle_temperature_range_high", "nozzle_temperature_range_low"})
        if (nozzle > 0.0)
            set(name, list_of(whole(nozzle)));
    for (const char* name : {"initial_no_support_extruder", "initial_extruder", "initial_filament_id",
                             "initial_no_support_filament_id", "initial_nozzle_id", "current_filament_id"})
        set(name, TemplateValue::of(0LL));
    // The tool a filament's G-code runs for (PrusaSlicer's), the hot ends
    // and the filament map of the OrcaSlicer family (1-based), measured with
    // the profile pack; Cura's print_temperature is the filament's.
    set("filament_extruder_id", TemplateValue::of(0LL));
    for (const char* name : {"initial_no_support_hotend", "current_hotend", "first_non_support_hotend"})
        set(name, TemplateValue::of(0LL));
    set("filament_map", list_of(TemplateValue::of(1LL)));
    set("first_filaments", list_of(TemplateValue::of(0LL)));
    set("total_toolchanges", TemplateValue::of(0LL));
    if (nozzle > 0.0)
        set("print_temperature", whole(nozzle));
    // The rest of Bambu Lab's, measured with the profile pack (rule 10 quater).
    set("first_non_support_filaments", list_of(TemplateValue::of(0LL)));
    set("filament_ids", list_of(TemplateValue::of(std::string())));
    set("curr_y_acceleration_limit", number(s.max_accel_y));
    // By physical nozzle, from 0; the nozzle of the first filament that is
    // not support, fixed at the start (the Bambu Lab H3C indexes the list
    // with it); the safe temperature for extruding the fins, 0 (off) when the
    // pack does not give it.
    {
        TemplateValue diameters = list_of(number(s.nozzle_diameter));
        if (!s.nozzle_diameters.empty()) {
            diameters.list.clear();
            for (const double d : s.nozzle_diameters)
                diameters.list.push_back(number(d));
        }
        set("nozzle_diameter_at_nozzle_id", std::move(diameters));
    }
    set("initial_no_support_nozzle_id", TemplateValue::of(0LL));
    set("fins_extrude_safe_temp", list_of(TemplateValue::of(0LL)));
    // Whether a first-layer filament is TPU; the current filament's long
    // retraction on an extruder change, false unless the pack gives it.
    {
        std::string type = s.filament_type;
        for (char& ch : type)
            ch = char(std::toupper(static_cast<unsigned char>(ch)));
        set("has_tpu_in_first_layer", TemplateValue::of(type == "TPU"));
    }
    set("long_retraction_when_ec", TemplateValue::of(false));
    set("overall_chamber_temperature", whole(s.chamber_temperature));
    set("min_vitrification_temperature", TemplateValue::of(0LL));
    set("temperature_vitrification", list_of(TemplateValue::of(0LL)));
    set("hold_chamber_temp_for_flat_print", TemplateValue::of(false));
    set("additional_fan_full_speed_layer", list_of(TemplateValue::of(0LL)));
    set("close_additional_fan_first_x_layers", list_of(TemplateValue::of(0LL)));

    // What the machine's G-code needs to take the same branches as in the
    // program it comes from (gcode_macros.md, rule 9 bis, GM22).
    {
        const std::size_t heads = std::size_t(std::max(s.extruders, 1));
        const auto each = [&](TemplateValue value) {
            return TemplateValue::of(std::vector<TemplateValue>(heads, value));
        };
        // The value when the profile does not say; the pack's own values
        // replace it.
        const auto fallback = [&](const char* name, TemplateValue value) {
            if (v.contains(name))
                return;
            v[name] = std::move(value);
            c.defaults.insert(name);
        };
        fallback("filament_abrasive", each(TemplateValue::of(s.abrasive)));
        TemplateValue high = TemplateValue::of(std::vector<TemplateValue>{}), kinds = high;
        for (std::size_t k = 0; k < heads; ++k) {
            const bool hf = !s.high_flow_nozzle.empty() && s.high_flow_nozzle[std::min(k, s.high_flow_nozzle.size() - 1)];
            high.list.push_back(TemplateValue::of(hf));
            kinds.list.push_back(TemplateValue::of(std::string(hf ? "High Flow" : "Standard")));
        }
        set("nozzle_high_flow", high);
        set("nozzle_volume_type", kinds);
        set("nozzle_volume_types", kinds);
        set("zhop", number(s.retract_lift));
        set("outer_wall_acceleration",
            list_of(number(s.accel_outer_wall > 0.0 ? s.accel_outer_wall : s.acceleration)));
        set("print_sequence", TemplateValue::of(std::string(s.sequential ? "by object" : "by layer")));
        fallback("timelapse_type", TemplateValue::of(0LL));
        fallback("has_single_extruder_multi_material_priming", TemplateValue::of(false));
        fallback("long_retraction_when_cut", TemplateValue::of(false));
        fallback("notes", TemplateValue::of(std::string()));
        set("build_volume_temperature", whole(s.chamber_temperature));
        // The second round (QA, 17-11-2026, GM22).
        set("wipe_tower_center_pos_valid", TemplateValue::of(s.wipe_tower));
        // The tower's centre from its corner and width, with or without a
        // tower (QA, 17-11-2026).
        set("wipe_tower_center_pos_x", number(s.wipe_tower_x + s.wipe_tower_width / 2.0));
        set("wipe_tower_center_pos_y", number(s.wipe_tower_y + s.wipe_tower_width / 2.0));
        set("jobname", TemplateValue::of(s.input_name));
        set("start_filament_gcode", each(TemplateValue::of(s.filament_start_gcode)));
        fallback("required_nozzle_HRC", each(TemplateValue::of(0LL)));
        fallback("filament_flush_volumetric_speed", each(number(flow > 0.0 ? flow : kSafeFlow)));
        fallback("filament_notes", each(TemplateValue::of(std::string())));
        // cura's own defaults: the heat zone's 16 mm, and the hop of a
        // retraction.
        fallback("switch_extruder_retraction_amount", number(16.0));
        fallback("retraction_hop_height_after_extruder_switch", number(s.retract_lift));
        set("inner_wall_speed", each(number(s.wall_speed)));
        set("outer_wall_speed", each(number(outer_speed)));
        set("initial_layer_speed",
            each(number(s.first_layer_speed_percent ? s.wall_speed * s.first_layer_speed / 100.0 : s.first_layer_speed)));
        set("outer_wall_line_width", number(outer_width));
        set("line_width", number(s.wall_width > 0.0 ? s.wall_width : auto_wall_width(s.nozzle_diameter)));
        set("extrusion_width", number(s.wall_width > 0.0 ? s.wall_width : auto_wall_width(s.nozzle_diameter)));
        set("top_bottom_thickness", number(s.top_layers * s.layer_height));
        // filament_cost is the print's cost in cura (the writer sets it as it
        // goes) and the price per kg elsewhere.
        if (s.machine_macros.dialect == "cura") {
            set("filament_amount", number(0.0));
            set("filament_cost", number(0.0));
            // The estimate so far, as cura writes it (the writer updates it).
            set("print_time", TemplateValue::of(std::string("0h 0m 0s")));
        } else {
            set("filament_cost", each(number(s.filament_cost)));
        }
        const double width = s.wall_width > 0.0 ? s.wall_width : auto_wall_width(s.nozzle_diameter);
        set("wall_thickness", number(s.wall_count * width));
        if (const double idle = first_number("idle_temperature"); idle > 0.0 || nozzle > 0.0)
            set("material_standby_temperature", whole(idle > 0.0 ? idle : nozzle));
        // Where the nozzle is and what is used; the writer sets them again at
        // each layer change and at the end.
        set("position", TemplateValue::of(std::vector<TemplateValue>{number(0.0), number(0.0), number(0.0)}));
        set("e_retracted", each(number(0.0)));
        set("extruded_volume", each(number(0.0)));
        set("extruded_weight_total", number(0.0));
        set("filament_weight", number(0.0));
        // The profiles' names: the print preset is the print profile, the
        // physical printer the printer (none named apart), cura's machine_name
        // the printer.
        const auto alias = [&](const char* name, const char* from) {
            if (const auto it = v.find(from); it != v.end())
                set(name, it->second);
        };
        alias("print_preset", "print_settings_id");
        alias("printer_preset", "printer_settings_id");
        alias("physical_printer_preset", "printer_settings_id");
        alias("machine_name", "printer_settings_id");
        // cura's date and time of slicing.
        const std::time_t now = std::time(nullptr);
        std::tm local{};
#ifdef _WIN32
        localtime_s(&local, &now);
#else
        localtime_r(&now, &local);
#endif
        char text[32];
        std::strftime(text, sizeof text, "%d-%m-%Y", &local);
        set("date", TemplateValue::of(std::string(text)));
        std::strftime(text, sizeof text, "%H:%M:%S", &local);
        set("time", TemplateValue::of(std::string(text)));
    }

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
            // Through the base's equivalent too (nozzle_temperature_initial_layer_1),
            // and the last value for a head beyond the list (rule 8).
            std::string base_name = name.substr(0, under);
            if (!v.contains(base_name))
                for (const MacroVariable& e : kVariables)
                    if (e.name == base_name && !e.equivalent.empty()) {
                        base_name = std::string(e.equivalent);
                        break;
                    }
            const auto base = v.find(base_name);
            const std::size_t at = std::stoul(name.substr(under + 1));
            if (base != v.end() && base->second.kind == TemplateValue::Kind::List && !base->second.list.empty()) {
                v[name] = base->second.list[std::min(at, base->second.list.size() - 1)];
                continue;
            }
        }
        v[name] = neutral(m.type);
        c.unfilled.insert(name);
    }
}

void name_profiles(Settings& settings, const std::string& printer, const std::string& filament,
                   const std::string& print) {
    for (const auto& [name, value] : {std::pair{"printer_settings_id", &printer},
                                      std::pair{"filament_settings_id", &filament},
                                      std::pair{"print_settings_id", &print}})
        if (!value->empty() && !settings.variables.contains(name))
            settings.variables[name] = {*value};
}

} // namespace tisma
