#pragma once

#include "geom/polygons.hpp"
#include "mesh/indexed_mesh.hpp"
#include "mesh/paint.hpp"
#include "slice/layers.hpp"

#include <array>
#include <cstdint>
#include <vector>

// Where the painted faces fall in each layer (docs/PAINTING.md). Own design;
// see PROVENANCE.md.
namespace tisma {

using PaintedPiece = std::array<Vec3f, 3>;

// The pieces of the mesh's faces where `channel` has `mark`. Triangle t of the
// painting is triangle t of the mesh, its corners in the same order.
std::vector<PaintedPiece> painted_pieces(const IndexedMesh& mesh, const Painting& painting, PaintChannel channel,
                                         std::uint8_t mark);

// The XY area of each layer the pieces reach into (bottom to top, both
// included): each piece cut to the layer, projected straight down and grown by
// `grow` mm. A painted strip of a wall projects to a line; grown, it is a band
// the wall's loops cross. Empty where nothing is painted. Layers in parallel.
std::vector<Polygons> painted_areas(const std::vector<PaintedPiece>& pieces, const std::vector<Layer>& layers,
                                    double grow);

} // namespace tisma
