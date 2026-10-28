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
// The firmware the G-code is for. Past Marlin, Klipper and RepRap, the old
// dialects of gcode_extras.md (rule 6): Repetier and Smoothie as RepRap, and
// how the others differ in src/gcode/dialect.hpp.
enum class GcodeFlavor {
    Marlin,
    Klipper,
    RepRap,
    MarlinLegacy,   // Marlin 1 (gcode_firmware.md)
    RepRapFirmware, // Duet's (gcode_firmware.md)
    Repetier,
    Smoothie,
    Teacup,
    MakerWare,
    Sailfish,
    Mach3,
    Machinekit,
    NoExtrusion
};
enum class SeamMode { Aligned, Nearest, Random, Rear };
// Where the brim goes (skirt_brim_raft.md, rules 5 to 7) and the draft shield (rule 4).
enum class BrimType { None, Outer, Inner, OuterAndInner };
enum class DraftShield { Disabled, Limited, Enabled };
// What the machine's limits are for (gcode_firmware.md, rule 2): written at
// the start and used for the time, used for the time only, or neither.
enum class LimitsUsage { TimeOnly, Emit, Ignore };
enum class ScarfMode { Nowhere, Contours, Everywhere };
enum class SupportStyle { Grid, Organic };
enum class FuzzyMode { None, External, All };

// How the walls are made (walls_extra.md, rule 1): beads of variable width
// that fill the region (Kuipers et al. 2020), or loops of one width with gap
// fill between them.
enum class WallGenerator { Variable, Fixed };
// How the G-code names each object (gcode_firmware.md, rule 4).
enum class ObjectLabels { None, OctoPrint, Firmware };
// Where a top surface keeps only its outer wall (walls_extra.md, rule 10).
enum class TopOneWall { None, Top, Topmost };

// Which surfaces are ironed (ironing.md): every top surface, only the highest
// of each part, or every solid layer.
enum class IroningSurfaces { Top, Topmost, Solid };
// Curved layers (docs/spec/features/nonplanar.md, nonplanar_modes.md).
enum class CurvedMode { Disabled, Wave, Conical, Overhangs, Stress, Surface, Automatic };
enum class CurvedPattern { Egg, Ridges, Twisted };
enum class CurvedFlow { Preserve, Uniform, Off };
// What the printer is: one that extrudes filament, or a resin printer with
// a display (resin.md).
enum class MachineTechnology { Fdm, Msla };

// The most extruders a machine has: as many as the colour channel of the
// painting tells apart (kPaintMaxMark, docs/PAINTING.md).
inline constexpr int kMaxExtruders = 16;

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
    std::optional<double> exposure;        // s, a resin printer's normal layers from there (the exposure tower)
    std::string gcode;                     // G-code put at that layer's start, with variables
    // A colour change or a pause at that height (gcode_templates.md, rule
    // 13): gcode.color_change or gcode.pause at the layer's start.
    bool color_change = false;
    bool pause = false;
};

// A speed in mm/s, or a percentage of another speed.
struct Speed {
    double value = 0.0;
    bool percent = false;
    double of(double base) const { return percent ? base * value / 100.0 : value; }
    friend bool operator==(const Speed&, const Speed&) = default;
};

// The engine's own settings format, documented in docs/CONFIG.md.
// When a calibration's result was applied (assistant.md, rule 22;
// settings_panel.md, rule 38): the date (YYYY-MM-DD), the nozzle it was done
// with, and for a filament's, on which printer.
struct CalibrationDate {
    std::string date;
    double nozzle = 0.0;
    std::string printer;
    friend bool operator==(const CalibrationDate&, const CalibrationDate&) = default;
};

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
    WallGenerator wall_generator = WallGenerator::Variable;
    bool wall_gap_fill = true; // with fixed walls, fill what is left between them
    // docs/spec/features/walls_extra.md: the inner walls' seams staggered back
    // a bead per level (rule 3); only the outer wall over a top surface (rule
    // 10) or on the first layer (rule 11). extra_if_needed (rule 6) and
    // avoid_curled_overhangs (rule 9) are read and kept: Tisma Slicer added no
    // wall on the spec's parts, and the second was not measured.
    bool staggered_inner_seams = false;
    TopOneWall top_one_wall = TopOneWall::None;
    bool first_layer_one_wall = false;
    bool extra_if_needed = true;
    bool extra_on_overhangs = false; // loops fill an overhang held from one side (rule 5)
    bool avoid_curled_overhangs = false;
    // Thin walls (docs/spec/features/perimeters.md, rule 3), mm or percent of
    // machine.nozzle_diameter: a wall thinner than min_line_width is printed
    // with a bead that wide, centred; thinner than min_detail, not at all.
    Speed wall_min_line_width{85.0, true};
    Speed wall_min_detail{25.0, true};
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
    double arcs_speed = 5.0;        // mm/s; 0: the bridges' (speed.bridges)
    double arcs_max_distance = 0.0; // mm from the arcs' centre; 0: no limit

    // ironing (ironing.md): after a top surface, the nozzle goes over it again
    // in lines `spacing` apart, with `flow` % of a layer's material per mm of
    // that spacing; the spec's speed, not the printer's.
    bool ironing = false;
    IroningSurfaces ironing_surfaces = IroningSurfaces::Top;
    double ironing_flow = 15.0;   // %
    double ironing_spacing = 0.1; // mm
    double ironing_speed = 15.0;  // mm/s
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
    // Where one extruder's part rests on another's, each keeps its own top
    // and bottom solid layers, as if apart (multi_extruder.md, rule 16).
    bool interface_shells = false;
    // Where two extruders' parts touch, beams of each go into the other
    // (multi_extruder.md, rule 17): how wide (mm), how many layers before
    // they turn 90 degrees, at what angle, how far from the border between
    // them and how far from the outside they stay (in beam widths).
    bool interlock_beams = false;
    double interlock_width = 0.8;
    int interlock_layers = 2;
    double interlock_orientation = 22.5;
    int interlock_depth = 2, interlock_avoidance = 2;
    // Painted regions of a colour (rule 18): their widest and how deep they
    // interlock, mm (read; the colour channel is not sliced yet).
    double paint_max_region_width = 0.0, paint_interlocking_depth = 0.0;
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
    // Interlocking waves (interlock_infill.md): the sparse infill on the egg
    // crate a · sin(2πX/λ) · sin(2πY/λ) in the bed's X and Y, the same in
    // every layer; a in percent of the layer height, λ in mm.
    bool infill_waves = false;
    double infill_wave_amplitude = 50.0;
    double infill_wave_length = 10.0;
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
    // Each extruder's nozzle, from the first (multimaterial.md, rule 1): a
    // list shorter than the extruders repeats its last value; empty: all
    // nozzle_diameter, which is the first's.
    std::vector<double> nozzle_diameters;
    double z_offset = 0.0;          // mm added to every Z of the part (gcode_extras.md, rule 5)
    double filament_diameter = 1.75; // mm
    bool relative_extrusion = false;
    GcodeFlavor flavor = GcodeFlavor::RepRap;
    std::string start_gcode, end_gcode;
    // Not a setting: the input file's name without its folder and extension,
    // for the templates' input_filename_base (gcode_templates.md, rule 7).
    std::string input_name;
    // Values for [name], {name} and {name[i]} in the start and end G-code, given
    // by the caller: a single value is a list of one. Numbers keep their JSON text.
    std::map<std::string, std::vector<std::string>> variables;
    // Motion limits, for the time estimate. Like every speed, from the
    // printer's profile; 0: not known, not applied.
    double max_feed_x = 0.0, max_feed_y = 0.0, max_feed_z = 0.0, max_feed_e = 0.0; // mm/s
    double acceleration = 1000.0;        // mm/s2, moves that extrude
    double travel_acceleration = 1250.0; // mm/s2, travels and retractions
    double corner_speed = 0.0;           // mm/s through a sharp corner, from the printer's profile; 0: not set
    // The rest of the machine's limits (gcode_firmware.md, rule 2;
    // print_time.md): the most each axis accelerates (M201), the retractions'
    // acceleration (M204 R; 0: the travels'), each axis' jerk (M205 X Y Z E)
    // or the junction deviation (M205 J, which takes its place), and the
    // slowest feeds extruding and travelling (M205 S T). 0: not known, not used.
    double max_accel_x = 0.0, max_accel_y = 0.0, max_accel_z = 0.0, max_accel_e = 0.0; // mm/s2
    double retract_acceleration = 0.0;                                                  // mm/s2
    double jerk_x = 0.0, jerk_y = 0.0, jerk_z = 0.0, jerk_e = 0.0;                     // mm/s
    double junction_deviation = 0.0;                                                    // mm
    double min_extrude_feed = 0.0, min_travel_feed = 0.0;                               // mm/s
    LimitsUsage limits_usage = LimitsUsage::TimeOnly;
    // A printer with a silent mode (gcode_extras.md, rule 7): a second time
    // estimate with the limits of that mode, by name (those of kLimitNames);
    // one not given is the normal one.
    bool silent_mode = false;
    std::map<std::string, double> silent_limits;
    // Where the command line puts the model (docs/spec/features/placement.md,
    // rules 2 and 4): with a bed, the centre of its box goes to `center`, or
    // to the centre of the bed's box; without one, it stays where it is. The
    // interface places objects itself and does not use these.
    bool placement_arrange = true;
    std::optional<std::array<double, 2>> placement_center;
    double placement_spacing = 6.0; // mm between the boxes of copies (placement.md, rule 2)
    // What the command line does to the model before it places it
    // (docs/spec/features/model_tools.md, rules 11 to 14), about the centre of
    // its box: scale (or fit in a box), turn about X, then Y, then Z, cut
    // with a horizontal plane into two objects, split into its loose pieces.
    // The interface does these with its own tools.
    double transform_scale = 1.0;
    std::optional<std::array<double, 3>> transform_scale_to_fit; // mm
    std::array<double, 3> transform_rotate{0.0, 0.0, 0.0};       // degrees
    std::optional<double> transform_cut_z;                       // mm above the bed
    bool transform_split = false;
    // Object by object (docs/spec/features/sequential.md): each whole before
    // the next, the head within clearance_radius of the nozzle and the X
    // axis clearance_height above its tip.
    bool sequential = false;
    double clearance_radius = 20.0, clearance_height = 20.0;
    // Several extruders (docs/spec/features/multi_extruder.md): how many the
    // machine has. Each object prints with its own (MeshObject::extruder),
    // and a kind of extrusion with the one given here, from 1 (rule 2); 0:
    // the object's, and for the support whichever extruder is printing.
    int extruders = 1;
    std::vector<std::array<double, 2>> extruder_offset; // mm, each nozzle from the first, by extruder (rule 8)
    int extruder_walls = 0, extruder_infill = 0, extruder_solid_infill = 0, extruder_support = 0,
        extruder_support_interface = 0;
    int bed_temperature_from = 0; // the extruder whose bed temperatures hold (rule 4); 0: the first to print
    bool ooze_prevention = false; // the extruders that wait cool down by standby_delta (rule 7)
    double standby_delta = -5.0;  // C
    // A tool change (rule 5): the leaving tool's retraction and what comes
    // back past it when the tool is used again, mm by extruder (a list
    // shorter than the extruders repeats its last value); the change's
    // G-code, with previous_extruder, next_extruder and toolchange_z.
    std::vector<double> toolchange_length{10.0}, toolchange_restart_extra{0.0};
    std::string gcode_toolchange;
    // One extruder that changes filaments (an MMU): a change purges the
    // filament (rule 12).
    bool single_extruder_mmu = false;
    // The MMU's change on the wipe tower (rule 15), mm: its cooling tube
    // (how long, how far from the nozzle), where a filament parks, and the
    // correction of the load; more motor current during the swap (read).
    double mmu_cooling_tube_length = 5.0, mmu_cooling_tube_position = 91.5;
    double mmu_parking_position = 92.0, mmu_extra_loading_move = -2.0;
    bool mmu_high_current_on_swap = false;
    // The wipe tower (rules 9 to 14): where (its corner, mm), turned how much
    // (degrees about the corner), how wide, its brim, the longest span of its
    // lines over the sparse layers, the cone at its base (read; not measured,
    // not applied), its lines' spacing and flow (%), no tower on the layers
    // without a change (the nozzle goes down to it), and the purge between each
    // pair of filaments, mm3 (row: from, column: to; empty: 140 between
    // different ones). extruder_wipe_tower prints the tower where no change
    // does (0: whichever is printing).
    bool wipe_tower = false;
    double wipe_tower_x = 180.0, wipe_tower_y = 140.0, wipe_tower_rotation = 0.0;
    double wipe_tower_width = 60.0, wipe_tower_brim = 2.0, wipe_tower_bridging = 10.0, wipe_tower_cone_angle = 0.0;
    double wipe_tower_extra_spacing = 100.0, wipe_tower_extra_flow = 100.0;
    bool wipe_tower_no_sparse_layers = false;
    std::vector<std::vector<double>> purge_volumes;
    int extruder_wipe_tower = 0;
    // Different nozzles (multimaterial.md, rule 3): the walls, the solid
    // infill, the support and the wipe tower with the finest, the sparse
    // infill with the coarsest, combined over as many layers as 75 % of its
    // nozzle holds. And whether to offer mixed colours (rules 9 and 11;
    // unset: not with a single-extruder MMU, which purges at each change).
    bool fine_walls_coarse_infill = false;
    std::optional<bool> color_mixing;
    // Curved layers (docs/spec/features/nonplanar.md): each layer the
    // surface z = s + D(x, y, z), D a wave or a cone, ramped in from
    // curved_flat_below over curved_ramp_height (and out at the top with
    // curved_flat_top); with curved_fit_head as curved as the head lets it
    // be. Moves split every curved_segment_length mm to follow the curve.
    CurvedMode curved_mode = CurvedMode::Disabled;
    CurvedPattern curved_pattern = CurvedPattern::Egg;
    double curved_amplitude = 0.6, curved_wavelength = 16.0; // mm
    double curved_angle = 0.0, curved_twist = 3.0;           // degrees, degrees per mm
    double curved_cone_angle = 15.0;                          // degrees
    bool curved_fit_head = true;
    double curved_flat_below = 0.6, curved_ramp_height = 5.0; // mm
    bool curved_flat_top = false;
    double curved_segment_length = 0.5; // mm
    double curved_max_slope = 20.0;     // degrees, without a measured head
    double curved_overhang_threshold = 45.0; // degrees from the vertical a flat layer still prints well (nonplanar_modes.md)
    CurvedFlow curved_flow = CurvedFlow::Preserve;
    double curved_uniform_flow = 0.0; // mm3/s; 0: 80 % of machine.max_flow
    // The print head around the nozzle (nonplanar.md, rule 6): the steepest
    // slope it lets the nozzle climb along X and along Y (degrees; 0: not
    // measured), how far below its lowest parts the tip is and how far out
    // those reach, and the free angle measured at several heights (mm,
    // degrees).
    double head_free_angle_x = 0.0, head_free_angle_y = 0.0;
    double head_clearance_height = 3.0, head_clearance_radius = 15.0;
    std::vector<std::array<double, 2>> head_profile;
    // A belt printer (belt_printer.md): the gantry at belt_angle degrees over
    // the belt; with belt_is_z the belt is the firmware's Z and the gantry Y,
    // else the other way round.
    bool belt_enabled = false;
    double belt_angle = 45.0;
    bool belt_is_z = true;
    // The printer's kind: the interface shows the settings of that kind and
    // slices for it (an MSLA printer gets a GOO file).
    MachineTechnology machine_technology = MachineTechnology::Fdm;
    // An MSLA resin printer (docs/spec/features/resin.md): its display's
    // pixels, the size of the area it lights (mm), and whether the images are
    // mirrored. The defaults are an Elegoo Mars 5's.
    int display_pixels_x = 4098, display_pixels_y = 2560;
    double display_width = 143.43, display_height = 89.6;
    bool display_mirror_x = true, display_mirror_y = false;
    // Resin printing (resin.md): every layer resin_layer_height thick, the
    // first resin_bottom_layers (at least one) exposed longer and lifted
    // further; seconds, mm and mm/s. The part is scaled first, the bottom
    // layers shrunk against the elephant's foot, every section grown by
    // resin_absolute_correction, and what hangs left dark for its first
    // resin_z_correction_layers layers. resin_gamma bends the greys of the
    // edges (0: black and white). The defaults are a generic standard resin's
    // on a Mars 5.
    double resin_layer_height = 0.05;
    double resin_exposure = 2.5, resin_bottom_exposure = 30.0;
    int resin_bottom_layers = 5;
    double resin_elephant_foot = 0.2;
    double resin_scale_x = 1.0, resin_scale_y = 1.0, resin_scale_z = 1.0;
    double resin_lift_distance = 5.0, resin_lift_speed = 1.6667;
    double resin_bottom_lift_distance = 7.0, resin_bottom_lift_speed = 1.3333;
    double resin_retract_speed = 2.5;
    double resin_wait_before_exposure = 2.5, resin_wait_after_cure = 1.0, resin_wait_after_lift = 0.0;
    int resin_light_pwm = 255, resin_bottom_light_pwm = 255;
    int resin_antialiasing = 8;
    double resin_density = 1.0; // g/ml
    // The resin's bottle (gcode_extras.md, rule 2): its cost, ml and kg; the
    // print costs its ml times the cost over the bottle's ml.
    double resin_bottle_cost = 0.0, resin_bottle_volume = 1000.0, resin_bottle_weight = 1.0;
    double resin_gamma = 1.0;
    int resin_z_correction_layers = 0;
    double resin_absolute_correction = 0.0;
    // What the file's header names: the printer's model and the resin's profile.
    std::string resin_printer_name = "MARS 5", resin_profile_name;
    // Resin supports, pad and hollowing (resin_supports.md): points on the
    // faces looking down, each a tip into the part on a pillar down to the
    // pad (mm, degrees, percent); the pad under the supports, or around a
    // part that stands on the platform; and the part hollowed with walls at
    // least resin_hollow_wall thick, its inside rounded by
    // resin_hollow_closing, with bars inside (percent, cells in mm).
    bool resin_supports = true;
    double resin_support_tip_diameter = 0.4, resin_support_tip_penetration = 0.2;
    double resin_support_pillar_diameter = 1.0, resin_support_elevation = 5.0;
    double resin_support_critical_angle = 45.0, resin_support_density = 100.0;
    bool resin_pad = true;
    double resin_pad_thickness = 2.0, resin_pad_margin = 1.6, resin_pad_wall_slope = 90.0;
    bool resin_pad_around_object = false;
    bool resin_hollow = false;
    double resin_hollow_wall = 3.0, resin_hollow_closing = 2.0;
    double resin_hollow_infill = 0.0, resin_hollow_infill_cell = 8.0;
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
    // Speeds of their own (gcode_motion.md; speeds_accels.md, rules 1 to 4),
    // also from the printer's profile; 0: the one in brackets.
    double bridge_speed = 0.0;      // bridges and overhang perimeters (speed.solid_infill)
    double gap_fill_speed = 0.0;    // gap fill (speed.solid_infill)
    Speed over_bridges_speed;       // solid infill right over a bridge, or a share of solid_infill (speed.solid_infill)
    Speed support_interface_speed;  // the support's interface, or a share of speed.support; not set: speed.support
    Speed first_layer_infill_speed; // the first layer's solid infill, past speed.first_layer, or a share of
                                    // solid_infill (speed.first_layer)
    double travel_z_speed = 0.0;    // moves in Z (speed.travel)
    Speed first_layer_over_raft;    // cap of the part's first layer on a raft, or a share of each speed (speed.first_layer)
    // Walls by how much of their bead rests on the layer below
    // (speeds_accels.md, rules 6 and 7): at overhang_speeds[k] with 0, 25, 50
    // and 75 %, their own with 100 %, in a straight line in between. Each in
    // mm/s or a share of the outer wall's; the spec's, not the printer's.
    bool dynamic_overhangs = false;
    // How fast the flow may change from one extrusion to the next
    // (speeds_accels.md, rules 8 and 9), mm3/s per second; 0: free.
    double flow_slope_up = 0.0, flow_slope_down = 0.0;
    std::array<Speed, 4> overhang_speeds = {Speed{15.0, false}, Speed{15.0, false}, Speed{20.0, false}, Speed{25.0, false}};

    // Accelerations by kind (speeds_accels.md, rules 10 to 13), mm/s2, set
    // before each extrusion and travel; 0: the one in brackets. Without
    // accel_default, only the travels' (and the extrusions go on with it).
    double accel_default = 0.0;
    double accel_walls = 0.0;            // inner walls (accel_default)
    double accel_outer_wall = 0.0;       // (accel_walls)
    double accel_infill = 0.0;           // sparse infill (accel_default)
    double accel_solid_infill = 0.0;     // (accel_infill)
    double accel_top_solid_infill = 0.0; // (accel_solid_infill)
    double accel_bridges = 0.0;          // bridges and overhang perimeters (accel_default)
    double accel_wipe_tower = 0.0;       // the wipe tower (accel_default)
    double accel_first_layer = 0.0;      // everything on the first layer (each kind's)
    double accel_first_layer_over_raft = 0.0; // the part's first layer on a raft (each kind's)
    double accel_travel = 0.0;           // travels (none)
    double accel_short_travel = 0.0;     // travels shorter than retraction.min_travel (accel_travel)

    // retraction
    double retract_length = 2.0;     // mm of filament
    double retract_speed = 0.0;      // mm/s, from the printer's profile; 0: not set
    double retract_lift = 0.0;       // mm of Z while travelling
    double retract_min_travel = 2.0; // mm: shorter travels do not retract
    // retraction_travel.md: the push back (rule 1), every layer change
    // retracting (2), the wipe (3), the lift's heights (4) and the firmware's
    // retraction (7).
    double unretract_speed = 0.0;    // mm/s; 0: retraction.speed
    double restart_extra = 0.0;      // mm of filament pushed past the retraction
    bool retract_layer_change = false;
    bool wipe = false;
    double before_wipe = 0.0;        // % of the retraction done standing, before the wipe
    double lift_above = 0.0, lift_below = 0.0; // mm: the lift only above / below; 0: no limit
    bool firmware_retraction = false; // G10 and G11
    // Travels (rules 5 and 8): a lift as a ramp, and the route inside the part.
    bool ramping_lift = false;
    double travel_slope = 0.0;      // degrees
    double max_lift = 0.0;          // mm the ramp climbs at most; 0: retraction.lift
    bool lift_before_obstacle = false; // read; not measured, not applied (rule 6)
    bool avoid_crossing_walls = true;  // travels run inside, round holes (docs/spec/BEHAVIOR.md)
    Speed max_detour;                  // mm or % of the straight travel a route may add; 0: no limit
    bool walls_clockwise = false;      // every loop clockwise (rule 9)
    bool walls_outer_first = false;    // each island's walls from the outer one in (perimeters.md, rule 4)
    // The filament's own values of these (rule 10), by name
    // ("retraction.length", "travel.slope"...); one given replaces the
    // printer's, 0 included. with_filament_retraction applies them.
    std::map<std::string, double> filament_retraction;

    // adhesion
    int skirt_loops = 1;
    double skirt_distance = 6.0; // mm from the part (or its brim; supports_skirt.md)
    int skirt_layers = 1;
    double brim_width = 0.0; // mm; 0: no brim
    // skirt_brim_raft.md: the skirt adds loops until it lays this much
    // filament (rule 1); a draft shield (rule 4); where the brim goes and how
    // far from the part (rules 5 to 8).
    double skirt_min_filament = 0.0; // mm of filament; 0: none
    DraftShield draft_shield = DraftShield::Disabled;
    BrimType brim_type = BrimType::Outer;
    double brim_gap = 0.0; // mm
    // The raft (skirt_brim_raft.md, rules 9 to 14): raft_layers of support
    // under the whole part, which goes up by them and raft_gap; 0: none.
    int raft_layers = 0;
    double raft_gap = 0.1;                    // mm between the raft's top and the part
    double raft_expansion = 1.5;              // mm round the part's first layer
    double raft_first_layer_density = 90.0;   // % of the raft's first layer
    double raft_first_layer_expansion = 3.0;  // mm more for the raft's first layer

    // support (docs/spec/BEHAVIOR.md, "Soportes")
    bool support = false;
    bool support_automatic = true; // false: support only under faces painted to have it (docs/PAINTING.md)
    bool support_buildplate_only = false; // only support standing on the bed (supports_more.md, rule 2)
    // The support on the part's layers (multi_extruder.md, rule 19): read;
    // the engine always prints the support on the part's layers.
    bool support_synchronize_layers = false;
    double support_angle = 0.0;  // degrees from the horizontal; 0: automatic (half a bead per layer)
    double support_z_gap = 0.2;  // mm between the support and the part above it
    double support_bottom_z_gap = 0.0; // mm between the part and the support standing on it; 0: support_z_gap
    // Between the support and the part beside it: mm, or percent of the
    // wall width (supports_skirt.md, rule 2: 50 %).
    Speed support_xy_gap{50.0, true};
    // The support's and its interface's bead width (supports_more.md, rule
    // 18): mm, or percent of the layer height; 0: walls.width.
    Speed support_width{0.0, false};
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
    int support_bottom_interface_layers = -1; // dense layers right over the part it stands on; -1: interface_layers

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
    // The fan up step by step until this layer (counted from 1) after the
    // ones without fan (cooling_more.md, rule 1); 0: at once.
    int full_fan_layer = 0;
    // Off: no slowing down for the minimum layer time and no fan by layer
    // time (cooling.md); the bridges' fan and the height changes' stay.
    bool cooling_enabled = true;
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
    // The sparse infill's caps for this filament (speeds_accels.md, rule 5):
    // patterns whose lines do not cross in a layer, and those whose lines do; 0: none.
    double infill_max_speed = 0.0;
    double infill_max_crossing_speed = 0.0;
    // Of the round bead of bridges and overhang perimeters (speeds_accels.md,
    // rule 14): its section times this, its diameter times the root.
    double bridge_flow_ratio = 1.0;
    double chamber_temperature = 0.0;  // C the filament wants in the chamber; 0: none
    double chamber_min_temperature = 0.0; // C the chamber must reach before printing (M191); 0: no wait
    // For the weight and cost at the end of the G-code (gcode_extras.md, rule 1).
    double filament_density = 0.0;     // g/cm3; 0: not known
    double filament_cost = 0.0;        // money per kg
    double spool_weight = 0.0;         // g of the empty spool (for the interface)
    // How much the filament shrinks as it cools, in %, across the layers and
    // along Z: the part is sliced 100 / (100 - p) times larger (gcode_extras.md, rule 4).
    double shrinkage_xy = 0.0, shrinkage_z = 0.0;
    bool abrasive = false;             // wears a brass nozzle
    bool needs_enclosure = false;
    // The filament's material ("PLA", "PETG"...), for the assistant's card
    // (assistant.md, rule 13); empty: found by the profile's name.
    std::string filament_type;
    // The calibrations done, by guide id or test name: the printer's in
    // machine.calibrations, the filament's in extrusion.calibrations.
    std::map<std::string, CalibrationDate> printer_calibrations, filament_calibrations;
    // The share of the purge each filament asks for (multi_extruder.md, rule
    // 12), %, by extruder; a shorter list repeats its last value.
    std::vector<double> purge_multiplier{100.0};
    // What a change purges on the wipe tower at least before it purges into
    // objects (rule 13), mm3, by extruder.
    std::vector<double> min_purge_on_tower{0.0};
    // The filament in an MMU's change (rule 15): its loading and unloading
    // speeds (the start's and the rest's, mm/s), the cooling moves in the
    // tube (how many, the filament's speed at the first and the last, mm/s),
    // the ramming's flow at each quarter of a second before unloading
    // (mm3/s), the wait after unloading, and the times the load and the
    // unload take for the estimate (s).
    double loading_speed = 28.0, loading_speed_start = 3.0, unloading_speed = 90.0, unloading_speed_start = 100.0;
    int cooling_moves = 4;
    double cooling_initial_speed = 2.2, cooling_final_speed = 3.4;
    std::vector<double> ramming{7.0, 7.4, 7.8, 8.2, 8.6, 9.0, 9.4, 9.8, 10.2, 10.6, 11.0};
    double toolchange_delay = 0.0, load_time = 0.0, unload_time = 0.0;
    // The filament's own start and end G-code (gcode_templates.md, rules 9
    // and 12): after the printer's start, before the printer's end.
    std::string filament_start_gcode, filament_end_gcode;

    // gcode
    // Points that move a path less than this go (docs/spec/features/gcode_extras.md); 0: every point is written.
    double gcode_resolution = 0.0125;
    // E in mm3 of plastic instead of mm of filament (gcode_extras.md, rule 11).
    bool volumetric_e = false;
    // Every line the engine writes says what it does in a comment (gcode_extras.md, rule 12).
    bool verbose_comments = false;
    // Runs of a path that follow a circle within gcode_resolution become G2
    // and G3 (gcode_extras.md, rule 10).
    bool gcode_arcs = false;
    // M73 with the share done and the minutes left (gcode_firmware.md, rule 5).
    bool gcode_progress = false;
    // Each object's moves marked, to cancel one while printing (gcode_firmware.md, rule 4).
    ObjectLabels object_labels = ObjectLabels::None;
    // The binary G-code format (gcode_extras.md, rule 9; src/gcode/binary.hpp).
    bool gcode_binary = false;
    // The temperatures the start G-code does not set itself, written round it
    // (gcode_templates.md, rule 10; gcode_firmware.md, rule 1).
    bool auto_temperatures = true;

    // output (gcode_templates.md, rules 15 to 17): the file's name in the
    // template language, and scripts run on the finished G-code.
    std::string filename_format = "[input_filename_base].gcode";
    std::vector<std::string> post_process;
    // Templates (gcode_templates.md, rules 11 and 13), with the variables of
    // machine.variables; the layer's ones also layer_num, layer_z and max_layer_z.
    std::string gcode_color_change = "M600";
    std::string gcode_pause = "M601";
    std::string gcode_before_layer; // right before each layer's move in Z, from the second layer
    std::string gcode_after_layer;  // right after it
    std::string gcode_between_objects; // sequential printing: between one object and the next (sequential.md, rule 3)

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

// The name of a flavor in the settings ("marlin", "no_extrusion"...).
const char* flavor_name(GcodeFlavor flavor);

// The name of a limits usage in the settings ("time_only", "emit", "ignore").
const char* limits_usage_name(LimitsUsage usage);

// The names of a draft shield and a brim type in the settings.
const char* draft_shield_name(DraftShield shield);
const char* top_one_wall_name(TopOneWall where);
const char* object_labels_name(ObjectLabels labels);
const char* brim_type_name(BrimType type);

// The machine's limits by name, as machine.silent names them: max_feed_x to
// _e, max_accel_x to _e, acceleration, travel_acceleration,
// retract_acceleration, jerk_x to _e, junction_deviation, min_extrude_feed,
// min_travel_feed and corner_speed.
extern const std::array<const char*, 19> kLimitNames;
double* limit_named(Settings& settings, std::string_view name);

// The settings with the silent mode's limits in place of the normal ones.
Settings silent_settings(const Settings& settings);

// A value of a list by extruder (from 0): a shorter list repeats its last
// value; an empty one gives `fallback`.
double per_extruder(const std::vector<double>& values, std::size_t extruder, double fallback = 0.0);
// Extruder `extruder`'s nozzle offset (machine.extruder_offset), mm; none
// with a single-extruder MMU, which has one nozzle.
std::array<double, 2> extruder_offset_of(const Settings& settings, std::size_t extruder);
// Extruder `extruder`'s nozzle (from 1; 0 or a single-extruder MMU:
// machine.nozzle_diameter), mm.
double nozzle_of(const Settings& settings, int extruder);
// The extruder of each kind of extrusion (extruders.*; 0: the object's):
// with multi.fine_walls_coarse_infill and nozzles of different sizes, the
// finest for all but the sparse infill, which takes the coarsest
// (multimaterial.md, rule 3); turned off, the print's own again (rule 4).
struct KindExtruders {
    int walls = 0, infill = 0, solid_infill = 0, support = 0, support_interface = 0, wipe_tower = 0;
};
KindExtruders kind_extruders(const Settings& settings);
// curved.mode's name in the settings.
const char* curved_mode_name(CurvedMode mode);
// Whether mixed colours are offered (multimaterial.md, rules 9 and 11):
// multi.color_mixing, else not with a single-extruder MMU.
bool color_mixing_offered(const Settings& settings);

// The settings a filament may give its own value of (retraction_travel.md,
// rule 10), and the settings with the filament's values in place.
extern const std::array<const char*, 14> kFilamentRetractionNames;
Settings with_filament_retraction(const Settings& settings);

// Parses settings JSON. Throws SettingsError naming the key on bad input.
Settings parse_settings(std::string_view json);

// The settings of an object or a part (model_tools.md, rules 5, 7 and 8):
// `own`, a JSON object of groups with the profile's keys, over `base`. Only
// the groups the slicing of a region reads are taken today (walls, infill,
// surfaces; with `layers`, a height range's layer height too); another
// throws SettingsError naming the key and `where` (rule 9: refused, not
// ignored). Empty `own`: `base`.
Settings own_settings(const Settings& base, std::string_view own, std::string_view where, bool layers = false);
// Two such JSON objects in one, `over`'s keys winning (a modifier's over its
// part's, model_tools.md rule 5).
std::string merged_own(std::string_view under, std::string_view over);
// Such a JSON object without one group; `had` says whether it had it.
std::string own_without(std::string_view own, std::string_view group, bool* had = nullptr);

// The engine's key for a key of the specs' `material` group
// ("material.nozzle_temp" → "machine.variables.temperature",
// "material.density" → "extrusion.density"); other keys as they are. None
// for material.flow_factor (a factor where extrusion.flow is a %) and for a
// key the engine does not know.
std::optional<std::string> engine_setting_key(std::string_view key);

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
