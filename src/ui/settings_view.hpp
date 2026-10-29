#pragma once

#include "ui/settings_layout.hpp"

#include <cstddef>
#include <functional>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

// What the settings panel shows besides its levels (docs/spec/features/
// settings_panel.md): the search (rule 5) and the Custom view, the settings
// the person chose (rules 15 to 19). Own design on that spec.
namespace tisma::ui {

// Text for matching: lower case, and the Spanish accents and ü taken off
// ("Línea" → "linea").
std::string fold(std::string_view text);

// A setting the search found, and how well: its name, then its card's,
// then its key, then what it does.
struct SettingHit {
    std::string key;
    SettingPlace place;
    int score = 0;
};

// What the search reads of a setting: its row's name and what it does, in
// the interface's language.
struct SettingText {
    std::string label, about;
};

// The settings whose name, card, section, key or explanation hold every word
// of the query (folded), best first; at most `limit`. `wanted` leaves keys
// out (a resin printer's on a filament printer, say).
std::vector<SettingHit> search_settings(std::string_view query,
                                        const std::function<SettingText(std::string_view key)>& text,
                                        const std::function<bool(std::string_view key)>& wanted = {},
                                        std::size_t limit = 12);

// Every key the panel places, in its order.
const std::vector<std::string>& panel_keys();

// The Custom view: the settings it shows, the level it started from, and
// every key the panel had when the list was made, so that a setting a newer
// version adds shows when its level is within that one (rule 18).
struct CustomView {
    ViewLevel base = ViewLevel::Normal; // Basic, Normal or Advanced
    std::set<std::string> visible;
    std::set<std::string> known;
    bool operator==(const CustomView&) const = default;
};

// The view that shows what `level` shows (rule 15).
CustomView custom_view_from(ViewLevel level);

// Whether the view shows the setting.
bool custom_shows(const CustomView& view, std::string_view key);

// Show or hide every setting of a card or a section at once (rule 16).
void custom_set(CustomView& view, const std::vector<SettingEntry>& settings, bool shown);

// The view as a file to take elsewhere (rule 17): {"format":
// "tisma-ui-view", "version": 1, "base": ..., "keys": [...]}.
std::string custom_view_json(const CustomView& view);

class ViewError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};
// A view read back. Keys this version does not know are left out without a
// word (rule 18); every key it has counts as known. Throws ViewError when
// the text is not such a file.
CustomView parse_custom_view(std::string_view json);

// A level's name in the preferences ("basic", "normal", "advanced",
// "custom") and back (Normal for anything else).
std::string_view view_level_name(ViewLevel level);
ViewLevel view_level_from(std::string_view name);

} // namespace tisma::ui
