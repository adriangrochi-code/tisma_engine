#include "ui/assistant.hpp"

#include "ui/settings_view.hpp"
#include "ui/text.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace tisma::ui {
namespace {

using nlohmann::ordered_json;

// A number as the panel writes it: up to four decimals, no trailing zeros,
// the decimal comma in Spanish.
std::string number_text(double v) {
    char buffer[64];
    std::snprintf(buffer, sizeof buffer, "%.4f", v);
    std::string s = buffer;
    while (s.back() == '0')
        s.pop_back();
    if (s.back() == '.')
        s.pop_back();
    if (s == "-0")
        s = "0";
    if (language() == Language::Spanish)
        std::replace(s.begin(), s.end(), '.', ',');
    return s;
}

std::string value_text(const ordered_json& v) {
    if (v.is_number())
        return number_text(v.get<double>());
    if (v.is_string())
        return v.get<std::string>();
    if (v.is_boolean())
        return tr(v.get<bool>() ? T::Yes : T::No);
    if (v.is_array()) {
        std::string out;
        for (const ordered_json& item : v)
            out += (out.empty() ? "" : ", ") + value_text(item);
        return out;
    }
    return v.is_null() ? "—" : v.dump();
}

// The value of a key in the settings JSON; null when there is no such key.
// A machine variable is its text (or texts).
ordered_json* find_value(ordered_json& root, std::string_view key) {
    const std::size_t dot = key.find('.');
    if (dot == std::string_view::npos)
        return nullptr;
    const std::string group(key.substr(0, dot)), name(key.substr(dot + 1));
    if (!root.contains(group) || !root.at(group).is_object())
        return nullptr;
    ordered_json& g = root.at(group);
    if (group == "machine" && name.starts_with("variables.")) {
        const std::string variable = name.substr(10);
        ordered_json& variables = g.at("variables");
        if (!variables.contains(variable))
            return nullptr;
        return &variables.at(variable);
    }
    return g.contains(name) ? &g.at(name) : nullptr;
}

// A machine variable's text as a number ("215" → 215).
std::optional<double> number_of(const ordered_json& v) {
    if (v.is_number())
        return v.get<double>();
    if (v.is_string()) {
        const std::string s = v.get<std::string>();
        char* end = nullptr;
        const double d = std::strtod(s.c_str(), &end);
        if (!s.empty() && end == s.c_str() + s.size())
            return d;
    }
    return std::nullopt;
}

// One value changed: a number, or a variable's text holding one.
ordered_json changed(const ordered_json& now, const SettingAction& action, const ordered_json& value,
                     std::optional<double> of, const SettingRange& range, bool& clamped) {
    if (action.op == SettingOp::Set && !value.is_number()) {
        if (now.is_string() != value.is_string() || now.is_boolean() != value.is_boolean())
            throw AssistantError(action.key + ": the value is not of the setting's kind");
        return value;
    }
    if (!value.is_number())
        throw AssistantError(action.key + ": the action needs a number");
    const std::optional<double> current = number_of(now);
    // A number goes only where one is (a null is a number not set yet).
    if (!current && !now.is_null())
        throw AssistantError(action.key + ": the setting is not a number");
    if (!current && action.op != SettingOp::Set && action.op != SettingOp::SetRelative)
        throw AssistantError(action.key + ": the setting has no value yet");
    const double v = value.get<double>();
    double result = action.op == SettingOp::Add        ? *current + v
                    : action.op == SettingOp::Multiply ? *current * v
                    : action.op == SettingOp::Set      ? v
                                                       : v * of.value_or(0.0);
    const double wanted = result;
    if (range.high && result > *range.high)
        result = *range.high;
    if (range.low && result < *range.low)
        result = *range.low; // with an open low end the engine refuses it, and says why
    if (now.is_number_integer() || (now.is_string() && current && *current == std::round(*current)))
        result = std::round(result);
    clamped |= std::abs(result - wanted) > 1e-9;
    if (now.is_string()) {
        char buffer[64];
        std::snprintf(buffer, sizeof buffer, "%.4f", result);
        std::string s = buffer;
        while (s.back() == '0')
            s.pop_back();
        if (s.back() == '.')
            s.pop_back();
        return s;
    }
    if (now.is_number_integer())
        return static_cast<long long>(result);
    return result;
}

// Spanish numbers in a range's text: "−360", "0,5", "100 000".
std::vector<double> numbers_in(std::string_view text) {
    std::vector<double> out;
    std::string s;
    // The minus sign (U+2212) as '-'.
    for (std::size_t i = 0; i < text.size(); ++i)
        if (text.substr(i, 3) == "\xE2\x88\x92")
            s += '-', i += 2;
        else
            s += text[i];
    for (std::size_t i = 0; i < s.size();) {
        const bool minus = s[i] == '-' && i + 1 < s.size() && std::isdigit(static_cast<unsigned char>(s[i + 1]));
        if (!minus && !std::isdigit(static_cast<unsigned char>(s[i]))) {
            ++i;
            continue;
        }
        std::string number = minus ? "-" : "";
        i += minus;
        for (; i < s.size(); ++i) {
            const char c = s[i];
            if (std::isdigit(static_cast<unsigned char>(c)))
                number += c;
            else if (c == ',' && i + 1 < s.size() && std::isdigit(static_cast<unsigned char>(s[i + 1])))
                number += '.';
            else if (c == ' ' && i + 3 < s.size() && std::isdigit(static_cast<unsigned char>(s[i + 1])) &&
                     std::isdigit(static_cast<unsigned char>(s[i + 2])) &&
                     std::isdigit(static_cast<unsigned char>(s[i + 3])) &&
                     (i + 4 == s.size() || !std::isdigit(static_cast<unsigned char>(s[i + 4]))))
                continue; // a thousands' space: "100 000"
            else
                break;
        }
        out.push_back(std::strtod(number.c_str(), nullptr));
    }
    return out;
}

} // namespace

std::optional<SettingOp> setting_op_from(std::string_view name) {
    if (name == "add")
        return SettingOp::Add;
    if (name == "multiply")
        return SettingOp::Multiply;
    if (name == "set")
        return SettingOp::Set;
    if (name == "set_relative")
        return SettingOp::SetRelative;
    return std::nullopt;
}

SettingRange parse_range(std::string_view text) {
    SettingRange range;
    const std::vector<double> n = numbers_in(text);
    if (n.empty())
        return range;
    const std::string folded = fold(text);
    const bool open = folded.starts_with("mas de") || folded.starts_with(">");
    if (n.size() == 1) {
        // "≥ 0", "> 0", "más de 0": a low end only.
        range.low = n.front();
        range.low_open = open;
        return range;
    }
    range.low = *std::min_element(n.begin(), n.end());
    range.high = *std::max_element(n.begin(), n.end());
    range.low_open = open;
    return range;
}

bool known_setting(const Settings& settings, std::string_view key) {
    ordered_json root = ordered_json::parse(settings_json(settings));
    return find_value(root, key) != nullptr;
}

std::optional<SettingChange> plan_setting(const Settings& settings, const SettingAction& asked,
                                          const SettingRange& range) {
    // The specs' material.* keys are the engine's under other names.
    SettingAction action = asked;
    const std::optional<std::string> key = engine_setting_key(asked.key);
    if (!key)
        return std::nullopt;
    action.key = *key;
    if (!asked.of.empty()) {
        const std::optional<std::string> of = engine_setting_key(asked.of);
        if (!of)
            throw AssistantError(asked.key + ": no setting " + asked.of + " to take the value from");
        action.of = *of;
    }
    ordered_json root = ordered_json::parse(settings_json(settings));
    const ordered_json* now = find_value(root, action.key);
    const std::size_t dot = action.key.find('.');
    const std::string group = dot == std::string::npos ? std::string() : action.key.substr(0, dot);
    const std::string name = dot == std::string::npos ? std::string() : action.key.substr(dot + 1);
    const bool variable = group == "machine" && name.starts_with("variables.");
    // A machine variable the profile does not have yet is set as a new text
    // (the temperature guides' [temperature]).
    const ordered_json unset;
    if (!now && variable && (action.op == SettingOp::Set || action.op == SettingOp::SetRelative))
        now = &unset;
    if (!now)
        return std::nullopt;
    const std::optional<ProfileKind> profile =
        setting_kind(group, variable ? "variables" : name, variable ? name.substr(10) : std::string());
    if (!profile)
        return std::nullopt; // not a profile's setting (the height changes)
    ordered_json value;
    try {
        value = ordered_json::parse(action.value);
    } catch (const ordered_json::exception&) {
        throw AssistantError(action.key + ": the action's value is not JSON");
    }
    std::optional<double> of;
    if (action.op == SettingOp::SetRelative) {
        const ordered_json* base = find_value(root, action.of);
        if (!base)
            throw AssistantError(action.key + ": no setting " + action.of + " to take the value from");
        of = number_of(base->is_array() && !base->empty() ? base->front() : *base);
        if (!of)
            throw AssistantError(action.key + ": " + action.of + " is not a number");
    }
    SettingChange change;
    change.key = action.key;
    change.profile = *profile;
    change.before = value_text(*now);
    ordered_json after;
    if (now->is_array()) {
        // A value per extruder: each one.
        after = ordered_json::array();
        for (const ordered_json& item : *now)
            after.push_back(changed(item, action, value, of, range, change.clamped));
    } else {
        after = changed(*now, action, value, of, range, change.clamped);
    }
    if (variable && after.is_number()) {
        char buffer[64];
        std::snprintf(buffer, sizeof buffer, "%.4f", after.get<double>());
        std::string text = buffer;
        while (text.back() == '0')
            text.pop_back();
        if (text.back() == '.')
            text.pop_back();
        after = text;
    }
    change.after = value_text(after);
    change.json = after.dump();
    return change;
}

Settings apply_setting(const Settings& settings, const SettingChange& change) {
    ordered_json root = ordered_json::parse(settings_json(settings));
    ordered_json* now = find_value(root, change.key);
    if (!now && change.key.starts_with("machine.variables.") && root.contains("machine") &&
        root.at("machine").contains("variables"))
        now = &root.at("machine").at("variables")[change.key.substr(18)];
    if (!now)
        throw SettingsError("settings: " + change.key + ": no such setting");
    *now = ordered_json::parse(change.json);
    Settings out = parse_settings(root.dump());
    out.ignored_groups = settings.ignored_groups;
    return out;
}

const MaterialInfo* find_material(const std::vector<MaterialInfo>& materials, std::string_view type,
                                  std::string_view profile_name) {
    const std::string t = fold(type);
    if (!t.empty())
        for (const MaterialInfo& m : materials)
            if (fold(m.id) == t || fold(m.name) == t)
                return &m;
    // By the profile's name: the material's name as a word of it.
    const std::string name = fold(profile_name);
    const auto word_at = [&](const std::string& w) {
        for (std::size_t at = name.find(w); at != std::string::npos; at = name.find(w, at + 1)) {
            const bool start = at == 0 || !std::isalnum(static_cast<unsigned char>(name[at - 1]));
            const std::size_t end = at + w.size();
            const bool stop = end == name.size() || !std::isalnum(static_cast<unsigned char>(name[end]));
            if (start && stop)
                return true;
        }
        return false;
    };
    const MaterialInfo* best = nullptr;
    for (const MaterialInfo& m : materials) {
        const std::string w = fold(m.name);
        if (!w.empty() && word_at(w) && (!best || w.size() > best->name.size()))
            best = &m;
    }
    return best;
}

std::vector<MaterialWarning> material_warnings(const MaterialInfo& m, const Settings& printer) {
    std::vector<MaterialWarning> out;
    if (m.enclosure_needed && !printer.enclosed)
        out.push_back(MaterialWarning::NeedsEnclosure);
    if (m.abrasive && !printer.hardened_nozzle)
        out.push_back(MaterialWarning::Abrasive);
    if (m.bed_min > 0 && printer.max_bed_temp > 0 && m.bed_min > printer.max_bed_temp)
        out.push_back(MaterialWarning::BedTooCold);
    if (m.nozzle_min > 0 && printer.max_nozzle_temp > 0 && m.nozzle_min > printer.max_nozzle_temp)
        out.push_back(MaterialWarning::NozzleTooCold);
    if (m.dry_before_printing)
        out.push_back(MaterialWarning::DryFirst);
    return out;
}

std::string material_warning_text(MaterialWarning warning, const MaterialInfo& m) {
    switch (warning) {
    case MaterialWarning::NeedsEnclosure:
        return tr(T::WarnEnclosure);
    case MaterialWarning::Abrasive:
        return tr(T::WarnAbrasive);
    case MaterialWarning::BedTooCold:
        return tr(T::WarnBedTemperature);
    case MaterialWarning::NozzleTooCold:
        return tr(T::WarnNozzleTemperature);
    case MaterialWarning::DryFirst: {
        std::string text = tr(T::WarnDry);
        std::string how;
        if (m.dry_min > 0 && m.dry_max > m.dry_min)
            how = number_text(m.dry_min) + tr(T::RangeTo) + number_text(m.dry_max) + " °C";
        else if (m.dry_min > 0 || m.dry_max > 0)
            how = number_text(std::max(m.dry_min, m.dry_max)) + " °C";
        if (m.dry_hours > 0)
            how += (how.empty() ? "" : ", ") + number_text(m.dry_hours) + " h";
        return how.empty() ? text : text + ": " + how;
    }
    }
    return {};
}

std::string_view review_problem(std::string_view kind) {
    // The review's warnings of rule 9 as the engine names them: travel in
    // the air without a retraction leaves strings; short layers have no time
    // to cool; a part that can come off the bed has too little first layer;
    // a part that starts in the air is an overhang without support.
    static constexpr std::pair<std::string_view, std::string_view> map[] = {
        {"travel_no_retraction", "stringing"},  {"layer_time", "overheating_small"},
        {"stability_peel", "first_layer_adhesion"}, {"stability_air", "support_surface"},
        {"corner_lift", "warping"},
    };
    for (const auto& [k, problem] : map)
        if (k == kind)
            return problem;
    return {};
}

namespace {
// Rule 20's table, the engine's test first; a test with two guides opens
// the first («Cómo se hace» of the temperature tower).
constexpr std::pair<std::string_view, std::string_view> kTestGuides[] = {
    {"first_layer", "first_layer"},       {"temperature", "temperature_tower"}, {"temperature", "stringing_temperature"},
    {"max_flow", "max_flow"},             {"pressure_advance", "pressure_advance"}, {"flow", "flow"},
    {"retraction", "retraction"},         {"corners", "cornering"},             {"vibrations", "input_shaping"},
    {"vfa", "vfa"},                       {"dimensions", "dimensional"},
};
} // namespace

std::string_view guide_for_test(std::string_view test) {
    for (const auto& [t, guide] : kTestGuides)
        if (t == test)
            return guide;
    return {};
}

std::string_view test_for_guide(std::string_view guide) {
    for (const auto& [test, g] : kTestGuides)
        if (g == guide)
            return test;
    return {};
}

std::vector<std::string> guides_missing(const std::vector<std::string>& before, const std::set<std::string>& done) {
    std::vector<std::string> out;
    for (const std::string& id : before)
        if (!done.contains(id))
            out.push_back(id);
    return out;
}

std::optional<GuideFormula> guide_formula(std::string_view id) {
    // The nozzle temperature is the start G-code's [temperature] variable,
    // the filament's (docs/CONFIG.md, machine.variables).
    static constexpr std::pair<std::string_view, GuideFormula> table[] = {
        {"temperature_tower", {"machine.variables.temperature", GuideMeasure::Value}},
        {"stringing_temperature", {"machine.variables.temperature", GuideMeasure::Value}},
        {"max_flow", {"extrusion.max_flow", GuideMeasure::Height}},
        {"pressure_advance", {"extrusion.pressure_advance", GuideMeasure::Height}},
        {"retraction", {"retraction.length", GuideMeasure::Height}},
        {"cornering", {"machine.corner_speed", GuideMeasure::Height}},
        {"flow", {"extrusion.flow", GuideMeasure::Value}},
        {"cooling", {"cooling.min_layer_time", GuideMeasure::Value}},
        {"overhang_angle", {"support.threshold_angle", GuideMeasure::Value}},
        {"supports", {"support.z_gap", GuideMeasure::Layers}},
        {"vfa", {"speed.outer_wall", GuideMeasure::Value}},
    };
    for (const auto& [guide, formula] : table)
        if (guide == id)
            return formula;
    return std::nullopt;
}

double band_value(double start, double step, double band, double height) {
    return start + step * std::floor(height / (band > 0.0 ? band : 1.0) + 1e-9);
}

std::optional<GuideOutcome> guide_result(std::string_view id, const GuideInput& in, double layer_height) {
    const std::optional<GuideFormula> f = guide_formula(id);
    if (!f)
        return std::nullopt;
    switch (f->measure) {
    case GuideMeasure::Height: {
        const double value = band_value(in.start, in.step, in.band, in.measured);
        // The low end of the guide's 10 to 20 % margin.
        return GuideOutcome{value, id == "max_flow" ? value * 0.9 : value};
    }
    case GuideMeasure::Layers:
        return GuideOutcome{in.measured, in.measured * layer_height};
    case GuideMeasure::Value:
        break;
    }
    return GuideOutcome{in.measured, in.measured};
}

std::optional<std::string> firmware_command(std::string_view id, GcodeFlavor flavor, double value) {
    if (id != "input_shaping" || !(value > 0.0))
        return std::nullopt;
    char text[96];
    switch (flavor) {
    case GcodeFlavor::Klipper:
        std::snprintf(text, sizeof text, "SET_INPUT_SHAPER SHAPER_FREQ_X=%.1f SHAPER_FREQ_Y=%.1f", value, value);
        return std::string(text);
    case GcodeFlavor::Marlin:
    case GcodeFlavor::RepRapFirmware:
        std::snprintf(text, sizeof text, "M593 F%.1f", value);
        return std::string(text);
    default:
        return std::nullopt;
    }
}

std::string_view next_material_guide(const std::set<std::string>& done) {
    static constexpr std::string_view order[] = {"temperature_tower", "max_flow", "pressure_advance", "flow",
                                                 "retraction"};
    for (const std::string_view id : order)
        if (!done.contains(std::string(id)))
            return id;
    return {};
}

} // namespace tisma::ui
