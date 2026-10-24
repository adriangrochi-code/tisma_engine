#pragma once

#include "config/settings.hpp"
#include "report/warnings.hpp"
#include "slice/pipeline.hpp"

#include <ostream>
#include <string>
#include <string_view>

namespace tisma {

// Replaces [name], {name} and {name[i]} in G-code text with the variables'
// values (docs/spec/GCODE_OUTPUT.md, first version). Unknown names and indices
// are left as they are.
std::string substitute_variables(std::string_view text, const std::map<std::string, std::vector<std::string>>& variables);

// The command that sets the chamber's temperature: M141 S, or Klipper's
// SET_HEATER_TEMPERATURE for a heater named "chamber".
std::string chamber_gcode(GcodeFlavor flavor, double celsius);

// What the filament asks that the machine cannot give (docs/CATALOG.md,
// "Capacidades"): a nozzle or bed hotter than its maximum, a chamber it does
// not heat, an enclosure, a hardened nozzle. write_gcode adds them to its
// warnings.
std::vector<Warning> capability_warnings(const Settings& settings);

// The command that sets the accelerations: M204 P (printing) T (travel) in
// Marlin and RepRapFirmware, SET_VELOCITY_LIMIT ACCEL= in Klipper (one value
// for both), from each firmware's public documentation. Written after the
// start G-code, so the printer runs at the acceleration the time estimate
// assumes, and by a height change (an acceleration tower).
std::string acceleration_gcode(GcodeFlavor flavor, double print, double travel);

// The command that sets the firmware's pressure advance: M900 K (Marlin's
// linear advance), SET_PRESSURE_ADVANCE ADVANCE= (Klipper) or M572 D0 S
// (RepRapFirmware), from each firmware's public documentation.
std::string pressure_advance_gcode(GcodeFlavor flavor, double value);

// Writes the G-code of a sliced model, following docs/spec/GCODE_OUTPUT.md:
// the header line, the start G-code, every layer with the comments the Tisma
// preview reads (;LAYER_CHANGE, ;Z:, ;HEIGHT:, ;TYPE:, ;WIDTH:), the end
// G-code and a "; key = value" line per setting. Within a layer: walls from the
// inside out with the outer wall last, then bottom and top surfaces, bridges,
// gap fill and sparse infill, each group in nearest order with loops starting
// at their seam (order_paths). E follows the volume laid down (width x height x
// length over the filament's cross-section; a variable-width piece uses its
// mean width). Every island of a layer is finished before the next (nearest
// first). Travels follow TravelPlanner and retract when they leave the part or
// are longer than retraction.min_travel. A layer faster than
// cooling.min_layer_time is slowed down (plan_slowdown) and gets the fan at
// cooling.fan_max, even among the first layers; others get cooling.fan_min,
// and the first cooling.fan_off_layers layers none. Layers that stay shorter than the
// minimum time even at 1 mm/min are added to warnings, when given.
void write_gcode(std::ostream& out, const SlicedModel& model, const Settings& settings,
                 std::vector<Warning>* warnings = nullptr);

} // namespace tisma
