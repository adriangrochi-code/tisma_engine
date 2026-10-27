#pragma once

#include "config/settings.hpp"
#include "geom/polygons.hpp"
#include "perimeters/sites.hpp"

#include <array>
#include <cstddef>
#include <vector>

// Several extruders (docs/spec/features/multi_extruder.md): which extruder
// lays each path, the order of the extruders in each layer, and the wipe
// tower where the tool changes prime and purge. Own design; see
// PROVENANCE.md.
namespace tisma {

struct SlicedModel;

// The extruder that lays a path (rule 2): its kind's (`chosen`, one of
// extruders.*), else its part's (1 when it has none) for a part's path;
// for what belongs to no part (`part` false: the support) 0, whichever is
// printing. With one extruder, always 0.
int path_tool(const Toolpath& path, int chosen, bool part, const Settings& settings);

// The extruders the paths of a layer ask for (index 0: some may go with any).
std::array<bool, kMaxExtruders + 1> layer_tools(const SlicedModel& model, std::size_t layer, const Settings& settings);

// The extruders of every layer in the order they print (rule 3): the one
// the layer below ended with first, the rest in order; {0} where no path
// asks for one. Each change is the start of a group whose extruder is not
// the one printing; the first extruder is the lowest of the first layer
// that asks for one.
struct ToolPlan {
    int initial = 0; // the first extruder (from 1); 0 with one extruder
    std::vector<std::vector<int>> order;
    std::array<bool, kMaxExtruders + 1> used{};
    // Per layer, for each group of `order`: the extruder printing before it
    // when that is another (a change), else 0.
    std::vector<std::vector<int>> change_from;
    // With a single-extruder MMU (rule 13), per layer and group: what the
    // change prints first with the next filament to purge, out of the paths
    // not printed yet of the objects that take it (their sparse infill with
    // wipe_into_infill, all of them with wipe_into_object), after at least
    // extrusion.min_purge_on_tower on the wipe tower; and what is left of
    // its purge for the tower, mm3.
    struct Sink {
        const Toolpath* path = nullptr;
        int kind = 0;   // 0 to 4: the fills (solid, top, bridge, gap fill, sparse); kWall... below
        int object = 0; // in SlicedModel::objects
    };
    static constexpr int kWall = 10, kWallGap = 11, kArc = 12, kOverhangWall = 13, kIroning = 14;
    std::vector<std::vector<std::vector<Sink>>> sinks;
    std::vector<std::vector<double>> tower_purge;
};
ToolPlan plan_tools(const SlicedModel& model, const Settings& settings);

// The wipe tower (rules 9 to 13): a rectangle wipe_tower.width wide with its
// corner at (wipe_tower.x, wipe_tower.y), turned wipe_tower.rotation degrees
// about it, as deep as the layer that purges most needs. On every layer up
// to the last change: its outline loop; one block of lines across it per
// change, laid by the next extruder (with a single-extruder MMU as many as
// wipe_tower.purge_volumes asks for that pair, by the filament's
// extrusion.purge_multiplier; with several nozzles kPrimeLines, to prime);
// the rest of the inside filled, dense on the first layer and in columns
// wipe_tower.bridging apart above, so the blocks bridge from column to
// column; and wipe_tower.brim_width of brim on the first layer.
// wipe_tower.extra_spacing spaces the lines more, wipe_tower.extra_flow
// makes them wider. Empty without the tower, with one extruder or with no
// change.
inline constexpr int kPrimeLines = 5;

struct WipeTowerLayer {
    std::vector<std::vector<Toolpath>> blocks; // one per change of the layer, in order
    std::vector<Toolpath> shell;               // the brim, the outline and the fill
    // With wipe_tower.no_sparse_layers (rule 11) only the first layer and
    // those with a change have the tower, each on the one before: its top
    // lies this far under the layer's, where the nozzle goes down to it.
    double drop = 0.0;
};

struct WipeTower {
    double width = 0.0, depth = 0.0; // mm
    Polygon outline;                 // on the bed, with its brim
    std::vector<WipeTowerLayer> layers; // one per layer of the model up to the last change; empty: no tower
    bool empty() const { return layers.empty(); }
};
WipeTower plan_wipe_tower(const SlicedModel& model, const Settings& settings, const ToolPlan& tools);

// The purge from one extruder to another (from 1), mm3: wipe_tower.purge_volumes
// (140 between different ones, 0 to itself), by the next one's
// extrusion.purge_multiplier.
double purge_volume(const Settings& settings, int from, int to);

} // namespace tisma
