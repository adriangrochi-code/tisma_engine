#pragma once

#include "config/settings.hpp"
#include "geom/polygons.hpp"
#include "perimeters/sites.hpp"
#include "perimeters/walls.hpp"
#include "surfaces/surfaces.hpp"

#include <vector>

namespace tisma {

struct DepthField;

// Parallel lines `spacing` mm apart at `angle_degrees` from the X axis, on a
// grid fixed to the origin so that the lines of different layers line up,
// clipped to the region shrunk by width / 2 (the beads stay inside). Each piece
// is an open toolpath of constant width, ordered across the lines and along
// each line. With `connect`, pieces on adjacent lines whose ends follow each
// other along the boundary, at most kMaxJoin spacings apart along it, are
// joined by that stretch of boundary into zig-zag polylines: the line ends are
// anchored and part of the edge band is filled, without entering the walls.
// With `fit`, the lines are not on the grid: as many as fit across each
// connected piece of the region (measured across its extent), centred, so the
// outer ones run along its shrunk boundary; for solid surfaces.
// With `monotonic` (instead of `connect`), the pieces all run the same way and
// each comes after every piece of the line below that touches it; among those
// that may come next, the one starting nearest to the last one's end.
// With `overlap` (mm, for unjoined lines), the lines reach that far past the
// region shrunk by half a bead, into the walls around it. `shift` (mm) moves
// the grid across the lines (the cubic infill's planes with the height).
inline constexpr double kMaxJoin = 3.0;
std::vector<Toolpath> rectilinear_fill(const Polygons& region, double spacing, double angle_degrees, double width,
                                       bool connect = false, bool fit = false, bool monotonic = false,
                                       double overlap = 0.0, double shift = 0.0);

// The infill toolpaths of one layer.
struct LayerFill {
    std::vector<Toolpath> top;    // top surfaces, at 100%
    std::vector<Toolpath> solid;  // bottom surfaces, at 100%
    std::vector<Toolpath> sparse; // the rest, at the infill density
    std::vector<Toolpath> bridge; // bridges: straight lines at the bridge's angle, at 100%
    std::vector<Toolpath> gaps;   // variable-width beads where the solid lines leave a gap (gap_fill)
    std::vector<Toolpath> support;           // support lines (support_areas)
    std::vector<Toolpath> support_interface; // dense support right under the part
    std::vector<Toolpath> arcs; // over overhangs held from one side, in printing order (arc_overhangs.md)
    // Loops over overhangs held from one side, from the zone's outline in
    // (walls.extra_on_overhangs, walls_extra.md, rule 5).
    std::vector<Toolpath> overhang_walls;
    std::vector<Toolpath> ironing; // over the top surfaces, after their fill (ironing.md)
};

// What lines leave uncovered in a region, filled with variable-width beads
// (the wall chain of Kuipers et al. 2020 without a wall limit): a gap of
// thickness d gets beads summing to d, and gaps thinner than half a bead get
// none. Used for the solid surfaces: the bands that fitted lines leave at
// inner corners and the ends of unjoined lines.
std::vector<Toolpath> gap_fill(const Polygons& region, const std::vector<Toolpath>& lines, double width);

struct FillParams {
    double width = 0.4;   // bead width
    double density = 0.2; // sparse infill, 0 to 1
    double angle = 45.0;  // degrees; odd layers add 90 (rectilinear)
    InfillPattern pattern = InfillPattern::Rectilinear;
    bool monotonic_top = true; // top surfaces in monotonic order instead of zig-zags
    bool gap_fill = false;     // fill what the solid lines leave with variable-width beads (gap_fill)
    double wall_overlap = 0.25; // monotonic top lines reach this fraction of a bead into the walls
    double bridge_width = 0.0;  // the bridges' round beads, and how far apart their lines are; 0: width
    const DepthField* depth = nullptr; // adaptive cubic: how deep the sparse infill lies (none: as cubic)
    Point2d centre{};                  // archimedean and octagram: the spirals' centre, the same in every layer
};

// Infill of layer `layer`. Solid surfaces: rectilinear lines one bead apart,
// fitted to the region, at the layer's angle; top surfaces in monotonic order
// (with monotonic_top), reaching wall_overlap of a bead into the walls since
// their ends are not joined, the rest connected into zig-zags; with gap_fill, what
// they leave is filled by gap_fill (off by default: on the sphere it costs
// 0.5 s for 1 to 2 % of the solid area). Sparse
// infill, by pattern:
//  - rectilinear: lines width / density apart on the grid at the layer's
//    angle, connected into zig-zags;
//  - grid: lines at the angle and at the angle + 90 in every layer, each set
//    2 width / density apart (the same material), not connected: the lines
//    cross each other and their ends touch the walls;
//  - gyroid: the gyroid's section at height z (gyroid_fill), with the period
//    for the density;
//  - aligned rectilinear: rectilinear, every layer at the angle;
//  - zigzag: rectilinear;
//  - triangles, stars and cubic: three families of lines 60 degrees apart;
//  - adaptive and support cubic: the cubic lines, fewer where `depth` says
//    the infill lies deep (adaptive_cubic_fill);
//  - line: rectilinear with every other line turned 6.7 degrees (line_fill);
//  - honeycomb: hexagons whose principal direction turns 60 degrees a layer
//    (honeycomb_fill);
//  - concentric: loops width / density apart (concentric_fill);
//  - lightning: none here, since each layer's branches come from the layer
//    above's (lightning_layers).
LayerFill fill_layer(const LayerSurfaces& surfaces, std::size_t layer, const FillParams& params, double z = 0.0);

// Every how many layers fill_layer gives the same toolpaths for the same
// surfaces: the directions turn with the layer number alone (2, or 6 with
// honeycomb's three). 0 when the pattern moves with the height or follows the
// depth field (gyroid, cubic, adaptive and support cubic).
std::size_t fill_period(const FillParams& params);

} // namespace tisma
