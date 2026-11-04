#pragma once

// A lattice Boltzmann flow solver for the aerodynamic analysis
// (docs/spec/features/aerodynamics.md, «Método», point 1): D3Q19 lattice
// (Qian, d'Humières and Lallemand, Europhysics Letters 17, 1992) with the
// two-relaxation-time collision (Ginzburg, Verhaeghe and d'Humières, Commun.
// Comput. Phys. 3, 2008; Λ = 3/16 puts the bounce-back wall halfway at any
// viscosity) and Smagorinsky's subgrid viscosity (Monthly Weather Review 91,
// 1963) folded into the symmetric relaxation time as Hou, Sterling, Chen and
// Doolen do it (Fields Institute Communications 6, 1996). The flow goes in
// +x: a fixed velocity at the inlet (x = 0), an absorbing layer of rising
// viscosity before a zero-gradient outlet, periodic sides, halfway
// bounce-back on solid cells. The force on the solid comes from the momentum
// the bounced links exchange (Ladd 1994; Mei, Yu, Shyy and Luo, Phys. Rev. E
// 65, 2002), with its pressure part (the links' equilibrium at rest) apart.
// Own implementation; the CPU version is the reference (PLAN.md, «Diseño
// para la visión»). See PROVENANCE.md.

#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

namespace tisma::aero {

struct LbmOptions {
    int nx = 0, ny = 0, nz = 0;
    double velocity = 0.05;   // lattice units: low Mach number
    double viscosity = 0.01;  // lattice units, molecular
    double smagorinsky = 0.16;
    // The two-relaxation-time collision (Λ = 3/16). Off: at the viscosity the
    // analysis runs at (tau within 0.01 of 1/2) the odd rate is near 0 and
    // the cube's drag fell from 1.01 to 0.44 (measured 2026-11-03).
    bool trt = false;
    int sponge = 0;           // cells of the absorbing layer before the outlet
    long steps = 1000;
    long average_from = 666;  // steps after which the density is averaged
};

// The lattice's solid cells (x fastest), the averaged density of every cell
// and whether the run stayed finite.
struct LbmResult {
    std::vector<float> mean_density;
    std::vector<float> mean_velocity; // 3 per cell (x, y, z), lattice units; 0 in solid cells
    long averaged_steps = 0;
    bool stable = true;
    double inlet_density = 1.0; // the averaged density just behind the inlet: the free stream's
    // The mean force on the solid cells over the averaged steps, lattice
    // units: all of it, and its pressure part (from the density next to the
    // wall, against `inlet_density`).
    double force[3] = {0, 0, 0}, pressure_force[3] = {0, 0, 0};
};

// Runs the lattice from rest at the inlet velocity everywhere. `progress`
// (0 to 1), when given, is called now and then; returning false stops the
// run early (the result then is not stable).
LbmResult run_lbm(const LbmOptions& options, const std::vector<std::uint8_t>& solid,
                  const std::function<bool(double)>& progress = {});

} // namespace tisma::aero
