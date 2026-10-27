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
enum class InfillPattern { Rectilinear, Grid, Gyroid, Triangles, Cubic, AdaptiveCubic, SupportCubic, Lightning, Stars, AlignedRectilinear, Zigzag, Line, Honeycomb, Concentric, Hilbert, Archimedean, Octagram };
enum class GcodeFlavor { Marlin, Klipper, RepRap };
enum class SeamMode { Aligned, Nearest, Random, Rear };
enum class ScarfMode { Nowhere, Contours, Everywhere };
enum class SupportStyle { Grid, Organic };
enum class FuzzyMode { None, External, All };

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
    friend bool operator==(const Speed&, const Speed&) = default;
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
    // The scarf seam (docs/spec/features/walls_extra.md, rule 4): a loop starts
    // on a ramp up from the layer below instead of a step.
    ScarfMode scarf = ScarfMode::Nowhere;
    bool scarf_only_smooth = true;    // only loops without sharp corners
    double scarf_start_height = 0.0;  // percent of the layer the ramp starts at, over the layer below
    double scarf_length = 20.0;       // mm of ramp before the seam
    bool scarf_entire_loop = false;   // the ramp the whole way round
    double scarf_max_segment = 1.0;   // mm, the longest piece of the ramp
    bool scarf_inner = false;         // the inner walls too
    // The stretches of a wall wholly over the air, or over the support, are
    // overhang perimeters, at the bridges' speed and fan
    // (docs/spec/features/walls_extra.md, rule 8).
    bool detect_overhangs = true;
    // Vase mode (docs/spec/features/vase_mode.md): one wall that rises in a
    // spiral, a layer each turn, over flat bottom layers.
    bool vase = false;
    // Fuzzy skin (docs/spec/features/fuzzy_skin.md): the outer wall made rough.
    FuzzyMode fuzzy_mode = FuzzyMode::None;
    double fuzzy_thickness = 0.3;      // mm off the line, either way
    double fuzzy_point_distance = 0.8; // mm between points, on average
    // Arcs over overhangs held from one side (docs/spec/features/arc_overhangs.md).
    bool arcs = false;
    double arcs_overlap = 10.0;     // percent of the width each arc lies on the one before
    double arcs_speed = 5.0;        // mm/s; 0: the bridges' (speed.solid_infill)
    double arcs_max_distance = 0.0; // mm from the arcs' centre; 0: no limit
    // The stability review (docs/spec/features/stability.md): measured with
    // its calibration piece; 0 is not measured yet.
    double review_nozzle_force = 0.0;   // N, the nozzle's push on the layer it prints
    double review_bed_adhesion = 0.0;   // N/mm2 the hold on the bed bears, for this bed and material
    double review_max_deflection = 0.1; // mm a slender part's tip may bend
    int top_layers = 3;      // solid layers under each top surface
    int bottom_layers = 3;   // solid layers over each bottom surface
    double top_min_thickness = 0.0;    // mm: more top solid layers until they add up to this (0: off)
    double bottom_min_thickness = 0.0; // mm: likewise over the bottom surfaces
    bool monotonic_top = true; // top surfaces: monotonic lines (true) or zig-zags (false)
    double infill_density = 20.0; // percent
    double infill_angle = 45.0;   // degrees
    InfillPattern infill_pattern = InfillPattern::Rectilinear;
    double infill_wall_overlap = 25.0; // percent of a bead that unjoined solid lines reach into the walls
    int infill_solid_every = 0;        // layers N, 2N... all solid infill (0: never)
    double infill_solid_below_area = 70.0; // mm2: an infill zone smaller than this is solid (0: never)
    // Combining the sparse infill of several layers (docs/INFILL.md).
    int infill_combine_layers = 1;            // 1: each layer its own
    bool infill_combine_auto = false;         // as many as fit in the height below
    Speed infill_combine_max_height{100.0, true}; // mm, or percent of the nozzle; never past the nozzle
    // Denser sparse infill right under the top surfaces (docs/INFILL.md).
    // Wavy nonplanar infill (docs/NONPLANAR_INFILL.md, N1).
    bool infill_nonplanar = false;
    double infill_nonplanar_amplitude = 250.0; // percent of the layer height, trough to crest
    double infill_nonplanar_wavelength = 2.0;  // mm
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
    // Motion limits, for the time estimate. Like every speed, from the
    // printer's profile; 0: not known, not applied.
    double max_feed_x = 0.0, max_feed_y = 0.0, max_feed_z = 0.0, max_feed_e = 0.0; // mm/s
    double acceleration = 1000.0;        // mm/s2, moves that extrude
    double travel_acceleration = 1250.0; // mm/s2, travels and retractions
    double corner_speed = 0.0;           // mm/s through a sharp corner, from the printer's profile; 0: not set
    // Where the command line puts the model (docs/spec/features/placement.md,
    // rules 2 and 4): with a bed, the centre of its box goes to `center`, or
    // to the centre of the bed's box; without one, it stays where it is. The
    // interface places objects itself and does not use these.
    bool placement_arrange = true;
    std::optional<std::array<double, 2>> placement_center;
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

    // speed, mm/s. No speed has a value of its own: they come from the
    // printer's profile (owner's decision, 2026-10-26); 0: not set.
    double wall_speed = 0.0;
    Speed outer_wall_speed; // in mm/s, or a share of wall_speed
    double infill_speed = 0.0;
    double solid_infill_speed = 0.0;
    double top_solid_infill_speed = 0.0;
    double support_speed = 0.0;
    double travel_speed = 0.0;
    double first_layer_speed = 0.0;
    double max_speed = 0.0; // no extrusion goes faster; 0: no cap

    // retraction
    double retract_length = 2.0;     // mm of filament
    double retract_speed = 0.0;      // mm/s, from the printer's profile; 0: not set
    double retract_lift = 0.0;       // mm of Z while travelling
    double retract_min_travel = 2.0; // mm: shorter travels do not retract

    // adhesion
    int skirt_loops = 1;
    double skirt_distance = 6.0; // mm from the part (or its brim; supports_skirt.md)
    int skirt_layers = 1;
    double brim_width = 0.0; // mm; 0: no brim

    // support (docs/spec/BEHAVIOR.md, "Soportes")
    bool support = false;
    bool support_automatic = true; // false: support only under faces painted to have it (docs/PAINTING.md)
    double support_angle = 0.0;  // degrees from the horizontal; 0: automatic (half a bead per layer)
    double support_z_gap = 0.2;  // mm between the support and the part above it
    // Between the support and the part beside it: mm, or percent of the
    // wall width (supports_skirt.md, rule 2: 50 %).
    Speed support_xy_gap{50.0, true};
    double support_spacing = 2.5;    // mm of free space between the base's lines (a bead's spacing more between centres)
    double support_line_angle = 0.0; // degrees, the base's lines; the interface's at 90 to them
    // Organic (tree) supports (docs/spec/features/supports_more.md, rules 15
    // to 17): round branches that join on their way down to the bed.
    SupportStyle support_style = SupportStyle::Grid;
    double tree_angle = 40.0;               // degrees, the steepest a branch leans to keep off the part
    double tree_angle_preferred = 25.0;     // degrees, how a branch leans to join another
    double tree_branch_diameter = 2.0;      // mm, the thinnest branch
    double tree_branch_diameter_angle = 5.0; // degrees the branches widen by on their way down
    double tree_double_wall_diameter = 3.0; // mm: a branch wider than this gets two walls; 0: never
    double tree_branch_distance = 1.0;      // how far apart the tips are, as a factor
    double tree_tip_diameter = 0.8;         // mm
    double tree_top_rate = 15.0;            // percent: how close the tips are under the part
    int support_interface_layers = 3; // dense layers right under the part

    // cooling
    double min_layer_time = 5.0;  // s: faster layers slow down (docs/spec/BEHAVIOR.md, "Enfriamiento")
    double min_print_speed = 0.0;  // mm/s, from the printer's profile: the first stage slows extrusions down to this
    // The fan by layer time (docs/spec/features/cooling.md, rule 3): fan_max
    // at the minimum layer time or less, down to fan_min at fan_below_time,
    // and off from there (fan_min with fan_always).
    double fan_min = 35.0;         // %
    double fan_max = 100.0;        // %: also every layer that had to slow down
    double fan_below_time = 60.0;  // s: layers this long or longer print without fan
    bool fan_always = false;       // the fan never below fan_min
    double fan_bridges = 100.0;    // %: the fan while bridging
    int fan_off_layers = 3;        // the first layers print with the fan off
    // The outer walls keep their speed while the rest slows down for the
    // minimum layer time, when that is enough (docs/GLOSS.md): an even sheen.
    bool keep_outer_speed = true;

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

    // gcode
    // Points that move a path less than this go (docs/spec/features/gcode_extras.md); 0: every point is written.
    double gcode_resolution = 0.0125;

    // changes from a height up, sorted by z
    std::vector<HeightChange> changes;

    // Groups present in the file that the engine does not use yet.
    std::vector<std::string> ignored_groups;
};

// The speeds (every setting in mm/s) come only from the printer's profile
// (owner's decision, 2026-10-26). The keys of those not set, in the order of
// docs/CONFIG.md.
std::vector<std::string> missing_speeds(const Settings& settings);

// Only when the printer's profile has none (a printer made from nothing, or
// one the catalog knows no speeds of): the speeds of a stock Creality Ender-3,
// the commonest and safest machine (owner's decision, 2026-10-26;
// docs/CONFIG.md, "Velocidades de respaldo"), into the speeds not set. A
// speed already set stays. Returns the keys filled.
std::vector<std::string> fill_missing_speeds(Settings& settings);

// Throws SettingsError naming the speeds not set, if any: the G-code needs
// all of them.
void require_speeds(const Settings& settings);

// The fastest an extrusion is asked to go, mm/s: the fastest extrusion speed,
// capped by speed.max when that is set.
double fastest_extrusion(const Settings& settings);

// Every setting as "group.key" and its value, for the end of the G-code.
std::vector<std::pair<std::string, std::string>> describe(const Settings& settings);

// The settings as JSON in the format of docs/CONFIG.md, every key written, in
// the order of that document: parse_settings(settings_json(s)) gives s back
// (machine.variables come back as text, as they were stored). Groups the
// engine ignored are not written.
std::string settings_json(const Settings& settings);

// The name of an infill pattern in the settings ("rectilinear", "grid"...).
const char* infill_pattern_name(InfillPattern pattern);

// Parses settings JSON. Throws SettingsError naming the key on bad input.
Settings parse_settings(std::string_view json);

// Whether a JSON text nests at most max_depth arrays and objects. Settings,
// profiles, projects and load cases are untrusted files: a deeper one would
// overflow the stack once written back or compared, so it is refused first.
bool json_depth_ok(std::string_view json, int max_depth = 64);

// Reads and parses a settings file, then checks it (check_settings). Throws
// SettingsError.
Settings load_settings(const std::filesystem::path& path);

// What a whole set of settings cannot print, whatever the model: layers
// thicker than the nozzle (or machine.max_layer_height) or thinner than
// machine.min_layer_height, lines narrower than a quarter of the nozzle or
// wider than four nozzles. Profiles hold only part of the settings, so this
// runs on the whole set, before slicing. Throws SettingsError.
void check_settings(const Settings& settings);

} // namespace tisma
