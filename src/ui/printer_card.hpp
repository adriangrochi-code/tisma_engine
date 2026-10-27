#pragma once

#include "calibration/calibration.hpp"
#include "catalog/catalog.hpp"
#include "config/settings.hpp"

#include <array>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

// The printer's card at the top of the settings panel (docs/spec/features/
// settings_panel.md, rules 21 to 36): which drawing a printer, its bed and its
// nozzle get, and the nozzles a printer's model comes with. Own design on
// that spec.
namespace tisma::ui {

// The drawing of a printer, by its profile (rule 34). Pellet printers are
// left for when the engine has their key (machine.feedstock, pellets.md).
enum class PrinterKind { Resin, Pellets, Belt, Delta, Enclosed, Open };
PrinterKind printer_kind(const Settings& settings);

// Whether a bed outline is a circle: twelve corners or more, all as far from
// the middle within 2 % (the catalog's round beds have 64).
bool round_bed(const std::vector<std::array<double, 2>>& bed);

// The drawing of a bed, by the family its name says (rule 35), whatever the
// case.
enum class BedFamily { Textured, Smooth, Glass, Cool, Engineering, Other };
BedFamily bed_family(std::string_view name);

// The drawing of a nozzle (rule 36): striped when hardened, longer for a
// high-flow one ("high flow" or "HF" in its name), and its tip from 0.2 to
// 1.2 mm grown into `size`, 0 to 1, so that sizes tell apart at a glance.
struct NozzleLook {
    double diameter = 0.4;
    bool hardened = false;
    bool high_flow = false;
    double size = 0.2;
};
NozzleLook nozzle_look(double diameter, bool hardened, std::string_view name = {});

// The catalog's model a printer profile was made from (its name is the
// catalog's "Vendor Model 0.4 mm", maybe with " (2)" after it), and the
// nozzle it names; none for other profiles.
struct CatalogMatch {
    const PrinterModel* model = nullptr;
    double nozzle = 0.0;
};
std::optional<CatalogMatch> catalog_model_of(const Catalog& catalog, std::string_view profile_name);

// The nozzles to offer for a printer (rule 21): its catalog model's, or only
// the one it has; with the profile each one is, in order of size.
struct NozzleChoice {
    double diameter = 0.4;
    std::string profile; // the profile's name; empty for a printer not from the catalog
};
std::vector<NozzleChoice> nozzle_choices(const Catalog& catalog, std::string_view profile_name, double nozzle);

// The printer page's Calibration section (rule 38): a result applied, with
// its day (YYYY-MM-DD), the nozzle and the filament it was made with.
struct CalibrationRecord {
    std::string date;
    double nozzle = 0.0;
    std::string filament;
    bool operator==(const CalibrationRecord&) const = default;
};
enum class CalibrationState { Missing, Done, OtherNozzle };
// Not calibrated, calibrated, or calibrated with another nozzle than the
// printer's now.
CalibrationState calibration_state(const CalibrationRecord* record, double nozzle);
// The calibrations a printer of this kind takes, in the order to make them
// (a resin printer's, or the rest).
std::vector<CalibrationKind> printer_calibrations(const Settings& settings);
// The setting a calibration fixes ("extrusion.flow"); empty for the tests
// that fix none (the first layer, the fit).
std::string_view calibration_key(CalibrationKind kind);
// Whether its result is the filament's (temperature, flow, pressure advance,
// retraction): the section says which filament it was made with.
bool filament_calibration(CalibrationKind kind);

// Where the dates live (assistant.md, rule 22): a filament's calibration in
// the filament profile (extrusion.calibrations, with the printer), a
// printer's in the printer profile (machine.calibrations), by guide id or by
// test name. `filament` names the record's filament (the profile chosen).
std::optional<CalibrationRecord> calibration_record(const Settings& settings, std::string_view name,
                                                    std::string_view printer, std::string_view filament);
// A result applied today (`date`) on this printer with its nozzle now.
void date_calibration(Settings& settings, std::string_view name, bool filament, std::string_view date,
                      std::string_view printer);
// The names dated for this printer.
std::set<std::string> calibrations_dated(const Settings& settings, std::string_view printer);

} // namespace tisma::ui
