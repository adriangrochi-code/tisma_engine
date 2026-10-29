#include "ui/printer_card.hpp"

#include "catalog/catalog.hpp"
#include "config/profiles.hpp"
#include "config/settings.hpp"
#include "ui/settings_layout.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <set>

using namespace tisma;
using namespace tisma::ui;

TEST_CASE("printer card: the printer's drawing by its profile (SP25)", "[ui][printer]") {
    Settings s;
    CHECK(printer_kind(s) == PrinterKind::Open);
    s.enclosed = true;
    CHECK(printer_kind(s) == PrinterKind::Enclosed);
    // A round bed: a delta.
    s.bed.clear();
    for (int i = 0; i < 64; ++i) {
        const double a = 2.0 * std::numbers::pi * i / 64;
        s.bed.push_back({100.0 * std::cos(a), 100.0 * std::sin(a)});
    }
    CHECK(round_bed(s.bed));
    CHECK(printer_kind(s) == PrinterKind::Delta);
    s.belt_enabled = true;
    CHECK(printer_kind(s) == PrinterKind::Belt);
    s.machine_technology = MachineTechnology::Msla;
    CHECK(printer_kind(s) == PrinterKind::Resin);
    // A rectangle, or an octagon, is not round.
    CHECK_FALSE(round_bed({{0, 0}, {200, 0}, {200, 200}, {0, 200}}));
    std::vector<std::array<double, 2>> octagon;
    for (int i = 0; i < 8; ++i)
        octagon.push_back({std::cos(i * std::numbers::pi / 4), std::sin(i * std::numbers::pi / 4)});
    CHECK_FALSE(round_bed(octagon));
}

TEST_CASE("printer card: the bed's drawing by its name (SP26)", "[ui][printer]") {
    CHECK(bed_family("Textured PEI Plate") == BedFamily::Textured);
    CHECK(bed_family("Satin sheet") == BedFamily::Textured);
    CHECK(bed_family("Smooth PEI Plate") == BedFamily::Smooth);
    CHECK(bed_family("PEI") == BedFamily::Smooth);
    CHECK(bed_family("Cool Plate") == BedFamily::Cool);
    CHECK(bed_family("Glass") == BedFamily::Glass);
    CHECK(bed_family("cama de VIDRIO") == BedFamily::Glass);
    CHECK(bed_family("Engineering Plate") == BedFamily::Engineering);
    CHECK(bed_family("High Temp Plate") == BedFamily::Engineering);
    CHECK(bed_family("Magnetic sheet") == BedFamily::Other);
}

TEST_CASE("printer card: the nozzle's drawing (SP27)", "[ui][printer]") {
    const NozzleLook brass = nozzle_look(0.2, false);
    const NozzleLook steel = nozzle_look(0.6, true);
    CHECK(steel.size > brass.size);
    CHECK(steel.hardened);
    CHECK_FALSE(brass.hardened);
    CHECK(nozzle_look(1.2, false).size == 1.0);
    CHECK(nozzle_look(0.1, false).size == 0.0);
    CHECK(nozzle_look(0.4, false, "0.4 mm HF").high_flow);
    CHECK(nozzle_look(0.4, false, "High Flow hot end").high_flow);
    CHECK_FALSE(nozzle_look(0.4, false, "Shfoo 0.4").high_flow);
}

TEST_CASE("printer card: the nozzles of a catalog printer (rule 21)", "[ui][printer]") {
    const Catalog& catalog = builtin_catalog();
    const auto it = std::find_if(catalog.printers.begin(), catalog.printers.end(),
                                 [](const PrinterModel& p) { return p.nozzles.size() > 1; });
    REQUIRE(it != catalog.printers.end());
    const std::string name = printer_profile_name(*it, it->nozzles.front());
    const auto match = catalog_model_of(catalog, name);
    REQUIRE(match);
    CHECK(match->model == &*it);
    CHECK(match->nozzle == it->nozzles.front());
    CHECK(catalog_model_of(catalog, name + " (2)"));
    CHECK_FALSE(catalog_model_of(catalog, "My own printer"));
    const std::vector<NozzleChoice> choices = nozzle_choices(catalog, name, it->nozzles.front());
    CHECK(choices.size() == it->nozzles.size());
    CHECK(std::is_sorted(choices.begin(), choices.end(),
                         [](const NozzleChoice& a, const NozzleChoice& b) { return a.diameter < b.diameter; }));
    for (const NozzleChoice& c : choices)
        CHECK(c.profile == printer_profile_name(*it, c.diameter));
    // Another printer: only the nozzle it has, no profile to go to.
    const std::vector<NozzleChoice> own = nozzle_choices(catalog, "My own printer", 0.6);
    REQUIRE(own.size() == 1);
    CHECK(own.front().diameter == 0.6);
    CHECK(own.front().profile.empty());
}

TEST_CASE("printer card: the calibrations a printer has made (rule 38, SP29, SP30)", "[ui][printer]") {
    Settings s;
    const std::vector<CalibrationKind> fdm = printer_calibrations(s);
    CHECK(fdm.size() == 12);
    CHECK(fdm.front() == CalibrationKind::Dimensions);
    CHECK(std::none_of(fdm.begin(), fdm.end(), [](CalibrationKind k) { return is_resin_calibration(k); }));
    s.machine_technology = MachineTechnology::Msla;
    CHECK(printer_calibrations(s) == std::vector{CalibrationKind::ResinExposure, CalibrationKind::ResinMeasure});
    // Each fixes a setting the panel has, but the first layer and the fit.
    for (int k = 0; k < kCalibrationKinds; ++k) {
        const auto kind = static_cast<CalibrationKind>(k);
        const std::string_view key = calibration_key(kind);
        INFO(calibration_kind_name(kind));
        CHECK(key.empty() == (kind == CalibrationKind::FirstLayer || kind == CalibrationKind::Fit));
        if (!key.empty())
            CHECK(setting_place(key));
    }
    CHECK(filament_calibration(CalibrationKind::Flow));
    CHECK_FALSE(filament_calibration(CalibrationKind::Dimensions));
    // SP29 and SP30: calibrated with this nozzle, then with another one.
    const CalibrationRecord flow{"2026-10-27", 0.4, "Generic PLA"};
    CHECK(calibration_state(nullptr, 0.4) == CalibrationState::Missing);
    CHECK(calibration_state(&flow, 0.4) == CalibrationState::Done);
    CHECK(calibration_state(&flow, 0.6) == CalibrationState::OtherNozzle);
}

TEST_CASE("printer page: the calibrations' dates in the profiles (assistant.md rule 22, AS21)", "[ui][printer_card]") {
    Settings s;
    s.nozzle_diameter = 0.4;
    // A filament's in the filament profile, with its printer; a printer's in the printer's.
    date_calibration(s, "flow", true, "2026-10-27", "Printer A");
    date_calibration(s, "input_shaping", false, "2026-10-26", "Printer A");
    REQUIRE(s.filament_calibrations.contains("flow"));
    CHECK(s.filament_calibrations.at("flow").printer == "Printer A");
    REQUIRE(s.printer_calibrations.contains("input_shaping"));
    CHECK(s.printer_calibrations.at("input_shaping").printer.empty());
    // AS21: flow dated, so the guide after it does not warn of it.
    CHECK(calibrations_dated(s, "Printer A") == std::set<std::string>{"flow", "input_shaping"});
    // On another printer the filament's date does not count.
    CHECK(calibrations_dated(s, "Printer B") == std::set<std::string>{"input_shaping"});
    const std::optional<CalibrationRecord> r = calibration_record(s, "flow", "Printer A", "Generic PLA");
    REQUIRE(r);
    CHECK(r->date == "2026-10-27");
    CHECK(r->nozzle == 0.4);
    CHECK(r->filament == "Generic PLA");
    CHECK_FALSE(calibration_record(s, "flow", "Printer B", "Generic PLA"));
    CHECK_FALSE(calibration_record(s, "max_flow", "Printer A", {}));
    // They go through the settings' JSON, each in its profile.
    const Settings back = parse_settings(settings_json(s));
    CHECK(back.filament_calibrations == s.filament_calibrations);
    CHECK(back.printer_calibrations == s.printer_calibrations);
    CHECK(setting_kind("extrusion", "calibrations", {}) == ProfileKind::Filament);
    CHECK(setting_kind("machine", "calibrations", {}) == ProfileKind::Printer);
    CHECK_THROWS_AS(parse_settings(R"({"machine": {"calibrations": {"flow": {"nozzle": 0.4}}}})"), SettingsError);
    CHECK_THROWS_AS(parse_settings(R"({"extrusion": {"calibrations": []}})"), SettingsError);
}
