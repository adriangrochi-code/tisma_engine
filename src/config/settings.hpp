#pragma once

#include <filesystem>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace tisma {

class SettingsError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

enum class FillMode { Union, EvenOdd, FillHoles };
enum class GpuMode { Auto, Off };
enum class InfillPattern { Rectilinear, Grid };

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
    double infill_density = 20.0; // percent
    double infill_angle = 45.0;   // degrees
    InfillPattern infill_pattern = InfillPattern::Rectilinear;
    GpuMode gpu = GpuMode::Auto;

    // Groups present in the file that the engine does not use yet.
    std::vector<std::string> ignored_groups;
};

// Parses settings JSON. Throws SettingsError naming the key on bad input.
Settings parse_settings(std::string_view json);

// Reads and parses a settings file. Throws SettingsError.
Settings load_settings(const std::filesystem::path& path);

} // namespace tisma
