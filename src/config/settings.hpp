#pragma once

#include <array>
#include <filesystem>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace tisma {

class SettingsError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

enum class FillMode { Union, EvenOdd, FillHoles };
enum class GpuMode { Auto, Off };
enum class InfillPattern { Rectilinear, Grid, Gyroid };
enum class GcodeFlavor { Marlin, Klipper, RepRap };
enum class SeamMode { Aligned, Nearest, Random, Rear };

// A speed in mm/s, or a percentage of another speed.
struct Speed {
    double value = 0.0;
    bool percent = false;
    double of(double base) const { return percent ? base * value / 100.0 : value; }
};

// The engine's own settings format, documented in docs/CONFIG.md.
struct Settings {
    double layer_height = 0.2;
    double first_layer_height = 0.2;
    FillMode fill_mode = FillMode::Union;
    double gap_closing_radius = 0.049;
    double xy_offset = 0.0;
    double first_layer_shrink = 0.0;
    double simplify_tolerance = 0.01;
    int wall_count = 3;      // walls from each outline; 0: no walls, all infill
    double wall_width = 0.4; // preferred bead width w*
    SeamMode seam = SeamMode::Aligned;
    int top_layers = 3;      // solid layers under each top surface
    int bottom_layers = 3;   // solid layers over each bottom surface
    bool monotonic_top = true; // top surfaces: monotonic lines (true) or zig-zags (false)
    double infill_density = 20.0; // percent
    double infill_angle = 45.0;   // degrees
    InfillPattern infill_pattern = InfillPattern::Rectilinear;
    double infill_wall_overlap = 25.0; // percent of a bead that unjoined solid lines reach into the walls
    GpuMode gpu = GpuMode::Auto;

    // machine
    double nozzle_diameter = 0.4;   // mm
    double filament_diameter = 1.75; // mm
    bool relative_extrusion = false;
    GcodeFlavor flavor = GcodeFlavor::RepRap;
    std::string start_gcode, end_gcode;
    // Values for [name], {name} and {name[i]} in the start and end G-code, given
    // by the caller: a single value is a list of one. Numbers keep their JSON text.
    std::map<std::string, std::vector<std::string>> variables;
    // Motion limits, for the time estimate.
    double max_feed_x = 200.0, max_feed_y = 200.0, max_feed_z = 12.0, max_feed_e = 120.0; // mm/s
    double acceleration = 1000.0;        // mm/s2, moves that extrude
    double travel_acceleration = 1250.0; // mm/s2, travels and retractions
    double corner_speed = 10.0;          // mm/s through a sharp corner
    // The bed in XY, mm: a polygon models must lie on; empty: not checked (the
    // engine does not move models, and the bench models are centred on the origin).
    std::vector<std::array<double, 2>> bed;
    double max_height = 0.0; // mm; 0: not checked

    // speed, mm/s
    double wall_speed = 60.0;
    Speed outer_wall_speed{50.0, true}; // of wall_speed
    double infill_speed = 80.0;
    double solid_infill_speed = 80.0;
    double top_solid_infill_speed = 50.0;
    double travel_speed = 130.0;
    double first_layer_speed = 30.0;
    double max_speed = 80.0; // no extrusion goes faster

    // retraction
    double retract_length = 2.0;     // mm of filament
    double retract_speed = 40.0;     // mm/s
    double retract_lift = 0.0;       // mm of Z while travelling
    double retract_min_travel = 2.0; // mm: shorter travels do not retract

    // adhesion
    int skirt_loops = 1;
    double skirt_distance = 3.0; // mm from the part (or its brim)
    int skirt_layers = 1;
    double brim_width = 0.0; // mm; 0: no brim

    // support (docs/spec/BEHAVIOR.md, "Soportes")
    bool support = false;
    double support_angle = 0.0;  // degrees from the horizontal; 0: automatic (half a bead per layer)
    double support_z_gap = 0.2;  // mm between the support and the part above it
    double support_xy_gap = 0.6; // mm between the support and the part beside it

    // cooling
    double min_layer_time = 5.0;  // s: faster layers slow down (docs/spec/BEHAVIOR.md, "Enfriamiento")
    double min_print_speed = 10.0; // mm/s: the first stage slows extrusions down to this
    double fan_min = 35.0;         // %: the fan on ordinary layers
    double fan_max = 100.0;        // %: the fan on layers that had to slow down
    int fan_off_layers = 1;        // the first layers print with the fan off

    // Groups present in the file that the engine does not use yet.
    std::vector<std::string> ignored_groups;
};

// Every setting as "group.key" and its value, for the end of the G-code.
std::vector<std::pair<std::string, std::string>> describe(const Settings& settings);

// Parses settings JSON. Throws SettingsError naming the key on bad input.
Settings parse_settings(std::string_view json);

// Reads and parses a settings file. Throws SettingsError.
Settings load_settings(const std::filesystem::path& path);

} // namespace tisma
