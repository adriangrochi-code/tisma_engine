#pragma once

// The current view of a design as a DXF of lines (docs/spec/features/
// cad_design.md, rule 20: `file.export_view_dxf`; stage D5b): the edges a
// drawing shows (borders, creases and silhouettes), projected along the
// view, the hidden ones dashed on their own layer or left out. Own design
// over the public DXF Reference (R12 TABLES with LTYPE and LAYER, LINE);
// the visibility test casts rays with Möller and Trumbore's ray-triangle
// test. See PROVENANCE.md.

#include "mesh/mesh.hpp"

#include <array>
#include <string>
#include <vector>

namespace tisma::cad {

struct ViewDxfOptions {
    std::array<double, 3> look{0.0, 0.0, -1.0}; // where the camera looks (from the eye into the scene)
    std::array<double, 3> up{0.0, 1.0, 0.0};    // the screen's up
    bool hidden = true;                         // hidden edges dashed (false: left out)
    double crease_degrees = 30.0;               // a fold sharper than this is an edge
};

struct ViewLine {
    double x0 = 0, y0 = 0, x1 = 0, y1 = 0; // mm, on the view's plane
    bool hidden = false;
};

// The drawing's lines: projected orthographically along `look`, visible and
// hidden pieces split where an edge goes behind the part.
std::vector<ViewLine> view_lines(const TriangleMesh& mesh, const ViewDxfOptions& options);

// The lines as ASCII DXF (R12, mm): VISIBLE (continuous) and HIDDEN (dashed)
// layers.
std::string write_view_dxf(const std::vector<ViewLine>& lines);

} // namespace tisma::cad
