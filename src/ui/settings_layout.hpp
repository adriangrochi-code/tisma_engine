#pragma once

#include "config/settings.hpp"
#include "ui/text.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace tisma::ui {

// Where each setting goes in the settings panel (docs/spec/features/
// settings_panel.md): a tab (Print, Material, Printer), a section (Quality,
// Strength...) and, inside it, a subsection, a folding card with a few
// settings (8 at most) and a one-line summary. Every setting has a level:
// Basic shows the settings almost every print needs, Normal adds those to
// fine-tune, Advanced shows them all. Own design on that spec.

enum class SettingsTab { Print, Material, Printer };
enum class SettingLevel { Basic, Normal, Advanced };
// What the panel shows: a level, or the settings the person chose.
enum class ViewLevel { Basic, Normal, Advanced, Custom };

struct SettingEntry {
    std::string_view key; // as docs/CONFIG.md and describe() name it
    SettingLevel level;
};
struct SubsectionLayout {
    std::string_view id;
    T title;
    std::vector<SettingEntry> settings;
};
struct SectionLayout {
    std::string_view id;
    T title;
    std::string_view icon; // a file of assets/icons/
    std::vector<SubsectionLayout> subsections;
};

const std::vector<SectionLayout>& settings_layout(SettingsTab tab);

struct SettingPlace {
    SettingsTab tab;
    const SectionLayout* section;
    const SubsectionLayout* subsection;
    SettingLevel level;
};
// The place of a setting; a key of machine.variables (the temperatures) by
// its own name. None for a key the panel does not show (compute.*).
std::optional<SettingPlace> setting_place(std::string_view key);

// Whether a level shows a setting of this level.
bool level_shows(ViewLevel view, SettingLevel level);

// The card's line when closed, from the current settings ("0.2 mm · first
// 0.2 mm"), in the interface's language.
std::string subsection_summary(std::string_view id, const Settings& s);

} // namespace tisma::ui
