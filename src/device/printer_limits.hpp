#pragma once

// The printer's limits, read from its firmware (printer_connections.md, «Leer
// los límites de la impresora», rules 1 to 8):
// - Marlin: M115 (firmware name; the work area when built with
//   M115_GEOMETRY_REPORT, «area:{full:{min:{x:..},max:{x:..}},work:{..}}»)
//   and M503 (the values in use: M203 max feedrates, M201 max accelerations,
//   M204 P R T, M205 jerks or junction deviation, M900 K), as Marlin's
//   documentation of each command describes them. M503's lines are read
//   leniently: any line holding one of those commands, then letter-number
//   parameters, with G20 (inches) or G21 (mm) for the units (rule 7).
// - Klipper: Moonraker's «toolhead» object (max_velocity, max_accel,
//   square_corner_velocity, axis_minimum, axis_maximum) and
//   «configfile.settings» (printer.max_z_velocity, extruder.pressure_advance).
// A value the printer does not report stays out (rule 6: never 0 for a value
// that is missing). Own design; see PROVENANCE.md.

#include <map>
#include <string>
#include <vector>

#include <nlohmann/json_fwd.hpp>

namespace tisma {
struct Settings;
}

namespace tisma::device {

struct PrinterLimits {
    std::string source;                    // «Marlin 2.1.3», «Klipper»
    std::map<std::string, double> values;  // by the names of kLimitNames (max_feed_x…), mm and s
    bool has_area = false;                 // the work area, mm
    double min_x = 0, min_y = 0, max_x = 0, max_y = 0, max_z = 0;
    bool has_pressure_advance = false;
    double pressure_advance = 0.0;
};

PrinterLimits parse_marlin_limits(const std::string& m115, const std::string& m503);
// `toolhead` and `settings` as Moonraker's objects; either may be null.
PrinterLimits parse_klipper_limits(const nlohmann::json& toolhead, const nlohmann::json& settings);

// What it changes in the settings: with `only_empty`, only the limits that are
// 0 (not known) and an empty bed or height (rule 4: a value the person wrote
// is never changed by itself). Gives the names it set, for the one-line
// notice and the list.
std::vector<std::string> apply_limits(Settings& settings, const PrinterLimits& limits, bool only_empty);

} // namespace tisma::device
