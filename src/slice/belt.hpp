#pragma once

#include "config/settings.hpp"
#include "gcode/preview.hpp"
#include "mesh/indexed_mesh.hpp"

#include <array>
#include <string>
#include <string_view>

// Belt printers (docs/spec/features/belt_printer.md): the gantry leans at
// belt.angle α over a belt that carries the print away. The layers are the
// planes along the gantry, h apart across them. The part, on the belt (x
// across it, y along it, z over it), is turned about X so that those planes
// lie flat: s = y·sin α + z·cos α across them and v = z·sin α − y·cos α along
// the gantry. It is sliced as on a bed, and the G-code goes to the machine's
// axes at the end. Own design after the spec; see PROVENANCE.md.
namespace tisma {

// The part turned for slicing: first moved along the belt and down onto it
// (its lowest y and z to 0), so that its first layer starts on the belt.
IndexedMesh belt_mesh(const IndexedMesh& mesh, const Settings& settings);

// The machine's axes for a point (x, v) of the layer at s (rule 2): x; the
// belt at s / sin α; the gantry at z / sin α = v + s·cot α. {x, belt, gantry}.
std::array<double, 3> belt_axes(const Settings& settings, double x, double v, double s);

// The G-code of the turned part on the machine's axes: from the first layer
// to the end G-code, every move's Y and Z (v and s) become the gantry and
// the belt, the belt in Z with belt.belt_is_z (the gantry in Y) or the other
// way round; X, E and F stay. ;Z: keeps s (rule 3). The rest passes as it is.
std::string belt_gcode(std::string_view text, const Settings& settings);

// The preview read from that G-code with the part as it lies on the belt:
// the belt's position b and the gantry's g back to y = y0 + b − g·cos α and
// z = g·sin α (y0: where the part starts along the belt), so the layers lean
// at α. The layers keep their ;Z:.
void belt_preview(GcodePreview& preview, const Settings& settings, double y0);

} // namespace tisma
