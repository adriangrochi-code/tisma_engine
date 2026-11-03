#pragma once

// A lattice Boltzmann flow solver for the aerodynamic analysis
// (docs/spec/features/aerodynamics.md, «Método», point 1): D3Q19 lattice,
// BGK collision (Qian, d'Humières and Lallemand, Europhysics Letters 17,
// 1992) with Smagorinsky's subgrid viscosity (Monthly Weather Review 91,
// 1963) folded into the relaxation time as Hou, Sterling, Chen and Doolen do
// it (Fields Institute Communications 6, 1996). The flow goes in +x: a fixed
// velocity at the inlet (x = 0), zero gradient at the outlet, periodic sides,
// halfway bounce-back on solid cells. Own implementation; the CPU version is
// the reference (PLAN.md, «Diseño para la visión»). See PROVENANCE.md.

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
};

// Runs the lattice from rest at the inlet velocity everywhere. `progress`
// (0 to 1), when given, is called now and then; returning false stops the
// run early (the result then is not stable).
LbmResult run_lbm(const LbmOptions& options, const std::vector<std::uint8_t>& solid,
                  const std::function<bool(double)>& progress = {});

} // namespace tisma::aero
