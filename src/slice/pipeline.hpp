#pragma once

#include "config/settings.hpp"
#include "geom/polygons.hpp"
#include "perimeters/walls.hpp"
#include "infill/rectilinear.hpp"
#include "surfaces/surfaces.hpp"
#include "slice/color_mix.hpp"
#include "slice/curved.hpp"
#include "slice/layers.hpp"
#include "slice/local_layers.hpp"
#include "slice/variable_layers.hpp"
#include "slice/zaa.hpp"
#include "infill/stress_density.hpp"
#include "mesh/paint.hpp"
#include "slice/placement.hpp"

#include <optional>
#include <string>
#include <span>
#include <vector>

namespace tisma {

namespace gpu {
class Context;
}

// An object of the plate as sliced (docs/spec/features/sequential.md): its
// name, its outline on the bed (the convex hull of its triangles) and how
// tall it is. Printed object by object, each has its own layers, from
// first_layer up to the next object's, its own raft, skirt and brim.
struct SlicedObject {
    std::string name;
    std::size_t object = 0, instance = 0; // as MeshObject
    // How the labels and messages name it: "<name> (Instance <instance + 1>)".
    std::string label() const { return name + " (Instance " + std::to_string(instance + 1) + ")"; }
    Polygon outline;
    double height = 0.0;
    std::size_t first_layer = 0;
    std::size_t raft_layers = 0;
    std::vector<Toolpath> skirt, brim;
    bool wipe_into_infill = false, wipe_into_object = false; // as MeshObject (multi_extruder.md, rule 13)
};

// The planar slicing of a model: one region per layer, with its walls.
struct SlicedModel {
    std::vector<Layer> layers;
    std::vector<Polygons> regions; // one per layer, after fill mode and offsets
    std::vector<LayerWalls> walls; // one per layer: wall toolpaths and the region left for infill
    std::vector<LayerSurfaces> surfaces; // one per layer: the infill region as top, bottom and sparse
    std::vector<LayerFill> fills;        // one per layer: solid and sparse infill toolpaths
    std::vector<Toolpath> skirt; // around the first layer, on the first adhesion.skirt_layers layers
    std::vector<Toolpath> brim;  // around the first layer, on the first layer
    std::vector<Polygons> support_areas; // one per layer (empty unless support.enabled): where support goes
    // One per layer: where a wall's bead lies wholly off the layer below, over
    // the air or the support (walls_extra.md, rule 8); the region past the
    // layer below's grown by half a bead. Empty on the first layer.
    std::vector<Polygons> hanging;
    // The part's first layer over the support (supports_skirt.md, rule 4), one
    // per layer: where it rests on the support's top, and how far under the
    // layer's bottom that top is (mm: the Z gap).
    std::vector<Polygons> on_support;
    std::vector<double> support_gap;
    // One per layer: its bridges over the air or the support (find_bridges),
    // not the solid layers over the sparse infill, for the solid infill right
    // above them (speeds_accels.md, rule 2).
    std::vector<Polygons> air_bridges;
    // Painted seams (docs/PAINTING.md), one area per layer; empty when none was painted.
    std::vector<Polygons> seam_enforce, seam_block;
    std::size_t open_contours = 0; // contours that stayed open after gap closing (dropped)
    std::size_t cut_away = 0;      // layers the negative volumes took all of (model_tools.md, rule 2; copied layers not counted)
    std::vector<std::size_t> open_by_layer; // the same per layer
    std::vector<double> thin_area; // per layer, mm2 of the region thinner than the thinnest bead (not printed)
    // Layer heights by zone (docs/LOCAL_LAYERS.md): the layers above are the
    // fine ones; each group of them prints its thick zone once, at its top.
    std::vector<LayerGroup> groups;
    std::vector<ThickLayer> thick; // one per layer, empty but at a group's top
    std::size_t zaa_points = 0;    // points Z anti-aliasing moved off their layer's top (docs/ZAA.md)
    // The raft's layers (skirt_brim_raft.md, rule 10): the first ones, before
    // the part's; their regions are the raft's, their fills its support.
    std::size_t raft_layers = 0;
    // The plate's objects, when the caller gave them; with `sequential`,
    // printed one after another, their layers one after the other's here.
    std::vector<SlicedObject> objects;
    bool sequential = false;
    // Settings of a part or a modifier the slicing left out, said so (a
    // modifier's layer height is the object's: model_tools.md, rules 4 and 9).
    std::vector<std::string> notices;
    // Curved layers (nonplanar.md): the field the layers were sliced
    // through; the layers above are flat in its space, and the G-code puts
    // every point back on its curve.
    std::optional<CurvedField> curved;
};

// The mesh as the compensation settings ask (docs/CALIBRATION.md): scaled by
// compensation.scale_x, _y and _z around the centre of its footprint and its
// bottom, so it stays where it was on the bed, and grown by the filament's
// shrinkage. The mesh itself when the scales are all 100 % and nothing shrinks.
IndexedMesh compensated(const IndexedMesh& mesh, const Settings& settings);

// The convex hull of points seen from above, counter-clockwise: an object's
// outline on the bed (the labels, sequential printing).
Polygon outline_of(std::span<const Vec3f> points);
// Another mesh (a negative volume, a modifier) moved as compensated() moves
// `part`: about the part's centre and bottom.
IndexedMesh compensated_like(const IndexedMesh& mesh, const IndexedMesh& part, const Settings& settings);

// The diameter of the round bead of bridges and overhang perimeters
// (gcode_motion.md, rule 1, "unos 0,40 mm" with a 0.4 nozzle): the nozzle's,
// times the root of extrusion.bridge_flow_ratio (speeds_accels.md, rule 14).
double bridge_bead(const Settings& settings);

// Slices a welded mesh with the given settings: layer plan, level-set
// contours with gap closing, simplification, regions by fill mode, XY offset,
// the extra first-layer shrink, the walls of every region, the top, bottom
// and sparse parts of what the walls leave, and their infill toolpaths (all
// layers in parallel). The mesh is compensated first, and the holes of every
// region offset by compensation.hole_offset. Uses the GPU when gpu is not null.
// With a stress field (infill.stress_density, docs/INFILL.md), the sparse
// infill goes denser where the part works hardest. With a painting (its
// triangle t on the mesh's triangle t; docs/PAINTING.md), the support follows
// the painted enforcers and blockers and the seams their painted areas.
// With the plate's objects (their triangles in the mesh), each one's outline
// is kept, and with sequential.enabled and more than one, each is sliced
// apart and their layers follow one another (sequential.md, rule 1). An
// object whose extruder is the number of one of `mixes` prints with that
// blend: each layer with the filament its cycle gives (color_mixing.md);
// a blend this machine cannot print is left out (rule 5).
// The negative volumes (model_tools.md, rule 2), in the mesh's coordinates,
// take their volume away from every layer they cross; they add no layers.
// The modifiers (rule 3) give the zone they share with their object's parts
// their settings: changing the walls, that zone has walls of its own.
SlicedModel slice_model(const IndexedMesh& mesh, const Settings& settings, gpu::Context* gpu = nullptr,
                        const StressField* stress = nullptr, const Painting* painting = nullptr,
                        const std::vector<MeshObject>* objects = nullptr, const std::vector<ColorMix>* mixes = nullptr,
                        const IndexedMesh* negative = nullptr, const std::vector<MeshModifier>* modifiers = nullptr);

// Object k of a model sliced object by object, as a model of its own (its
// layers from 0), for what looks at a stack of layers (the stability review).
SlicedModel object_part(const SlicedModel& model, std::size_t k);

} // namespace tisma
