#pragma once

#include "mesh/indexed_mesh.hpp"

#include <cstdint>
#include <span>
#include <vector>

namespace tisma {

// Identifies a mesh edge by its two vertex indices, smaller one first.
using EdgeKey = std::uint64_t;

inline EdgeKey edge_key(std::uint32_t a, std::uint32_t b) {
    return a < b ? (EdgeKey(a) << 32) | b : (EdgeKey(b) << 32) | a;
}

// Piece of an iso-contour inside one triangle. It runs from the crossing on
// edge_from to the crossing on edge_to; the next piece of the contour is the one
// whose edge_from equals this edge_to.
struct LevelSegment {
    Vec3f from;
    Vec3f to;
    EdgeKey edge_from;
    EdgeKey edge_to;
    std::uint32_t triangle;
};

// Iso-contour pieces of a scalar field given at the mesh vertices (design rule 1
// in PLAN.md: layers are level sets of a field; planar layers use the field z).
//
// A vertex counts as above the level when value >= iso, so a triangle is crossed
// by exactly 0 or 2 of its edges even when vertices lie on the level. Crossing
// points are computed from the edge's vertices in index order, so both triangles
// of an edge get bitwise-equal points.
//
// For a closed mesh wound counter-clockwise seen from outside, with the planar
// field z, outer contours run counter-clockwise seen from +z and holes clockwise.
//
// Appends to out; values must have one entry per vertex.
void level_set_segments(const IndexedMesh& mesh, std::span<const float> values, float iso,
                        std::vector<LevelSegment>& out);

// Same, for the listed triangles only (for example those whose value range
// contains iso).
void level_set_segments(const IndexedMesh& mesh, std::span<const float> values, float iso,
                        std::span<const std::uint32_t> triangles, std::vector<LevelSegment>& out);

// The planar field: the z coordinate of each vertex.
std::vector<float> height_field(const IndexedMesh& mesh);

} // namespace tisma
