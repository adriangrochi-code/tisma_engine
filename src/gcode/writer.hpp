#pragma once

#include "config/settings.hpp"
#include "gcode/print_time.hpp"
#include "gcode/template.hpp"
#include "report/warnings.hpp"
#include "slice/pipeline.hpp"

#include <ostream>
#include <optional>
#include <string>
#include <string_view>

namespace tisma {

// Replaces [name], {name} and {name[i]} in G-code text with the variables'
// values (docs/spec/GCODE_OUTPUT.md, first version). Unknown names and indices
// are left as they are.
std::string substitute_variables(std::string_view text, const std::map<std::string, std::vector<std::string>>& variables);

// v with `decimals` decimals as the G-code carries it: rounded as
// std::to_chars rounds (the exact binary value, ties to even), without a
// negative zero. Exposed for its test against std::to_chars.
std::string fixed_number(double v, int decimals);

// The command that sets the chamber's temperature: M141 S, or Klipper's
// SET_HEATER_TEMPERATURE for a heater named "chamber".
std::string chamber_gcode(GcodeFlavor flavor, double celsius);

// What the filament asks that the machine cannot give (docs/CATALOG.md,
// "Capacidades"): a nozzle or bed hotter than its maximum, a chamber it does
// not heat, an enclosure, a hardened nozzle. write_gcode adds them to its
// warnings.
std::vector<Warning> capability_warnings(const Settings& settings);

// No nozzle temperature anywhere and nothing in the start G-code that heats
// it (the spec team's BH6): the command line warns; the interface always
// gives a temperature.
std::optional<Warning> heating_warning(const Settings& settings);

// The speeds the melt limit (the lowest of machine.max_flow,
// extrusion.max_flow and speed.max_flow) slows down, by kind of extrusion,
// with their beads and the layer height (pellets.md, rule 6): one warning
// saying from how much to how much; none when no speed passes it.
std::optional<Warning> melt_limit_warning(const Settings& settings);

// The command that sets the accelerations: M204 P (printing) T (travel) in
// Marlin and RepRapFirmware, SET_VELOCITY_LIMIT ACCEL= in Klipper (one value
// for both), from each firmware's public documentation. Written after the
// start G-code, so the printer runs at the acceleration the time estimate
// assumes, and by a height change (an acceleration tower).
std::string acceleration_gcode(GcodeFlavor flavor, double print, double travel);

// The machine's limits as the firmware takes them (gcode_firmware.md, rule 2),
// one command per line: Marlin 2's M201, M203, M204 P R T, M205 X Y Z E (or J
// with a junction deviation) and M205 S T; Marlin 1's, with M204 S T;
// RepRapFirmware's M201, M203 and M566 (mm/min) and M204 P T. Nothing for
// the other flavors. Values not known (0) are left out.
std::string machine_limits_gcode(const Settings& settings);

// What the G-code templates of a print see (gcode_templates.md, rule 7):
// every setting by its name (layers.height...), a few by their usual names
// (layer_height, first_layer_height, nozzle_diameter, filament_diameter),
// machine.variables as lists, and the print's own: total_layer_count,
// num_extruders, initial_extruder, initial_tool, is_extruder_used,
// input_filename_base, first_layer_print_min, _max and _size, year,
// timestamp, and layer_num, layer_z and max_layer_z (the first layer's until
// a layer change).
TemplateContext template_context(const Settings& settings, const SlicedModel& model);

// What is wrong with the tool change's templates (gcode.toolchange, and the
// filament's end and start), rendered as for a change from the first
// extruder to the second; empty when they render. Mixed colours change at
// nearly every layer: this tells before slicing what the export would fail
// on (color_mixing.md, rule 13).
std::string toolchange_template_error(const Settings& settings);

// Whether G-code sets a nozzle, bed or chamber temperature (M104, M109, M140,
// M190, M141, M191, M116, or RepRapFirmware's G10 S): the start G-code that
// does is left to it (gcode_firmware.md, rule 1).
bool sets_temperature(std::string_view gcode);

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
// cooling.fan_max, even among the first layers; others get a fan by their
// time (docs/spec/features/cooling.md, rule 3), and the first
// cooling.fan_off_layers layers none. Layers that stay shorter than the
// minimum time even at 1 mm/min are added to warnings, when given. Every
// speed must be set (require_speeds): the writer has none of its own.
void write_gcode(std::ostream& out, const SlicedModel& model, const Settings& settings,
                 std::vector<Warning>* warnings = nullptr);

// The machine's limits the time estimate uses (print_time.md, rule 2): the
// feeds, accelerations and corner speed, and with machine.limits_usage other
// than ignore the axes' accelerations, jerks and the rest.
Kinematics machine_kinematics(const Settings& settings);

} // namespace tisma
