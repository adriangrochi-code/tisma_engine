#pragma once

#include "geom/bitgrid.hpp"
#include "geom/polygons.hpp"

#include <span>
#include <vector>

namespace tisma {

namespace gpu {
class Context;
}

struct SupportParams {
    double threshold_angle = 0.0; // degrees from the horizontal; 0: automatic (half a bead per layer)
    int z_gap_layers = 1;         // empty layers between the support and the part above it
    // The same, layer by layer, when the layers are not all as thick
    // (support.z_gap is in mm): the empty layers over support layer l. Null:
    // z_gap_layers everywhere.
    const std::vector<int>* gap_above = nullptr;
    int bottom_gap_layers = 0;    // empty layers between the part and the support standing on it
    // The same, layer by layer (support.bottom_z_gap in mm over the layers'
    // own thicknesses): the empty layers under support layer l. Null:
    // bottom_gap_layers everywhere.
    const std::vector<int>* gap_below = nullptr;
    // Only support that stands on the bed (supports_more.md, rule 2): a zone
    // that rests on the part anywhere goes whole.
    bool buildplate_only = false;
    // Support under bridges too (supports_skirt.md, rule 1): an automatic
    // overhang held at two ends (is_bridge) gets none without it.
    bool under_bridges = false;
    double xy_gap = 0.6;          // mm between the support and the part beside it
    double bead = 0.4;            // mm, the wall bead width
    double margin = 0.0;          // mm the column reaches past what overhangs (supports_skirt.md, rule 2: about 1.6)
    double closing = 0.0;         // mm, the radius gaps in the column are closed by; 0: a bead
    // Painted faces (docs/PAINTING.md; supports_more.md, rules 3 to 5), one
    // area per layer, or null. In an enforcer, whatever reaches out past the
    // layer below gets support, however steep; in a blocker, nothing does.
    // With automatic off, only the enforced overhangs get support.
    bool automatic = true;
    const std::vector<Polygons>* enforce = nullptr;
    const std::vector<Polygons>* block = nullptr;
    gpu::Context* gpu = nullptr; // the column's steps on the GPU when given, with the same bits
};

// What each layer overhangs (docs/spec/BEHAVIOR.md, "Soportes"): the part of
// its region farther out than the layer below allows. A face flatter than the
// threshold angle from the horizontal reaches out more than height / tan(angle)
// per layer; that much is allowed. Layer 0 rests on the bed. heights[l] is the
// thickness of layer l.
std::vector<Polygons> overhangs(const std::vector<Polygons>& regions, const std::vector<double>& heights,
                                const SupportParams& params);

// Whether an overhang (one island of it: its outer contour, then its holes)
// is a bridge, held at two ends (supports_skirt.md, rule 1, S6, S10): the
// layer below meets its outline, grown by `reach`, in two or more separate
// stretches, or all round it (a lid on a tube). A ledge held along one side,
// or a ring held only inside it (the underside of a sphere), is no bridge.
bool is_bridge(const Polygons& island, const Polygons& below, double reach);

// The part of an overhang island that is bridge, point by point
// (supports_skirt.md, rule 3, S11): a point whose line, along one of 8
// directions (every 22.5°), meets the layer below on both sides without
// leaving `region` (the island's layer) on the way, however far. Both ends
// must meet the layer below's edge within 30° of straight on, so a corner of
// two walls in an L is no bridge. Measured on rows `row` mm apart.
Polygons bridged_part(const Polygons& island, const Polygons& region, const Polygons& below, double row);

// What each layer needs support under: the overhangs the automatic support
// finds (with automatic on; without under_bridges, not the bridges), plus
// the enforced ones, less the blocked ones.
std::vector<Polygons> supported_overhangs(const std::vector<Polygons>& regions, const std::vector<double>& heights,
                                          const SupportParams& params);

// The support area of every layer: the overhangs above it (those the
// automatic support finds, plus the enforced ones, less the blocked ones)
// The empty layers between support layer l and the part it holds: z_gap
// over the layers' own thicknesses, to the nearest layer (support.z_gap is in
// mm; with variable layer heights a fixed count of layers is not).
std::vector<int> support_gaps_above(const std::vector<double>& thickness, double z_gap);
// The empty layers between part layer l and the support under it.
std::vector<int> support_gaps_below(const std::vector<double>& thickness, double z_gap);

// projected straight down, starting z_gap_layers below the overhang and ending at the bed or where
// the part is below (the support then stands on the part), kept xy_gap away
// from the part; gaps narrower than two beads in it are closed. Own design.
std::vector<Polygons> support_areas(const std::vector<Polygons>& regions, const std::vector<double>& heights,
                                    const SupportParams& params);

// support.buildplate_only (supports_more.md, rule 2), on any support's
// areas (grid or organic): a zone of support (its islands, joined layer to
// layer where they overlap) that stands on the part anywhere, within
// bottom_gap layers, is taken out whole, its part on the bed too.
void keep_from_bed(std::vector<Polygons>& support, const std::vector<Polygons>& regions, int bottom_gap);

// The column's steps over a chunk of layers, on its grid (support_areas):
// from the chunk's last layer down, the column takes adds[k] and is closed by
// `radius` pixels when adds[k] has a grid (an overhang above), then loses
// parts[k]; kept[k] becomes the column after it. The CPU reference of
// gpu::Context::support_column.
void support_column(BitGrid& column, std::span<const BitGrid> parts, std::span<const BitGrid> adds,
                    std::span<BitGrid> kept, int radius);

} // namespace tisma
