#include "ui/settings_view.hpp"

#include "config/profiles.hpp"
#include "config/settings.hpp"
#include "ui/text.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <string>
#include <vector>

using namespace tisma;
using namespace tisma::ui;

namespace {

SettingText no_text(std::string_view) { return {}; }

const SectionLayout& section(SettingsTab tab, std::string_view id) {
    for (const SectionLayout& s : settings_layout(tab))
        if (s.id == id)
            return s;
    FAIL("no section " << id);
    throw;
}

} // namespace

TEST_CASE("settings view: folded text for the search", "[ui][layout]") {
    CHECK(fold("Línea Ñandú ÁÉÍÓÚ Pingüino") == "linea nandu aeiou pinguino");
    CHECK(fold("Z anti-aliasing") == "z anti-aliasing");
}

TEST_CASE("settings view: the search finds a setting by its card, name or key (SP6)", "[ui][layout]") {
    const Language before = language();
    set_language(Language::Spanish);
    // «costura»: the Seam card of Quality, walls.seam first.
    const std::vector<SettingHit> seam = search_settings("costura", no_text);
    REQUIRE_FALSE(seam.empty());
    CHECK(seam.front().place.section->id == "quality");
    CHECK(seam.front().place.subsection->id == "seam");
    CHECK(std::any_of(seam.begin(), seam.end(), [](const SettingHit& h) { return h.key == "walls.seam"; }));
    // Accents and case do not matter.
    CHECK(search_settings("COSTURA", no_text).front().key == seam.front().key);
    // A row's name counts most, and every word must be found.
    const auto label = [](std::string_view key) {
        return SettingText{key == "infill.wall_overlap" ? "Solape con las paredes" : "", ""};
    };
    const std::vector<SettingHit> overlap = search_settings("solape paredes", label);
    REQUIRE_FALSE(overlap.empty());
    CHECK(overlap.front().key == "infill.wall_overlap");
    CHECK(search_settings("solape zzz", label).empty());
    // By key, and what is not wanted is left out.
    CHECK(search_settings("belt.angle", no_text).front().key == "belt.angle");
    CHECK(search_settings("belt.angle", no_text, [](std::string_view k) { return !k.starts_with("belt."); }).empty());
    CHECK(search_settings("   ", no_text).empty());
    CHECK(search_settings("relleno", no_text, {}, 3).size() == 3);
    set_language(before);
}

TEST_CASE("settings view: Custom starts as the level it came from (SP10)", "[ui][layout]") {
    for (const ViewLevel level : {ViewLevel::Basic, ViewLevel::Normal, ViewLevel::Advanced}) {
        const CustomView view = custom_view_from(level);
        CHECK(view.base == level);
        for (const std::string& key : panel_keys()) {
            INFO(key);
            CHECK(custom_shows(view, key) == level_shows(level, setting_place(key)->level));
        }
    }
    CHECK(view_level_from(view_level_name(ViewLevel::Custom)) == ViewLevel::Custom);
    CHECK(view_level_from("basic") == ViewLevel::Basic);
    CHECK(view_level_from("whatever") == ViewLevel::Normal);
}

TEST_CASE("settings view: choosing what Custom shows, and taking it elsewhere (SP11, SP13, SP14)", "[ui][layout]") {
    CustomView view = custom_view_from(ViewLevel::Basic);
    // Speed off as a whole; the infill's wall overlap on.
    const SectionLayout& speed = section(SettingsTab::Print, "speed");
    for (const SubsectionLayout& sub : speed.subsections)
        custom_set(view, sub.settings, false);
    custom_set(view, {SettingEntry{"infill.wall_overlap", SettingLevel::Advanced}}, true);
    CHECK(custom_shows(view, "infill.wall_overlap"));
    for (const SubsectionLayout& sub : speed.subsections)
        for (const SettingEntry& e : sub.settings)
            CHECK_FALSE(custom_shows(view, e.key));
    CHECK(custom_shows(view, "walls.seam"));
    // Out and back in: the same settings.
    const std::string text = custom_view_json(view);
    CHECK(text.find("\"format\": \"tisma-ui-view\"") != std::string::npos);
    const CustomView back = parse_custom_view(text);
    CHECK(back.visible == view.visible);
    CHECK(back.base == ViewLevel::Basic);
    // A key this version does not have is left out; the rest applies.
    const CustomView other = parse_custom_view(
        R"({"format": "tisma-ui-view", "version": 1, "keys": ["walls.seam", "walls.no_such_thing"]})");
    CHECK(other.visible == std::set<std::string>{"walls.seam"});
    CHECK(other.base == ViewLevel::Normal);
    CHECK_THROWS_AS(parse_custom_view("{}"), ViewError);
    CHECK_THROWS_AS(parse_custom_view("not json"), ViewError);
    CHECK_THROWS_AS(parse_custom_view(R"({"format": "tisma-ui-view", "version": 2, "keys": []})"), ViewError);
    CHECK_THROWS_AS(parse_custom_view(R"({"format": "tisma-profile", "version": 1, "keys": []})"), ViewError);
    // A setting newer than the list shows by its level (rule 18).
    CustomView older = custom_view_from(ViewLevel::Normal);
    older.known.erase("walls.seam");
    older.visible.erase("walls.seam");
    CHECK(custom_shows(older, "walls.seam")); // a Basic setting, within Normal
    older.known.erase("infill.wall_overlap");
    CHECK_FALSE(custom_shows(older, "infill.wall_overlap")); // Advanced
}

TEST_CASE("settings view: a card's Reset takes its keys from the profile (rule 8)", "[ui][layout]") {
    Settings saved;
    saved.variables["temperature"] = {"215"};
    Settings now = saved;
    now.infill_density = 40.0;
    now.infill_pattern = InfillPattern::Gyroid;
    now.wall_count = 4;
    now.variables["temperature"] = {"230"};
    now.variables["chamber"] = {"40"};
    const Settings reset =
        with_keys(now, saved, {"infill.density", "infill.pattern", "machine.variables.temperature", "machine.variables.chamber"});
    CHECK(reset.infill_density == saved.infill_density);
    CHECK(reset.infill_pattern == saved.infill_pattern);
    CHECK(reset.wall_count == 4); // not in the card
    CHECK(reset.variables.at("temperature") == std::vector<std::string>{"215"});
    CHECK_FALSE(reset.variables.contains("chamber"));
    // A key of its own.
    now.changes.push_back({});
    now.changes.back().z = 5.0;
    now.changes.back().fan = 50.0;
    CHECK(with_keys(now, saved, {"changes"}).changes.empty());
}
