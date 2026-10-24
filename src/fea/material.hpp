#pragma once

#include <array>
#include <cstdint>

namespace tisma {

// A printed material is orthotropic (PLAN.md, A1: adhesion between layers and
// between lines). Its axes: 1 along the extruded line, 2 across the lines in
// the layer, 3 across the layers (the build direction). Units: MPa. The
// defaults are a placeholder; measured materials come from materials.hpp.
struct OrthotropicMaterial {
    double e1 = 3500.0, e2 = 3500.0, e3 = 3500.0;     // Young's moduli
    double g12 = 1300.0, g13 = 1300.0, g23 = 1300.0; // shear moduli
    double nu12 = 0.35, nu13 = 0.35, nu23 = 0.35;     // Poisson's ratios (strain in j from stress in i: -nu_ij s_i / E_i)
    // Strengths (positive): tension and compression along each axis, shear in each plane.
    double xt = 50.0, xc = 60.0; // along the line
    double yt = 30.0, yc = 50.0; // across lines: the bond between neighbouring lines
    double zt = 25.0, zc = 50.0; // across layers: the bond between layers
    double s12 = 30.0, s13 = 25.0, s23 = 25.0;
};

// Stresses and strains as (xx, yy, zz, xy, yz, zx) with engineering shear
// strain; in the material's axes the order is (11, 22, 33, 12, 23, 31).
using Voigt = std::array<double, 6>;
using Stiffness = std::array<double, 36>; // row-major 6 x 6

// The isotropic stiffness (Lame's constants from E and nu).
Stiffness isotropic_stiffness(double young, double poisson);

// The stiffness in the material's axes: the inverse of the orthotropic
// compliance (textbook: Jones, Mechanics of Composite Materials, 1999, Ch. 2).
Stiffness local_stiffness(const OrthotropicMaterial& m);

// Global strains to material strains for a line at angle theta (radians) from
// x in the layer, axis 3 along z (rotation of axes, Jones 1999, Ch. 2).
Stiffness strain_rotation(double theta);

// Global stresses to material stresses for a line at angle theta.
Stiffness stress_rotation(double theta);

// The stiffness in global axes for a line at angle theta: T^T D T.
Stiffness global_stiffness(const OrthotropicMaterial& m, double theta);

// The same averaged over every direction in the layer (the layer is
// transversely isotropic when the lines' directions are unknown).
Stiffness layer_average_stiffness(const OrthotropicMaterial& m);

// How many times the stresses could grow before failing by the Tsai-Wu
// criterion (Tsai and Wu 1971), stresses in the material's axes; the
// interaction terms are -1/2 sqrt(F_ii F_jj). Infinity for no stress.
double tsai_wu_safety(const OrthotropicMaterial& m, const Voigt& local_stress);

// What gives way first: the line itself, the bond between neighbouring lines
// (stress across the lines and in-layer shear) or the bond between layers
// (stress across the layers and the shears on the layer's plane). Chosen by
// the largest ratio of a stress to its strength (tension or compression by
// its sign). None for no stress.
enum class FailureMode : std::uint8_t { None, Line, BetweenLines, BetweenLayers };
FailureMode failure_mode(const OrthotropicMaterial& m, const Voigt& local_stress);

} // namespace tisma
