#pragma once

#include "config/settings.hpp"
#include "geom/polygons.hpp"
#include "infill/rectilinear.hpp"
#include "perimeters/sites.hpp"

#include <cstddef>
#include <vector>

// Layer heights by zone (docs/LOCAL_LAYERS.md): thick layers where the part
// keeps its shape, fine sub-layers only where it changes, so two places at
// the same height can have layers of different heights. After Tyberg and Bøhn
// 1998 ("Local adaptive slicing") and Mani, Kulkarni and Dutta 1999
// ("Region-based adaptive slicing"); the zones from the difference of the
// sections and the clipping of the toolpaths are own design. See PROVENANCE.md.
namespace tisma {

struct LocalLayerParams {
    int ratio = 2;           // fine sub-layers per thick layer (2 to 4)
    double tolerance = 0.05; // mm of step accepted: narrower differences ask for no fine layers
    double margin = 1.35;    // mm the fine zone grows around the detail
    double min_coarse = 0.9; // mm: thick pieces narrower than this go fine (not worth a thick bead)
};

// A thick layer: the fine layers first to last, and the zone printed once,
// thick, at the top of the last one.
struct LayerGroup {
    std::size_t first = 0, last = 0; // fine layers, inclusive
    Polygons coarse;                 // empty: every part of the group prints fine
};

// Groups the fine layers after the first (which stays on its own) in runs of
// params.ratio, and finds each group's thick zone: where the sections of all
// its layers agree, away from where they differ (by more than the tolerance)
// by the margin. A last group shorter than the ratio has no thick zone.
std::vector<LayerGroup> plan_layer_groups(const std::vector<Polygons>& regions, const LocalLayerParams& params);

// What a thick layer prints at the top of its last fine layer: the parts of
// that layer's walls and fills in the thick zone (support stays fine).
struct ThickLayer {
    double height = 0.0; // mm: the whole group
    std::vector<Toolpath> walls;
    LayerFill fill;
    bool empty() const {
        return walls.empty() && fill.top.empty() && fill.solid.empty() && fill.sparse.empty() && fill.bridge.empty() &&
               fill.gaps.empty();
    }
};

// The thinnest sub-layer, mm.
inline constexpr double kThinnestSubLayer = 0.04;

// The fine sub-layers per thick layer for the settings: layer_height over
// detail_height, rounded and kept from 2 to 4, and fewer if the sub-layers
// would be thinner than kThinnestSubLayer; 1 (off) when detail_height is 0,
// not below layer_height, or no two sub-layers fit.
int detail_ratio(const Settings& settings);

// Splits every group's layers at its thick zone: the fine layers keep what
// is outside it, and the group's last layer gets, as its thick layer, what is
// inside it (the fills reach `overlap` mm past the zone's edge, so the two
// sides bond). Returns one ThickLayer per fine layer (empty for most).
std::vector<ThickLayer> split_thick_zones(const std::vector<LayerGroup>& groups, std::vector<LayerWalls>& walls,
                                          std::vector<LayerFill>& fills, const std::vector<double>& group_heights,
                                          double overlap);

// The parts of the toolpaths inside the zone (or outside it), cut where they
// cross its edge, with the width interpolated at the cut. A closed path that
// stays on one side stays closed; a cut one becomes open paths, the piece
// through its start joined back into one.
std::vector<Toolpath> clip_toolpaths(const std::vector<Toolpath>& paths, const Polygons& zone, bool inside);

} // namespace tisma
