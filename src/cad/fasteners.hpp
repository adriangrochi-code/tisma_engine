// Metric fasteners for printing: bolts, nuts, washers and threaded rods with
// the ISO metric thread, made a little loose so plastic parts screw together
// (the owner's request, 2026-11-02). Own design from the public ISO tables
// (thread profile ISO 68-1, pitches ISO 261, nuts ISO 4032, hex heads
// ISO 4017, socket heads ISO 4762, washers ISO 7089, clearance holes ISO 273);
// see PROVENANCE.md.

#pragma once

#include "mesh/mesh.hpp"

#include <span>
#include <string>
#include <string_view>

namespace tisma::cad {

// One row of the size table, in mm.
struct MetricSize {
    const char* name;      // "M8"
    double diameter;       // nominal (major) diameter d
    double pitch;          // coarse pitch P (ISO 261)
    double nut_width;      // across flats s (ISO 4032; the hex head too)
    double nut_height;     // m (ISO 4032)
    double head_height;    // hex head k (ISO 4017)
    double socket_head;    // socket head diameter dk (ISO 4762)
    double socket_key;     // hex key s (ISO 4762)
    double washer_inner;   // d1 (ISO 7089)
    double washer_outer;   // d2
    double washer_height;  // h
    double clearance_hole; // medium series (ISO 273)
};

// M3 to M24, smallest first.
std::span<const MetricSize> metric_sizes();
// The row named `name` ("M8", case-insensitive); nullptr when unknown.
const MetricSize* metric_size(std::string_view name);

// The thumb screw and the thumb nut have a knurled head to turn by hand;
// the standoff is a hexagonal bar threaded inside from end to end; the
// spacer a plain tube for a bolt to go through.
enum class FastenerKind { HexBolt, SocketBolt, ThumbScrew, HexNut, ThumbNut, Washer, ThreadedRod, Standoff, Spacer };
inline constexpr FastenerKind kFastenerKinds[] = {
    FastenerKind::HexBolt,  FastenerKind::SocketBolt, FastenerKind::ThumbScrew,
    FastenerKind::HexNut,   FastenerKind::ThumbNut,   FastenerKind::Washer,
    FastenerKind::ThreadedRod, FastenerKind::Standoff, FastenerKind::Spacer};
// "hex_bolt", "socket_bolt", "thumb_screw", "hex_nut", "thumb_nut", "washer",
// "threaded_rod", "standoff", "spacer"
const char* fastener_id(FastenerKind kind);
bool has_length(FastenerKind kind); // bolts, rods, standoffs and spacers
bool has_thread(FastenerKind kind); // all but the washer and the spacer
bool is_nut(FastenerKind kind);     // threaded inside: nuts and standoffs

struct FastenerSpec {
    FastenerKind kind = FastenerKind::HexBolt;
    std::string size = "M8"; // a row of the table, or "custom"
    double diameter = 8.0;   // only for "custom"
    double pitch = 0.0;      // 0: the size's coarse pitch
    double length = 20.0;    // shank of a bolt, whole rod
    double clearance = 0.2;  // radial play for printing: bolts thinner, nuts wider
    int segments = 72;       // around the axis; a multiple of 6
};

// The numbers a spec resolves to (the custom size by proportions of the
// table's M8 row).
struct FastenerDims {
    double d = 0, pitch = 0, nut_width = 0, nut_height = 0, head_height = 0, socket_head = 0, socket_key = 0;
    double washer_inner = 0, washer_outer = 0, washer_height = 0, clearance_hole = 0;
    // Own proportions (no standard): the knurled head or nut's diameter and
    // height, the thumb nut's height, a spacer's outside.
    double knurl_diameter = 0, knurl_height = 0, thumb_nut_height = 0, spacer_outer = 0;
};
FastenerDims fastener_dims(const FastenerSpec& spec);

// An empty string when the spec can be made, else why not (English; the
// interface shows its own text).
std::string check_fastener(const FastenerSpec& spec);

// The ISO 68-1 basic profile: the radius of the thread's surface at phase
// u in [0, 1) along one pitch, for a thread of major diameter d and pitch p
// (crest flat p/8 at d/2, root flat p/4 at d/2 - 5H/8, 60° flanks).
double thread_profile_radius(double d, double p, double u);

// The part, standing on the bed at z = 0, centred on the axis; a closed,
// outward-facing mesh. Bolts stand on their head. Throws std::invalid_argument
// when check_fastener() does not pass.
TriangleMesh make_fastener(const FastenerSpec& spec);

std::string fastener_json(const FastenerSpec& spec);
FastenerSpec parse_fastener(std::string_view json); // throws std::runtime_error

} // namespace tisma::cad
