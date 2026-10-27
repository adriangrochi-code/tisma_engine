#pragma once

#include "config/profiles.hpp"
#include "config/settings.hpp"

#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

// The offline assistant (docs/spec/features/assistant.md): what applying a
// `setting` action does to the settings (rule 7), the order of a problem's
// causes (rule 6) and the warnings of a material (rules 13 and 14). Own
// design on that spec; the data files are read elsewhere.
namespace tisma::ui {

class AssistantError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

// A `setting` action: `add` adds `value`, `multiply` multiplies by it, `set`
// sets it, `set_relative` sets `value` times the setting `of`. `value` is JSON
// text: a number, or for `set` also a text or a boolean ("\"rear\"", "true").
enum class SettingOp { Add, Multiply, Set, SetRelative };
std::optional<SettingOp> setting_op_from(std::string_view name);
struct SettingAction {
    std::string key; // "group.key", or "machine.variables.name"
    SettingOp op = SettingOp::Set;
    std::string value = "0";
    std::string of; // set_relative's setting
};

// What an action would do, to show before it is confirmed: the setting, the
// profile it belongs to, its value now and after (as the panel writes them),
// and whether the new value was cut to the setting's range ("Se usó el
// límite"). `json` is the new value as settings JSON.
struct SettingChange {
    std::string key;
    ProfileKind profile = ProfileKind::Print;
    std::string before, after;
    bool clamped = false;
    std::string json;
};

// The range a setting takes, read from the «Rango» column of docs/CONFIG.md
// ("0 a 100", "≥ 0", "más de 0 a 1000", "−360 a 360", "0,5 a 2"...); none
// when the text names no number. `low_open` when the low end is not taken
// ("más de 0", "> 0").
struct SettingRange {
    std::optional<double> low, high;
    bool low_open = false;
};
SettingRange parse_range(std::string_view text);

// The change the action makes to these settings, the value cut to `range`.
// None when the engine has no such setting (rule 7: no button); throws
// AssistantError when the action cannot apply to it (multiplying a text...).
std::optional<SettingChange> plan_setting(const Settings& settings, const SettingAction& action,
                                          const SettingRange& range = {});

// The settings with the change made. Throws SettingsError when the engine
// does not take the value.
Settings apply_setting(const Settings& settings, const SettingChange& change);

// Whether the engine has this setting ("group.key"; any machine variable).
bool known_setting(const Settings& settings, std::string_view key);

// A problem's causes in the order to show them (rule 6): the file's order,
// those marked `last_resort` at the end, in their order.
template <class Cause>
std::vector<const Cause*> causes_in_order(const std::vector<Cause>& causes) {
    std::vector<const Cause*> out;
    for (const bool last : {false, true})
        for (const Cause& c : causes)
            if (c.last_resort == last)
                out.push_back(&c);
    return out;
}

// What the assistant knows of a material (knowledge/materials.json), in the
// units of the engine: °C, hours. 0 or empty: not known.
struct MaterialInfo {
    std::string id;   // "pla", "abs"
    std::string name; // "PLA", "ABS"
    double nozzle_min = 0, nozzle_max = 0;
    double bed_min = 0, bed_max = 0;
    bool enclosure_needed = false;
    bool abrasive = false;
    bool dry_before_printing = false;
    double dry_min = 0, dry_max = 0; // °C
    double dry_hours = 0;
};

// The material of a filament profile (rule 13): by the pack's material type,
// else by the profile's name holding the material's name as a word ("Generic
// PETG" → PETG), whatever the case; the longest name wins ("PLA-CF" over
// "PLA"). None when neither finds it.
const MaterialInfo* find_material(const std::vector<MaterialInfo>& materials, std::string_view type,
                                  std::string_view profile_name);

// A material's warnings for this printer (rule 14). A warning whose condition
// needs something the printer does not say (0) is left out.
enum class MaterialWarning { NeedsEnclosure, Abrasive, BedTooCold, NozzleTooCold, DryFirst };
std::vector<MaterialWarning> material_warnings(const MaterialInfo& material, const Settings& printer);

// The warning's text in the interface's language («Conviene secarlo antes de
// imprimir: 45 a 55 °C, 4 h»).
std::string material_warning_text(MaterialWarning warning, const MaterialInfo& material);

// The help's problem for a warning of the review before printing (rule 9),
// by the warning's kind ("layer_time" → "overheating_small"); empty for the
// kinds without one.
std::string_view review_problem(std::string_view warning_kind);

// The guide of an engine calibration test (rule 20), by the test's name
// (calibration_kind_name: "max_flow" → "max_flow", "temperature" →
// "temperature_tower"); empty for the tests without one.
std::string_view guide_for_test(std::string_view test_name);
// The test a guide's «Calibrar» opens: the reverse of guide_for_test.
std::string_view test_for_guide(std::string_view guide_id);

// The guides of `before` not done yet (rule 22), in the order given: `done`
// holds the ids of the guides with a date.
std::vector<std::string> guides_missing(const std::vector<std::string>& before, const std::set<std::string>& done);

// What the person writes after a guide's test (rule 23): the test's start
// and step per millimetre of height, and what was measured (the height in
// mm where the print failed, or the block chosen).
struct GuideInput {
    double start = 0.0, step = 0.0, measured = 0.0;
};
// The value read from the print and the one proposed for the setting.
struct GuideOutcome {
    double value = 0.0, proposed = 0.0;
};
// The guide's formula: today max_flow, the flow where the print failed
// (start + height × step) and 10 % less (AS17). None for the guides whose
// formula the engine does not compute yet.
std::optional<GuideOutcome> guide_result(std::string_view guide_id, const GuideInput& input);

} // namespace tisma::ui
