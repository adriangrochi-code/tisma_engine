#pragma once

#include "config/profiles.hpp"
#include "config/settings.hpp"

#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// The catalog of printers and filaments on the market (docs/CATALOG.md):
// facts from each manufacturer's own pages and data sheets, every entry with
// its sources, and the profiles made from them. Own design; see PROVENANCE.md.
namespace tisma {

struct PrinterModel {
    std::string vendor, model;
    std::array<double, 3> build_volume{}; // mm; a round bed's diameter is the first
    bool circular = false;                // a round bed centred on the origin (delta printers)
    std::vector<double> nozzles;          // mm, the stock one first
    double filament_diameter = 1.75;
    std::string firmware;                 // "marlin", "klipper", "reprap", "bambu" or "other"
    bool direct_drive = true;             // or a Bowden tube
    bool heated_bed = true;
    std::optional<double> max_bed_temp, max_nozzle_temp, max_speed, max_acceleration; // C, mm/s, mm/s2
    std::optional<bool> auto_leveling;
    bool enclosed = false;
    std::string multi_material;           // the maker's multi-material option, if any
    std::vector<std::string> sources;     // the official pages the numbers come from
};

struct FilamentModel {
    std::string vendor, name, material; // material: the base polymer ("PLA", "PETG", "PA-CF", ...)
    double diameter = 1.75;
    std::optional<double> density;      // g/cm3
    std::optional<std::array<double, 2>> nozzle_temp, bed_temp, fan, print_speed; // C, C, %, mm/s
    std::optional<double> nozzle_recommended, bed_recommended;
    std::optional<bool> enclosure;
    std::vector<std::string> sources;
};

struct Catalog {
    std::vector<PrinterModel> printers;   // sorted by vendor, then model
    std::vector<FilamentModel> filaments; // sorted by vendor, then name
};

// Reads the catalog's two JSON files (resources/catalog/). Throws
// SettingsError naming the entry and the key on a missing or bad value, a
// repeated entry, or a source that is not allowed (docs/CATALOG.md).
Catalog parse_catalog(std::string_view printers_json, std::string_view filaments_json);

// The catalog compiled into the engine.
const Catalog& builtin_catalog();

// Whether a source address may hold catalog facts: never a code host or a
// slicer's site (their profiles are other programs' work, docs/CATALOG.md).
bool allowed_source(std::string_view url);

// The entries whose vendor and name hold every word of the query (any case).
std::vector<const PrinterModel*> find_printers(const Catalog& catalog, std::string_view query);
std::vector<const FilamentModel*> find_filaments(const Catalog& catalog, std::string_view query);

// "Vendor Model 0.4 mm", "Vendor Name".
std::string printer_profile_name(const PrinterModel& printer, double nozzle);
std::string filament_profile_name(const FilamentModel& filament);

// A printer profile for the model with that nozzle (docs/CATALOG.md, "Perfiles"):
// its bed (a rectangle from the origin, or a circle around it), height,
// nozzle, G-code flavor, retraction for its extruder, its speed and
// acceleration limits when known, and the engine's own start and end G-code.
Profile printer_profile(const PrinterModel& printer, double nozzle);

// A filament profile: its diameter, the nozzle and bed temperatures (the
// recommended ones, else the middle of the range rounded to 5 C), and the fan
// from the data sheet or, when it gives no numbers, the engine's default for
// its material.
Profile filament_profile(const FilamentModel& filament);

// Firmwares that need more than plain G-code to start well (their own
// sequences or file headers): the start G-code must be checked by hand.
bool needs_own_start_gcode(const PrinterModel& printer);

} // namespace tisma
