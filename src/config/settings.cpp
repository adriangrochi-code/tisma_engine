// Reader for the engine's settings format (docs/CONFIG.md), on nlohmann/json
// (MIT). Own design; see PROVENANCE.md.

#include "config/settings.hpp"
#include "gcode/dialect.hpp"

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
        {"speed.bridges", &s.bridge_speed},
        {"speed.gap_fill", &s.gap_fill_speed},
        {"speed.travel_z", &s.travel_z_speed},
        {"retraction.speed", &s.retract_speed},
        {"cooling.min_speed", &s.min_print_speed},
    };
    for (const auto& [name, target] : speeds)
        if (name == key)
            return target;
    throw std::logic_error("no speed " + std::string(key));
}

// The acceleration a key of the acceleration group names.
template <typename S>
auto acceleration_of(S& s, std::string_view name) -> decltype(&s.accel_default) {
    const std::pair<std::string_view, decltype(&s.accel_default)> accelerations[] = {
        {"default", &s.accel_default},
        {"walls", &s.accel_walls},
        {"outer_wall", &s.accel_outer_wall},
        {"infill", &s.accel_infill},
        {"solid_infill", &s.accel_solid_infill},
        {"top_solid_infill", &s.accel_top_solid_infill},
        {"bridges", &s.accel_bridges},
        {"first_layer", &s.accel_first_layer},
        {"first_layer_over_raft", &s.accel_first_layer_over_raft},
        {"travel", &s.accel_travel},
        {"short_travel", &s.accel_short_travel},
    };
    for (const auto& [n, target] : accelerations)
        if (n == name)
            return target;
    throw std::logic_error("no acceleration " + std::string(name));
}

constexpr const char* kAccelerationNames[] = {"default",          "walls",       "outer_wall",
                                              "infill",           "solid_infill", "top_solid_infill",
                                              "bridges",          "first_layer",  "first_layer_over_raft",
                                              "travel",           "short_travel"};

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

// The flavors by name (machine.flavor).
const std::map<std::string, GcodeFlavor>& flavors() {
    static const std::map<std::string, GcodeFlavor> names = {
        {"marlin", GcodeFlavor::Marlin},       {"klipper", GcodeFlavor::Klipper},
        {"reprap", GcodeFlavor::RepRap},       {"repetier", GcodeFlavor::Repetier},
        {"smoothie", GcodeFlavor::Smoothie},   {"teacup", GcodeFlavor::Teacup},
        {"makerware", GcodeFlavor::MakerWare}, {"sailfish", GcodeFlavor::Sailfish},
        {"mach3", GcodeFlavor::Mach3},         {"machinekit", GcodeFlavor::Machinekit},
        {"no_extrusion", GcodeFlavor::NoExtrusion}, {"marlin_legacy", GcodeFlavor::MarlinLegacy},
        {"reprapfirmware", GcodeFlavor::RepRapFirmware}};
    return names;
}

const std::array<const char*, 19> kLimitNames = {
    "max_feed_x",       "max_feed_y",      "max_feed_z",   "max_feed_e",          "max_accel_x",
    "max_accel_y",      "max_accel_z",     "max_accel_e",  "acceleration",        "travel_acceleration",
    "retract_acceleration", "jerk_x",      "jerk_y",       "jerk_z",              "jerk_e",
    "junction_deviation", "min_extrude_feed", "min_travel_feed", "corner_speed"};

double* limit_named(Settings& s, std::string_view n) {
    const std::pair<std::string_view, double*> limits[] = {
        {"max_feed_x", &s.max_feed_x},
        {"max_feed_y", &s.max_feed_y},
        {"max_feed_z", &s.max_feed_z},
        {"max_feed_e", &s.max_feed_e},
        {"max_accel_x", &s.max_accel_x},
        {"max_accel_y", &s.max_accel_y},
        {"max_accel_z", &s.max_accel_z},
        {"max_accel_e", &s.max_accel_e},
        {"acceleration", &s.acceleration},
        {"travel_acceleration", &s.travel_acceleration},
        {"retract_acceleration", &s.retract_acceleration},
        {"jerk_x", &s.jerk_x},
        {"jerk_y", &s.jerk_y},
        {"jerk_z", &s.jerk_z},
        {"jerk_e", &s.jerk_e},
        {"junction_deviation", &s.junction_deviation},
        {"min_extrude_feed", &s.min_extrude_feed},
        {"min_travel_feed", &s.min_travel_feed},
        {"corner_speed", &s.corner_speed}};
    for (const auto& [name, value] : limits)
        if (name == n)
            return value;
    return nullptr;
}

const std::array<const char*, 14> kFilamentRetractionNames = {
    "retraction.length",      "retraction.speed",      "retraction.unretract_speed", "retraction.restart_extra",
    "retraction.lift",        "retraction.lift_above", "retraction.lift_below",      "retraction.min_travel",
    "retraction.on_layer_change", "retraction.wipe",   "retraction.before_wipe",     "travel.ramping_lift",
    "travel.slope",           "travel.max_lift"};

double per_extruder(const std::vector<double>& values, std::size_t extruder, double fallback) {
    if (values.empty())
        return fallback;
    return values[std::min(extruder, values.size() - 1)];
}

std::array<double, 2> extruder_offset_of(const Settings& settings, std::size_t extruder) {
    // A single-extruder MMU has one nozzle (multi_extruder.md): no offsets.
    if (settings.single_extruder_mmu || extruder >= settings.extruder_offset.size())
        return {0.0, 0.0};
    return settings.extruder_offset[extruder];
}

Settings with_filament_retraction(const Settings& settings) {
    Settings s = settings;
    for (const auto& [key, v] : settings.filament_retraction) {
        const std::pair<std::string_view, double*> numbers[] = {
            {"retraction.length", &s.retract_length},        {"retraction.speed", &s.retract_speed},
            {"retraction.unretract_speed", &s.unretract_speed}, {"retraction.restart_extra", &s.restart_extra},
            {"retraction.lift", &s.retract_lift},            {"retraction.lift_above", &s.lift_above},
            {"retraction.lift_below", &s.lift_below},        {"retraction.min_travel", &s.retract_min_travel},
            {"retraction.before_wipe", &s.before_wipe},      {"travel.slope", &s.travel_slope},
            {"travel.max_lift", &s.max_lift}};
        for (const auto& [name, field] : numbers)
            if (key == name)
                *field = v;
        if (key == "retraction.on_layer_change")
            s.retract_layer_change = v != 0.0;
        else if (key == "retraction.wipe")
            s.wipe = v != 0.0;
        else if (key == "travel.ramping_lift")
            s.ramping_lift = v != 0.0;
    }
    // A speed of 0 from the filament is not a speed: the printer's stays.
    if (!(s.retract_speed > 0.0))
        s.retract_speed = settings.retract_speed;
    return s;
}

Settings silent_settings(const Settings& settings) {
    Settings out = settings;
    for (const auto& [name, value] : settings.silent_limits)
        if (double* limit = limit_named(out, name))
            *limit = value;
    return out;
}

const char* object_labels_name(ObjectLabels labels) {
    return labels == ObjectLabels::Firmware ? "firmware" : labels == ObjectLabels::OctoPrint ? "octoprint" : "none";
}

const char* top_one_wall_name(TopOneWall where) {
    return where == TopOneWall::Top ? "top" : where == TopOneWall::Topmost ? "topmost" : "none";
}

const char* draft_shield_name(DraftShield shield) {
    return shield == DraftShield::Enabled ? "enabled" : shield == DraftShield::Limited ? "limited" : "disabled";
}

const char* brim_type_name(BrimType type) {
    switch (type) {
    case BrimType::None:
        return "none";
    case BrimType::Inner:
        return "inner";
    case BrimType::OuterAndInner:
        return "outer_and_inner";
    case BrimType::Outer:
        break;
    }
    return "outer";
}

const char* limits_usage_name(LimitsUsage usage) {
    return usage == LimitsUsage::Emit ? "emit" : usage == LimitsUsage::Ignore ? "ignore" : "time_only";
}

const char* flavor_name(GcodeFlavor flavor) {
    for (const auto& [name, value] : flavors())
        if (value == flavor)
            return name.c_str();
    return "reprap";
}

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
            group == "fuzzy" || group == "vase" || group == "placement" || group == "arcs" || group == "review" ||
            group == "sequential" || group == "extruders" || group == "wipe_tower" || group == "mmu" ||
            group == "acceleration" || group == "ironing" || group == "output" || group == "travel")
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
                "scarf_entire_loop", "scarf_max_segment", "scarf_inner", "detect_overhangs", "min_line_width",
                "min_detail", "generator", "gap_fill", "clockwise", "staggered_inner_seams", "top_one_wall",
                "first_layer_one_wall", "extra_if_needed", "avoid_curled_overhangs", "extra_on_overhangs"},
               [&](const json& g, const std::string& n, const std::string& k) {
        if (n == "generator") {
            s.wall_generator =
                choice<WallGenerator>(g, n, k, {{"variable", WallGenerator::Variable}, {"fixed", WallGenerator::Fixed}});
            return;
        }
        if (n == "top_one_wall") {
            s.top_one_wall = choice<TopOneWall>(
                g, n, k, {{"none", TopOneWall::None}, {"top", TopOneWall::Top}, {"topmost", TopOneWall::Topmost}});
            return;
        }
        if (n == "staggered_inner_seams" || n == "first_layer_one_wall" || n == "extra_if_needed" ||
            n == "avoid_curled_overhangs" || n == "extra_on_overhangs") {
            if (!g.at(n).is_boolean())
                fail(k, "must be true or false");
            (n == "extra_on_overhangs"     ? s.extra_on_overhangs
             : n == "staggered_inner_seams" ? s.staggered_inner_seams
             : n == "first_layer_one_wall" ? s.first_layer_one_wall
             : n == "extra_if_needed"      ? s.extra_if_needed
                                           : s.avoid_curled_overhangs) = g.at(n).get<bool>();
            return;
        }
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
        if (n == "scarf_only_smooth" || n == "scarf_entire_loop" || n == "scarf_inner" || n == "detect_overhangs" ||
            n == "gap_fill" || n == "clockwise") {
            if (!g.at(n).is_boolean())
                fail(k, "must be true or false");
            (n == "clockwise"           ? s.walls_clockwise
             : n == "scarf_only_smooth" ? s.scarf_only_smooth
             : n == "scarf_entire_loop" ? s.scarf_entire_loop
             : n == "scarf_inner"       ? s.scarf_inner
             : n == "gap_fill"          ? s.wall_gap_fill
                                        : s.detect_overhangs) = g.at(n).get<bool>();
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
        if (n == "min_line_width" || n == "min_detail") {
            Speed& target = n == "min_line_width" ? s.wall_min_line_width : s.wall_min_detail;
            if (g.at(n).is_string()) {
                const std::string t = g.at(n).get<std::string>();
                char* end = nullptr;
                const double p = std::strtod(t.c_str(), &end);
                if (t.empty() || t.back() != '%' || end != t.c_str() + t.size() - 1 || !(p >= 0.0) || p > 1000.0)
                    fail(k, "must be a number of mm or a percentage of the nozzle like \"85%\"");
                target = {p, true};
                return;
            }
            const double v = number(g, n, k);
            if (!(v >= 0.0 && v <= 40.0))
                fail(k, "must be from 0 to 40 mm");
            target = {v, false};
            return;
        }
        const double v = number(g, n, k);
        if (!(v >= 0.05 && v <= 40.0)) // and a quarter to four nozzles (check_settings; pellets.md)
            fail(k, "must be from 0.05 to 40 mm");
        s.wall_width = v;
    });
    read_group(root, "placement", {"arrange", "center", "spacing"}, [&](const json& g, const std::string& n, const std::string& k) {
        const json& v = g.at(n);
        if (n == "spacing") {
            const double d = number(g, n, k);
            if (!(d >= 0.0 && d <= 1000.0))
                fail(k, "must be from 0 to 1000 mm");
            s.placement_spacing = d;
            return;
        }
        if (n == "arrange") {
            if (!v.is_boolean())
                fail(k, "must be true or false");
            s.placement_arrange = v.get<bool>();
            return;
        }
        if (v.is_null()) {
            s.placement_center.reset();
            return;
        }
        if (!v.is_array() || v.size() != 2 || !v[0].is_number() || !v[1].is_number())
            fail(k, "must be null (the centre of the bed) or [x, y] in mm");
        s.placement_center = std::array<double, 2>{v[0].get<double>(), v[1].get<double>()};
    });
    read_group(root, "sequential", {"enabled", "clearance_radius", "clearance_height"},
               [&](const json& g, const std::string& n, const std::string& k) {
        if (n == "enabled") {
            if (!g.at(n).is_boolean())
                fail(k, "must be true or false");
            s.sequential = g.at(n).get<bool>();
            return;
        }
        const double d = number(g, n, k);
        if (!(d >= 0.0 && d <= 1000.0))
            fail(k, "must be from 0 to 1000 mm");
        (n == "clearance_radius" ? s.clearance_radius : s.clearance_height) = d;
    });
    // Which extruder prints what (multi_extruder.md, rules 2, 4 and 7).
    read_group(root, "extruders",
               {"walls", "infill", "solid_infill", "support", "support_interface", "wipe_tower", "bed_temperature_from",
                "ooze_prevention", "standby_delta"},
               [&](const json& g, const std::string& n, const std::string& k) {
        if (n == "ooze_prevention") {
            if (!g.at(n).is_boolean())
                fail(k, "must be true or false");
            s.ooze_prevention = g.at(n).get<bool>();
            return;
        }
        const double d = number(g, n, k);
        if (n == "standby_delta") {
            if (!(d >= -300.0 && d <= 300.0))
                fail(k, "must be from -300 to 300 C");
            s.standby_delta = d;
            return;
        }
        if (!(d >= 0.0 && d <= double(kMaxExtruders)) || d != std::floor(d))
            fail(k, "must be 0 (the object's) or an extruder from 1 to " + std::to_string(kMaxExtruders));
        (n == "walls"                ? s.extruder_walls
         : n == "infill"             ? s.extruder_infill
         : n == "solid_infill"       ? s.extruder_solid_infill
         : n == "support"            ? s.extruder_support
         : n == "support_interface"  ? s.extruder_support_interface
         : n == "wipe_tower"         ? s.extruder_wipe_tower
                                     : s.bed_temperature_from) = int(d);
    });
    // The wipe tower (multi_extruder.md, rules 9 to 14).
    read_group(root, "wipe_tower",
               {"enabled", "x", "y", "rotation", "width", "brim_width", "bridging", "cone_angle", "extra_spacing",
                "extra_flow", "no_sparse_layers", "purge_volumes"},
               [&](const json& g, const std::string& n, const std::string& k) {
        const json& v = g.at(n);
        if (n == "enabled" || n == "no_sparse_layers") {
            if (!v.is_boolean())
                fail(k, "must be true or false");
            (n == "enabled" ? s.wipe_tower : s.wipe_tower_no_sparse_layers) = v.get<bool>();
            return;
        }
        if (n == "purge_volumes") {
            // One volume between every two different filaments, or a square
            // list of lists (row: from, column: to).
            const char* what = "must be a volume from 0 to 10000 mm3, or a list of rows of them, one per extruder";
            const auto volume = [&](const json& x) {
                if (!x.is_number() || !(x.get<double>() >= 0.0 && x.get<double>() <= 10000.0))
                    fail(k, what);
                return x.get<double>();
            };
            std::vector<std::vector<double>> rows;
            if (v.is_number()) {
                const double each = volume(v);
                for (int i = 0; i < kMaxExtruders; ++i) {
                    rows.emplace_back(std::size_t(kMaxExtruders), each);
                    rows.back()[std::size_t(i)] = 0.0;
                }
            } else {
                if (!v.is_array() || v.size() > std::size_t(kMaxExtruders))
                    fail(k, what);
                for (const json& row : v) {
                    if (!row.is_array() || row.size() != v.size())
                        fail(k, what);
                    rows.emplace_back();
                    for (const json& x : row)
                        rows.back().push_back(volume(x));
                }
            }
            s.purge_volumes = std::move(rows);
            return;
        }
        const double d = number(g, n, k);
        if (n == "x" || n == "y") {
            if (!(d >= -10000.0 && d <= 10000.0))
                fail(k, "must be from -10000 to 10000 mm");
            (n == "x" ? s.wipe_tower_x : s.wipe_tower_y) = d;
        } else if (n == "rotation") {
            if (!(d >= -360.0 && d <= 360.0))
                fail(k, "must be from -360 to 360 degrees");
            s.wipe_tower_rotation = d;
        } else if (n == "width") {
            if (!(d >= 1.0 && d <= 1000.0))
                fail(k, "must be from 1 to 1000 mm");
            s.wipe_tower_width = d;
        } else if (n == "brim_width" || n == "bridging") {
            if (!(d >= 0.0 && d <= 100.0) || (n == "bridging" && !(d >= 1.0)))
                fail(k, n == "bridging" ? "must be from 1 to 100 mm" : "must be from 0 to 100 mm");
            (n == "brim_width" ? s.wipe_tower_brim : s.wipe_tower_bridging) = d;
        } else if (n == "cone_angle") {
            if (!(d >= 0.0 && d <= 90.0))
                fail(k, "must be from 0 to 90 degrees");
            s.wipe_tower_cone_angle = d;
        } else {
            if (!(d >= 50.0 && d <= 500.0))
                fail(k, "must be from 50 to 500 %");
            (n == "extra_spacing" ? s.wipe_tower_extra_spacing : s.wipe_tower_extra_flow) = d;
        }
    });
    // The MMU's change (multi_extruder.md, rule 15).
    read_group(root, "mmu",
               {"cooling_tube_length", "cooling_tube_position", "parking_position", "extra_loading_move",
                "high_current_on_swap"},
               [&](const json& g, const std::string& n, const std::string& k) {
        if (n == "high_current_on_swap") {
            if (!g.at(n).is_boolean())
                fail(k, "must be true or false");
            s.mmu_high_current_on_swap = g.at(n).get<bool>();
            return;
        }
        const double d = number(g, n, k);
        if (!(d >= (n == "extra_loading_move" ? -1000.0 : 0.0) && d <= 1000.0))
            fail(k, n == "extra_loading_move" ? "must be from -1000 to 1000 mm" : "must be from 0 to 1000 mm");
        (n == "cooling_tube_length"     ? s.mmu_cooling_tube_length
         : n == "cooling_tube_position" ? s.mmu_cooling_tube_position
         : n == "parking_position"      ? s.mmu_parking_position
                                        : s.mmu_extra_loading_move) = d;
    });
    read_group(root, "vase", {"enabled"}, [&](const json& g, const std::string& n, const std::string& k) {
        if (!g.at(n).is_boolean())
            fail(k, "must be true or false");
        s.vase = g.at(n).get<bool>();
    });
    read_group(root, "surfaces", {"top_layers", "bottom_layers", "top_pattern", "top_min_thickness",
                                  "bottom_min_thickness", "interface_shells"},
               [&](const json& g, const std::string& n, const std::string& k) {
        if (n == "interface_shells") {
            if (!g.at(n).is_boolean())
                fail(k, "must be true or false");
            s.interface_shells = g.at(n).get<bool>();
            return;
        }
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
                "hardened_nozzle", "min_layer_height", "max_layer_height", "z_offset", "max_accel_x", "max_accel_y",
                "max_accel_z", "max_accel_e", "retract_acceleration", "jerk_x", "jerk_y", "jerk_z", "jerk_e",
                "junction_deviation", "min_extrude_feed", "min_travel_feed", "limits_usage", "silent_mode", "silent",
                "extruders", "extruder_offset", "single_extruder_mmu"},
               [&](const json& g, const std::string& n, const std::string& k) {
                   const json& v = g.at(n);
                   // Several extruders (multi_extruder.md): as many as the
                   // colour channel paints, each nozzle's offset from the first.
                   if (n == "extruders") {
                       const double d = number(g, n, k);
                       if (!(d >= 1.0 && d <= double(kMaxExtruders)) || d != std::floor(d))
                           fail(k, "must be a whole number from 1 to " + std::to_string(kMaxExtruders));
                       s.extruders = int(d);
                       return;
                   }
                   if (n == "single_extruder_mmu") {
                       if (!v.is_boolean())
                           fail(k, "must be true or false");
                       s.single_extruder_mmu = v.get<bool>();
                       return;
                   }
                   if (n == "extruder_offset") {
                       const char* what = "must be a list of [x, y] offsets in mm, one per extruder, from -1000 to 1000";
                       if (!v.is_array() || v.size() > std::size_t(kMaxExtruders))
                           fail(k, what);
                       std::vector<std::array<double, 2>> offsets;
                       for (const json& p : v) {
                           if (!p.is_array() || p.size() != 2 || !p[0].is_number() || !p[1].is_number())
                               fail(k, what);
                           const double x = p[0].get<double>(), y = p[1].get<double>();
                           if (!(std::abs(x) <= 1000.0 && std::abs(y) <= 1000.0))
                               fail(k, what);
                           offsets.push_back({x, y});
                       }
                       s.extruder_offset = std::move(offsets);
                       return;
                   }
                   if (n == "limits_usage") {
                       s.limits_usage = choice<LimitsUsage>(
                           g, n, k,
                           {{"time_only", LimitsUsage::TimeOnly}, {"emit", LimitsUsage::Emit}, {"ignore", LimitsUsage::Ignore}});
                       return;
                   }
                   if (n == "silent_mode") {
                       if (!v.is_boolean())
                           fail(k, "must be true or false");
                       s.silent_mode = v.get<bool>();
                       return;
                   }
                   if (n == "silent") { // the silent mode's limits, by name
                       if (!v.is_object())
                           fail(k, "must be an object of limit: value");
                       Settings probe;
                       std::map<std::string, double> limits;
                       for (const auto& [name, value] : v.items()) {
                           if (!limit_named(probe, name))
                               fail(k + "." + name, "is not a machine limit");
                           if (!value.is_number() || !(value.get<double>() >= 0.0))
                               fail(k + "." + name, "must be a number, 0 or more");
                           limits[name] = value.get<double>();
                       }
                       s.silent_limits = std::move(limits);
                       return;
                   }
                   if (n.starts_with("max_accel_") || n.starts_with("jerk_") || n == "retract_acceleration" ||
                       n == "junction_deviation" || n == "min_extrude_feed" || n == "min_travel_feed") {
                       const double d = number(g, n, k);
                       if (!(d >= 0.0))
                           fail(k, "must not be negative");
                       *limit_named(s, n) = d;
                       return;
                   }
                   if (n == "z_offset") {
                       const double d = number(g, n, k);
                       if (!(d >= -10.0 && d <= 10.0))
                           fail(k, "must be from -10 to 10 mm");
                       s.z_offset = d;
                       return;
                   }
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
                       s.flavor = choice<GcodeFlavor>(g, n, k, flavors());
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
               {"walls", "outer_wall", "infill", "solid_infill", "top_solid_infill", "support", "travel", "first_layer", "max",
                "bridges", "gap_fill", "over_bridges", "first_layer_infill", "travel_z", "dynamic_overhangs", "overhang_0",
                "overhang_1", "overhang_2", "overhang_3", "flow_slope_up", "flow_slope_down", "first_layer_over_raft"},
               [&](const json& g, const std::string& n, const std::string& k) {
                   const json& v = g.at(n);
                   if (n == "flow_slope_up" || n == "flow_slope_down") { // not a speed: 0 is free
                       const double d = number(g, n, k);
                       if (!(d >= 0.0 && d <= 1000.0))
                           fail(k, "must be from 0 to 1000 mm3/s2");
                       (n == "flow_slope_up" ? s.flow_slope_up : s.flow_slope_down) = d;
                       return;
                   }
                   if (n == "dynamic_overhangs") {
                       if (!v.is_boolean())
                           fail(k, "must be true or false");
                       s.dynamic_overhangs = v.get<bool>();
                       return;
                   }
                   // The speeds that may be a share of another.
                   Speed* const share = n == "outer_wall"           ? &s.outer_wall_speed
                                        : n == "over_bridges"       ? &s.over_bridges_speed
                                        : n == "first_layer_infill" ? &s.first_layer_infill_speed
                                        : n == "first_layer_over_raft" ? &s.first_layer_over_raft
                                        : n.rfind("overhang_", 0) == 0
                                            ? &s.overhang_speeds[std::size_t(n.back() - '0')]
                                            : nullptr;
                   if (v.is_null()) { // not set: from the printer's profile
                       if (share)
                           *share = {};
                       else
                           *speed_of(s, "speed." + n) = 0.0;
                       return;
                   }
                   if (share && v.is_string()) {
                       const std::string t = v.get<std::string>();
                       char* end = nullptr;
                       const double p = std::strtod(t.c_str(), &end);
                       // strtod reads "inf%" too: a finite share, up to 1000 %.
                       if (t.empty() || t.back() != '%' || end != t.c_str() + t.size() - 1 || !(p > 0.0) ||
                           !(p <= 1000.0))
                           fail(k, "must be a number of mm/s or a percentage from 0 to 1000 like \"50%\"");
                       *share = {p, true};
                       return;
                   }
                   const double d = number(g, n, k);
                   if (!(d > 0.0))
                       fail(k, "must be greater than 0");
                   if (share)
                       *share = {d, false};
                   else if (n == "bridges" || n == "gap_fill" || n == "travel_z")
                       *speed_of(s, "speed." + n) = d;
                   else if (n == "walls")
                       s.wall_speed = d;
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
    read_group(root, "acceleration",
               {"default", "walls", "outer_wall", "infill", "solid_infill", "top_solid_infill", "bridges", "first_layer",
                "first_layer_over_raft", "travel", "short_travel"},
               [&](const json& g, const std::string& n, const std::string& k) {
                   const double v = number(g, n, k);
                   if (!(v >= 0.0 && v <= 100000.0))
                       fail(k, "must be from 0 to 100000 mm/s2");
                   *acceleration_of(s, n) = v;
               });
    read_group(root, "retraction",
               {"length", "speed", "lift", "min_travel", "unretract_speed", "restart_extra", "on_layer_change", "wipe",
                "before_wipe", "lift_above", "lift_below", "firmware", "toolchange_length", "toolchange_restart_extra"},
               [&](const json& g, const std::string& n, const std::string& k) {
                   // A tool change's (multi_extruder.md, rule 5): one value for
                   // every extruder, or a list by extruder.
                   if (n == "toolchange_length" || n == "toolchange_restart_extra") {
                       const double top = n == "toolchange_length" ? 200.0 : 10.0;
                       const std::string what = "must be a length from 0 to " + std::to_string(int(top)) +
                                                " mm, or a list of them by extruder";
                       const json& v = g.at(n);
                       std::vector<double> list;
                       for (const json& x : v.is_array() ? v : json::array({v})) {
                           if (!x.is_number() || !(x.get<double>() >= 0.0 && x.get<double>() <= top))
                               fail(k, what);
                           list.push_back(x.get<double>());
                       }
                       if (list.empty() || list.size() > std::size_t(kMaxExtruders))
                           fail(k, what);
                       (n == "toolchange_length" ? s.toolchange_length : s.toolchange_restart_extra) = std::move(list);
                       return;
                   }
                   if (n == "on_layer_change" || n == "wipe" || n == "firmware") {
                       if (!g.at(n).is_boolean())
                           fail(k, "must be true or false");
                       (n == "wipe" ? s.wipe : n == "firmware" ? s.firmware_retraction : s.retract_layer_change) =
                           g.at(n).get<bool>();
                       return;
                   }
                   if (n == "unretract_speed" || n == "restart_extra" || n == "before_wipe" || n == "lift_above" ||
                       n == "lift_below") {
                       const double d = number(g, n, k);
                       const double top = n == "unretract_speed" ? 1000.0 : n == "before_wipe" ? 100.0 : n == "restart_extra" ? 10.0 : 10000.0;
                       if (!(d >= 0.0 && d <= top))
                           fail(k, "must be from 0 to " + std::to_string(int(top)));
                       (n == "unretract_speed" ? s.unretract_speed
                        : n == "restart_extra" ? s.restart_extra
                        : n == "before_wipe"   ? s.before_wipe
                        : n == "lift_above"    ? s.lift_above
                                               : s.lift_below) = d;
                       return;
                   }
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
    read_group(root, "adhesion",
               {"skirt_loops", "skirt_distance", "skirt_layers", "brim_width", "raft_layers", "raft_gap", "raft_expansion",
                "raft_first_layer_density", "raft_first_layer_expansion", "skirt_min_filament", "draft_shield", "brim_type",
                "brim_gap"},
               [&](const json& g, const std::string& n, const std::string& k) {
                   if (n == "draft_shield") {
                       s.draft_shield = choice<DraftShield>(g, n, k,
                                                            {{"disabled", DraftShield::Disabled},
                                                             {"limited", DraftShield::Limited},
                                                             {"enabled", DraftShield::Enabled}});
                       return;
                   }
                   if (n == "brim_type") {
                       s.brim_type = choice<BrimType>(g, n, k,
                                                      {{"none", BrimType::None},
                                                       {"outer", BrimType::Outer},
                                                       {"inner", BrimType::Inner},
                                                       {"outer_and_inner", BrimType::OuterAndInner}});
                       return;
                   }
                   if (n == "skirt_min_filament" || n == "brim_gap") {
                       const double d = number(g, n, k);
                       if (!(d >= 0.0 && d <= (n == "brim_gap" ? 100.0 : 100000.0)))
                           fail(k, n == "brim_gap" ? "must be from 0 to 100 mm" : "must be from 0 to 100000 mm");
                       (n == "brim_gap" ? s.brim_gap : s.skirt_min_filament) = d;
                       return;
                   }
                   if (n == "skirt_loops" || n == "skirt_layers") {
                       const json& v = g.at(n);
                       if (!v.is_number_integer() || v.get<long long>() < 0 || v.get<long long>() > 1000)
                           fail(k, "must be a whole number from 0 to 1000");
                       (n == "skirt_loops" ? s.skirt_loops : s.skirt_layers) = static_cast<int>(v.get<long long>());
                       return;
                   }
                   if (n == "raft_layers") {
                       const json& v = g.at(n);
                       if (!v.is_number_integer() || v.get<long long>() < 0 || v.get<long long>() > 100)
                           fail(k, "must be a whole number from 0 to 100");
                       s.raft_layers = static_cast<int>(v.get<long long>());
                       return;
                   }
                   const double d = number(g, n, k);
                   if (n == "raft_first_layer_density") {
                       if (!(d >= 5.0 && d <= 100.0))
                           fail(k, "must be from 5 to 100 %");
                       s.raft_first_layer_density = d;
                       return;
                   }
                   if (d < 0.0 || (n.starts_with("raft_") && d > 100.0))
                       fail(k, n.starts_with("raft_") ? "must be from 0 to 100 mm" : "must not be negative");
                   (n == "skirt_distance"           ? s.skirt_distance
                    : n == "brim_width"             ? s.brim_width
                    : n == "raft_gap"               ? s.raft_gap
                    : n == "raft_expansion"         ? s.raft_expansion
                                                    : s.raft_first_layer_expansion) = d;
               });
    read_group(root, "support",
               {"enabled", "automatic", "threshold_angle", "z_gap", "xy_gap", "width", "spacing", "line_angle", "interface_layers", "style",
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
                   if (n == "width") {
                       if (g.at(n).is_string()) {
                           const std::string t = g.at(n).get<std::string>();
                           char* end = nullptr;
                           const double p = std::strtod(t.c_str(), &end);
                           if (t.empty() || t.back() != '%' || end != t.c_str() + t.size() - 1 || !(p > 0.0) || p > 1000.0)
                               fail(k, "must be a number of mm or a percentage of the layer height like \"150%\"");
                           s.support_width = {p, true};
                           return;
                       }
                       const double v = number(g, n, k);
                       if (!(v >= 0.0 && v <= 40.0))
                           fail(k, "must be from 0 to 40 mm");
                       s.support_width = {v, false};
                       return;
                   }
                   if (n == "xy_gap" && g.at(n).is_string()) {
                       const std::string t = g.at(n).get<std::string>();
                       char* end = nullptr;
                       const double p = std::strtod(t.c_str(), &end);
                       if (t.empty() || t.back() != '%' || end != t.c_str() + t.size() - 1 || !(p >= 0.0) || p > 1000.0)
                           fail(k, "must be a number of mm or a percentage of the wall width like \"50%\"");
                       s.support_xy_gap = {p, true};
                       return;
                   }
                   const double d = number(g, n, k);
                   if (n == "line_angle") {
                       if (!(d >= -360.0 && d <= 360.0))
                           fail(k, "must be from -360 to 360 degrees");
                       s.support_line_angle = d;
                       return;
                   }
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
                   if (n == "z_gap")
                       s.support_z_gap = d;
                   else
                       s.support_xy_gap = {d, false};
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
    read_group(root, "ironing", {"enabled", "surfaces", "flow", "spacing", "speed"},
               [&](const json& g, const std::string& n, const std::string& k) {
                   if (n == "enabled") {
                       if (!g.at(n).is_boolean())
                           fail(k, "must be true or false");
                       s.ironing = g.at(n).get<bool>();
                       return;
                   }
                   if (n == "surfaces") {
                       s.ironing_surfaces = choice<IroningSurfaces>(
                           g, n, k,
                           {{"top", IroningSurfaces::Top}, {"topmost", IroningSurfaces::Topmost}, {"solid", IroningSurfaces::Solid}});
                       return;
                   }
                   const double v = number(g, n, k);
                   if (n == "flow") {
                       if (!(v > 0.0 && v <= 100.0))
                           fail(k, "must be more than 0 and at most 100 %");
                       s.ironing_flow = v;
                   } else if (n == "spacing") {
                       if (!(v >= 0.01 && v <= 2.0))
                           fail(k, "must be from 0.01 to 2 mm");
                       s.ironing_spacing = v;
                   } else {
                       if (!(v > 0.0 && v <= 1000.0))
                           fail(k, "must be more than 0 and at most 1000 mm/s");
                       s.ironing_speed = v;
                   }
               });
    read_group(root, "arcs", {"enabled", "overlap", "speed", "max_distance"},
               [&](const json& g, const std::string& n, const std::string& k) {
                   if (n == "enabled") {
                       if (!g.at(n).is_boolean())
                           fail(k, "must be true or false");
                       s.arcs = g.at(n).get<bool>();
                       return;
                   }
                   const double v = number(g, n, k);
                   if (n == "overlap") {
                       if (v < 0.0 || v > 90.0)
                           fail(k, "must be from 0 to 90 %");
                       s.arcs_overlap = v;
                       return;
                   }
                   if (n == "speed") {
                       if (v < 0.0 || v > 1000.0)
                           fail(k, "must be from 0 to 1000 mm/s");
                       s.arcs_speed = v;
                       return;
                   }
                   if (v < 0.0 || v > 1000.0)
                       fail(k, "must be from 0 to 1000 mm");
                   s.arcs_max_distance = v;
               });
    read_group(root, "review", {"nozzle_force", "bed_adhesion", "max_deflection"},
               [&](const json& g, const std::string& n, const std::string& k) {
                   const double v = number(g, n, k);
                   if (n == "max_deflection") {
                       if (v <= 0.0 || v > 10.0)
                           fail(k, "must be more than 0, up to 10 mm");
                       s.review_max_deflection = v;
                       return;
                   }
                   if (v < 0.0 || v > 1000.0)
                       fail(k, n == "nozzle_force" ? "must be from 0 to 1000 N" : "must be from 0 to 1000 N/mm2");
                   (n == "nozzle_force" ? s.review_nozzle_force : s.review_bed_adhesion) = v;
               });
    read_group(root, "travel",
               {"ramping_lift", "slope", "max_lift", "lift_before_obstacle", "avoid_crossing_walls", "max_detour"},
               [&](const json& g, const std::string& n, const std::string& k) {
        const json& v = g.at(n);
        if (n == "ramping_lift" || n == "lift_before_obstacle" || n == "avoid_crossing_walls") {
            if (!v.is_boolean())
                fail(k, "must be true or false");
            (n == "ramping_lift" ? s.ramping_lift : n == "lift_before_obstacle" ? s.lift_before_obstacle : s.avoid_crossing_walls) =
                v.get<bool>();
            return;
        }
        if (n == "max_detour") {
            if (v.is_string()) {
                const std::string t = v.get<std::string>();
                char* end = nullptr;
                const double p = std::strtod(t.c_str(), &end);
                if (t.empty() || t.back() != '%' || end != t.c_str() + t.size() - 1 || !(p >= 0.0) || !(p <= 10000.0))
                    fail(k, "must be a length in mm or a percentage like \"200%\"");
                s.max_detour = {p, true};
                return;
            }
            const double d = number(g, n, k);
            if (!(d >= 0.0))
                fail(k, "must not be negative");
            s.max_detour = {d, false};
            return;
        }
        const double d = number(g, n, k);
        if (n == "slope" && !(d >= 0.0 && d <= 89.0))
            fail(k, "must be from 0 to 89 degrees");
        if (n == "max_lift" && !(d >= 0.0 && d <= 10.0))
            fail(k, "must be from 0 to 10 mm");
        (n == "slope" ? s.travel_slope : s.max_lift) = d;
    });
    read_group(root, "output", {"filename_format", "post_process"},
               [&](const json& g, const std::string& n, const std::string& k) {
        const json& v = g.at(n);
        if (n == "filename_format") {
            if (!v.is_string() || v.get<std::string>().empty())
                fail(k, "must be a non-empty string");
            s.filename_format = v.get<std::string>();
            return;
        }
        if (!v.is_array())
            fail(k, "must be a list of paths");
        std::vector<std::string> scripts;
        for (const json& item : v) {
            if (!item.is_string() || item.get<std::string>().empty())
                fail(k, "must be a list of paths");
            scripts.push_back(item.get<std::string>());
        }
        s.post_process = std::move(scripts);
    });
    read_group(root, "gcode", {"resolution", "color_change", "pause", "before_layer", "after_layer", "between_objects", "volumetric_e",
                               "verbose_comments", "arcs", "progress", "auto_temperatures", "binary", "object_labels",
                               "toolchange"},
               [&](const json& g, const std::string& n, const std::string& k) {
        if (n == "object_labels") {
            s.object_labels = choice<ObjectLabels>(
                g, n, k,
                {{"none", ObjectLabels::None}, {"octoprint", ObjectLabels::OctoPrint}, {"firmware", ObjectLabels::Firmware}});
            return;
        }
        if (n == "volumetric_e" || n == "verbose_comments" || n == "arcs" || n == "progress" || n == "auto_temperatures" ||
            n == "binary") {
            if (!g.at(n).is_boolean())
                fail(k, "must be true or false");
            (n == "volumetric_e"        ? s.volumetric_e
             : n == "binary"            ? s.gcode_binary
             : n == "arcs"              ? s.gcode_arcs
             : n == "progress"          ? s.gcode_progress
             : n == "auto_temperatures" ? s.auto_temperatures
                                        : s.verbose_comments) = g.at(n).get<bool>();
            return;
        }
        if (n != "resolution") {
            if (!g.at(n).is_string())
                fail(k, "must be a string");
            (n == "color_change" ? s.gcode_color_change
             : n == "pause"      ? s.gcode_pause
             : n == "toolchange" ? s.gcode_toolchange
             : n == "before_layer" ? s.gcode_before_layer
             : n == "between_objects" ? s.gcode_between_objects
                                   : s.gcode_after_layer) = g.at(n).get<std::string>();
            return;
        }
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
    read_group(root, "extrusion",
               {"flow", "pressure_advance", "max_flow", "chamber_temperature", "abrasive", "needs_enclosure",
                "infill_max_speed", "infill_max_crossing_speed", "bridge_flow_ratio", "chamber_min_temperature", "density",
                "cost", "spool_weight", "shrinkage_xy", "shrinkage_z", "start_gcode", "end_gcode", "retraction", "travel",
                "purge_multiplier", "min_purge_on_tower", "loading_speed", "loading_speed_start", "unloading_speed",
                "unloading_speed_start", "cooling_moves", "cooling_initial_speed", "cooling_final_speed", "ramming",
                "toolchange_delay", "load_time", "unload_time"},
               [&](const json& g, const std::string& n, const std::string& k) {
        // An MMU's change (multi_extruder.md, rule 15).
        if (n == "ramming") {
            const json& v = g.at(n);
            const char* what = "must be a list of up to 100 flows from 0 to 100 mm3/s, one per quarter of a second";
            if (!v.is_array() || v.size() > 100)
                fail(k, what);
            std::vector<double> list;
            for (const json& x : v) {
                if (!x.is_number() || !(x.get<double>() >= 0.0 && x.get<double>() <= 100.0))
                    fail(k, what);
                list.push_back(x.get<double>());
            }
            s.ramming = std::move(list);
            return;
        }
        if (n == "cooling_moves") {
            const json& v = g.at(n);
            if (!v.is_number_integer() || v.get<long long>() < 0 || v.get<long long>() > 100)
                fail(k, "must be a whole number from 0 to 100");
            s.cooling_moves = int(v.get<long long>());
            return;
        }
        if (n == "loading_speed" || n == "loading_speed_start" || n == "unloading_speed" || n == "unloading_speed_start" ||
            n == "cooling_initial_speed" || n == "cooling_final_speed" || n == "toolchange_delay" || n == "load_time" ||
            n == "unload_time") {
            const double d = number(g, n, k);
            const bool speed = n.find("speed") != std::string::npos;
            if (!(speed ? d > 0.0 && d <= 1000.0 : d >= 0.0 && d <= 1000.0))
                fail(k, speed ? "must be more than 0 and at most 1000 mm/s" : "must be from 0 to 1000 s");
            (n == "loading_speed"           ? s.loading_speed
             : n == "loading_speed_start"   ? s.loading_speed_start
             : n == "unloading_speed"       ? s.unloading_speed
             : n == "unloading_speed_start" ? s.unloading_speed_start
             : n == "cooling_initial_speed" ? s.cooling_initial_speed
             : n == "cooling_final_speed"   ? s.cooling_final_speed
             : n == "toolchange_delay"      ? s.toolchange_delay
             : n == "load_time"             ? s.load_time
                                            : s.unload_time) = d;
            return;
        }
        // multi_extruder.md, rules 12 and 13: one value, or one per extruder.
        if (n == "purge_multiplier" || n == "min_purge_on_tower") {
            const json& v = g.at(n);
            const double top = n == "purge_multiplier" ? 1000.0 : 10000.0;
            const std::string what = n == "purge_multiplier" ? "must be from 0 to 1000 %, or a list of them by extruder"
                                                             : "must be from 0 to 10000 mm3, or a list of them by extruder";
            std::vector<double> list;
            for (const json& x : v.is_array() ? v : json::array({v})) {
                if (!x.is_number() || !(x.get<double>() >= 0.0 && x.get<double>() <= top))
                    fail(k, what);
                list.push_back(x.get<double>());
            }
            if (list.empty() || list.size() > std::size_t(kMaxExtruders))
                fail(k, what);
            (n == "purge_multiplier" ? s.purge_multiplier : s.min_purge_on_tower) = std::move(list);
            return;
        }
        if (n == "retraction" || n == "travel") { // the filament's own values (retraction_travel.md, rule 10)
            const json& v = g.at(n);
            if (!v.is_object())
                fail(k, "must be an object of setting: value");
            for (const auto& [name, value] : v.items()) {
                const std::string key = n + "." + name;
                if (std::find_if(kFilamentRetractionNames.begin(), kFilamentRetractionNames.end(),
                                 [&](const char* x) { return key == x; }) == kFilamentRetractionNames.end())
                    fail(k + "." + name, "is not a retraction or travel setting a filament may give");
                if (value.is_null()) { // empty: the printer's
                    s.filament_retraction.erase(key);
                } else if (value.is_boolean()) {
                    s.filament_retraction[key] = value.get<bool>() ? 1.0 : 0.0;
                } else if (value.is_number() && value.get<double>() >= 0.0 && value.get<double>() <= 1000.0) {
                    s.filament_retraction[key] = value.get<double>();
                } else {
                    fail(k + "." + name, "must be a number from 0 to 1000, true or false");
                }
            }
            return;
        }
        if (n == "start_gcode" || n == "end_gcode") {
            if (!g.at(n).is_string())
                fail(k, "must be a string");
            (n == "start_gcode" ? s.filament_start_gcode : s.filament_end_gcode) = g.at(n).get<std::string>();
            return;
        }
        if (n == "abrasive" || n == "needs_enclosure") {
            if (!g.at(n).is_boolean())
                fail(k, "must be true or false");
            (n == "abrasive" ? s.abrasive : s.needs_enclosure) = g.at(n).get<bool>();
            return;
        }
        const double v = number(g, n, k);
        if (n == "max_flow" || n == "chamber_temperature" || n == "chamber_min_temperature") {
            if (v < 0.0 || v > (n == "max_flow" ? 1000.0 : 200.0))
                fail(k, n == "max_flow" ? "must be from 0 to 1000 mm3/s" : "must be from 0 to 200 C");
            (n == "max_flow"              ? s.filament_max_flow
             : n == "chamber_temperature" ? s.chamber_temperature
                                          : s.chamber_min_temperature) = v;
            return;
        }
        if (n == "shrinkage_xy" || n == "shrinkage_z") {
            if (v < 0.0 || v > 20.0)
                fail(k, "must be a percentage from 0 to 20");
            (n == "shrinkage_xy" ? s.shrinkage_xy : s.shrinkage_z) = v;
            return;
        }
        if (n == "density" || n == "cost" || n == "spool_weight") {
            if (v < 0.0 || v > (n == "density" ? 30.0 : 1e6))
                fail(k, n == "density" ? "must be from 0 to 30 g/cm3" : "must not be negative");
            (n == "density" ? s.filament_density : n == "cost" ? s.filament_cost : s.spool_weight) = v;
            return;
        }
        if (n == "pressure_advance") {
            if (v < 0.0 || v > 2.0)
                fail(k, "must be from 0 to 2");
            s.pressure_advance = v;
            return;
        }
        if (n == "infill_max_speed" || n == "infill_max_crossing_speed") {
            if (v < 0.0 || v > 1000.0)
                fail(k, "must be from 0 to 1000 mm/s");
            (n == "infill_max_speed" ? s.infill_max_speed : s.infill_max_crossing_speed) = v;
            return;
        }
        if (n == "bridge_flow_ratio") {
            if (v < 0.1 || v > 2.0)
                fail(k, "must be from 0.1 to 2");
            s.bridge_flow_ratio = v;
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
                } else if (n == "color_change" || n == "pause") {
                    if (!v.is_boolean())
                        fail(where + "." + n, "must be true or false");
                    (n == "color_change" ? c.color_change : c.pause) = v.get<bool>();
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
        if (c.color_change)
            j["color_change"] = true;
        if (c.pause)
            j["pause"] = true;
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
                                     s.first_layer_speed, s.bridge_speed, s.gap_fill_speed, s.arcs_speed,
                                     s.over_bridges_speed.of(s.solid_infill_speed),
                                     s.first_layer_infill_speed.of(s.solid_infill_speed)});
    return s.max_speed > 0.0 ? std::min(fastest, s.max_speed) : fastest;
}

std::vector<std::pair<std::string, std::string>> describe(const Settings& s) {
    auto num = [](double v) { return json(v).dump(); };
    auto speed = [](double v) { return v > 0.0 ? json(v).dump() : std::string("null"); }; // 0: not set
    auto share = [&](const Speed& v) { return v.value <= 0.0 ? std::string("null") : num(v.value) + (v.percent ? "%" : ""); };
    auto flag = [](bool b) { return std::string(b ? "true" : "false"); };
    auto text = [](const std::string& t) { return json(t).dump(); };
    const char* fill_mode = s.fill_mode == FillMode::Union ? "union" : s.fill_mode == FillMode::EvenOdd ? "even_odd" : "fill_holes";
    const char* pattern = infill_pattern_name(s.infill_pattern);
    const char* flavor = flavor_name(s.flavor);
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
        {"walls.detect_overhangs", flag(s.detect_overhangs)},
        {"walls.generator", s.wall_generator == WallGenerator::Fixed ? "fixed" : "variable"},
        {"walls.gap_fill", flag(s.wall_gap_fill)},
        {"walls.min_line_width", s.wall_min_line_width.percent ? num(s.wall_min_line_width.value) + "%"
                                                               : num(s.wall_min_line_width.value)},
        {"walls.min_detail", s.wall_min_detail.percent ? num(s.wall_min_detail.value) + "%" : num(s.wall_min_detail.value)},
        {"vase.enabled", flag(s.vase)},
        {"sequential.enabled", flag(s.sequential)},
        {"sequential.clearance_radius", num(s.clearance_radius)},
        {"sequential.clearance_height", num(s.clearance_height)},
        {"machine.extruders", std::to_string(s.extruders)},
        {"machine.extruder_offset", json(s.extruder_offset).dump()},
        {"extruders.walls", std::to_string(s.extruder_walls)},
        {"extruders.infill", std::to_string(s.extruder_infill)},
        {"extruders.solid_infill", std::to_string(s.extruder_solid_infill)},
        {"extruders.support", std::to_string(s.extruder_support)},
        {"extruders.support_interface", std::to_string(s.extruder_support_interface)},
        {"extruders.bed_temperature_from", std::to_string(s.bed_temperature_from)},
        {"extruders.ooze_prevention", flag(s.ooze_prevention)},
        {"extruders.standby_delta", num(s.standby_delta)},
        {"retraction.toolchange_length", json(s.toolchange_length).dump()},
        {"retraction.toolchange_restart_extra", json(s.toolchange_restart_extra).dump()},
        {"gcode.toolchange", text(s.gcode_toolchange)},
        {"machine.single_extruder_mmu", flag(s.single_extruder_mmu)},
        {"extruders.wipe_tower", std::to_string(s.extruder_wipe_tower)},
        {"wipe_tower.enabled", flag(s.wipe_tower)},
        {"wipe_tower.x", num(s.wipe_tower_x)},
        {"wipe_tower.y", num(s.wipe_tower_y)},
        {"wipe_tower.rotation", num(s.wipe_tower_rotation)},
        {"wipe_tower.width", num(s.wipe_tower_width)},
        {"wipe_tower.brim_width", num(s.wipe_tower_brim)},
        {"wipe_tower.bridging", num(s.wipe_tower_bridging)},
        {"wipe_tower.cone_angle", num(s.wipe_tower_cone_angle)},
        {"wipe_tower.extra_spacing", num(s.wipe_tower_extra_spacing)},
        {"wipe_tower.extra_flow", num(s.wipe_tower_extra_flow)},
        {"wipe_tower.no_sparse_layers", flag(s.wipe_tower_no_sparse_layers)},
        {"wipe_tower.purge_volumes", json(s.purge_volumes).dump()},
        {"extrusion.purge_multiplier", json(s.purge_multiplier).dump()},
        {"extrusion.min_purge_on_tower", json(s.min_purge_on_tower).dump()},
        {"mmu.cooling_tube_length", num(s.mmu_cooling_tube_length)},
        {"mmu.cooling_tube_position", num(s.mmu_cooling_tube_position)},
        {"mmu.parking_position", num(s.mmu_parking_position)},
        {"mmu.extra_loading_move", num(s.mmu_extra_loading_move)},
        {"mmu.high_current_on_swap", flag(s.mmu_high_current_on_swap)},
        {"extrusion.loading_speed", num(s.loading_speed)},
        {"extrusion.loading_speed_start", num(s.loading_speed_start)},
        {"extrusion.unloading_speed", num(s.unloading_speed)},
        {"extrusion.unloading_speed_start", num(s.unloading_speed_start)},
        {"extrusion.cooling_moves", std::to_string(s.cooling_moves)},
        {"extrusion.cooling_initial_speed", num(s.cooling_initial_speed)},
        {"extrusion.cooling_final_speed", num(s.cooling_final_speed)},
        {"extrusion.ramming", json(s.ramming).dump()},
        {"extrusion.toolchange_delay", num(s.toolchange_delay)},
        {"extrusion.load_time", num(s.load_time)},
        {"extrusion.unload_time", num(s.unload_time)},
        {"placement.spacing", num(s.placement_spacing)},
        {"placement.arrange", flag(s.placement_arrange)},
        {"placement.center", s.placement_center ? json(*s.placement_center).dump() : std::string("bed")},
        {"fuzzy.mode", fuzzy_mode_name(s.fuzzy_mode)},
        {"fuzzy.thickness", num(s.fuzzy_thickness)},
        {"fuzzy.point_distance", num(s.fuzzy_point_distance)},
        {"arcs.enabled", flag(s.arcs)},
        {"arcs.overlap", num(s.arcs_overlap)},
        {"arcs.speed", num(s.arcs_speed)},
        {"arcs.max_distance", num(s.arcs_max_distance)},
        {"ironing.enabled", flag(s.ironing)},
        {"ironing.surfaces", s.ironing_surfaces == IroningSurfaces::Topmost ? "topmost"
                             : s.ironing_surfaces == IroningSurfaces::Solid ? "solid"
                                                                            : "top"},
        {"ironing.flow", num(s.ironing_flow)},
        {"ironing.spacing", num(s.ironing_spacing)},
        {"ironing.speed", num(s.ironing_speed)},
        {"review.nozzle_force", num(s.review_nozzle_force)},
        {"review.bed_adhesion", num(s.review_bed_adhesion)},
        {"review.max_deflection", num(s.review_max_deflection)},
        {"surfaces.top_layers", std::to_string(s.top_layers)},
        {"surfaces.bottom_layers", std::to_string(s.bottom_layers)},
        {"surfaces.top_pattern", s.monotonic_top ? "monotonic" : "rectilinear"},
        {"surfaces.top_min_thickness", num(s.top_min_thickness)},
        {"surfaces.bottom_min_thickness", num(s.bottom_min_thickness)},
        {"surfaces.interface_shells", flag(s.interface_shells)},
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
        {"machine.max_accel_x", num(s.max_accel_x)},
        {"machine.max_accel_y", num(s.max_accel_y)},
        {"machine.max_accel_z", num(s.max_accel_z)},
        {"machine.max_accel_e", num(s.max_accel_e)},
        {"machine.retract_acceleration", num(s.retract_acceleration)},
        {"machine.jerk_x", num(s.jerk_x)},
        {"machine.jerk_y", num(s.jerk_y)},
        {"machine.jerk_z", num(s.jerk_z)},
        {"machine.jerk_e", num(s.jerk_e)},
        {"machine.junction_deviation", num(s.junction_deviation)},
        {"machine.min_extrude_feed", num(s.min_extrude_feed)},
        {"machine.min_travel_feed", num(s.min_travel_feed)},
        {"machine.limits_usage", json(limits_usage_name(s.limits_usage)).dump()},
        {"machine.silent_mode", flag(s.silent_mode)},
        {"machine.silent", json(s.silent_limits).dump()},
        {"gcode.progress", flag(s.gcode_progress)},
        {"gcode.object_labels", object_labels_name(s.object_labels)},
        {"output.filename_format", text(s.filename_format)},
        {"output.post_process", json(s.post_process).dump()},
        {"gcode.auto_temperatures", flag(s.auto_temperatures)},
        {"gcode.binary", flag(s.gcode_binary)},
        {"extrusion.start_gcode", text(s.filament_start_gcode)},
        {"extrusion.end_gcode", text(s.filament_end_gcode)},
        {"machine.corner_speed", speed(s.corner_speed)},
        {"machine.bed", json(s.bed).dump()},
        {"machine.max_height", num(s.max_height)},
        {"machine.min_layer_height", num(s.min_layer_height)},
        {"machine.max_layer_height", num(s.max_layer_height)},
        {"speed.walls", speed(s.wall_speed)},
        {"speed.outer_wall", share(s.outer_wall_speed)},
        {"speed.infill", speed(s.infill_speed)},
        {"speed.solid_infill", speed(s.solid_infill_speed)},
        {"speed.top_solid_infill", speed(s.top_solid_infill_speed)},
        {"speed.support", speed(s.support_speed)},
        {"speed.travel", speed(s.travel_speed)},
        {"speed.first_layer", speed(s.first_layer_speed)},
        {"speed.max", speed(s.max_speed)},
        {"speed.bridges", speed(s.bridge_speed)},
        {"speed.gap_fill", speed(s.gap_fill_speed)},
        {"speed.over_bridges", share(s.over_bridges_speed)},
        {"speed.first_layer_infill", share(s.first_layer_infill_speed)},
        {"speed.travel_z", speed(s.travel_z_speed)},
        {"speed.first_layer_over_raft", share(s.first_layer_over_raft)},
        {"speed.dynamic_overhangs", flag(s.dynamic_overhangs)},
        {"speed.overhang_0", share(s.overhang_speeds[0])},
        {"speed.overhang_1", share(s.overhang_speeds[1])},
        {"speed.overhang_2", share(s.overhang_speeds[2])},
        {"speed.overhang_3", share(s.overhang_speeds[3])},
        {"speed.flow_slope_up", num(s.flow_slope_up)},
        {"speed.flow_slope_down", num(s.flow_slope_down)},
        {"acceleration.default", num(s.accel_default)},
        {"acceleration.walls", num(s.accel_walls)},
        {"acceleration.outer_wall", num(s.accel_outer_wall)},
        {"acceleration.infill", num(s.accel_infill)},
        {"acceleration.solid_infill", num(s.accel_solid_infill)},
        {"acceleration.top_solid_infill", num(s.accel_top_solid_infill)},
        {"acceleration.bridges", num(s.accel_bridges)},
        {"acceleration.first_layer", num(s.accel_first_layer)},
        {"acceleration.first_layer_over_raft", num(s.accel_first_layer_over_raft)},
        {"acceleration.travel", num(s.accel_travel)},
        {"acceleration.short_travel", num(s.accel_short_travel)},
        {"retraction.length", num(s.retract_length)},
        {"retraction.speed", speed(s.retract_speed)},
        {"retraction.lift", num(s.retract_lift)},
        {"retraction.min_travel", num(s.retract_min_travel)},
        {"retraction.unretract_speed", num(s.unretract_speed)},
        {"retraction.restart_extra", num(s.restart_extra)},
        {"retraction.on_layer_change", flag(s.retract_layer_change)},
        {"retraction.wipe", flag(s.wipe)},
        {"retraction.before_wipe", num(s.before_wipe)},
        {"retraction.lift_above", num(s.lift_above)},
        {"retraction.lift_below", num(s.lift_below)},
        {"retraction.firmware", flag(s.firmware_retraction)},
        {"travel.ramping_lift", flag(s.ramping_lift)},
        {"travel.slope", num(s.travel_slope)},
        {"travel.max_lift", num(s.max_lift)},
        {"travel.lift_before_obstacle", flag(s.lift_before_obstacle)},
        {"travel.avoid_crossing_walls", flag(s.avoid_crossing_walls)},
        {"travel.max_detour", num(s.max_detour.value) == "0" ? "0" : share(s.max_detour)},
        {"walls.clockwise", flag(s.walls_clockwise)},
        {"walls.staggered_inner_seams", flag(s.staggered_inner_seams)},
        {"walls.top_one_wall", top_one_wall_name(s.top_one_wall)},
        {"walls.first_layer_one_wall", flag(s.first_layer_one_wall)},
        {"walls.extra_if_needed", flag(s.extra_if_needed)},
        {"walls.avoid_curled_overhangs", flag(s.avoid_curled_overhangs)},
        {"walls.extra_on_overhangs", flag(s.extra_on_overhangs)},
        {"extrusion.retraction", json(s.filament_retraction).dump()},
        {"adhesion.skirt_loops", std::to_string(s.skirt_loops)},
        {"adhesion.skirt_distance", num(s.skirt_distance)},
        {"adhesion.skirt_layers", std::to_string(s.skirt_layers)},
        {"adhesion.brim_width", num(s.brim_width)},
        {"adhesion.skirt_min_filament", num(s.skirt_min_filament)},
        {"adhesion.draft_shield", json(draft_shield_name(s.draft_shield)).dump()},
        {"adhesion.brim_type", json(brim_type_name(s.brim_type)).dump()},
        {"adhesion.brim_gap", num(s.brim_gap)},
        {"adhesion.raft_layers", std::to_string(s.raft_layers)},
        {"adhesion.raft_gap", num(s.raft_gap)},
        {"adhesion.raft_expansion", num(s.raft_expansion)},
        {"adhesion.raft_first_layer_density", num(s.raft_first_layer_density)},
        {"adhesion.raft_first_layer_expansion", num(s.raft_first_layer_expansion)},
        {"support.enabled", flag(s.support)},
        {"support.automatic", flag(s.support_automatic)},
        {"support.threshold_angle", num(s.support_angle)},
        {"support.z_gap", num(s.support_z_gap)},
        {"support.xy_gap", s.support_xy_gap.percent ? num(s.support_xy_gap.value) + "%" : num(s.support_xy_gap.value)},
        {"support.width", s.support_width.percent ? num(s.support_width.value) + "%" : num(s.support_width.value)},
        {"support.line_angle", num(s.support_line_angle)},
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
        {"extrusion.infill_max_speed", num(s.infill_max_speed)},
        {"extrusion.infill_max_crossing_speed", num(s.infill_max_crossing_speed)},
        {"extrusion.bridge_flow_ratio", num(s.bridge_flow_ratio)},
        {"extrusion.chamber_temperature", num(s.chamber_temperature)},
        {"extrusion.abrasive", flag(s.abrasive)},
        {"extrusion.needs_enclosure", flag(s.needs_enclosure)},
        {"gcode.resolution", num(s.gcode_resolution)},
        {"gcode.color_change", text(s.gcode_color_change)},
        {"gcode.pause", text(s.gcode_pause)},
        {"gcode.before_layer", text(s.gcode_before_layer)},
        {"gcode.after_layer", text(s.gcode_after_layer)},
        {"gcode.between_objects", text(s.gcode_between_objects)},
        {"gcode.volumetric_e", flag(s.volumetric_e)},
        {"gcode.verbose_comments", flag(s.verbose_comments)},
        {"gcode.arcs", flag(s.gcode_arcs)},
        {"machine.z_offset", num(s.z_offset)},
        {"extrusion.chamber_min_temperature", num(s.chamber_min_temperature)},
        {"extrusion.density", num(s.filament_density)},
        {"extrusion.cost", num(s.filament_cost)},
        {"extrusion.spool_weight", num(s.spool_weight)},
        {"extrusion.shrinkage_xy", num(s.shrinkage_xy)},
        {"extrusion.shrinkage_z", num(s.shrinkage_z)},
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
    const char* flavor = flavor_name(s.flavor);
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
    // A speed that may be a share: mm/s, or a text like "50%".
    const auto share = [](const Speed& v) {
        return v.value <= 0.0 ? ordered_json(nullptr)
               : v.percent    ? ordered_json(json(v.value).dump() + "%")
                              : ordered_json(v.value);
    };
    const ordered_json outer_wall = share(s.outer_wall_speed);
    // The filament's own retraction and travel values, by group (retraction_travel.md, rule 10).
    const auto filament_values = [&](const std::string& prefix) {
        ordered_json group = ordered_json::object();
        for (const auto& [key, v] : s.filament_retraction)
            if (key.starts_with(prefix)) {
                const std::string name = key.substr(prefix.size());
                const bool flag = name == "on_layer_change" || name == "wipe" || name == "ramping_lift";
                group[name] = flag ? ordered_json(v != 0.0) : ordered_json(v);
            }
        return group;
    };
    ordered_json acceleration = ordered_json::object();
    for (const char* n : kAccelerationNames)
        acceleration[n] = *acceleration_of(s, n);
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
          {"scarf_inner", s.scarf_inner},
          {"detect_overhangs", s.detect_overhangs},
          {"generator", s.wall_generator == WallGenerator::Fixed ? "fixed" : "variable"},
          {"gap_fill", s.wall_gap_fill},
          {"clockwise", s.walls_clockwise},
          {"staggered_inner_seams", s.staggered_inner_seams},
          {"top_one_wall", top_one_wall_name(s.top_one_wall)},
          {"first_layer_one_wall", s.first_layer_one_wall},
          {"extra_if_needed", s.extra_if_needed},
          {"avoid_curled_overhangs", s.avoid_curled_overhangs},
          {"extra_on_overhangs", s.extra_on_overhangs},
          {"min_line_width", s.wall_min_line_width.percent
                                 ? ordered_json(json(s.wall_min_line_width.value).dump() + "%")
                                 : ordered_json(s.wall_min_line_width.value)},
          {"min_detail", s.wall_min_detail.percent ? ordered_json(json(s.wall_min_detail.value).dump() + "%")
                                                   : ordered_json(s.wall_min_detail.value)}}},
        {"vase", {{"enabled", s.vase}}},
        {"placement",
         {{"arrange", s.placement_arrange},
          {"center", s.placement_center ? json(*s.placement_center) : json(nullptr)},
          {"spacing", s.placement_spacing}}},
        {"sequential",
         {{"enabled", s.sequential}, {"clearance_radius", s.clearance_radius}, {"clearance_height", s.clearance_height}}},
        {"extruders",
         {{"walls", s.extruder_walls},
          {"infill", s.extruder_infill},
          {"solid_infill", s.extruder_solid_infill},
          {"support", s.extruder_support},
          {"support_interface", s.extruder_support_interface},
          {"wipe_tower", s.extruder_wipe_tower},
          {"bed_temperature_from", s.bed_temperature_from},
          {"ooze_prevention", s.ooze_prevention},
          {"standby_delta", s.standby_delta}}},
        {"mmu",
         {{"cooling_tube_length", s.mmu_cooling_tube_length},
          {"cooling_tube_position", s.mmu_cooling_tube_position},
          {"parking_position", s.mmu_parking_position},
          {"extra_loading_move", s.mmu_extra_loading_move},
          {"high_current_on_swap", s.mmu_high_current_on_swap}}},
        {"wipe_tower",
         {{"enabled", s.wipe_tower},
          {"x", s.wipe_tower_x},
          {"y", s.wipe_tower_y},
          {"rotation", s.wipe_tower_rotation},
          {"width", s.wipe_tower_width},
          {"brim_width", s.wipe_tower_brim},
          {"bridging", s.wipe_tower_bridging},
          {"cone_angle", s.wipe_tower_cone_angle},
          {"extra_spacing", s.wipe_tower_extra_spacing},
          {"extra_flow", s.wipe_tower_extra_flow},
          {"no_sparse_layers", s.wipe_tower_no_sparse_layers},
          {"purge_volumes", s.purge_volumes}}},
        {"surfaces",
         {{"top_layers", s.top_layers},
          {"bottom_layers", s.bottom_layers},
          {"top_pattern", s.monotonic_top ? "monotonic" : "rectilinear"},
          {"top_min_thickness", s.top_min_thickness},
          {"bottom_min_thickness", s.bottom_min_thickness},
          {"interface_shells", s.interface_shells}}},
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
          {"max_accel_x", s.max_accel_x},
          {"max_accel_y", s.max_accel_y},
          {"max_accel_z", s.max_accel_z},
          {"max_accel_e", s.max_accel_e},
          {"retract_acceleration", s.retract_acceleration},
          {"jerk_x", s.jerk_x},
          {"jerk_y", s.jerk_y},
          {"jerk_z", s.jerk_z},
          {"jerk_e", s.jerk_e},
          {"junction_deviation", s.junction_deviation},
          {"min_extrude_feed", s.min_extrude_feed},
          {"min_travel_feed", s.min_travel_feed},
          {"limits_usage", limits_usage_name(s.limits_usage)},
          {"silent_mode", s.silent_mode},
          {"silent", s.silent_limits},
          {"bed", bed},
          {"max_height", s.max_height},
          {"min_layer_height", s.min_layer_height},
          {"max_layer_height", s.max_layer_height},
          {"z_offset", s.z_offset},
          {"extruders", s.extruders},
          {"extruder_offset", s.extruder_offset},
          {"single_extruder_mmu", s.single_extruder_mmu},
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
          {"max", speed(s.max_speed)},
          {"bridges", speed(s.bridge_speed)},
          {"gap_fill", speed(s.gap_fill_speed)},
          {"over_bridges", share(s.over_bridges_speed)},
          {"first_layer_infill", share(s.first_layer_infill_speed)},
          {"travel_z", speed(s.travel_z_speed)},
          {"first_layer_over_raft", share(s.first_layer_over_raft)},
          {"dynamic_overhangs", s.dynamic_overhangs},
          {"overhang_0", share(s.overhang_speeds[0])},
          {"overhang_1", share(s.overhang_speeds[1])},
          {"overhang_2", share(s.overhang_speeds[2])},
          {"overhang_3", share(s.overhang_speeds[3])},
          {"flow_slope_up", s.flow_slope_up},
          {"flow_slope_down", s.flow_slope_down}}},
        {"acceleration", acceleration},
        {"retraction",
         {{"length", s.retract_length},
          {"speed", speed(s.retract_speed)},
          {"lift", s.retract_lift},
          {"min_travel", s.retract_min_travel},
          {"unretract_speed", s.unretract_speed},
          {"restart_extra", s.restart_extra},
          {"on_layer_change", s.retract_layer_change},
          {"wipe", s.wipe},
          {"before_wipe", s.before_wipe},
          {"lift_above", s.lift_above},
          {"lift_below", s.lift_below},
          {"firmware", s.firmware_retraction},
          {"toolchange_length", s.toolchange_length},
          {"toolchange_restart_extra", s.toolchange_restart_extra}}},
        {"travel",
         {{"ramping_lift", s.ramping_lift},
          {"slope", s.travel_slope},
          {"max_lift", s.max_lift},
          {"lift_before_obstacle", s.lift_before_obstacle},
          {"avoid_crossing_walls", s.avoid_crossing_walls},
          {"max_detour", s.max_detour.value <= 0.0 ? ordered_json(0.0) : share(s.max_detour)}}},
        {"adhesion",
         {{"skirt_loops", s.skirt_loops},
          {"skirt_distance", s.skirt_distance},
          {"skirt_layers", s.skirt_layers},
          {"brim_width", s.brim_width},
          {"skirt_min_filament", s.skirt_min_filament},
          {"draft_shield", draft_shield_name(s.draft_shield)},
          {"brim_type", brim_type_name(s.brim_type)},
          {"brim_gap", s.brim_gap},
          {"raft_layers", s.raft_layers},
          {"raft_gap", s.raft_gap},
          {"raft_expansion", s.raft_expansion},
          {"raft_first_layer_density", s.raft_first_layer_density},
          {"raft_first_layer_expansion", s.raft_first_layer_expansion}}},
        {"support",
         {{"enabled", s.support},
          {"automatic", s.support_automatic},
          {"threshold_angle", s.support_angle},
          {"z_gap", s.support_z_gap},
          {"xy_gap", s.support_xy_gap.percent ? ordered_json(json(s.support_xy_gap.value).dump() + "%")
                                              : ordered_json(s.support_xy_gap.value)},
          {"width", s.support_width.percent ? ordered_json(json(s.support_width.value).dump() + "%")
                                            : ordered_json(s.support_width.value)},
          {"line_angle", s.support_line_angle},
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
        {"arcs",
         {{"enabled", s.arcs},
          {"overlap", s.arcs_overlap},
          {"speed", s.arcs_speed},
          {"max_distance", s.arcs_max_distance}}},
        {"ironing",
         {{"enabled", s.ironing},
          {"surfaces", s.ironing_surfaces == IroningSurfaces::Topmost ? "topmost"
                       : s.ironing_surfaces == IroningSurfaces::Solid ? "solid"
                                                                      : "top"},
          {"flow", s.ironing_flow},
          {"spacing", s.ironing_spacing},
          {"speed", s.ironing_speed}}},
        {"review",
         {{"nozzle_force", s.review_nozzle_force},
          {"bed_adhesion", s.review_bed_adhesion},
          {"max_deflection", s.review_max_deflection}}},
        {"compensation",
         {{"scale_x", s.scale_x}, {"scale_y", s.scale_y}, {"scale_z", s.scale_z}, {"hole_offset", s.hole_offset}}},
        {"extrusion",
         {{"flow", s.flow},
          {"pressure_advance", s.pressure_advance},
          {"max_flow", s.filament_max_flow},
          {"infill_max_speed", s.infill_max_speed},
          {"infill_max_crossing_speed", s.infill_max_crossing_speed},
          {"bridge_flow_ratio", s.bridge_flow_ratio},
          {"chamber_min_temperature", s.chamber_min_temperature},
          {"density", s.filament_density},
          {"cost", s.filament_cost},
          {"spool_weight", s.spool_weight},
          {"shrinkage_xy", s.shrinkage_xy},
          {"shrinkage_z", s.shrinkage_z},
          {"start_gcode", s.filament_start_gcode},
          {"end_gcode", s.filament_end_gcode},
          {"purge_multiplier", s.purge_multiplier},
          {"min_purge_on_tower", s.min_purge_on_tower},
          {"loading_speed", s.loading_speed},
          {"loading_speed_start", s.loading_speed_start},
          {"unloading_speed", s.unloading_speed},
          {"unloading_speed_start", s.unloading_speed_start},
          {"cooling_moves", s.cooling_moves},
          {"cooling_initial_speed", s.cooling_initial_speed},
          {"cooling_final_speed", s.cooling_final_speed},
          {"ramming", s.ramming},
          {"toolchange_delay", s.toolchange_delay},
          {"load_time", s.load_time},
          {"unload_time", s.unload_time},
          {"retraction", filament_values("retraction.")},
          {"travel", filament_values("travel.")},
          {"chamber_temperature", s.chamber_temperature},
          {"abrasive", s.abrasive},
          {"needs_enclosure", s.needs_enclosure}}},
        {"output", {{"filename_format", s.filename_format}, {"post_process", s.post_process}}},
        {"gcode",
         {{"resolution", s.gcode_resolution},
          {"color_change", s.gcode_color_change},
          {"pause", s.gcode_pause},
          {"before_layer", s.gcode_before_layer},
          {"after_layer", s.gcode_after_layer},
          {"between_objects", s.gcode_between_objects},
          {"toolchange", s.gcode_toolchange},
          {"volumetric_e", s.volumetric_e},
          {"verbose_comments", s.verbose_comments},
          {"arcs", s.gcode_arcs},
          {"progress", s.gcode_progress},
          {"object_labels", object_labels_name(s.object_labels)},
          {"auto_temperatures", s.auto_temperatures},
          {"binary", s.gcode_binary}}},
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
    // Klipper takes its limits from its own configuration (gcode_firmware.md, rule 2, FW4).
    if (s.flavor == GcodeFlavor::Klipper && s.limits_usage == LimitsUsage::Emit)
        throw SettingsError("machine.limits_usage: Klipper takes the machine's limits from its own configuration; they "
                            "cannot be written in the G-code (emit)");
    // Vase mode lays one wall in a spiral: nothing can stand under it
    // (vase_mode.md, rule 2).
    if (s.vase && s.support)
        throw SettingsError("vase.enabled: vase mode is not compatible with supports (support.enabled)");
    // The firmware's retraction moves no E: no wipe can retract while it
    // moves (retraction_travel.md, rule 7, R6).
    if (s.firmware_retraction && with_filament_retraction(s).wipe)
        throw SettingsError("retraction.wipe: the wipe retracts while the nozzle moves, which the firmware's "
                            "retraction (retraction.firmware) cannot do");
    // The wipe tower takes relative E (multi_extruder.md, rule 9, ME6).
    if (s.wipe_tower && s.extruders > 1 && !relative_e(s))
        throw SettingsError("wipe_tower.enabled: the wipe tower needs relative E (machine.relative_extrusion, on a "
                            "flavor that has M83)");
    // An extrusion's extruder must be one the machine has (multi_extruder.md, rule 2).
    for (const auto& [key, e] : {std::pair{"extruders.walls", s.extruder_walls}, {"extruders.infill", s.extruder_infill},
                                 {"extruders.solid_infill", s.extruder_solid_infill}, {"extruders.support", s.extruder_support},
                                 {"extruders.support_interface", s.extruder_support_interface},
                                 {"extruders.wipe_tower", s.extruder_wipe_tower},
                                 {"extruders.bed_temperature_from", s.bed_temperature_from}})
        if (e > s.extruders) {
            std::snprintf(text, sizeof text, "%s: extruder %d, but the machine has %d (machine.extruders)", key, e,
                          s.extruders);
            throw SettingsError(text);
        }
    if (s.wall_width < 0.25 * nozzle || s.wall_width > 4.0 * nozzle) {
        std::snprintf(text, sizeof text, "walls.width: %.3g mm cannot come out of a %.3g mm nozzle (%.3g to %.3g mm)",
                      s.wall_width, nozzle, 0.25 * nozzle, 4.0 * nozzle);
        throw SettingsError(text);
    }
}

} // namespace tisma
