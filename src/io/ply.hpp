#pragma once

#include "fea/analysis.hpp"

#include <array>
#include <cstdint>
#include <iosfwd>

namespace tisma {

// The colour of a voxel by its safety factor, in the bands the report counts
// (docs/ANALYSIS.md, "La malla"): red below 1 (fails), orange to 1.5, yellow
// to 2, green from 2; grey where nothing stresses it.
std::array<std::uint8_t, 3> safety_colour(double safety);

// Writes the surface of an analysis' voxels as a binary PLY mesh (Turk, "The
// PLY Polygon File Format", public description): every face between an
// analysed voxel and an empty one, as a quad facing out with its own four
// vertices coloured by safety_colour, and per face the voxel's safety factor
// and failure mode. The header's comments give the voxel counts. Own
// implementation; see PROVENANCE.md.
void write_voxel_mesh_ply(std::ostream& out, const AnalysisReport& report);

} // namespace tisma
