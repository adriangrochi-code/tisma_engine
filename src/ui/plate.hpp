#pragma once

#include "mesh/mesh.hpp"
#include "ui/math.hpp"

#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <vector>

// The plate: the models on the bed, each with where it goes, how it is turned
// and how big it is, and what the Arrange tool does with them. Nothing here
// touches OpenGL, so it is tested like the rest of the engine. Own design; see
// PROVENANCE.md.
namespace tisma::ui {

// A rotation as a row-major 3 x 3 matrix.
using Rotation = std::array<double, 9>;
inline constexpr Rotation kNoRotation{1, 0, 0, 0, 1, 0, 0, 0, 1};

// Where a model goes: scaled along its own axes about the centre of its box,
// then turned, then set down with its base on the bed and the centre of its
// box at (x, y).
struct Placement {
    double x = 0.0, y = 0.0;
    Rotation rotation = kNoRotation;
    std::array<double, 3> scale{1.0, 1.0, 1.0}; // each above 0: a mirror would turn the triangles inside out
};

struct PlateObject {
    std::string name;
    TriangleMesh mesh; // as its file has it
    Placement place;
};

// r turned by `degrees` about the bed's axis 0 (X), 1 (Y) or 2 (Z).
Rotation rotated(const Rotation& r, int axis, double degrees);

// r turned so that a face whose normal, already turned by r, is `normal` lies
// on the bed (the normal points straight down).
Rotation laid_on(const Rotation& r, Vec3 normal);

// The angles in degrees about X, then Y, then Z that give the rotation, for the
// panel's fields (r = Rz * Ry * Rx).
std::array<double, 3> euler_degrees(const Rotation& r);
Rotation from_euler_degrees(const std::array<double, 3>& degrees);

// The model's mesh as placed on the bed.
TriangleMesh placed_mesh(const PlateObject& object);

// Every placed mesh in one, as the slicer takes the plate.
TriangleMesh plate_mesh(const std::vector<TriangleMesh>& placed);

// The size of the box of the model, scaled and turned (mm).
std::array<double, 3> placed_size(const PlateObject& object);

// Arrange (docs/spec/features/placement.md, rule 2): the centres for boxes of
// these sizes (x, y) in a compact grid, `spacing` mm between boxes, the whole
// set centred on `centre`. Rows of about the square root of the count, filled
// with the deepest boxes first; nothing is turned or scaled.
std::vector<std::array<double, 2>> arrange(const std::vector<std::array<double, 2>>& sizes,
                                           std::array<double, 2> centre, double spacing);

// What a ray from the eye hits first among the placed meshes: the model, the
// triangle and how far along the ray (direction of unit length).
struct PlateHit {
    std::size_t object = 0, triangle = 0;
    double distance = 0.0;
};
std::optional<PlateHit> pick(const std::vector<TriangleMesh>& placed, Vec3 origin, Vec3 direction);

// The unit normal of a triangle of a mesh (from its winding).
Vec3 triangle_normal(const TriangleMesh& mesh, std::size_t triangle);

} // namespace tisma::ui
