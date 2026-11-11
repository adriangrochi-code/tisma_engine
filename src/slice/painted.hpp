#pragma once

#include "geom/polygons.hpp"
#include "mesh/indexed_mesh.hpp"
#include "mesh/paint.hpp"
#include "perimeters/sites.hpp"
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

// The zones of each layer painted with a colour (multi_extruder.md, rule 1;
// color_mixing_plus.md, rule 16; own design for the depth): every point of the
// layer's region takes the mark of the nearest face, painted or not (an
// unpainted face keeps the part's own filament), found by growing each
// mark's band at the outline `step` mm at a time, at most `depth` mm in (0: no
// limit). By layer, the marks present with their zone.
struct ColorZone {
    std::uint8_t mark = 0;
    Polygons area;
};
std::vector<std::vector<ColorZone>> color_zones(const IndexedMesh& mesh, const Painting& painting,
                                                const std::vector<Layer>& layers, const std::vector<Polygons>& regions,
                                                double step, double depth);

// A path cut where it crosses the zones, each piece with the zone's mark or
// 0 outside them; pieces of at most `step` mm are told apart by their middle,
// so a border is placed within half a step.
std::vector<std::pair<std::uint8_t, Toolpath>> split_by_zones(const Toolpath& path, const std::vector<ColorZone>& zones,
                                                              double step);

} // namespace tisma
