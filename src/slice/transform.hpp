#pragma once

#include "config/settings.hpp"
#include "mesh/mesh.hpp"

#include <array>
#include <vector>

// What can be done to a model before it is sliced (docs/spec/features/
// model_tools.md, rules 11 to 14 and 16): scale, fit in a box, turn, mirror,
// cut with a horizontal plane and split into its loose pieces. Everything
// about the centre of the model's box. Own design on that spec; the cut's
// faces are closed by ear clipping (Eberly, «Triangulation by Ear Clipping»,
// Geometric Tools, 2002).
namespace tisma {

// The centre of the mesh's box.
std::array<double, 3> box_centre(const TriangleMesh& mesh);

// Every corner scaled by (sx, sy, sz) about the box's centre. A negative
// factor mirrors that axis; with an odd number of them the triangles are
// turned round, so that they still face out.
void scale_mesh(TriangleMesh& mesh, double sx, double sy, double sz);

// The factor that makes the mesh as large as fits in a box of this size: the
// smallest of size / the mesh's size over the three axes (rule 11). 1 for an
// empty mesh.
double fit_factor(const TriangleMesh& mesh, const std::array<double, 3>& size);

// Turned by these angles in degrees about X, then Y, then Z, about the box's
// centre (rule 12).
void rotate_mesh(TriangleMesh& mesh, double x_degrees, double y_degrees, double z_degrees);

// Mirrored on one axis (0 X, 1 Y, 2 Z) about the box's centre (rule 16).
void mirror_mesh(TriangleMesh& mesh, int axis);

// A closed mesh cut by the plane z = height (rule 13): what is below and what
// is above, each closed again with the section's faces (holes included). A
// corner on the plane counts as above. Either side is empty when the plane
// misses the mesh.
struct CutResult {
    TriangleMesh below, above;
};
CutResult cut_mesh(const TriangleMesh& mesh, double height);

// The mesh's loose pieces (rule 14), each one closed shell with the shells
// inside it (a hollow part keeps its cavity), in the order they first appear.
std::vector<TriangleMesh> split_mesh(const TriangleMesh& mesh);

// The settings' transform group done to a model, in order (docs/CONFIG.md,
// «transform»): scale and fit, turn, then the cut and the split, which give
// several objects. One object when neither cuts nor splits it.
std::vector<TriangleMesh> apply_transform(TriangleMesh mesh, const Settings& settings);

// Triangles that fill a simple polygon with simple holes inside it (outer
// counter-clockwise, holes clockwise, as the section of a closed mesh comes),
// as indices into the outer's points followed by each hole's, in order. The
// triangles turn counter-clockwise.
std::vector<std::array<std::size_t, 3>> triangulate_polygon(const std::vector<std::array<double, 2>>& outer,
                                                            const std::vector<std::vector<std::array<double, 2>>>& holes);

} // namespace tisma
