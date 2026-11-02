#pragma once

#include "mesh/mesh.hpp"

namespace tisma {

// Simple closed solids to add to the plate, faces wound outwards, centred on
// the origin in X and Y and standing on Z = 0. Sizes in mm.

// A box sx by sy by sz.
TriangleMesh make_box(double sx, double sy, double sz);

// An upright cylinder of radius r and height h, with `sides` sides (at least 3).
TriangleMesh make_cylinder(double r, double h, int sides = 64);

// A sphere of radius r: `rings` bands from pole to pole (at least 2) and
// `segments` around (at least 3).
TriangleMesh make_sphere(double r, int rings = 32, int segments = 64);

// An upright cone of base radius r, top radius `top` (0: a point) and height
// h, with `sides` sides.
TriangleMesh make_cone(double r, double top, double h, int sides = 64);

// An upright tube of outer radius r, inner radius `inner` (below r) and height
// h, with `sides` sides.
TriangleMesh make_tube(double r, double inner, double h, int sides = 64);

// A torus lying on the bed: `major` from its axis to the middle of its round
// section of radius `minor`, `segments` around the axis and `sides` around
// the section.
TriangleMesh make_torus(double major, double minor, int segments = 64, int sides = 32);

} // namespace tisma
