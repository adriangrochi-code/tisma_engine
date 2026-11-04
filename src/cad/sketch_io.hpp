#pragma once

// 2D drawings from other programs into a sketch, and a sketch out as DXF
// (docs/spec/features/cad_design.md, rules 19 bis and 20; stage D5b). The
// curves land as the sketch's own entities, so they are trimmed, dimensioned
// and extruded like those drawn by hand. Own reader and writer over the
// public formats: Autodesk's DXF Reference (ASCII DXF: group codes,
// HEADER $INSUNITS, ENTITIES, BLOCKS and INSERT) and the W3C's SVG 1.1
// (shapes, path data, transforms, viewBox). See PROVENANCE.md.

#include "cad/sketch.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace tisma::cad {

struct SketchImport {
    std::vector<Entity> entities; // in mm, on the sketch's plane coordinates
    std::vector<std::string> notes; // what was left out (paper space, text, points...)
    std::string error;              // the file could not be read: nothing to add
    bool ok() const { return error.empty(); }
};

// ASCII DXF: lines, circles, arcs, polylines (with bulges), ellipses and
// splines (as point chains), and blocks inserted with scale, rotation and
// rows and columns, in mm by $INSUNITS. Paper space, text and points are
// left out with a note.
SketchImport read_dxf(std::string_view text);

// SVG: path, rect, circle, ellipse, line, polyline and polygon, with the
// transforms of their groups; user units in mm by the root's width and
// viewBox (px at 96 per inch without them); y turned up.
SketchImport read_svg(std::string_view text);

// The sketch as ASCII DXF (R12, mm): lines and chains as LINE or POLYLINE,
// rectangles as closed POLYLINE, circles and arcs as CIRCLE and ARC;
// construction geometry on its own layer.
std::string write_dxf(const Sketch& sketch);

} // namespace tisma::cad
