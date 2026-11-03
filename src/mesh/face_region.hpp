#pragma once

#include "mesh/mesh.hpp"

#include <cstdint>
#include <vector>

// The face of a part a click picks (engineering_panel.md, rule 5: «la parte
// plana o lisa hasta los bordes marcados»): the triangles reached from the
// clicked one across edges that bend less than `max_bend` degrees, so a
// cube's side stops at its edges and a cylinder's wall is one face. Own
// design; see PROVENANCE.md.
namespace tisma {

// The triangles of the face, `seed` among them, in increasing order. Edges
// are shared by exact vertices (weld_exact); a degenerate triangle joins
// nothing but itself.
std::vector<std::uint32_t> face_region(const TriangleMesh& mesh, std::uint32_t seed, double max_bend = 20.0);

} // namespace tisma
