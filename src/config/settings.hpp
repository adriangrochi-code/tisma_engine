#pragma once

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

    // speed, mm/s
    double wall_speed = 60.0;
    Speed outer_wall_speed{50.0, true}; // of wall_speed
    double infill_speed = 80.0;
    double solid_infill_speed = 80.0;
    double top_solid_infill_speed = 50.0;
    double travel_speed = 130.0;
    double first_layer_speed = 30.0;
    double max_speed = 80.0; // no extrusion goes faster

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
