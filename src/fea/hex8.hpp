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
// box of sides hx, hy, hz (mm) and Young's modulus 1: for modulus E the
// stiffness is E K. Own implementation; see PROVENANCE.md.
struct Hex8 {
    std::array<double, 24 * 24> stiffness; // row-major
    std::array<double, 6 * 24> b_centre;   // strains at the centre per unit corner displacement
    std::array<double, 6 * 6> elasticity;  // D for E = 1: stresses from strains (xx, yy, zz, xy, yz, zx; engineering shear)
};

Hex8 hex8(double poisson, double hx = 1.0, double hy = 1.0, double hz = 1.0);

// The same element for any elasticity matrix D (6 x 6, row-major, in the
// order above), for example an orthotropic material at some angle; the
// stiffness comes out with D's own modulus.
Hex8 hex8(const std::array<double, 36>& d, double hx, double hy, double hz);

// The von Mises stress of a stress vector (xx, yy, zz, xy, yz, zx).
double von_mises(const double stress[6]);

} // namespace tisma
