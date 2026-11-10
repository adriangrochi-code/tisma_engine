#pragma once

#include "mesh/mesh.hpp"
#include "mesh/paint.hpp"
#include "project/project.hpp"
#include "slice/placement.hpp"
#include "ui/plate.hpp"
#include "ui/slicing.hpp"

#include <vector>

// What a slice of the plate takes, built the same way by the interface and by
// the command line that slices a saved project (cloud.md, rule 18). Own
// design; see PROVENANCE.md.
namespace tisma::ui {

// A project's models as objects of the plate, as the interface opens them:
// each mesh moved by its offset, centred where it stands, with its volumes,
// ranges, painting, extruder and settings. The paintings are moved out;
// the meshes stay, as the file had them.
std::vector<PlateObject> project_objects(std::vector<ProjectModel>& models);

// The slicer's input for the plate, but for its mesh (plate_mesh of the
// placed meshes, which the caller makes once): the painting in that order, each object's triangles and settings, the
// modifiers and negative volumes where the objects stand, and whether the
// slicer takes the objects one by one (several of them, more than one
// extruder, volumes, or own settings or ranges). `placed[i]` is
// placed_mesh(objects[i]).
struct PlateSlice {
    Painting painting;
    std::vector<MeshObject> objects;
    SliceVolumes volumes;
    bool by_object = false;
};
PlateSlice plate_slice(const std::vector<PlateObject>& objects, const std::vector<TriangleMesh>& placed,
                       const Settings& settings);

} // namespace tisma::ui
