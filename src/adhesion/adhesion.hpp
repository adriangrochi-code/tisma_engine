#pragma once

#include "config/settings.hpp"
#include "geom/polygons.hpp"
#include "perimeters/sites.hpp"

#include <vector>

namespace tisma {

// Skirt and brim around the first layer (supports_skirt.md, rules 8 and 9).
// Own design with Clipper2 offsets of the outer contours of the layer's region
// (holes are left alone), with rounded corners: each loop is the part grown.
// Loops lie at the beads' spacing (loop_spacing), so neighbours overlap as
// printed beads do:
// - brim: floor(width / spacing) loops, the first touching the part, the
//   outer one reaching the width asked; loops of nearby parts merge where they
//   meet;
// - skirt: `loops` loops, the first `distance` mm away from the part (or from
//   its brim), outer contours only (no loops in the gaps between parts).
// Loop k (0 the innermost) has index -1 - k, apart from the walls' indices.
// `height` is the first layer's.
std::vector<Toolpath> brim_loops(const Polygons& region, double width, double bead, double height);
std::vector<Toolpath> skirt_loops(const Polygons& region, double brim_width, int loops, double distance,
                                  double bead, double height);

// The most loops a skirt takes to reach its filament (skirt_brim_raft.md, rule 1).
inline constexpr int kMaxSkirtLoops = 1000;

// How the brim and the skirt are laid (skirt_brim_raft.md).
struct AdhesionShape {
    double brim_width = 0.0; // mm
    BrimType brim_type = BrimType::Outer;
    double brim_gap = 0.0; // mm between the part and the brim's first loop (rule 8)
    int skirt_loops = 0;
    double skirt_distance = 0.0; // mm
    // A draft shield (rule 4): the skirt measured from the part, and the
    // brim's loops where the skirt goes left out.
    bool shield = false;
    // The skirt adds loops outwards until it lays this much filament (rule 1);
    // 0: none. Each loop takes its length times bead x height over filament_area.
    double skirt_min_filament = 0.0; // mm of filament
    double filament_area = 0.0;      // mm2
};

// The same shape from the settings.
AdhesionShape adhesion_shape(const Settings& settings);

// The brim (rules 5 to 8): `outer` loops grown out of the outer contours,
// `inner` loops inside every hole from its edge inwards (the region grown,
// its holes shrinking round any island in them), until the hole is full;
// both brim_gap away from the part and brim_width wide.
std::vector<Toolpath> brim_loops(const Polygons& region, const AdhesionShape& shape, double bead, double height);
// The skirt: skirt_loops loops, then more until skirt_min_filament, from
// the outer brim's last loop, or from the part with a shield or without an
// outer brim.
std::vector<Toolpath> skirt_loops(const Polygons& region, const AdhesionShape& shape, double bead, double height);

// The distance between the axes of two beads side by side that fill the gap
// between them: a bead's section is a rectangle with half-circle ends
// (Comminal et al. 2018), of area w h - (1 - pi / 4) h^2, so the spacing is
// w - (1 - pi / 4) h.
double loop_spacing(double bead, double height);

} // namespace tisma
