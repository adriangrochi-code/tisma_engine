// Reader for the engine's settings format (docs/CONFIG.md), on nlohmann/json
// (MIT). Own design; see PROVENANCE.md.

#include "config/settings.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace tisma {
namespace {

using nlohmann::json;

const std::vector<std::string> kPlannedGroups = {"material"};

// The speeds a G-code needs, in the order of docs/CONFIG.md, and what a stock
// Creality Ender-3 prints at, for a printer whose profile has none (owner's
// decision, 2026-10-26; the numbers and their sources in docs/CONFIG.md,
// "Velocidades de respaldo").
struct NeededSpeed {
    const char* key;
    double fallback; // mm/s
};
constexpr NeededSpeed kNeededSpeeds[] = {
    {"machine.corner_speed", 10.0}, {"speed.walls", 40.0},       {"speed.outer_wall", 30.0},
    {"speed.infill", 60.0},         {"speed.solid_infill", 60.0}, {"speed.top_solid_infill", 40.0},
    {"speed.support", 40.0},        {"speed.travel", 180.0},      {"speed.first_layer", 25.0},
    {"retraction.speed", 25.0},     {"cooling.min_speed", 10.0},
};

// The setting a speed key names (not speed.outer_wall, which may be a share).
template <typename S>
auto speed_of(S& s, std::string_view key) -> decltype(&s.wall_speed) {
    const std::pair<std::string_view, decltype(&s.wall_speed)> speeds[] = {
        {"machine.corner_speed", &s.corner_speed},
        {"speed.walls", &s.wall_speed},
        {"speed.infill", &s.infill_speed},
        {"speed.solid_infill", &s.solid_infill_speed},
        {"speed.top_solid_infill", &s.top_solid_infill_speed},
        {"speed.support", &s.support_speed},
        {"speed.travel", &s.travel_speed},
        {"speed.first_layer", &s.first_layer_speed},
        {"speed.max", &s.max_speed},
        {"retraction.speed", &s.retract_speed},
        {"cooling.min_speed", &s.min_print_speed},
    };
    for (const auto& [name, target] : speeds)
        if (name == key)
            return target;
    throw std::logic_error("no speed " + std::string(key));
}

const char* fuzzy_mode_name(FuzzyMode m) {
    return m == FuzzyMode::External ? "external" : m == FuzzyMode::All ? "all" : "none";
}

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

// The infill patterns by name, for reading and writing.
const std::map<std::string, InfillPattern>& infill_patterns() {
    static const std::map<std::string, InfillPattern> names = {
        {"rectilinear", InfillPattern::Rectilinear},
        {"aligned_rectilinear", InfillPattern::AlignedRectilinear},
        {"zigzag", InfillPattern::Zigzag},
        {"grid", InfillPattern::Grid},
        {"gyroid", InfillPattern::Gyroid},
        {"triangles", InfillPattern::Triangles},
        {"stars", InfillPattern::Stars},
        {"cubic", InfillPattern::Cubic},
        {"adaptive_cubic", InfillPattern::AdaptiveCubic},
        {"support_cubic", InfillPattern::SupportCubic},
        {"lightning", InfillPattern::Lightning},
        {"line", InfillPattern::Line},
        {"honeycomb", InfillPattern::Honeycomb},
        {"concentric", InfillPattern::Concentric},
        {"hilbert", InfillPattern::Hilbert},
        {"archimedean", InfillPattern::Archimedean},
        {"octagram", InfillPattern::Octagram}};
    return names;
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

Settings parse_checked(std::string_view text);

} // namespace

const char* infill_pattern_name(InfillPattern pattern) {
    for (const auto& [name, value] : infill_patterns())
        if (value == pattern)
            return name.c_str();
    return "rectilinear";
}

bool json_depth_ok(std::string_view text, int max_depth) {
    int depth = 0;
    bool in_string = false, escaped = false;
    for (const char c : text) {
        if (in_string) {
            if (escaped)
                escaped = false;
            else if (c == '\\')
                escaped = true;
            else if (c == '"')
                in_string = false;
        } else if (c == '"') {
            in_string = true;
        } else if (c == '[' || c == '{') {
            if (++depth > max_depth)
                return false;
        } else if (c == ']' || c == '}') {
            --depth;
        }
    }
    return true;
}

Settings parse_settings(std::string_view text) {
    if (!json_depth_ok(text))
        throw SettingsError("settings: nested more than 64 levels deep");
    try {
        return parse_checked(text);
    } catch (const json::exception& e) {
        // A number out of range, a value of the wrong type deep inside: still
        // a bad settings file, not an internal error.
        throw SettingsError(std::string("settings: ") + e.what());
    }
}

namespace {

Settings parse_checked(std::string_view text) {
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
            group == "machine" || group == "speed" || group == "retraction" || group == "cooling" || group == "adhesion" || group == "support" ||
            group == "compute" || group == "compensation" || group == "extrusion" || group == "changes" || group == "zaa" || group == "gcode" ||
            group == "fuzzy" || group == "vase")
            continue;
        if (std::find(kPlannedGroups.begin(), kPlannedGroups.end(), group) != kPlannedGroups.end())
            s.ignored_groups.push_back(group);
        else
            fail(group, "unknown group (see docs/CONFIG.md)");
    }

    read_group(root, "layers",
               {"height", "first_height", "detail_height", "detail_tolerance", "detail_margin", "heights", "cusp_height"},
               [&](const json& g, const std::string& n, const std::string& k) {
        if (n == "heights") {
            const json& v = g.at(n);
            const char* what = "must be a list of [z, height] pairs in mm, sorted by z, heights from 0.01 to 10";
            if (!v.is_array())
                fail(k, what);
            std::vector<std::array<double, 2>> steps;
            for (const json& p : v) {
                if (!p.is_array() || p.size() != 2 || !p[0].is_number() || !p[1].is_number())
                    fail(k, what);
                const double z = p[0].get<double>(), h = p[1].get<double>();
                if (!(z >= 0.0 && z <= 10000.0) || !(h >= 0.01 && h <= 10.0) || (!steps.empty() && z <= steps.back()[0]))
                    fail(k, what);
                steps.push_back({z, h});
            }
            s.layer_heights = std::move(steps);
            return;
        }
        const double v = number(g, n, k);
        if (n == "cusp_height") {
            if (v < 0.0 || v > 1.0)
                fail(k, "must be from 0 to 1 mm");
            s.cusp_height = v;
            return;
        }
        if (n == "detail_height" || n == "detail_margin") {
            if (v < 0.0 || v > 10.0)
                fail(k, "must be from 0 to 10 mm");
            (n == "detail_height" ? s.detail_height : s.detail_margin) = v;
            return;
        }
        if (n == "detail_tolerance") {
            if (v < 0.001 || v > 1.0)
                fail(k, "must be from 0.001 to 1 mm");
            s.detail_tolerance = v;
            return;
        }
        // Below 0.01 mm the layers run to the hundreds of thousands; the
        // nozzle limits them from above (check_settings): pellet printers
        // with 5 to 8 mm nozzles lay 2.5 to 4 mm layers (pellets.md).
        if (!(v >= 0.01 && v <= 10.0))
            fail(k, "must be from 0.01 to 10 mm");
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
                   // Offsets beyond these overflow the polygon library or
                   // empty the part.
                   if (n == "xy_offset") {
                       if (!(v >= -10.0 && v <= 10.0))
                           fail(k, "must be from -10 to 10 mm");
                       s.xy_offset = v;
                       return;
                   }
                   if (v < 0.0)
                       fail(k, "must not be negative");
                   if (n == "gap_closing_radius")
                       s.gap_closing_radius = v;
                   else if (n == "first_layer_shrink") {
                       if (v > 2.0)
                           fail(k, "must be from 0 to 2 mm");
                       s.first_layer_shrink = v;
                   }
                   else
                       s.simplify_tolerance = v;
               });
    read_group(root, "walls",
               {"count", "width", "seam", "scarf", "scarf_only_smooth", "scarf_start_height", "scarf_length",
                "scarf_entire_loop", "scarf_max_segment", "scarf_inner"},
               [&](const json& g, const std::string& n, const std::string& k) {
        if (n == "seam") {
            s.seam = choice<SeamMode>(g, n, k,
                                      {{"aligned", SeamMode::Aligned}, {"nearest", SeamMode::Nearest},
                                       {"random", SeamMode::Random}, {"rear", SeamMode::Rear}});
            return;
        }
        if (n == "scarf") {
            s.scarf = choice<ScarfMode>(g, n, k,
                                        {{"nowhere", ScarfMode::Nowhere}, {"contours", ScarfMode::Contours},
                                         {"everywhere", ScarfMode::Everywhere}});
            return;
        }
        if (n == "scarf_only_smooth" || n == "scarf_entire_loop" || n == "scarf_inner") {
            if (!g.at(n).is_boolean())
                fail(k, "must be true or false");
            (n == "scarf_only_smooth"   ? s.scarf_only_smooth
             : n == "scarf_entire_loop" ? s.scarf_entire_loop
                                        : s.scarf_inner) = g.at(n).get<bool>();
            return;
        }
        if (n == "scarf_start_height") {
            const double v = number(g, n, k);
            if (!(v >= 0.0 && v <= 100.0))
                fail(k, "must be from 0 to 100 %");
            s.scarf_start_height = v;
            return;
        }
        if (n == "scarf_length" || n == "scarf_max_segment") {
            const double v = number(g, n, k);
            if (!(v > 0.0 && v <= 1000.0))
                fail(k, "must be more than 0 and at most 1000 mm");
            (n == "scarf_length" ? s.scarf_length : s.scarf_max_segment) = v;
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
        if (!(v >= 0.05 && v <= 40.0)) // and a quarter to four nozzles (check_settings; pellets.md)
            fail(k, "must be from 0.05 to 40 mm");
        s.wall_width = v;
    });
    read_group(root, "vase", {"enabled"}, [&](const json& g, const std::string& n, const std::string& k) {
        if (!g.at(n).is_boolean())
            fail(k, "must be true or false");
        s.vase = g.at(n).get<bool>();
    });
    read_group(root, "surfaces", {"top_layers", "bottom_layers", "top_pattern", "top_min_thickness",
                                  "bottom_min_thickness"},
               [&](const json& g, const std::string& n, const std::string& k) {
        if (n == "top_min_thickness" || n == "bottom_min_thickness") {
            const double v = number(g, n, k);
            if (v < 0.0 || v > 100.0)
                fail(k, "must be from 0 to 100 mm");
            (n == "top_min_thickness" ? s.top_min_thickness : s.bottom_min_thickness) = v;
            return;
        }
        if (n == "top_pattern") {
            s.monotonic_top = choice<bool>(g, n, k, {{"monotonic", true}, {"rectilinear", false}});
            return;
        }
        const json& v = g.at(n);
        if (!v.is_number_integer() || v.get<long long>() < 0 || v.get<long long>() > 1000)
            fail(k, "must be a whole number from 0 to 1000");
        (n == "top_layers" ? s.top_layers : s.bottom_layers) = static_cast<int>(v.get<long long>());
    });
    read_group(root, "infill", {"density", "angle", "pattern", "wall_overlap", "combine_layers", "combine_auto",
                                "combine_max_height", "dense_under_top", "dense_density", "stress_density",
                                "solid_every", "solid_below_area", "nonplanar", "nonplanar_amplitude",
                                "nonplanar_wavelength"},
               [&](const json& g, const std::string& n, const std::string& k) {
        if (n == "combine_auto" || n == "dense_under_top" || n == "nonplanar") {
            if (!g.at(n).is_boolean())
                fail(k, "must be true or false");
            (n == "combine_auto"      ? s.infill_combine_auto
             : n == "dense_under_top" ? s.infill_dense_under_top
                                      : s.infill_nonplanar) = g.at(n).get<bool>();
            return;
        }
        if (n == "nonplanar_amplitude") {
            const double v = number(g, n, k);
            if (v < 0.0 || v > 500.0)
                fail(k, "must be from 0 to 500 (percent of the layer height)");
            s.infill_nonplanar_amplitude = v;
            return;
        }
        if (n == "nonplanar_wavelength") {
            const double v = number(g, n, k);
            if (v < 0.5 || v > 20.0)
                fail(k, "must be from 0.5 to 20 mm");
            s.infill_nonplanar_wavelength = v;
            return;
        }
        if (n == "dense_density") {
            const double v = number(g, n, k);
            if (!(v > 0.0) || v > 100.0)
                fail(k, "must be from 0 to 100");
            s.infill_dense_density = v;
            return;
        }
        if (n == "solid_every") {
            const json& v = g.at(n);
            if (!v.is_number_integer() || v.get<long long>() < 0 || v.get<long long>() > 1000)
                fail(k, "must be a whole number from 0 to 1000");
            s.infill_solid_every = static_cast<int>(v.get<long long>());
            return;
        }
        if (n == "solid_below_area") {
            const double v = number(g, n, k);
            if (v < 0.0 || v > 10000.0)
                fail(k, "must be from 0 to 10000 mm2");
            s.infill_solid_below_area = v;
            return;
        }
        if (n == "stress_density") {
            const double v = number(g, n, k);
            if (v < 0.0 || v > 100.0)
                fail(k, "must be from 0 to 100");
            s.infill_stress_density = v;
            return;
        }
        if (n == "combine_max_height") {
            const json& v = g.at(n);
            if (v.is_string()) {
                const std::string t = v.get<std::string>();
                char* end = nullptr;
                const double p = std::strtod(t.c_str(), &end);
                if (t.empty() || t.back() != '%' || end != t.c_str() + t.size() - 1 || !(p > 0.0) || p > 1000.0)
                    fail(k, "must be a number of mm or a percentage of the nozzle like \"100%\"");
                s.infill_combine_max_height = {p, true};
                return;
            }
            const double d = number(g, n, k);
            if (!(d > 0.0) || d > 10.0)
                fail(k, "must be from 0 to 10 mm, or a percentage of the nozzle");
            s.infill_combine_max_height = {d, false};
            return;
        }
        if (n == "combine_layers") {
            const double v = number(g, n, k);
            if (v != std::floor(v) || v < 1.0 || v > 20.0)
                fail(k, "must be a whole number from 1 to 20");
            s.infill_combine_layers = int(v);
            return;
        }
        if (n == "pattern") {
            s.infill_pattern = choice<InfillPattern>(g, n, k, infill_patterns());
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
               {"nozzle_diameter", "filament_diameter", "relative_extrusion", "flavor", "start_gcode", "end_gcode", "variables",
                "max_feed_x", "max_feed_y", "max_feed_z", "max_feed_e", "acceleration", "travel_acceleration", "corner_speed",
                "bed", "max_height", "max_flow", "max_nozzle_temp", "max_bed_temp", "max_chamber_temp", "enclosed",
                "hardened_nozzle", "min_layer_height", "max_layer_height"},
               [&](const json& g, const std::string& n, const std::string& k) {
                   const json& v = g.at(n);
                   if (v.is_null() && (n.starts_with("max_feed_") || n == "corner_speed")) {
                       // Not set: the printer's profile gives it, or nothing does (docs/CONFIG.md).
                       (n == "max_feed_x"   ? s.max_feed_x
                        : n == "max_feed_y" ? s.max_feed_y
                        : n == "max_feed_z" ? s.max_feed_z
                        : n == "max_feed_e" ? s.max_feed_e
                                            : s.corner_speed) = 0.0;
                       return;
                   }
                   if (n == "relative_extrusion" || n == "enclosed" || n == "hardened_nozzle") {
                       if (!v.is_boolean())
                           fail(k, "must be true or false");
                       (n == "relative_extrusion" ? s.relative_extrusion : n == "enclosed" ? s.enclosed : s.hardened_nozzle) =
                           v.get<bool>();
                   } else if (n == "max_flow" || n == "max_nozzle_temp" || n == "max_bed_temp" || n == "max_chamber_temp") {
                       const double d = number(g, n, k);
                       if (d < 0.0 || d > (n == "max_flow" ? 1000.0 : 600.0))
                           fail(k, n == "max_flow" ? "must be from 0 to 1000 mm3/s" : "must be from 0 to 600 C");
                       (n == "max_flow"          ? s.machine_max_flow
                        : n == "max_nozzle_temp" ? s.max_nozzle_temp
                        : n == "max_bed_temp"    ? s.max_bed_temp
                                                 : s.max_chamber_temp) = d;
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
                           // Names go into [brackets] of the start G-code and
                           // into comments: no line breaks or brackets.
                           if (name.empty() || std::any_of(name.begin(), name.end(), [](char c) {
                                   return static_cast<unsigned char>(c) < 0x20 || c == '[' || c == ']';
                               }))
                               fail(k + "." + name, "names must be text without line breaks or brackets");
                           std::vector<std::string> list;
                           if (value.is_array())
                               for (const json& item : value)
                                   list.push_back(as_text(item, k + "." + name));
                           else
                               list.push_back(as_text(value, k + "." + name));
                           s.variables[name] = std::move(list);
                       }
                   } else if (n == "bed") {
                       // Empty is the default (not checked), so it may be written too.
                       const char* what = "must be empty or a list of at least 3 [x, y] points in mm";
                       if (!v.is_array() || v.size() == 1 || v.size() == 2)
                           fail(k, what);
                       std::vector<std::array<double, 2>> bed;
                       for (const json& p : v) {
                           if (!p.is_array() || p.size() != 2 || !p[0].is_number() || !p[1].is_number())
                               fail(k, what);
                           bed.push_back({p[0].get<double>(), p[1].get<double>()});
                       }
                       s.bed = std::move(bed);
                   } else if (n == "min_layer_height" || n == "max_layer_height") {
                       const double d = number(g, n, k);
                       if (d != 0.0 && !(d >= 0.01 && d <= 10.0))
                           fail(k, "must be 0 (unknown) or from 0.01 to 10 mm");
                       (n == "min_layer_height" ? s.min_layer_height : s.max_layer_height) = d;
                   } else if (n == "corner_speed" || n == "max_height") {
                       const double d = number(g, n, k);
                       if (d < 0.0)
                           fail(k, "must not be negative");
                       (n == "corner_speed" ? s.corner_speed : s.max_height) = d;
                   } else {
                       const double d = number(g, n, k);
                       if (!(d > 0.0))
                           fail(k, "must be greater than 0");
                       const std::map<std::string, double*> targets = {
                           {"nozzle_diameter", &s.nozzle_diameter}, {"filament_diameter", &s.filament_diameter},
                           {"max_feed_x", &s.max_feed_x},           {"max_feed_y", &s.max_feed_y},
                           {"max_feed_z", &s.max_feed_z},           {"max_feed_e", &s.max_feed_e},
                           {"acceleration", &s.acceleration},       {"travel_acceleration", &s.travel_acceleration}};
                       *targets.at(n) = d;
                   }
               });
    read_group(root, "speed",
               {"walls", "outer_wall", "infill", "solid_infill", "top_solid_infill", "support", "travel", "first_layer", "max"},
               [&](const json& g, const std::string& n, const std::string& k) {
                   const json& v = g.at(n);
                   if (v.is_null()) { // not set: from the printer's profile
                       if (n == "outer_wall")
                           s.outer_wall_speed = {};
                       else
                           *speed_of(s, "speed." + n) = 0.0;
                       return;
                   }
                   if (n == "outer_wall" && v.is_string()) {
                       const std::string t = v.get<std::string>();
                       char* end = nullptr;
                       const double p = std::strtod(t.c_str(), &end);
                       // strtod reads "inf%" too: a finite share, up to 1000 %.
                       if (t.empty() || t.back() != '%' || end != t.c_str() + t.size() - 1 || !(p > 0.0) ||
                           !(p <= 1000.0))
                           fail(k, "must be a number of mm/s or a percentage from 0 to 1000 like \"50%\"");
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
                   else if (n == "support")
                       s.support_speed = d;
                   else if (n == "travel")
                       s.travel_speed = d;
                   else if (n == "first_layer")
                       s.first_layer_speed = d;
                   else
                       s.max_speed = d;
               });
    read_group(root, "retraction", {"length", "speed", "lift", "min_travel"},
               [&](const json& g, const std::string& n, const std::string& k) {
                   if (n == "speed" && g.at(n).is_null()) { // not set: from the printer's profile
                       s.retract_speed = 0.0;
                       return;
                   }
                   const double d = number(g, n, k);
                   if (n == "speed") {
                       if (!(d > 0.0))
                           fail(k, "must be greater than 0");
                       s.retract_speed = d;
                       return;
                   }
                   if (d < 0.0)
                       fail(k, "must not be negative");
                   // As the changes by height: more would pull the filament
                   // out of the hot end, or lift the head off the machine.
                   if (n == "length" && d > 50.0)
                       fail(k, "must be from 0 to 50 mm");
                   if (n == "lift" && d > 10.0)
                       fail(k, "must be from 0 to 10 mm");
                   (n == "length" ? s.retract_length : n == "lift" ? s.retract_lift : s.retract_min_travel) = d;
               });
    read_group(root, "adhesion", {"skirt_loops", "skirt_distance", "skirt_layers", "brim_width"},
               [&](const json& g, const std::string& n, const std::string& k) {
                   if (n == "skirt_loops" || n == "skirt_layers") {
                       const json& v = g.at(n);
                       if (!v.is_number_integer() || v.get<long long>() < 0 || v.get<long long>() > 1000)
                           fail(k, "must be a whole number from 0 to 1000");
                       (n == "skirt_loops" ? s.skirt_loops : s.skirt_layers) = static_cast<int>(v.get<long long>());
                       return;
                   }
                   const double d = number(g, n, k);
                   if (d < 0.0)
                       fail(k, "must not be negative");
                   (n == "skirt_distance" ? s.skirt_distance : s.brim_width) = d;
               });
    read_group(root, "support",
               {"enabled", "automatic", "threshold_angle", "z_gap", "xy_gap", "spacing", "interface_layers", "style",
                "tree_angle", "tree_angle_preferred", "tree_branch_diameter", "tree_branch_diameter_angle",
                "tree_double_wall_diameter", "tree_branch_distance", "tree_tip_diameter", "tree_top_rate"},
               [&](const json& g, const std::string& n, const std::string& k) {
                   if (n == "style") {
                       s.support_style = choice<SupportStyle>(g, n, k, {{"grid", SupportStyle::Grid}, {"organic", SupportStyle::Organic}});
                       return;
                   }
                   if (n.starts_with("tree_")) {
                       const double d = number(g, n, k);
                       const bool angle = n == "tree_angle" || n == "tree_angle_preferred" || n == "tree_branch_diameter_angle";
                       if (angle ? !(d >= 0.0 && d <= 85.0) : n == "tree_top_rate" ? !(d > 0.0 && d <= 100.0)
                                   : n == "tree_double_wall_diameter" ? !(d >= 0.0 && d <= 100.0) : !(d > 0.0 && d <= 100.0))
                           fail(k, angle ? "must be from 0 to 85 degrees" : n == "tree_top_rate" ? "must be more than 0 and at most 100 %"
                                                                                                 : "must be more than 0 and at most 100");
                       (n == "tree_angle"                   ? s.tree_angle
                        : n == "tree_angle_preferred"       ? s.tree_angle_preferred
                        : n == "tree_branch_diameter"       ? s.tree_branch_diameter
                        : n == "tree_branch_diameter_angle" ? s.tree_branch_diameter_angle
                        : n == "tree_double_wall_diameter"  ? s.tree_double_wall_diameter
                        : n == "tree_branch_distance"       ? s.tree_branch_distance
                        : n == "tree_tip_diameter"          ? s.tree_tip_diameter
                                                            : s.tree_top_rate) = d;
                       return;
                   }
                   if (n == "enabled" || n == "automatic") {
                       if (!g.at(n).is_boolean())
                           fail(k, "must be true or false");
                       (n == "enabled" ? s.support : s.support_automatic) = g.at(n).get<bool>();
                       return;
                   }
                   if (n == "interface_layers") {
                       const json& v = g.at(n);
                       if (!v.is_number_integer() || v.get<long long>() < 0 || v.get<long long>() > 1000)
                           fail(k, "must be a whole number from 0 to 1000");
                       s.support_interface_layers = static_cast<int>(v.get<long long>());
                       return;
                   }
                   const double d = number(g, n, k);
                   if (n == "spacing") {
                       if (!(d > 0.0))
                           fail(k, "must be greater than 0");
                       s.support_spacing = d;
                       return;
                   }
                   if (n == "threshold_angle") {
                       if (d < 0.0 || d >= 90.0)
                           fail(k, "must be from 0 (automatic) to less than 90 degrees");
                       s.support_angle = d;
                       return;
                   }
                   if (d < 0.0)
                       fail(k, "must not be negative");
                   (n == "z_gap" ? s.support_z_gap : s.support_xy_gap) = d;
               });
    read_group(root, "cooling",
               {"min_layer_time", "min_speed", "fan_min", "fan_max", "fan_below_time", "fan_always", "fan_bridges",
                "fan_off_layers", "keep_outer_speed"},
               [&](const json& g, const std::string& n, const std::string& k) {
                   if (n == "keep_outer_speed" || n == "fan_always") {
                       if (!g.at(n).is_boolean())
                           fail(k, "must be true or false");
                       (n == "fan_always" ? s.fan_always : s.keep_outer_speed) = g.at(n).get<bool>();
                       return;
                   }
                   if (n == "fan_off_layers") {
                       const json& v = g.at(n);
                       if (!v.is_number_integer() || v.get<long long>() < 0 || v.get<long long>() > 1000)
                           fail(k, "must be a whole number from 0 to 1000");
                       s.fan_off_layers = static_cast<int>(v.get<long long>());
                       return;
                   }
                   if (n == "min_speed" && g.at(n).is_null()) { // not set: from the printer's profile
                       s.min_print_speed = 0.0;
                       return;
                   }
                   const double d = number(g, n, k);
                   if (n == "min_speed") {
                       if (!(d > 0.0))
                           fail(k, "must be greater than 0");
                       s.min_print_speed = d;
                   } else if (n == "min_layer_time" || n == "fan_below_time") {
                       if (d < 0.0)
                           fail(k, "must not be negative");
                       (n == "fan_below_time" ? s.fan_below_time : s.min_layer_time) = d;
                   } else {
                       if (d < 0.0 || d > 100.0)
                           fail(k, "must be a percentage from 0 to 100");
                       (n == "fan_min" ? s.fan_min : n == "fan_max" ? s.fan_max : s.fan_bridges) = d;
                   }
               });
    read_group(root, "compute", {"gpu"}, [&](const json& g, const std::string& n, const std::string& k) {
        s.gpu = choice<GpuMode>(g, n, k, {{"auto", GpuMode::Auto}, {"off", GpuMode::Off}});
    });
    read_group(root, "zaa", {"enabled", "min_thickness", "raise"}, [&](const json& g, const std::string& n, const std::string& k) {
        if (n == "min_thickness") {
            const double v = number(g, n, k);
            if (v < 1.0 || v > 100.0)
                fail(k, "must be from 1 to 100 percent");
            s.zaa_min_thickness = v;
            return;
        }
        if (!g.at(n).is_boolean())
            fail(k, "must be true or false");
        (n == "enabled" ? s.zaa : s.zaa_raise) = g.at(n).get<bool>();
    });
    read_group(root, "fuzzy", {"mode", "thickness", "point_distance"},
               [&](const json& g, const std::string& n, const std::string& k) {
                   if (n == "mode") {
                       s.fuzzy_mode = choice<FuzzyMode>(
                           g, n, k, {{"none", FuzzyMode::None}, {"external", FuzzyMode::External}, {"all", FuzzyMode::All}});
                       return;
                   }
                   const double v = number(g, n, k);
                   if (n == "thickness") {
                       if (v < 0.0 || v > 5.0)
                           fail(k, "must be from 0 to 5 mm");
                       s.fuzzy_thickness = v;
                       return;
                   }
                   if (v < 0.1 || v > 20.0)
                       fail(k, "must be from 0.1 to 20 mm");
                   s.fuzzy_point_distance = v;
               });
    read_group(root, "gcode", {"resolution"}, [&](const json& g, const std::string& n, const std::string& k) {
        const double v = number(g, n, k);
        if (v < 0.0 || v > 1.0)
            fail(k, "must be from 0 to 1 mm");
        s.gcode_resolution = v;
    });
    read_group(root, "compensation", {"scale_x", "scale_y", "scale_z", "hole_offset"},
               [&](const json& g, const std::string& n, const std::string& k) {
                   const double v = number(g, n, k);
                   if (n == "hole_offset") {
                       if (v < -5.0 || v > 5.0)
                           fail(k, "must be from -5 to 5 mm");
                       s.hole_offset = v;
                       return;
                   }
                   if (v < 50.0 || v > 200.0)
                       fail(k, "must be a percentage from 50 to 200");
                   (n == "scale_x" ? s.scale_x : n == "scale_y" ? s.scale_y : s.scale_z) = v;
               });
    read_group(root, "extrusion", {"flow", "pressure_advance", "max_flow", "chamber_temperature", "abrasive", "needs_enclosure"},
               [&](const json& g, const std::string& n, const std::string& k) {
        if (n == "abrasive" || n == "needs_enclosure") {
            if (!g.at(n).is_boolean())
                fail(k, "must be true or false");
            (n == "abrasive" ? s.abrasive : s.needs_enclosure) = g.at(n).get<bool>();
            return;
        }
        const double v = number(g, n, k);
        if (n == "max_flow" || n == "chamber_temperature") {
            if (v < 0.0 || v > (n == "max_flow" ? 1000.0 : 200.0))
                fail(k, n == "max_flow" ? "must be from 0 to 1000 mm3/s" : "must be from 0 to 200 C");
            (n == "max_flow" ? s.filament_max_flow : s.chamber_temperature) = v;
            return;
        }
        if (n == "pressure_advance") {
            if (v < 0.0 || v > 2.0)
                fail(k, "must be from 0 to 2");
            s.pressure_advance = v;
            return;
        }
        if (v < 10.0 || v > 300.0)
            fail(k, "must be a percentage from 10 to 300");
        s.flow = v;
    });
    if (root.contains("changes")) {
        const json& list = root.at("changes");
        if (!list.is_array())
            fail("changes", "must be a list of {\"z\": height, ...}");
        for (std::size_t i = 0; i < list.size(); ++i) {
            const json& item = list[i];
            const std::string where = "changes[" + std::to_string(i) + "]";
            if (!item.is_object())
                fail(where, "must be an object");
            if (!item.contains("z"))
                fail(where + ".z", "is missing");
            HeightChange c;
            auto ranged = [&](const std::string& n, double lo, double hi, const char* what) {
                const double v = number(item, n, where + "." + n);
                if (v < lo || v > hi)
                    fail(where + "." + n, what);
                return v;
            };
            for (const auto& [n, v] : item.items()) {
                if (n == "z")
                    c.z = ranged(n, 0.0, 100000.0, "must not be negative");
                else if (n == "temperature")
                    c.temperature = ranged(n, 0.0, 500.0, "must be from 0 to 500 C");
                else if (n == "bed_temperature")
                    c.bed_temperature = ranged(n, 0.0, 200.0, "must be from 0 to 200 C");
                else if (n == "fan")
                    c.fan = ranged(n, 0.0, 100.0, "must be a percentage from 0 to 100");
                else if (n == "retraction_length")
                    c.retract_length = ranged(n, 0.0, 50.0, "must be from 0 to 50 mm");
                else if (n == "retraction_speed")
                    c.retract_speed = ranged(n, 1.0, 500.0, "must be from 1 to 500 mm/s");
                else if (n == "flow")
                    c.flow = ranged(n, 10.0, 300.0, "must be a percentage from 10 to 300");
                else if (n == "speed")
                    c.speed = ranged(n, 5.0, 1000.0, "must be a percentage from 5 to 1000");
                else if (n == "pressure_advance")
                    c.pressure_advance = ranged(n, 0.0, 2.0, "must be from 0 to 2");
                else if (n == "volumetric_flow")
                    c.volumetric_flow = ranged(n, 0.5, 1000.0, "must be from 0.5 to 1000 mm3/s");
                else if (n == "acceleration")
                    c.acceleration = ranged(n, 100.0, 100000.0, "must be from 100 to 100000 mm/s2");
                else if (n == "gcode") {
                    if (!v.is_string())
                        fail(where + ".gcode", "must be a string");
                    c.gcode = v.get<std::string>();
                } else {
                    fail(where + "." + n, "unknown setting (see docs/CONFIG.md)");
                }
            }
            s.changes.push_back(std::move(c));
        }
        std::stable_sort(s.changes.begin(), s.changes.end(),
                         [](const HeightChange& a, const HeightChange& b) { return a.z < b.z; });
    }
    return s;
}

nlohmann::ordered_json changes_json(const std::vector<HeightChange>& changes) {
    nlohmann::ordered_json list = nlohmann::ordered_json::array();
    for (const HeightChange& c : changes) {
        nlohmann::ordered_json j;
        j["z"] = c.z;
        const std::pair<const char*, const std::optional<double>*> values[] = {
            {"temperature", &c.temperature},         {"bed_temperature", &c.bed_temperature},
            {"fan", &c.fan},                         {"retraction_length", &c.retract_length},
            {"retraction_speed", &c.retract_speed}, {"flow", &c.flow},
            {"speed", &c.speed},                     {"pressure_advance", &c.pressure_advance},
            {"volumetric_flow", &c.volumetric_flow}, {"acceleration", &c.acceleration}};
        for (const auto& [name, v] : values)
            if (*v)
                j[name] = **v;
        if (!c.gcode.empty())
            j["gcode"] = c.gcode;
        list.push_back(j);
    }
    return list;
}

} // namespace

std::vector<std::string> missing_speeds(const Settings& s) {
    std::vector<std::string> out;
    for (const NeededSpeed& n : kNeededSpeeds) {
        const double v = std::string_view(n.key) == "speed.outer_wall" ? s.outer_wall_speed.value : *speed_of(s, n.key);
        if (!(v > 0.0))
            out.emplace_back(n.key);
    }
    return out;
}

std::vector<std::string> fill_missing_speeds(Settings& s) {
    std::vector<std::string> out = missing_speeds(s);
    for (const std::string& key : out) {
        double fallback = 0.0;
        for (const NeededSpeed& n : kNeededSpeeds)
            if (key == n.key)
                fallback = n.fallback;
        if (key == "speed.outer_wall")
            s.outer_wall_speed = {fallback, false};
        else
            *speed_of(s, key) = fallback;
    }
    return out;
}

void require_speeds(const Settings& settings) {
    const std::vector<std::string> missing = missing_speeds(settings);
    if (missing.empty())
        return;
    std::string list;
    for (const std::string& key : missing)
        list += (list.empty() ? "" : ", ") + key;
    throw SettingsError("settings: no speed for " + list +
                        ": speeds come from the printer's profile (docs/CONFIG.md, \"Velocidades\")");
}

double fastest_extrusion(const Settings& s) {
    const double fastest = std::max({s.wall_speed, s.outer_wall_speed.of(s.wall_speed), s.infill_speed,
                                     s.solid_infill_speed, s.top_solid_infill_speed, s.support_speed,
                                     s.first_layer_speed});
    return s.max_speed > 0.0 ? std::min(fastest, s.max_speed) : fastest;
}

std::vector<std::pair<std::string, std::string>> describe(const Settings& s) {
    auto num = [](double v) { return json(v).dump(); };
    auto speed = [](double v) { return v > 0.0 ? json(v).dump() : std::string("null"); }; // 0: not set
    auto flag = [](bool b) { return std::string(b ? "true" : "false"); };
    auto text = [](const std::string& t) { return json(t).dump(); };
    const char* fill_mode = s.fill_mode == FillMode::Union ? "union" : s.fill_mode == FillMode::EvenOdd ? "even_odd" : "fill_holes";
    const char* pattern = infill_pattern_name(s.infill_pattern);
    const char* flavor = s.flavor == GcodeFlavor::Marlin ? "marlin" : s.flavor == GcodeFlavor::Klipper ? "klipper" : "reprap";
    std::vector<std::pair<std::string, std::string>> out = {
        {"layers.height", num(s.layer_height)},
        {"layers.first_height", num(s.first_layer_height)},
        {"layers.detail_height", num(s.detail_height)},
        {"layers.detail_tolerance", num(s.detail_tolerance)},
        {"layers.detail_margin", num(s.detail_margin)},
        {"layers.heights", json(s.layer_heights).dump()},
        {"layers.cusp_height", num(s.cusp_height)},
        {"slicing.fill_mode", fill_mode},
        {"slicing.gap_closing_radius", num(s.gap_closing_radius)},
        {"slicing.xy_offset", num(s.xy_offset)},
        {"slicing.first_layer_shrink", num(s.first_layer_shrink)},
        {"slicing.simplify_tolerance", num(s.simplify_tolerance)},
        {"walls.count", std::to_string(s.wall_count)},
        {"walls.width", num(s.wall_width)},
        {"walls.seam", s.seam == SeamMode::Aligned ? "aligned" : s.seam == SeamMode::Nearest ? "nearest"
                       : s.seam == SeamMode::Random  ? "random" : "rear"},
        {"walls.scarf", s.scarf == ScarfMode::Contours ? "contours" : s.scarf == ScarfMode::Everywhere ? "everywhere" : "nowhere"},
        {"walls.scarf_only_smooth", flag(s.scarf_only_smooth)},
        {"walls.scarf_start_height", num(s.scarf_start_height)},
        {"walls.scarf_length", num(s.scarf_length)},
        {"walls.scarf_entire_loop", flag(s.scarf_entire_loop)},
        {"walls.scarf_max_segment", num(s.scarf_max_segment)},
        {"walls.scarf_inner", flag(s.scarf_inner)},
        {"vase.enabled", flag(s.vase)},
        {"fuzzy.mode", fuzzy_mode_name(s.fuzzy_mode)},
        {"fuzzy.thickness", num(s.fuzzy_thickness)},
        {"fuzzy.point_distance", num(s.fuzzy_point_distance)},
        {"surfaces.top_layers", std::to_string(s.top_layers)},
        {"surfaces.bottom_layers", std::to_string(s.bottom_layers)},
        {"surfaces.top_pattern", s.monotonic_top ? "monotonic" : "rectilinear"},
        {"surfaces.top_min_thickness", num(s.top_min_thickness)},
        {"surfaces.bottom_min_thickness", num(s.bottom_min_thickness)},
        {"infill.density", num(s.infill_density)},
        {"infill.angle", num(s.infill_angle)},
        {"infill.pattern", pattern},
        {"infill.wall_overlap", num(s.infill_wall_overlap)},
        {"infill.combine_layers", std::to_string(s.infill_combine_layers)},
        {"infill.combine_auto", flag(s.infill_combine_auto)},
        {"infill.dense_under_top", flag(s.infill_dense_under_top)},
        {"infill.dense_density", num(s.infill_dense_density)},
        {"infill.stress_density", num(s.infill_stress_density)},
        {"infill.solid_every", std::to_string(s.infill_solid_every)},
        {"infill.solid_below_area", num(s.infill_solid_below_area)},
        {"infill.nonplanar", flag(s.infill_nonplanar)},
        {"infill.nonplanar_amplitude", num(s.infill_nonplanar_amplitude)},
        {"infill.nonplanar_wavelength", num(s.infill_nonplanar_wavelength)},
        {"infill.combine_max_height", s.infill_combine_max_height.percent ? num(s.infill_combine_max_height.value) + "%"
                                                                          : num(s.infill_combine_max_height.value)},
        {"machine.nozzle_diameter", num(s.nozzle_diameter)},
        {"machine.filament_diameter", num(s.filament_diameter)},
        {"machine.relative_extrusion", flag(s.relative_extrusion)},
        {"machine.flavor", flavor},
        {"machine.start_gcode", text(s.start_gcode)},
        {"machine.end_gcode", text(s.end_gcode)},
        {"machine.max_feed_x", speed(s.max_feed_x)},
        {"machine.max_feed_y", speed(s.max_feed_y)},
        {"machine.max_feed_z", speed(s.max_feed_z)},
        {"machine.max_feed_e", speed(s.max_feed_e)},
        {"machine.acceleration", num(s.acceleration)},
        {"machine.travel_acceleration", num(s.travel_acceleration)},
        {"machine.corner_speed", speed(s.corner_speed)},
        {"machine.bed", json(s.bed).dump()},
        {"machine.max_height", num(s.max_height)},
        {"machine.min_layer_height", num(s.min_layer_height)},
        {"machine.max_layer_height", num(s.max_layer_height)},
        {"speed.walls", speed(s.wall_speed)},
        {"speed.outer_wall", s.outer_wall_speed.value <= 0.0 ? std::string("null")
                             : s.outer_wall_speed.percent ? num(s.outer_wall_speed.value) + "%"
                                                          : num(s.outer_wall_speed.value)},
        {"speed.infill", speed(s.infill_speed)},
        {"speed.solid_infill", speed(s.solid_infill_speed)},
        {"speed.top_solid_infill", speed(s.top_solid_infill_speed)},
        {"speed.support", speed(s.support_speed)},
        {"speed.travel", speed(s.travel_speed)},
        {"speed.first_layer", speed(s.first_layer_speed)},
        {"speed.max", speed(s.max_speed)},
        {"retraction.length", num(s.retract_length)},
        {"retraction.speed", speed(s.retract_speed)},
        {"retraction.lift", num(s.retract_lift)},
        {"retraction.min_travel", num(s.retract_min_travel)},
        {"adhesion.skirt_loops", std::to_string(s.skirt_loops)},
        {"adhesion.skirt_distance", num(s.skirt_distance)},
        {"adhesion.skirt_layers", std::to_string(s.skirt_layers)},
        {"adhesion.brim_width", num(s.brim_width)},
        {"support.enabled", flag(s.support)},
        {"support.automatic", flag(s.support_automatic)},
        {"support.threshold_angle", num(s.support_angle)},
        {"support.z_gap", num(s.support_z_gap)},
        {"support.xy_gap", num(s.support_xy_gap)},
        {"support.spacing", num(s.support_spacing)},
        {"support.interface_layers", std::to_string(s.support_interface_layers)},
        {"support.style", s.support_style == SupportStyle::Organic ? "organic" : "grid"},
        {"support.tree_angle", num(s.tree_angle)},
        {"support.tree_angle_preferred", num(s.tree_angle_preferred)},
        {"support.tree_branch_diameter", num(s.tree_branch_diameter)},
        {"support.tree_branch_diameter_angle", num(s.tree_branch_diameter_angle)},
        {"support.tree_double_wall_diameter", num(s.tree_double_wall_diameter)},
        {"support.tree_branch_distance", num(s.tree_branch_distance)},
        {"support.tree_tip_diameter", num(s.tree_tip_diameter)},
        {"support.tree_top_rate", num(s.tree_top_rate)},
        {"cooling.min_layer_time", num(s.min_layer_time)},
        {"cooling.min_speed", speed(s.min_print_speed)},
        {"cooling.fan_min", num(s.fan_min)},
        {"cooling.fan_max", num(s.fan_max)},
        {"cooling.fan_below_time", num(s.fan_below_time)},
        {"cooling.fan_always", s.fan_always ? "true" : "false"},
        {"cooling.fan_bridges", num(s.fan_bridges)},
        {"cooling.fan_off_layers", std::to_string(s.fan_off_layers)},
        {"cooling.keep_outer_speed", s.keep_outer_speed ? "true" : "false"},
        {"compute.gpu", s.gpu == GpuMode::Auto ? "auto" : "off"},
        {"zaa.enabled", s.zaa ? "true" : "false"},
        {"zaa.min_thickness", num(s.zaa_min_thickness)},
        {"zaa.raise", s.zaa_raise ? "true" : "false"},
        {"compensation.scale_x", num(s.scale_x)},
        {"compensation.scale_y", num(s.scale_y)},
        {"compensation.scale_z", num(s.scale_z)},
        {"compensation.hole_offset", num(s.hole_offset)},
        {"extrusion.flow", num(s.flow)},
        {"extrusion.pressure_advance", num(s.pressure_advance)},
        {"extrusion.max_flow", num(s.filament_max_flow)},
        {"extrusion.chamber_temperature", num(s.chamber_temperature)},
        {"extrusion.abrasive", flag(s.abrasive)},
        {"extrusion.needs_enclosure", flag(s.needs_enclosure)},
        {"gcode.resolution", num(s.gcode_resolution)},
        {"machine.max_flow", num(s.machine_max_flow)},
        {"machine.max_nozzle_temp", num(s.max_nozzle_temp)},
        {"machine.max_bed_temp", num(s.max_bed_temp)},
        {"machine.max_chamber_temp", num(s.max_chamber_temp)},
        {"machine.enclosed", flag(s.enclosed)},
        {"machine.hardened_nozzle", flag(s.hardened_nozzle)},
    };
    if (!s.changes.empty())
        out.push_back({"changes", changes_json(s.changes).dump()});
    for (const auto& [name, values] : s.variables) {
        std::string v;
        for (const std::string& item : values)
            v += (v.empty() ? "" : ",") + item;
        out.push_back({"machine.variables." + name, v});
    }
    return out;
}

std::string settings_json(const Settings& s) {
    using nlohmann::ordered_json;
    const char* fill_mode = s.fill_mode == FillMode::Union ? "union" : s.fill_mode == FillMode::EvenOdd ? "even_odd" : "fill_holes";
    const char* pattern = infill_pattern_name(s.infill_pattern);
    const char* flavor = s.flavor == GcodeFlavor::Marlin ? "marlin" : s.flavor == GcodeFlavor::Klipper ? "klipper" : "reprap";
    const char* seam = s.seam == SeamMode::Aligned ? "aligned"
                       : s.seam == SeamMode::Nearest ? "nearest"
                       : s.seam == SeamMode::Random  ? "random"
                                                     : "rear";
    ordered_json variables = ordered_json::object();
    // Values are kept as text (numbers keep their JSON text), so they go back
    // as strings: parse_settings reads a string as that same text.
    for (const auto& [name, values] : s.variables)
        variables[name] = values.size() == 1 ? ordered_json(values.front()) : ordered_json(values);
    ordered_json bed = ordered_json::array();
    for (const auto& p : s.bed)
        bed.push_back({p[0], p[1]});
    ordered_json layer_heights = ordered_json::array();
    for (const auto& p : s.layer_heights)
        layer_heights.push_back({p[0], p[1]});
    const ordered_json outer_wall = s.outer_wall_speed.value <= 0.0 ? ordered_json(nullptr)
                                    : s.outer_wall_speed.percent    ? ordered_json(json(s.outer_wall_speed.value).dump() + "%")
                                                                    : ordered_json(s.outer_wall_speed.value);
    // A speed not set is null: the printer's profile gives it (docs/CONFIG.md).
    const auto speed = [](double v) { return v > 0.0 ? ordered_json(v) : ordered_json(nullptr); };
    const ordered_json root = {
        {"layers",
         {{"height", s.layer_height},
          {"first_height", s.first_layer_height},
          {"detail_height", s.detail_height},
          {"detail_tolerance", s.detail_tolerance},
          {"detail_margin", s.detail_margin},
          {"heights", layer_heights},
          {"cusp_height", s.cusp_height}}},
        {"slicing",
         {{"fill_mode", fill_mode},
          {"gap_closing_radius", s.gap_closing_radius},
          {"xy_offset", s.xy_offset},
          {"first_layer_shrink", s.first_layer_shrink},
          {"simplify_tolerance", s.simplify_tolerance}}},
        {"walls",
         {{"count", s.wall_count},
          {"width", s.wall_width},
          {"seam", seam},
          {"scarf", s.scarf == ScarfMode::Contours ? "contours" : s.scarf == ScarfMode::Everywhere ? "everywhere" : "nowhere"},
          {"scarf_only_smooth", s.scarf_only_smooth},
          {"scarf_start_height", s.scarf_start_height},
          {"scarf_length", s.scarf_length},
          {"scarf_entire_loop", s.scarf_entire_loop},
          {"scarf_max_segment", s.scarf_max_segment},
          {"scarf_inner", s.scarf_inner}}},
        {"vase", {{"enabled", s.vase}}},
        {"surfaces",
         {{"top_layers", s.top_layers},
          {"bottom_layers", s.bottom_layers},
          {"top_pattern", s.monotonic_top ? "monotonic" : "rectilinear"},
          {"top_min_thickness", s.top_min_thickness},
          {"bottom_min_thickness", s.bottom_min_thickness}}},
        {"infill",
         {{"density", s.infill_density},
          {"angle", s.infill_angle},
          {"pattern", pattern},
          {"wall_overlap", s.infill_wall_overlap},
          {"combine_layers", s.infill_combine_layers},
          {"combine_auto", s.infill_combine_auto},
          {"dense_under_top", s.infill_dense_under_top},
          {"dense_density", s.infill_dense_density},
          {"stress_density", s.infill_stress_density},
          {"solid_every", s.infill_solid_every},
          {"solid_below_area", s.infill_solid_below_area},
          {"nonplanar", s.infill_nonplanar},
          {"nonplanar_amplitude", s.infill_nonplanar_amplitude},
          {"nonplanar_wavelength", s.infill_nonplanar_wavelength},
          {"combine_max_height", s.infill_combine_max_height.percent
                                     ? ordered_json(json(s.infill_combine_max_height.value).dump() + "%")
                                     : ordered_json(s.infill_combine_max_height.value)}}},
        {"machine",
         {{"nozzle_diameter", s.nozzle_diameter},
          {"filament_diameter", s.filament_diameter},
          {"relative_extrusion", s.relative_extrusion},
          {"flavor", flavor},
          {"start_gcode", s.start_gcode},
          {"end_gcode", s.end_gcode},
          {"variables", variables},
          {"max_feed_x", speed(s.max_feed_x)},
          {"max_feed_y", speed(s.max_feed_y)},
          {"max_feed_z", speed(s.max_feed_z)},
          {"max_feed_e", speed(s.max_feed_e)},
          {"acceleration", s.acceleration},
          {"travel_acceleration", s.travel_acceleration},
          {"corner_speed", speed(s.corner_speed)},
          {"bed", bed},
          {"max_height", s.max_height},
          {"min_layer_height", s.min_layer_height},
          {"max_layer_height", s.max_layer_height},
          {"max_flow", s.machine_max_flow},
          {"max_nozzle_temp", s.max_nozzle_temp},
          {"max_bed_temp", s.max_bed_temp},
          {"max_chamber_temp", s.max_chamber_temp},
          {"enclosed", s.enclosed},
          {"hardened_nozzle", s.hardened_nozzle}}},
        {"speed",
         {{"walls", speed(s.wall_speed)},
          {"outer_wall", outer_wall},
          {"infill", speed(s.infill_speed)},
          {"solid_infill", speed(s.solid_infill_speed)},
          {"top_solid_infill", speed(s.top_solid_infill_speed)},
          {"support", speed(s.support_speed)},
          {"travel", speed(s.travel_speed)},
          {"first_layer", speed(s.first_layer_speed)},
          {"max", speed(s.max_speed)}}},
        {"retraction",
         {{"length", s.retract_length},
          {"speed", speed(s.retract_speed)},
          {"lift", s.retract_lift},
          {"min_travel", s.retract_min_travel}}},
        {"adhesion",
         {{"skirt_loops", s.skirt_loops},
          {"skirt_distance", s.skirt_distance},
          {"skirt_layers", s.skirt_layers},
          {"brim_width", s.brim_width}}},
        {"support",
         {{"enabled", s.support},
          {"automatic", s.support_automatic},
          {"threshold_angle", s.support_angle},
          {"z_gap", s.support_z_gap},
          {"xy_gap", s.support_xy_gap},
          {"spacing", s.support_spacing},
          {"interface_layers", s.support_interface_layers},
          {"style", s.support_style == SupportStyle::Organic ? "organic" : "grid"},
          {"tree_angle", s.tree_angle},
          {"tree_angle_preferred", s.tree_angle_preferred},
          {"tree_branch_diameter", s.tree_branch_diameter},
          {"tree_branch_diameter_angle", s.tree_branch_diameter_angle},
          {"tree_double_wall_diameter", s.tree_double_wall_diameter},
          {"tree_branch_distance", s.tree_branch_distance},
          {"tree_tip_diameter", s.tree_tip_diameter},
          {"tree_top_rate", s.tree_top_rate}}},
        {"cooling",
         {{"min_layer_time", s.min_layer_time},
          {"min_speed", speed(s.min_print_speed)},
          {"fan_min", s.fan_min},
          {"fan_max", s.fan_max},
          {"fan_below_time", s.fan_below_time},
          {"fan_always", s.fan_always},
          {"fan_bridges", s.fan_bridges},
          {"fan_off_layers", s.fan_off_layers},
          {"keep_outer_speed", s.keep_outer_speed}}},
        {"compute", {{"gpu", s.gpu == GpuMode::Auto ? "auto" : "off"}}},
        {"zaa", {{"enabled", s.zaa}, {"min_thickness", s.zaa_min_thickness}, {"raise", s.zaa_raise}}},
        {"fuzzy",
         {{"mode", fuzzy_mode_name(s.fuzzy_mode)},
          {"thickness", s.fuzzy_thickness},
          {"point_distance", s.fuzzy_point_distance}}},
        {"compensation",
         {{"scale_x", s.scale_x}, {"scale_y", s.scale_y}, {"scale_z", s.scale_z}, {"hole_offset", s.hole_offset}}},
        {"extrusion",
         {{"flow", s.flow},
          {"pressure_advance", s.pressure_advance},
          {"max_flow", s.filament_max_flow},
          {"chamber_temperature", s.chamber_temperature},
          {"abrasive", s.abrasive},
          {"needs_enclosure", s.needs_enclosure}}},
        {"gcode", {{"resolution", s.gcode_resolution}}},
        {"changes", changes_json(s.changes)},
    };
    return root.dump(2) + "\n";
}

Settings load_settings(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in)
        throw SettingsError("settings: cannot open '" + path.string() + "'");
    std::ostringstream text;
    text << in.rdbuf();
    Settings s = parse_settings(text.str());
    check_settings(s);
    return s;
}

void check_settings(const Settings& s) {
    char text[200];
    if (s.min_layer_height > 0.0 && s.max_layer_height > 0.0 && s.min_layer_height > s.max_layer_height + 1e-9) {
        std::snprintf(text, sizeof text,
                      "machine.min_layer_height: %.3g mm is above machine.max_layer_height (%.3g mm): no layer fits "
                      "between them",
                      s.min_layer_height, s.max_layer_height);
        throw SettingsError(text);
    }
    const double nozzle = s.nozzle_diameter;
    const double top = s.max_layer_height > 0.0 ? std::min(s.max_layer_height, nozzle) : nozzle;
    for (const auto& [key, h] : {std::pair{"layers.height", s.layer_height}, {"layers.first_height", s.first_layer_height}}) {
        if (h > top + 1e-9) {
            std::snprintf(text, sizeof text,
                          "%s: %.3g mm is thicker than %.3g mm, the most this printer lays (the nozzle, or "
                          "machine.max_layer_height)",
                          key, h, top);
            throw SettingsError(text);
        }
        if (s.min_layer_height > 0.0 && h < s.min_layer_height - 1e-9) {
            std::snprintf(text, sizeof text, "%s: %.3g mm is thinner than machine.min_layer_height (%.3g mm)", key, h,
                          s.min_layer_height);
            throw SettingsError(text);
        }
    }
    // Vase mode lays one wall in a spiral: nothing can stand under it
    // (vase_mode.md, rule 2).
    if (s.vase && s.support)
        throw SettingsError("vase.enabled: vase mode is not compatible with supports (support.enabled)");
    if (s.wall_width < 0.25 * nozzle || s.wall_width > 4.0 * nozzle) {
        std::snprintf(text, sizeof text, "walls.width: %.3g mm cannot come out of a %.3g mm nozzle (%.3g to %.3g mm)",
                      s.wall_width, nozzle, 0.25 * nozzle, 4.0 * nozzle);
        throw SettingsError(text);
    }
}

} // namespace tisma
