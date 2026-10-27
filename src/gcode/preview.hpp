#pragma once

#include "gcode/print_time.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

namespace tisma {

// What a move lays down, from the ";TYPE:" comments of docs/spec/GCODE_OUTPUT.md.
enum class LineKind : std::uint8_t {
    Travel,
    OuterWall,        // External perimeter
    InnerWall,        // Perimeter
    OverhangWall,     // Overhang perimeter
    SparseInfill,     // Internal infill
    SolidInfill,      // Solid infill
    TopSolidInfill,   // Top solid infill
    BridgeInfill,     // Bridge infill
    GapFill,          // Gap fill
    Ironing,          // Ironing
    SkirtBrim,        // Skirt/Brim
    Support,          // Support material
    SupportInterface, // Support material interface
    WipeTower,        // Wipe tower
    Custom,           // the user's start and end G-code
    Other,            // a type this reader does not know, or none given
};
inline constexpr std::size_t kLineKinds = 16;

// The kind named by a ";TYPE:" value (exact words of GCODE_OUTPUT.md);
// Other for anything else.
LineKind line_kind(std::string_view type);

// A short stable name for the kind ("outer_wall", "travel"...), for reports and
// settings files.
const char* line_kind_id(LineKind kind);

// One straight move, in the order of the file. Coordinates in mm, at the
// nozzle (an extrusion's top is at z1, its bottom at z1 - height).
struct PreviewMove {
    float x0 = 0, y0 = 0, z0 = 0;
    float x1 = 0, y1 = 0, z1 = 0;
    float width = 0;   // mm; 0 for travels
    float height = 0;  // mm; 0 for travels
    float feed = 0;    // mm/s
    float seconds = 0; // with the machine's accelerations (time_preview); 0 until then
    LineKind kind = LineKind::Travel;
    std::uint8_t fan = 0;          // % of the part cooling fan (M106, M107)
    std::uint8_t tool = 0;         // the extruder (T)
    std::uint16_t temperature = 0; // C the nozzle is set to (M104, M109)
    std::uint16_t color = 0;       // the colour changes (M600) before it
    std::uint32_t layer = 0;       // index into GcodePreview::layers
};

struct PreviewLayer {
    double z = 0.0;      // mm
    double height = 0.0; // mm
    std::size_t first = 0; // its moves are moves[first, first + count)
    std::size_t count = 0;
    std::size_t first_extrusion = 0; // its extrusions are extrusions[first_extrusion, + extrusion_count)
    std::size_t extrusion_count = 0;
    double seconds = 0.0; // its moves' time (time_preview)
};

struct GcodePreview {
    std::vector<PreviewMove> moves;         // every move that goes somewhere in XY, Z or both
    std::vector<std::uint32_t> extrusions;  // indices into moves of the moves that extrude, in order
    std::vector<PreviewLayer> layers;
    std::array<float, 3> min{0, 0, 0}, max{0, 0, 0}; // box of the extrusions (zero if none)
    double filament = 0.0;  // mm of filament pushed, net of retractions
    double extruded = 0.0;  // mm of extruding path
    double travelled = 0.0; // mm of travel path
    double estimated_seconds = -1.0; // from "; estimated printing time = 1h 7m 36s"; -1 when absent
    std::size_t lines = 0;
    std::size_t arcs = 0; // G2/G3 moves (with I and J, drawn along their circle)
    std::array<std::size_t, kLineKinds> kind_moves{}; // extrusions per kind (travels for Travel)
    std::array<double, kLineKinds> kind_filament{};   // mm of filament per kind
    // With time_preview: the moves' time by kind, in all and in the first
    // layer that extrudes (the time of retractions and waits left out).
    std::array<double, kLineKinds> kind_seconds{};
    double moves_seconds = 0.0, first_layer_seconds = 0.0;
};

// Reads G-code for display. Lenient: it opens any planar G-code, not only the
// engine's. It follows G0/G1, G2/G3 with I and J (in pieces of 5 degrees at
// most; with R, as straight moves), G90/G91, M82/M83, G92,
// G28, the comments ;LAYER_CHANGE, ;Z:, ;HEIGHT:, ;TYPE: and ;WIDTH:, and the
// estimated printing time (docs/spec/GCODE_OUTPUT.md, "Tiempos"). A
// move extrudes when E grows. Without ;LAYER_CHANGE comments a new layer starts
// at the first extrusion at a new Z. Without ;WIDTH:, the width comes from the
// filament pushed over the move's length and height (filament_diameter, mm).
// extruder_axis: the extruder's letter, A in the Mach3 and Machinekit dialects
// (gcode_extras.md, rule 6). Own design; see PROVENANCE.md.
GcodePreview read_gcode_preview(std::string_view text, double filament_diameter = 1.75, char extruder_axis = 'E');

// Each move's time with the machine's speed limits and accelerations (the
// estimate's kinematics, print_time.hpp), planned layer by layer from rest to
// rest, and the sums by layer and by kind (preview.md, rules 6 and 8).
void time_preview(GcodePreview& preview, const Kinematics& kinematics, double filament_diameter = 1.75);

// Seconds in a duration written as "2d 1h 7m 36s" (any of the parts, in that
// order, as format_duration writes them); -1 if it does not read so.
double parse_duration(std::string_view text);

} // namespace tisma
