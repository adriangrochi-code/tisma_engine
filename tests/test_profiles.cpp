#include "config/profiles.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>

using namespace tisma;
using Catch::Matchers::ContainsSubstring;

namespace {

// An empty folder of its own for each test (CTest runs them in parallel).
std::filesystem::path fresh_folder(const std::string& name) {
    const auto dir = std::filesystem::temp_directory_path() / ("tisma_test_profiles_" + name);
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    return dir;
}

Settings changed_everywhere() {
    Settings s;
    s.layer_height = 0.12;               // print
    s.infill_density = 35.0;             // print
    s.wall_speed = 45.0;                 // print
    s.nozzle_diameter = 0.6;             // printer
    s.flavor = GcodeFlavor::Klipper;     // printer
    s.retract_length = 0.8;              // printer
    s.start_gcode = "G28\nPRINT_START";  // printer
    s.filament_diameter = 2.85;          // filament
    s.fan_min = 80.0;                    // filament
    s.flow = 96.0;                       // filament
    s.scale_x = 100.4;                   // filament
    s.variables["temperature"] = {"245"};     // filament
    s.variables["bed_temperature"] = {"85"};  // filament
    s.variables["chamber"] = {"1"};           // printer
    HeightChange change;
    change.z = 5.0;
    change.temperature = 230.0;
    s.changes.push_back(change);
    return s;
}

} // namespace

TEST_CASE("every setting belongs to one profile kind, except the height changes", "[profiles]") {
    const nlohmann::json all = nlohmann::json::parse(settings_json(changed_everywhere()));
    int printer = 0, filament = 0, print = 0;
    for (const auto& [group, keys] : all.items()) {
        if (group == "changes") {
            CHECK_FALSE(setting_kind(group, ""));
            continue;
        }
        REQUIRE(keys.is_object());
        for (const auto& [key, value] : keys.items()) {
            INFO(group + "." + key);
            const auto kind = setting_kind(group, key);
            REQUIRE(kind);
            printer += *kind == ProfileKind::Printer;
            filament += *kind == ProfileKind::Filament;
            print += *kind == ProfileKind::Print;
        }
    }
    CHECK(printer > 10);
    CHECK(filament > 5);
    CHECK(print > 20);
    CHECK(setting_kind("machine", "variables", "temperature") == ProfileKind::Filament);
    CHECK(setting_kind("machine", "variables", "first_layer_bed_temperature") == ProfileKind::Filament);
    CHECK(setting_kind("machine", "variables", "chamber") == ProfileKind::Printer);
    CHECK(setting_kind("retraction", "length") == ProfileKind::Printer);
    CHECK(setting_kind("compensation", "scale_x") == ProfileKind::Filament);
    for (const ProfileKind kind : kProfileKinds)
        CHECK(profile_kind_from_name(profile_kind_name(kind)) == kind);
    CHECK_FALSE(profile_kind_from_name("machine"));
}

TEST_CASE("a profile takes only its own settings", "[profiles]") {
    const Settings source = changed_everywhere();
    const Settings base;

    const Settings filament = with_profile(base, ProfileKind::Filament, source);
    CHECK(filament.filament_diameter == 2.85);
    CHECK(filament.fan_min == 80.0);
    CHECK(filament.flow == 96.0);
    CHECK(filament.scale_x == 100.4);
    CHECK(filament.variables.at("temperature") == std::vector<std::string>{"245"});
    CHECK(filament.variables.at("bed_temperature") == std::vector<std::string>{"85"});
    CHECK_FALSE(filament.variables.contains("chamber"));
    CHECK(filament.layer_height == base.layer_height);
    CHECK(filament.nozzle_diameter == base.nozzle_diameter);
    CHECK(filament.retract_length == base.retract_length);
    CHECK(filament.changes.empty());

    const Settings printer = with_profile(base, ProfileKind::Printer, source);
    CHECK(printer.nozzle_diameter == 0.6);
    CHECK(printer.flavor == GcodeFlavor::Klipper);
    CHECK(printer.retract_length == 0.8);
    CHECK(printer.start_gcode == "G28\nPRINT_START");
    CHECK(printer.variables.at("chamber") == std::vector<std::string>{"1"});
    CHECK_FALSE(printer.variables.contains("temperature"));
    CHECK(printer.filament_diameter == base.filament_diameter);
    CHECK(printer.infill_density == base.infill_density);

    const Settings print = with_profile(base, ProfileKind::Print, source);
    CHECK(print.layer_height == 0.12);
    CHECK(print.infill_density == 35.0);
    CHECK(print.wall_speed == 45.0);
    CHECK(print.nozzle_diameter == base.nozzle_diameter);
    CHECK(print.fan_min == base.fan_min);

    // The three together give everything back but the height changes, which
    // stay with what they were applied to.
    Settings all = base;
    all.changes = source.changes;
    for (const ProfileKind kind : kProfileKinds)
        all = with_profile(all, kind, source);
    CHECK(settings_json(all) == settings_json(source));

    // A printer profile replaces the printer's variables and keeps the filament's.
    Settings other = source;
    other.variables = {{"temperature", {"200"}}, {"old_printer", {"x"}}};
    const Settings swapped = with_profile(other, ProfileKind::Printer, source);
    CHECK(swapped.variables.at("temperature") == std::vector<std::string>{"200"});
    CHECK(swapped.variables.at("chamber") == std::vector<std::string>{"1"});
    CHECK_FALSE(swapped.variables.contains("old_printer"));

    CHECK(same_profile(ProfileKind::Filament, filament, source));
    CHECK_FALSE(same_profile(ProfileKind::Print, filament, source));
    CHECK(same_profile(ProfileKind::Print, print, source));
}

TEST_CASE("profile files are read back and reject settings of another kind", "[profiles]") {
    Profile p;
    p.kind = ProfileKind::Filament;
    p.name = "PETG Negro — 0,4";
    p.settings = changed_everywhere();
    const std::string text = profile_file_json(p);
    CHECK_THAT(text, ContainsSubstring("\"format\": \"tisma-profile\""));
    CHECK_THAT(text, ContainsSubstring("\"kind\": \"filament\""));
    CHECK_THAT(text, !ContainsSubstring("\"layers\""));
    CHECK_THAT(text, !ContainsSubstring("changes"));

    const Profile back = parse_profile(text);
    CHECK(back.kind == ProfileKind::Filament);
    CHECK(back.name == p.name);
    CHECK(same_profile(ProfileKind::Filament, back.settings, p.settings));
    CHECK(back.settings.layer_height == Settings{}.layer_height);

    const auto fails = [](const std::string& json, const std::string& message) {
        INFO(json);
        try {
            parse_profile(json);
            FAIL("accepted");
        } catch (const SettingsError& e) {
            CHECK_THAT(e.what(), ContainsSubstring(message));
        }
    };
    fails("{", "not valid JSON");
    fails(R"({"format": "x"})", "not a profile");
    fails(R"({"format": "tisma-profile", "version": 2, "kind": "print", "name": "a", "settings": {}})", "newer");
    fails(R"({"format": "tisma-profile", "version": 1, "kind": "nozzle", "name": "a", "settings": {}})", "kind");
    fails(R"({"format": "tisma-profile", "version": 1, "kind": "print", "name": "", "settings": {}})", "name");
    fails(R"({"format": "tisma-profile", "version": 1, "kind": "filament", "name": "a",
              "settings": {"layers": {"height": 0.1}}})",
          "layers.height: a print setting, not a filament one");
    fails(R"({"format": "tisma-profile", "version": 1, "kind": "printer", "name": "a",
              "settings": {"machine": {"variables": {"temperature": 200}}}})",
          "machine.variables.temperature: not a printer setting");
    fails(R"({"format": "tisma-profile", "version": 1, "kind": "print", "name": "a",
              "settings": {"changes": []}})",
          "changes");
    fails(R"({"format": "tisma-profile", "version": 1, "kind": "print", "name": "a",
              "settings": {"layers": {"height": -1}}})",
          "layers.height");
}

TEST_CASE("profiles are saved, listed, replaced and removed in their folders", "[profiles]") {
    const auto root = fresh_folder("store");
    CHECK(load_profiles(root, ProfileKind::Print).empty());

    Profile a;
    a.kind = ProfileKind::Print;
    a.name = "Rápido: 0.28/0.4 mm";
    a.settings.layer_height = 0.28;
    save_profile(root, a);
    Profile b = a;
    b.name = "detalle";
    b.settings.layer_height = 0.12;
    save_profile(root, b);
    Profile c;
    c.kind = ProfileKind::Filament;
    c.name = "detalle"; // the same name in another kind is another profile
    save_profile(root, c);

    auto prints = load_profiles(root, ProfileKind::Print);
    REQUIRE(prints.size() == 2);
    CHECK(prints[0].name == "detalle"); // sorted ignoring case
    CHECK(prints[1].name == a.name);
    CHECK(prints[1].settings.layer_height == 0.28);
    CHECK(load_profiles(root, ProfileKind::Filament).size() == 1);
    CHECK(std::filesystem::exists(root / "print"));

    // Saving the same name replaces it, in the same file.
    a.settings.layer_height = 0.3;
    save_profile(root, a);
    prints = load_profiles(root, ProfileKind::Print);
    REQUIRE(prints.size() == 2);
    CHECK(prints[1].settings.layer_height == 0.3);
    int files = 0;
    for (const auto& entry : std::filesystem::directory_iterator(root / "print")) {
        CHECK(entry.path().extension() == ".json");
        ++files;
    }
    CHECK(files == 2);

    // Names that make the same file name get files of their own.
    Profile d = b;
    d.name = "detalle.";
    save_profile(root, d);
    CHECK(load_profiles(root, ProfileKind::Print).size() == 3);

    // A damaged file is skipped and reported.
    std::ofstream(root / "print" / "roto.json") << "{";
    std::vector<std::string> errors;
    CHECK(load_profiles(root, ProfileKind::Print, &errors).size() == 3);
    REQUIRE(errors.size() == 1);
    CHECK_THAT(errors[0], ContainsSubstring("roto.json"));

    CHECK(remove_profile(root, ProfileKind::Print, "detalle"));
    CHECK_FALSE(remove_profile(root, ProfileKind::Print, "detalle"));
    CHECK(load_profiles(root, ProfileKind::Print).size() == 2);
    CHECK(load_profiles(root, ProfileKind::Filament).size() == 1);

    Profile unnamed;
    CHECK_THROWS_AS(save_profile(root, unnamed), SettingsError);
    std::filesystem::remove_all(root);
}
