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
// Each bar its own diameter (one per bar).
TriangleMesh lattice_mesh(const std::vector<LatticeBar>& bars, const std::vector<double>& diameters);

// Whether a point is inside a closed mesh: the faces a ray straight up from
// it crosses, counted (odd: inside). Own design.
bool inside_mesh(const TriangleMesh& mesh, const std::array<double, 3>& p);

// The bars that touch the part: some of 5 points along each (its ends,
// quarters and middle) inside it; the slicing keeps of them only what is in
// the part, so they reach its walls.
std::vector<LatticeBar> bars_touching(const std::vector<LatticeBar>& bars, const TriangleMesh& part);

// The lattice of cells of side `cell` over the part's box, its bars of
// `diameter` that touch the part, as one mesh (empty when none does).
TriangleMesh lattice_in(const TriangleMesh& part, double cell, double diameter);

// The density of the lattice: the bars' volume over the cell's,
// pi (1 + 4 sqrt 2) (r / a)^2 for cells of side a and bars of radius r.
double lattice_density(double diameter, double a);

} // namespace tisma
