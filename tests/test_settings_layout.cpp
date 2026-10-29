#include "ui/settings_layout.hpp"

#include "config/settings.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <set>
#include <string>

using namespace tisma;
using namespace tisma::ui;

TEST_CASE("settings panel: every setting has a section, a card and a level (SP1)", "[ui][layout]") {
    for (const auto& [key, value] : describe(Settings{})) {
        CAPTURE(key);
        if (key.starts_with("compute.")) // a preference of the program, not of a profile
            continue;
        // The command line's: the interface has tools, and each object its menu.
        if (key.starts_with("placement.") || key.starts_with("transform.") || key.starts_with("object."))
            continue;
        const std::optional<SettingPlace> place = setting_place(key);
        REQUIRE(place);
        CHECK(place->section != nullptr);
        CHECK(place->subsection != nullptr);
    }
    // Cards of 8 settings at most, every key once, unique ids.
    std::set<std::string> keys, sections, subsections;
    for (const SettingsTab tab : {SettingsTab::Print, SettingsTab::Material, SettingsTab::Printer})
        for (const SectionLayout& section : settings_layout(tab)) {
            CHECK(sections.insert(std::string(section.id) + "/" + std::to_string(int(tab))).second);
            CHECK_FALSE(section.subsections.empty());
            for (const SubsectionLayout& sub : section.subsections) {
                CAPTURE(sub.id);
                CHECK(subsections.insert(std::string(sub.id)).second);
                CHECK(sub.settings.size() <= 8);
                for (const SettingEntry& e : sub.settings)
                    CHECK(keys.insert(std::string(e.key)).second);
            }
        }
    CHECK_FALSE(setting_place("compute.gpu"));
    CHECK(setting_place("machine.variables.first_layer_temperature")->subsection->id == "variables");
}

TEST_CASE("settings panel: Basic shows the spec's list, Normal more, Advanced all (rule 10)", "[ui][layout]") {
    const std::set<std::string> basic = {
        "layers.height", "layers.first_height", "walls.seam", "walls.count", "surfaces.top_layers",
        "surfaces.bottom_layers", "surfaces.top_min_thickness", "surfaces.bottom_min_thickness", "surfaces.top_pattern",
        "infill.density", "infill.pattern", "support.enabled", "support.automatic", "adhesion.skirt_loops",
        "adhesion.skirt_distance", "adhesion.brim_width", "machine.variables.temperature",
        "machine.variables.bed_temperature", "cooling.fan_min", "cooling.fan_max", "retraction.length", "retraction.lift",
        // the printer's, in its own page
        "machine.bed", "machine.max_height", "machine.nozzle_diameter", "machine.filament_diameter"};
    std::size_t shown = 0, normal = 0, all = 0;
    for (const SettingsTab tab : {SettingsTab::Print, SettingsTab::Material, SettingsTab::Printer})
        for (const SectionLayout& section : settings_layout(tab))
            for (const SubsectionLayout& sub : section.subsections)
                for (const SettingEntry& e : sub.settings) {
                    CAPTURE(e.key);
                    CHECK(level_shows(ViewLevel::Basic, e.level) == basic.contains(std::string(e.key)));
                    shown += level_shows(ViewLevel::Basic, e.level);
                    normal += level_shows(ViewLevel::Normal, e.level);
                    CHECK(level_shows(ViewLevel::Advanced, e.level));
                    ++all;
                }
    CHECK(shown == basic.size());
    CHECK(shown < normal);
    CHECK(normal < all);
    // The spec's Advanced-only settings.
    for (const char* key : {"layers.detail_height", "layers.cusp_height", "zaa.min_thickness", "infill.solid_every",
                            "infill.combine_layers", "cooling.keep_outer_speed", "extrusion.pressure_advance",
                            "compensation.scale_x", "slicing.xy_offset", "gcode.resolution", "changes"}) {
        CAPTURE(key);
        CHECK(setting_place(key)->level == SettingLevel::Advanced);
    }
}

TEST_CASE("settings panel: the closed cards' summaries (SP2, SP3)", "[ui][layout]") {
    set_language(Language::Spanish);
    Settings s;
    s.variables["temperature"] = {"215"};
    CHECK(subsection_summary("layer_height", s) == "0,2 mm · primera 0,2 mm");
    CHECK(subsection_summary("infill", s) == "20 % · Rectilínea");
    s.infill_density = 40.0;
    CHECK(subsection_summary("infill", s) == "40 % · Rectilínea");
    CHECK(subsection_summary("nozzle_temp", s) == "215 °C");
    CHECK(subsection_summary("support", s) == "Sin soportes");
    s.scarf = ScarfMode::Contours;
    CHECK(subsection_summary("seam", s) == "Alineada · en bufanda");
    CHECK(subsection_summary("tree", s) == "Ramas de 2 mm · puntas de 0,8 mm");
    set_language(Language::English);
    CHECK(subsection_summary("layer_height", s) == "0.2 mm · first 0.2 mm");
    set_language(Language::Spanish);
    // Every card has a summary.
    for (const SettingsTab tab : {SettingsTab::Print, SettingsTab::Material, SettingsTab::Printer})
        for (const SectionLayout& section : settings_layout(tab))
            for (const SubsectionLayout& sub : section.subsections) {
                CAPTURE(sub.id);
                CHECK_FALSE(subsection_summary(sub.id, Settings{}).empty());
            }
}

TEST_CASE("interface text: numbers in the language, the decimal comma in Spanish (the spec team's TR1, TR2, TR4)",
          "[ui][layout]") {
    set_language(Language::Spanish);
    CHECK(format("Z %.2f mm · %.1f %% · %d capas · %s", 10.0, 20.9, 50, "v0.10.0") ==
          "Z 10,00 mm · 20,9 % · 50 capas · v0.10.0"); // a text's own dots stay
    CHECK(format("%g mm · %zu", 0.4, std::size_t(3)) == "0,4 mm · 3");
    CHECK(format("%5.1f|%-6s|%03d|%x", 1.25, "ab", 7, 255u) == "  1,2|ab    |007|ff");
    CHECK(grouped(4013) == "4 013");
    CHECK(grouped(1930764) == "1 930 764");
    CHECK(localized("100.0") == "100,0");
    // Thin walls: one bead, in the singular.
    Settings s;
    s.wall_spread_count = 1;
    CHECK(subsection_summary("walls_thin", s) == "1 cordón toma el cambio de ancho");
    set_language(Language::English);
    CHECK(format("Z %.2f mm", 10.0) == "Z 10.00 mm");
    CHECK(grouped(4013) == "4,013");
    CHECK(grouped(240964) == "240,964");
    CHECK(grouped(12) == "12");
    CHECK(subsection_summary("walls_thin", s) == "1 bead takes the change in width");
    s.wall_spread_count = 2;
    CHECK(subsection_summary("walls_thin", s) == "2 beads share the change in width");
    set_language(Language::Spanish);
}
