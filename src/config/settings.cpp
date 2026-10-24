// Reader for the engine's settings format (docs/CONFIG.md), on nlohmann/json
// (MIT). Own design; see PROVENANCE.md.

#include "config/settings.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <map>
#include <sstream>

namespace tisma {
namespace {

using nlohmann::json;

const std::vector<std::string> kPlannedGroups = {"material", "cooling", "support"};

[[noreturn]] void fail(const std::string& key, const std::string& message) {
    throw SettingsError("settings: " + key + ": " + message);
}

double number(const json& group, const std::string& name, const std::string& key) {
    const json& v = group.at(name);
    if (!v.is_number())
        fail(key, "must be a number");
    return v.get<double>();
}

template <typename Enum>
Enum choice(const json& group, const std::string& name, const std::string& key, const std::map<std::string, Enum>& options) {
    const json& v = group.at(name);
    if (!v.is_string())
        fail(key, "must be a string");
    const auto it = options.find(v.get<std::string>());
    if (it == options.end()) {
        std::string list;
        for (const auto& [option, value] : options)
            list += (list.empty() ? "" : ", ") + option;
        fail(key, "must be one of " + list);
    }
    return it->second;
}

// Calls read(name, key) for every member of the group; rejects unknown names.
template <typename Read>
void read_group(const json& root, const std::string& group_name, const std::vector<std::string>& known, Read read) {
    if (!root.contains(group_name))
        return;
    const json& group = root.at(group_name);
    if (!group.is_object())
        fail(group_name, "must be an object");
    for (const auto& [name, value] : group.items()) {
        const std::string key = group_name + "." + name;
        if (std::find(known.begin(), known.end(), name) == known.end())
            fail(key, "unknown setting (see docs/CONFIG.md)");
        read(group, name, key);
    }
}

} // namespace

Settings parse_settings(std::string_view text) {
    json root;
    try {
        root = json::parse(text.begin(), text.end());
    } catch (const json::parse_error& e) {
        throw SettingsError(std::string("settings: not valid JSON: ") + e.what());
    }
    if (!root.is_object())
        throw SettingsError("settings: the top level must be an object");

    Settings s;
    for (const auto& [group, value] : root.items()) {
        if (group == "layers" || group == "slicing" || group == "walls" || group == "surfaces" || group == "infill" ||
            group == "machine" || group == "speed" || group == "retraction" || group == "compute")
            continue;
        if (std::find(kPlannedGroups.begin(), kPlannedGroups.end(), group) != kPlannedGroups.end())
            s.ignored_groups.push_back(group);
        else
            fail(group, "unknown group (see docs/CONFIG.md)");
    }

    read_group(root, "layers", {"height", "first_height"}, [&](const json& g, const std::string& n, const std::string& k) {
        const double v = number(g, n, k);
        if (!(v > 0.0))
            fail(k, "must be greater than 0");
        (n == "height" ? s.layer_height : s.first_layer_height) = v;
    });
    read_group(root, "slicing", {"fill_mode", "gap_closing_radius", "xy_offset", "first_layer_shrink", "simplify_tolerance"},
               [&](const json& g, const std::string& n, const std::string& k) {
                   if (n == "fill_mode") {
                       s.fill_mode = choice<FillMode>(
                           g, n, k,
                           {{"union", FillMode::Union}, {"even_odd", FillMode::EvenOdd}, {"fill_holes", FillMode::FillHoles}});
                       return;
                   }
                   const double v = number(g, n, k);
                   if (n == "xy_offset") {
                       s.xy_offset = v;
                       return;
                   }
                   if (v < 0.0)
                       fail(k, "must not be negative");
                   if (n == "gap_closing_radius")
                       s.gap_closing_radius = v;
                   else if (n == "first_layer_shrink")
                       s.first_layer_shrink = v;
                   else
                       s.simplify_tolerance = v;
               });
    read_group(root, "walls", {"count", "width", "seam"}, [&](const json& g, const std::string& n, const std::string& k) {
        if (n == "seam") {
            s.seam = choice<SeamMode>(g, n, k,
                                      {{"aligned", SeamMode::Aligned}, {"nearest", SeamMode::Nearest},
                                       {"random", SeamMode::Random}, {"rear", SeamMode::Rear}});
            return;
        }
        if (n == "count") {
            const json& v = g.at(n);
            if (!v.is_number_integer() || v.get<long long>() < 0 || v.get<long long>() > 1000)
                fail(k, "must be a whole number from 0 to 1000");
            s.wall_count = static_cast<int>(v.get<long long>());
            return;
        }
        const double v = number(g, n, k);
        if (!(v > 0.0))
            fail(k, "must be greater than 0");
        s.wall_width = v;
    });
    read_group(root, "surfaces", {"top_layers", "bottom_layers", "top_pattern"}, [&](const json& g, const std::string& n,
                                                                                  const std::string& k) {
        if (n == "top_pattern") {
            s.monotonic_top = choice<bool>(g, n, k, {{"monotonic", true}, {"rectilinear", false}});
            return;
        }
        const json& v = g.at(n);
        if (!v.is_number_integer() || v.get<long long>() < 0 || v.get<long long>() > 1000)
            fail(k, "must be a whole number from 0 to 1000");
        (n == "top_layers" ? s.top_layers : s.bottom_layers) = static_cast<int>(v.get<long long>());
    });
    read_group(root, "infill", {"density", "angle", "pattern", "wall_overlap"}, [&](const json& g, const std::string& n,
                                                                                const std::string& k) {
        if (n == "pattern") {
            s.infill_pattern =
                choice<InfillPattern>(g, n, k,
                                      {{"rectilinear", InfillPattern::Rectilinear},
                                       {"grid", InfillPattern::Grid},
                                       {"gyroid", InfillPattern::Gyroid}});
            return;
        }
        const double v = number(g, n, k);
        if (n == "density" || n == "wall_overlap") {
            if (v < 0.0 || v > 100.0)
                fail(k, "must be from 0 to 100");
            (n == "density" ? s.infill_density : s.infill_wall_overlap) = v;
        } else {
            s.infill_angle = v;
        }
    });
    read_group(root, "machine",
               {"nozzle_diameter", "filament_diameter", "relative_extrusion", "flavor", "start_gcode", "end_gcode", "variables"},
               [&](const json& g, const std::string& n, const std::string& k) {
                   const json& v = g.at(n);
                   if (n == "relative_extrusion") {
                       if (!v.is_boolean())
                           fail(k, "must be true or false");
                       s.relative_extrusion = v.get<bool>();
                   } else if (n == "flavor") {
                       s.flavor = choice<GcodeFlavor>(
                           g, n, k, {{"marlin", GcodeFlavor::Marlin}, {"klipper", GcodeFlavor::Klipper}, {"reprap", GcodeFlavor::RepRap}});
                   } else if (n == "start_gcode" || n == "end_gcode") {
                       if (!v.is_string())
                           fail(k, "must be a string");
                       (n == "start_gcode" ? s.start_gcode : s.end_gcode) = v.get<std::string>();
                   } else if (n == "variables") {
                       if (!v.is_object())
                           fail(k, "must be an object of name: value or [values]");
                       auto as_text = [&](const json& x, const std::string& key) {
                           if (x.is_string())
                               return x.get<std::string>();
                           if (x.is_number() || x.is_boolean())
                               return x.dump();
                           fail(key, "values must be strings, numbers or booleans");
                       };
                       for (const auto& [name, value] : v.items()) {
                           std::vector<std::string> list;
                           if (value.is_array())
                               for (const json& item : value)
                                   list.push_back(as_text(item, k + "." + name));
                           else
                               list.push_back(as_text(value, k + "." + name));
                           s.variables[name] = std::move(list);
                       }
                   } else {
                       const double d = number(g, n, k);
                       if (!(d > 0.0))
                           fail(k, "must be greater than 0");
                       (n == "nozzle_diameter" ? s.nozzle_diameter : s.filament_diameter) = d;
                   }
               });
    read_group(root, "speed",
               {"walls", "outer_wall", "infill", "solid_infill", "top_solid_infill", "travel", "first_layer", "max"},
               [&](const json& g, const std::string& n, const std::string& k) {
                   const json& v = g.at(n);
                   if (n == "outer_wall" && v.is_string()) {
                       const std::string t = v.get<std::string>();
                       char* end = nullptr;
                       const double p = std::strtod(t.c_str(), &end);
                       if (t.empty() || t.back() != '%' || end != t.c_str() + t.size() - 1 || !(p > 0.0))
                           fail(k, "must be a number of mm/s or a percentage like \"50%\"");
                       s.outer_wall_speed = {p, true};
                       return;
                   }
                   const double d = number(g, n, k);
                   if (!(d > 0.0))
                       fail(k, "must be greater than 0");
                   if (n == "walls")
                       s.wall_speed = d;
                   else if (n == "outer_wall")
                       s.outer_wall_speed = {d, false};
                   else if (n == "infill")
                       s.infill_speed = d;
                   else if (n == "solid_infill")
                       s.solid_infill_speed = d;
                   else if (n == "top_solid_infill")
                       s.top_solid_infill_speed = d;
                   else if (n == "travel")
                       s.travel_speed = d;
                   else if (n == "first_layer")
                       s.first_layer_speed = d;
                   else
                       s.max_speed = d;
               });
    read_group(root, "retraction", {"length", "speed", "lift", "min_travel"},
               [&](const json& g, const std::string& n, const std::string& k) {
                   const double d = number(g, n, k);
                   if (n == "speed") {
                       if (!(d > 0.0))
                           fail(k, "must be greater than 0");
                       s.retract_speed = d;
                       return;
                   }
                   if (d < 0.0)
                       fail(k, "must not be negative");
                   (n == "length" ? s.retract_length : n == "lift" ? s.retract_lift : s.retract_min_travel) = d;
               });
    read_group(root, "compute", {"gpu"}, [&](const json& g, const std::string& n, const std::string& k) {
        s.gpu = choice<GpuMode>(g, n, k, {{"auto", GpuMode::Auto}, {"off", GpuMode::Off}});
    });
    return s;
}

std::vector<std::pair<std::string, std::string>> describe(const Settings& s) {
    auto num = [](double v) { return json(v).dump(); };
    auto flag = [](bool b) { return std::string(b ? "true" : "false"); };
    auto text = [](const std::string& t) { return json(t).dump(); };
    const char* fill_mode = s.fill_mode == FillMode::Union ? "union" : s.fill_mode == FillMode::EvenOdd ? "even_odd" : "fill_holes";
    const char* pattern = s.infill_pattern == InfillPattern::Rectilinear ? "rectilinear"
                          : s.infill_pattern == InfillPattern::Grid      ? "grid"
                                                                         : "gyroid";
    const char* flavor = s.flavor == GcodeFlavor::Marlin ? "marlin" : s.flavor == GcodeFlavor::Klipper ? "klipper" : "reprap";
    std::vector<std::pair<std::string, std::string>> out = {
        {"layers.height", num(s.layer_height)},
        {"layers.first_height", num(s.first_layer_height)},
        {"slicing.fill_mode", fill_mode},
        {"slicing.gap_closing_radius", num(s.gap_closing_radius)},
        {"slicing.xy_offset", num(s.xy_offset)},
        {"slicing.first_layer_shrink", num(s.first_layer_shrink)},
        {"slicing.simplify_tolerance", num(s.simplify_tolerance)},
        {"walls.count", std::to_string(s.wall_count)},
        {"walls.width", num(s.wall_width)},
        {"walls.seam", s.seam == SeamMode::Aligned ? "aligned" : s.seam == SeamMode::Nearest ? "nearest"
                       : s.seam == SeamMode::Random  ? "random" : "rear"},
        {"surfaces.top_layers", std::to_string(s.top_layers)},
        {"surfaces.bottom_layers", std::to_string(s.bottom_layers)},
        {"surfaces.top_pattern", s.monotonic_top ? "monotonic" : "rectilinear"},
        {"infill.density", num(s.infill_density)},
        {"infill.angle", num(s.infill_angle)},
        {"infill.pattern", pattern},
        {"infill.wall_overlap", num(s.infill_wall_overlap)},
        {"machine.nozzle_diameter", num(s.nozzle_diameter)},
        {"machine.filament_diameter", num(s.filament_diameter)},
        {"machine.relative_extrusion", flag(s.relative_extrusion)},
        {"machine.flavor", flavor},
        {"machine.start_gcode", text(s.start_gcode)},
        {"machine.end_gcode", text(s.end_gcode)},
        {"speed.walls", num(s.wall_speed)},
        {"speed.outer_wall", s.outer_wall_speed.percent ? num(s.outer_wall_speed.value) + "%" : num(s.outer_wall_speed.value)},
        {"speed.infill", num(s.infill_speed)},
        {"speed.solid_infill", num(s.solid_infill_speed)},
        {"speed.top_solid_infill", num(s.top_solid_infill_speed)},
        {"speed.travel", num(s.travel_speed)},
        {"speed.first_layer", num(s.first_layer_speed)},
        {"speed.max", num(s.max_speed)},
        {"retraction.length", num(s.retract_length)},
        {"retraction.speed", num(s.retract_speed)},
        {"retraction.lift", num(s.retract_lift)},
        {"retraction.min_travel", num(s.retract_min_travel)},
        {"compute.gpu", s.gpu == GpuMode::Auto ? "auto" : "off"},
    };
    for (const auto& [name, values] : s.variables) {
        std::string v;
        for (const std::string& item : values)
            v += (v.empty() ? "" : ",") + item;
        out.push_back({"machine.variables." + name, v});
    }
    return out;
}

Settings load_settings(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in)
        throw SettingsError("settings: cannot open '" + path.string() + "'");
    std::ostringstream text;
    text << in.rdbuf();
    return parse_settings(text.str());
}

} // namespace tisma
