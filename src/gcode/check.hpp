#pragma once

#include "slice/curved.hpp"

#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace tisma {

struct SlicedModel;
struct Settings;

// What the engine's own moves may do, from the part and the settings (not
// from the toolpaths the writer used, so a writer mistake shows).
struct GcodeLimits {
    double min_x = 0.0, min_y = 0.0, max_x = 0.0, max_y = 0.0; // where moves may go, mm
    double max_z = 0.0;           // the top layer plus the travel lift, mm
    double min_z = 0.0;           // below the bed only with a negative machine.z_offset
    double max_feed = 0.0;        // the fastest move, mm/s
    double filament_area = 1.0;   // mm2
    // extrusion.flow_by_speed (pellets.md, rule 5): each extrusion's E by
    // its F.
    std::vector<std::array<double, 2>> flow_by_speed;
    BeadSection section = BeadSection::Rounded; // extrusion.section
    double retract_length = 0.0;  // mm of filament
    // retraction_travel.md: the filament pushed past each retraction undone
    // (rule 1), a retraction partly while the nozzle wipes (rule 3), and G10
    // and G11 in place of E (rule 7).
    double restart_extra = 0.0;
    bool wipe = false;
    bool firmware_retraction = false;
    // Several extruders (multi_extruder.md, rule 5): a tool change's
    // retraction and the extra when the tool comes back, by extruder from 0;
    // empty with one extruder.
    std::vector<double> toolchange_length, toolchange_restart_extra;
    // The wipe tower without sparse layers (rule 11): its extrusions may lie
    // under the layer's Z, as thick as the layer.
    bool tower_below = false;
    // A single-extruder MMU (rule 15): the change's G-code unloads and loads
    // the filament; the new one comes with its tip at the nozzle.
    bool mmu = false;
    double flow = 1.0;            // the share of width x height x length laid down (extrusion.flow / 100)
    bool relative_extrusion = false;
    // The dialect (gcode_extras.md, rule 6): the extruder's axis ('A' on Mach3
    // and Machinekit; 0: none, no_extrusion) and the letter of M106's value.
    char e_axis = 'E';
    char fan_parameter = 'S';
    // Z anti-aliasing (docs/ZAA.md): how far an extrusion may leave its layer's
    // Z, down and up, as shares of the layer height; its bead is as thick as
    // what is left down to the layer's bottom.
    double zaa_below = 0.0, zaa_above = 0.0;
    // Wavy infill (docs/NONPLANAR_INFILL.md): how far, in mm, an extrusion in
    // a ;NONPLANAR block may leave its layer's Z either way; there each
    // move's ;HEIGHT is its bead's own thickness.
    double wave = 0.0;
    // Curved layers (nonplanar.md): each extrusion at its layer's flat Z
    // bent by the field, its bead as thick as the layer by J there.
    std::optional<CurvedField> curved;
    double z_offset = 0.0; // machine.z_offset, which the field does not bend
    // A belt printer (belt_printer.md): the moves are on the machine's axes,
    // the belt and the gantry, and go back to the slicing planes to be checked.
    bool belt = false;
    double belt_angle = 45.0; // degrees
    bool belt_is_z = true;
    // The scarf seam (docs/spec/features/walls_extra.md, rule 4) and vase
    // mode's turns: how far below its layer's Z an extrusion in a ;NONPLANAR
    // block may go, as a share of the layer's height (they start down at the
    // layer below).
    double scarf = 0.0;
    // From the height changes of the settings: the values for the layers whose
    // top is above z, in order of z.
    struct Change {
        double z = 0.0;
        double retract_length = 0.0;
        double flow = 1.0;
    };
    std::vector<Change> changes;
};

// The part's footprint grown by the skirt, the brim and a bead of slack, the
// bed's box when one is set, the top layer, the speeds (a speed change raises
// the fastest extrusion), the flow and the retraction, with their changes by
// height.
GcodeLimits gcode_limits(const SlicedModel& model, const Settings& settings);

struct GcodeProblem {
    std::size_t line = 0; // 1-based
    std::string message;
};

struct GcodeStats {
    std::size_t lines = 0;
    std::size_t layers = 0;
    std::size_t extrusions = 0;  // moves that extrude
    std::size_t travels = 0;     // moves that do not
    std::size_t retractions = 0;
    double filament = 0.0;       // mm of filament pushed (net of retractions)
    double extruded_path = 0.0;  // mm of extruding moves
    double travel_path = 0.0;    // mm of travels
    double max_z = 0.0;
};

struct GcodeCheck {
    std::vector<GcodeProblem> problems; // the first kMaxProblems
    std::size_t problem_count = 0;      // all of them
    GcodeStats stats;
    bool ok() const { return problem_count == 0; }
};

inline constexpr std::size_t kMaxProblems = 50;

// Reads G-code back and checks what the engine writes (docs/GCODE_CHECK.md):
// every line readable, every number finite, the engine's moves inside the
// limits and never faster than allowed, layers going up and extruding at
// their height, extrusion that matches each move's length, width and layer
// height, retractions that are undone before extruding again, the fan
// within range. The user's start and end G-code (the ";TYPE:Custom" blocks)
// only update the state (positioning and extrusion modes, G92, homing): they
// are not judged. Own design; see PROVENANCE.md.
GcodeCheck check_gcode(std::string_view text, const GcodeLimits& limits);

} // namespace tisma
