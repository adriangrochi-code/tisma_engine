#pragma once

#include <cstddef>
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
    double max_feed = 0.0;        // the fastest move, mm/s
    double filament_area = 1.0;   // mm2
    double retract_length = 0.0;  // mm of filament
    bool relative_extrusion = false;
};

// The part's footprint grown by the skirt, the brim and a bead of slack, the
// bed's box when one is set, the top layer and the speeds.
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
