#pragma once

#include <array>
#include <filesystem>
#include <map>
#include <optional>
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
enum class InfillPattern { Rectilinear, Grid, Gyroid, Triangles, Cubic, AdaptiveCubic, SupportCubic };
enum class GcodeFlavor { Marlin, Klipper, RepRap };
enum class SeamMode { Aligned, Nearest, Random, Rear };

// Values that change from a height up (calibration towers, docs/CONFIG.md,
// "changes"): each set value takes effect from the first layer whose top is
// above z and stays until another change sets it again.
struct HeightChange {
    double z = 0.0;                        // mm
    std::optional<double> temperature;     // nozzle, C (M104)
    std::optional<double> bed_temperature; // C (M140)
    std::optional<double> fan;             // %, replaces cooling.fan_min and fan_max
    std::optional<double> retract_length;  // mm of filament
    std::optional<double> retract_speed;   // mm/s
    std::optional<double> flow;            // %, replaces extrusion.flow
    std::optional<double> speed;           // %, of every extrusion's feed
    std::optional<double> pressure_advance; // the firmware's pressure advance (0 included)
    std::optional<double> volumetric_flow; // mm3/s: every extrusion runs at this flow, past every cap (a flow tower)
    std::optional<double> acceleration;    // mm/s2, written in the flavor's command (M204, SET_VELOCITY_LIMIT)
    std::string gcode;                     // G-code put at that layer's start, with variables
};

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
    // Layer heights by zone (docs/LOCAL_LAYERS.md): the height of the fine
    // sub-layers where the part changes, layer_height elsewhere; 0: off.
    double detail_height = 0.0;
    double detail_tolerance = 0.05; // mm of step accepted
    double detail_margin = 0.0;     // mm the fine zone grows around the detail; 0: three line widths
    // Variable layer height by Z (docs/VARIABLE_LAYERS.md): (z above the bed,
    // height) steps, each height from its z up, sorted by z; empty: none.
    std::vector<std::array<double, 2>> layer_heights;
    // Adaptive by slope: the highest step a sloping surface may leave, mm
    // (Dolenc and Mäkelä 1994); 0: off.
    double cusp_height = 0.0;
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
    // Combining the sparse infill of several layers (docs/INFILL.md).
    int infill_combine_layers = 1;            // 1: each layer its own
    bool infill_combine_auto = false;         // as many as fit in the height below
    Speed infill_combine_max_height{100.0, true}; // mm, or percent of the nozzle; never past the nozzle
    // Denser sparse infill right under the top surfaces (docs/INFILL.md).
    bool infill_dense_under_top = false;
    double infill_dense_density = 40.0; // percent
    // Stress-guided infill (docs/INFILL.md): up to this density where the
    // part works hardest under the load case given to slice; 0: off.
    double infill_stress_density = 0.0; // percent
    GpuMode gpu = GpuMode::Auto;
    // Z anti-aliasing (docs/ZAA.md, docs/spec/features/z_antialiasing.md).
    bool zaa = false;
    double zaa_min_thickness = 25.0; // percent of the layer height an extrusion keeps at least
    bool zaa_raise = false;          // also up to the surface above the top, half a layer at most

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
    // The layer heights the machine prints, mm; 0: not known (then 0.04 mm and
    // the nozzle diameter). Variable layers keep within them.
    double min_layer_height = 0.0;
    double max_layer_height = 0.0;
    // What the machine can do (docs/CATALOG.md, "Capacidades"); 0 or false:
    // not known. The writer keeps to the flow and heats the chamber; the
    // checks warn when a filament asks for more.
    double machine_max_flow = 0.0;  // mm3/s the hot end melts
    double max_nozzle_temp = 0.0;   // C
    double max_bed_temp = 0.0;      // C
    double max_chamber_temp = 0.0;  // C of an actively heated chamber; 0: none
    bool enclosed = false;
    bool hardened_nozzle = false;   // takes abrasive filaments

    // speed, mm/s
    double wall_speed = 60.0;
    Speed outer_wall_speed{50.0, true}; // of wall_speed
    double infill_speed = 80.0;
    double solid_infill_speed = 80.0;
    double top_solid_infill_speed = 50.0;
    double support_speed = 80.0;
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
    double support_spacing = 2.5;    // mm between support lines
    int support_interface_layers = 3; // dense layers right under the part

    // cooling
    double min_layer_time = 5.0;  // s: faster layers slow down (docs/spec/BEHAVIOR.md, "Enfriamiento")
    double min_print_speed = 10.0; // mm/s: the first stage slows extrusions down to this
    double fan_min = 35.0;         // %: the fan on ordinary layers
    double fan_max = 100.0;        // %: the fan on layers that had to slow down
    int fan_off_layers = 1;        // the first layers print with the fan off

    // compensation (docs/CALIBRATION.md): the model is scaled around the centre
    // of its footprint and its bottom before slicing; holes grow by hole_offset.
    double scale_x = 100.0, scale_y = 100.0, scale_z = 100.0; // %
    double hole_offset = 0.0;                                 // mm, on the radius; negative shrinks holes

    // extrusion
    double flow = 100.0; // %, of the volume every extrusion lays down
    // The firmware's pressure advance, written at the start in the flavor's
    // command (M900 K, SET_PRESSURE_ADVANCE, M572); 0: not written.
    double pressure_advance = 0.0;
    double filament_max_flow = 0.0;    // mm3/s the filament takes; 0: no limit of its own
    double chamber_temperature = 0.0;  // C the filament wants in the chamber; 0: none
    bool abrasive = false;             // wears a brass nozzle
    bool needs_enclosure = false;

    // changes from a height up, sorted by z
    std::vector<HeightChange> changes;

    // Groups present in the file that the engine does not use yet.
    std::vector<std::string> ignored_groups;
};

// Every setting as "group.key" and its value, for the end of the G-code.
std::vector<std::pair<std::string, std::string>> describe(const Settings& settings);

// The settings as JSON in the format of docs/CONFIG.md, every key written, in
// the order of that document: parse_settings(settings_json(s)) gives s back
// (machine.variables come back as text, as they were stored). Groups the
// engine ignored are not written.
std::string settings_json(const Settings& settings);

// Parses settings JSON. Throws SettingsError naming the key on bad input.
Settings parse_settings(std::string_view json);

// Reads and parses a settings file. Throws SettingsError.
Settings load_settings(const std::filesystem::path& path);

} // namespace tisma
