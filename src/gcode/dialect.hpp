#pragma once

#include "config/settings.hpp"

namespace tisma {

// How a flavor's G-code differs from RepRap's in what the engine writes
// (gcode_extras.md, rule 6, test X5; gcode_firmware.md). Repetier and
// Smoothie write as RepRap.
struct Dialect {
    enum class Fan { M106, M106OffS0, OnOff, M106P };
    // How the machine's limits are written (gcode_firmware.md, rule 2): not at
    // all, Marlin 2's M201 M203 M204 P R T M205, Marlin 1's (M204 S T), or
    // RepRapFirmware's (M203 and M566 in mm/min).
    enum class Limits { None, Marlin2, Marlin1, RepRapFirmware };
    // How the start waits for the nozzle (rule 1): M109, M116 after the set
    // temperature, or not at all.
    enum class Wait { M109, M116, None };

    char e_axis = 'E';         // 'A' on Mach3 and Machinekit; 0: no E at all (no_extrusion)
    bool resets_e = true;      // absolute E restarts with G92 at each layer
    bool e_mode = true;        // M82 or M83 is written; without it, E is absolute
    char temperature = 'S';    // the letter of M104 and M140: P on Mach3 and Machinekit
    bool tool_on_m104 = false; // M104 S<t> T0 (MakerWare, Sailfish)
    Fan fan = Fan::M106;       // M106 S and M107; M106 S0 for off (Teacup); M126 and M127; M106 P
    bool progress = false;     // M73 P<percent> at each layer (MakerWare, Sailfish)
    bool extruder_on_off = false; // M101 before extruding, M103 after (MakerWare)
    bool ends_m2 = false;      // M2 at the very end (Machinekit)
    // M204 and pressure advance: firmwares that know them. The old dialects
    // get neither (own choice: the spec does not measure them there).
    bool accelerations = true;
    Limits limits = Limits::None;
    bool m204_print_travel = false; // the kinds' accelerations as M204 P and T; else M204 S
    Wait wait = Wait::M109;
    bool g10 = false;               // the nozzle's temperature as G10 S<t> P0 (RepRapFirmware)
    bool g92_relative = false;      // relative E restarts with G92 E0 at each layer (Marlin, rule 3)
    bool silent_progress = true;    // M73 Q S for the silent mode too (not on generic RepRap)
};

inline Dialect dialect_of(GcodeFlavor flavor) {
    Dialect d;
    switch (flavor) {
    case GcodeFlavor::Marlin:
        d.limits = Dialect::Limits::Marlin2;
        d.m204_print_travel = true;
        d.g92_relative = true;
        break;
    case GcodeFlavor::MarlinLegacy:
        d.limits = Dialect::Limits::Marlin1;
        d.g92_relative = true;
        break;
    case GcodeFlavor::RepRapFirmware:
        d.limits = Dialect::Limits::RepRapFirmware;
        d.m204_print_travel = true;
        d.wait = Dialect::Wait::M116;
        d.g10 = true;
        break;
    case GcodeFlavor::Klipper:
        break;
    case GcodeFlavor::RepRap:
    case GcodeFlavor::Repetier:
    case GcodeFlavor::Smoothie:
        d.silent_progress = false;
        break;
    case GcodeFlavor::Teacup:
        d.fan = Dialect::Fan::M106OffS0;
        d.accelerations = false;
        d.wait = Dialect::Wait::M116;
        d.silent_progress = false;
        break;
    case GcodeFlavor::MakerWare:
    case GcodeFlavor::Sailfish:
        d.resets_e = false;
        d.e_mode = false;
        d.tool_on_m104 = true;
        d.fan = Dialect::Fan::OnOff;
        d.progress = true;
        d.extruder_on_off = flavor == GcodeFlavor::MakerWare;
        d.accelerations = false;
        d.wait = Dialect::Wait::None;
        d.silent_progress = false;
        break;
    case GcodeFlavor::Mach3:
    case GcodeFlavor::Machinekit:
        d.e_axis = 'A';
        d.resets_e = flavor == GcodeFlavor::Machinekit;
        d.temperature = 'P';
        d.fan = Dialect::Fan::M106P;
        d.ends_m2 = flavor == GcodeFlavor::Machinekit;
        d.accelerations = false;
        d.silent_progress = false;
        break;
    case GcodeFlavor::NoExtrusion:
        d.e_axis = 0;
        d.resets_e = false;
        d.accelerations = false;
        d.silent_progress = false;
        break;
    }
    return d;
}

// Whether E is relative in the G-code: as asked, unless the dialect has no
// M82 and M83 (MakerWare and Sailfish), where it is absolute.
inline bool relative_e(const Settings& s) { return s.relative_extrusion && dialect_of(s.flavor).e_mode; }

} // namespace tisma
