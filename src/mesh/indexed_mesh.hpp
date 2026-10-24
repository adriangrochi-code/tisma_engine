#pragma once

#include "mesh/mesh.hpp"

#include <array>
#include <cstdint>
#include <vector>

namespace tisma {

// Triangle mesh with shared vertices: triangle t uses vertices[triangles[t][k]].
// Slicing needs shared vertices so that the crossing point on an edge is the
// same object for both triangles of that edge.
struct IndexedMesh {
    std::vector<Vec3f> vertices;
    std::vector<std::array<std::uint32_t, 3>> triangles;

    std::size_t triangle_count() const { return triangles.size(); }
};

// Merges corners with bitwise-equal coordinates (STL repeats each shared vertex
// in every facet that uses it). Triangles keep their order and winding.
// Vertices are numbered in order of first appearance, so the result does not
// depend on thread count. Triangles with two equal corners are kept; callers
// decide what to do with them.
IndexedMesh weld_exact(const TriangleMesh& mesh);

} // namespace tisma
