#pragma once

#include "fea/analysis.hpp"

#include <iosfwd>

namespace tisma {

// Writes an analysis as a legacy VTK file (Kitware, "VTK File Formats",
// public documentation): a binary rectilinear grid with the voxels' planes as
// coordinates. Cell data: solid (1 analysed, 0 empty, a gap or loose), safety (-1
// where empty, at most 1e9 where unstressed), peak_safety (the same from the
// voxel's own stress), mode (FailureMode's value),
// von_mises (MPa), direction (degrees in [0, 180), -1 unknown). Point data:
// displacement (mm). Opens in ParaView and other VTK readers. Own
// implementation; see PROVENANCE.md.
void write_analysis_vtk(std::ostream& out, const AnalysisReport& report);

} // namespace tisma
