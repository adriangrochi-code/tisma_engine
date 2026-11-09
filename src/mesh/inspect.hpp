#pragma once

// A mesh's summary and its flat faces, for the command line
// (docs/spec/features/orca_2_5.md, rules 25 and 26).

#include "mesh/mesh.hpp"

#include <array>
#include <cstddef>
#include <optional>
#include <string>

namespace tisma {

// What --inspect-mesh writes (rule 26): the box's size in mm, the volume in
// mm3, the triangles, whether every edge joins exactly two triangles
// (closed), the edges that do not, and the separate parts (triangles joined
// by shared corners).
struct MeshInspection {
    std::array<double, 3> size{0.0, 0.0, 0.0};
    double volume = 0.0;
    std::size_t triangles = 0;
    bool closed = false;
    std::size_t open_edges = 0;
    std::size_t parts = 0;
};
MeshInspection inspect_mesh(const TriangleMesh& mesh);
std::string inspection_json(const MeshInspection& inspection);

// The outward normal of the largest flat face (rule 25): the triangles in one
// plane (normals within about 0.5 degrees, the plane within 0.01 mm), their
// areas added. None for an empty mesh.
std::optional<std::array<double, 3>> largest_flat_face(const TriangleMesh& mesh);

// The outward normal of the triangle the point lies on (within 0.01 mm);
// none when no triangle holds it.
std::optional<std::array<double, 3>> face_through(const TriangleMesh& mesh, const std::array<double, 3>& point);

// The mesh turned so that `normal` points straight down, onto the bed (the
// shortest rotation; a normal already down leaves it as it is).
TriangleMesh laid_on_face(const TriangleMesh& mesh, std::array<double, 3> normal);

} // namespace tisma
