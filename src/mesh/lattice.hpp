#pragma once

// A lattice printable without support (structural_optimization.md, rule 7):
// cubic cells with vertical bars and crosses at 45 degrees in the XZ and YZ
// planes, no horizontal bars; each bar a six-sided prism, the prisms
// overlapping at the nodes. Own design; see PROVENANCE.md.

#include "mesh/mesh.hpp"

#include <array>
#include <vector>

namespace tisma {

struct LatticeBar {
    std::array<double, 3> from{}, to{}; // mm
};

// The bars of the cells of side `a` filling the box from `lo` to `hi`
// (whole cells from lo, the last ones cut at hi), each bar once: on each cell
// the 4 vertical edges and the 2 diagonals of each of its 4 side faces.
std::vector<LatticeBar> lattice_bars(const std::array<double, 3>& lo, const std::array<double, 3>& hi, double a);

// The bars as closed six-sided prisms of this diameter (across the corners),
// faces out.
TriangleMesh lattice_mesh(const std::vector<LatticeBar>& bars, double diameter);

// The density of the lattice: the bars' volume over the cell's,
// pi (1 + 4 sqrt 2) (r / a)^2 for cells of side a and bars of radius r.
double lattice_density(double diameter, double a);

} // namespace tisma
