#pragma once

#include "config/settings.hpp"
#include "mesh/mesh.hpp"

#include <array>
#include <cstddef>
#include <string>
#include <vector>

namespace tisma {

// Puts a model where the command line prints it (docs/spec/features/
// placement.md, rule 2): with placement.arrange and a bed, the centre of its
// box in X and Y moves to placement.center, or to the centre of the bed's
// box; Z is left alone (the slicer lays the lowest point on the bed). Without
// a bed, or with placement.arrange off, nothing moves (rules 1 and 4).
// `follower` (the negative volumes) moves as the mesh does.
void place_on_bed(TriangleMesh& mesh, const Settings& settings, TriangleMesh* follower = nullptr);

// An object of the plate inside one mesh: its name and its triangles, from
// `first`, `count` of them (the sequential printing slices each apart,
// docs/spec/features/sequential.md). Copies of one model share its name and
// `object`, each its own `instance` (from 0), as the object labels name them
// (gcode_firmware.md, rule 4). Each prints with its extruder, from 1
// (docs/spec/features/multi_extruder.md, rule 1), and a change of filament
// may purge into its sparse infill, or into all of it (rule 13).
struct MeshObject {
    std::string name;
    std::size_t first = 0, count = 0;
    std::size_t object = 0, instance = 0;
    int extruder = 1;
    bool wipe_into_infill = false, wipe_into_object = false;
    friend bool operator==(const MeshObject&, const MeshObject&) = default;
};

// Arrange (placement.md, rule 2): the centres for boxes of these sizes (x,
// y) in a compact grid, `spacing` mm between boxes, the whole set centred on
// `centre`. Rows of about the square root of the count, filled with the
// deepest boxes first; nothing is turned or scaled.
std::vector<std::array<double, 2>> arrange_boxes(const std::vector<std::array<double, 2>>& sizes,
                                                 std::array<double, 2> centre, double spacing);

// Where placement centres what it places: placement.center, or the centre of
// the bed's box (the origin without a bed).
std::array<double, 2> bed_centre(const Settings& settings);

// `copies` of a model in one mesh (placement.md, rule 2; sequential.md, rule
// 2): with placement.arrange and a bed (or placement.center), in a grid with
// placement.spacing between their boxes, at least sequential.clearance_radius
// in sequential printing, the set centred; else on top of one another where
// the model is. Each copy, an instance of object 0 named `name`, in `objects`.
TriangleMesh place_copies(const TriangleMesh& mesh, std::size_t copies, const Settings& settings, const std::string& name,
                          std::vector<MeshObject>* objects);

// Several objects in one mesh (model_tools.md, rules 13 and 14): each
// lowered onto the bed and the set arranged as copies are, with
// placement.spacing between their boxes; object k named `name` and its
// number from 1.
TriangleMesh place_objects(const std::vector<TriangleMesh>& meshes, const Settings& settings, const std::string& name,
                           std::vector<MeshObject>* objects);

} // namespace tisma
