#pragma once

#include <array>

namespace tisma {

// The eight corners of a voxel element, as offsets from its lowest corner,
// in this order. Degree of freedom 3 a + d is corner a's displacement along
// axis d (x, y, z).
inline constexpr int kHexCorner[8][3] = {{0, 0, 0}, {1, 0, 0}, {1, 1, 0}, {0, 1, 0},
                                         {0, 0, 1}, {1, 0, 1}, {1, 1, 1}, {0, 1, 1}};

// The trilinear hexahedron (8 nodes, 24 degrees of freedom) for isotropic
// linear elasticity, from the textbook formulation (Hughes, The Finite Element
// Method, 2000, Ch. 3; Zienkiewicz and Taylor, Vol. 1, Ch. 6): stiffness
// K = sum over the 2 x 2 x 2 Gauss points of B^T D B det J. Computed for a
// cube of side 1 and Young's modulus 1; for side h and modulus E the
// stiffness is E h K. Own implementation; see PROVENANCE.md.
struct Hex8 {
    std::array<double, 24 * 24> stiffness; // row-major
    std::array<double, 6 * 24> b_centre;   // strains at the centre per unit displacement (side 1; divide by h)
    std::array<double, 6 * 6> elasticity;  // D for E = 1: stresses from strains (xx, yy, zz, xy, yz, zx; engineering shear)
};

Hex8 hex8(double poisson);

// The von Mises stress of a stress vector (xx, yy, zz, xy, yz, zx).
double von_mises(const double stress[6]);

} // namespace tisma
