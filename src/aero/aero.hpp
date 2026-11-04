#pragma once

// The aerodynamic analysis of a part as it prints (docs/spec/features/
// aerodynamics.md): drag split into pressure (form) from a lattice Boltzmann
// simulation at a low, resolvable Reynolds number, and friction from flat
// plate correlations at the real one, with the roughness the layers leave.
// Own design over the spec's method; see PROVENANCE.md.

#include "mesh/mesh.hpp"

#include <array>
#include <functional>
#include <string>
#include <vector>

namespace tisma::aero {

enum class AeroQuality { Friction, Fast, Normal, High, Extreme };
// "friction", "fast", "normal", "high" and back (false when unknown).
const char* quality_name(AeroQuality q);
bool quality_named(const std::string& name, AeroQuality& q);
// Lattice cells across the frontal size: 0 (no simulation), 16, 24, 32 and
// 48 (extreme, the owner's request, 2026-11-03).
int quality_cells(AeroQuality q);

struct AeroInput {
    // The part in print coordinates (Z up, the layers along Z), mm, closed.
    TriangleMesh mesh;
    // Where the air goes, in the same coordinates (any length but 0).
    std::array<double, 3> direction{1.0, 0.0, 0.0};
    double speed = 10.0;        // m/s, more than 0 and up to 100
    AeroQuality quality = AeroQuality::Normal;
    double layer_height = 0.2;  // mm: the roughness
    // Air at 20 °C.
    double density = 1.204;      // kg/m3
    double viscosity = 1.516e-5; // m2/s, kinematic
    // For tests and the GPU comparison: other cells across the frontal size
    // (0: the quality's), and how many times the air crosses the domain.
    int cells = 0;
    double flow_throughs = 3.0;
    // Simulate again with two thirds of the cells (about a fifth more time)
    // to see how much the pressure drag depends on the lattice: the error
    // band's resolution part.
    bool resolution_check = true;
    // Stop when the drag settles (relative change over a quarter of a
    // flow-through; 0: never early; below 0: the quality's, 1 % fast to
    // 0.3 % extreme).
    double converge = -1.0;
    // Start the fine lattice from the resolution check's flow. Off: measured
    // 2026-11-04 it saved 10 % of the steps on the cube but moved the
    // unsteady EP9 cube's Cd by 9 % (1.111 to 1.216).
    bool warm_start = false;
};

struct AeroResult {
    bool ok = false;
    std::string error;
    double drag_n = 0.0, cd = 0.0, cd_pressure = 0.0, cd_friction = 0.0;
    double side_force_n = 0.0;     // across the air, from the pressure
    // The pressure drag's sources: the momentum the walls take (before the
    // blockage correction) and the density on the faces (the map's);
    // the simulated total with the low Reynolds number's friction; the
    // domain's blockage (frontal over cross-section).
    double cd_pressure_surface = 0.0, cd_total_simulated = 0.0, blockage = 0.0;
    // The resolution check: the pressure drag with fewer cells.
    bool resolution_checked = false;
    double cd_pressure_coarse = 0.0;
    int cells_coarse = 0;
    // The likely range of the drag coefficient (±1 sigma, relative
    // `uncertainty`): the method's own error (12 %, from the drag of the
    // validation shapes against Hoerner's) with the resolution's change in
    // quadrature; at «friction» quality the correlations' 15 % (and the
    // pressure drag is missing).
    double cd_low = 0.0, cd_high = 0.0, uncertainty = 0.0;
    double frontal_area_mm2 = 0.0; // the part's shadow on a plane across the air
    double frontal_size_mm = 0.0;  // the larger of its two sizes across the air
    double reynolds = 0.0, reynolds_simulated = 0.0;
    bool simulated = false;        // false at «friction» quality: no pressure part
    double ra_mean_um = 0.0;       // area-weighted roughness
    double roughness_share = 0.0;  // the part of the friction the roughness adds (0 to 1)
    // Per triangle of the input mesh: pressure coefficient (−1.5 suction to 1
    // stagnation; 0 when not simulated) and roughness Ra in µm.
    std::vector<float> cp, ra_um;
    // Streamlines of the mean flow (the owner's request, 2026-11-03): from a
    // grid of seeds in front of the part, in the input's coordinates (mm),
    // each point with the pressure coefficient where it passes. Empty when
    // not simulated.
    struct Streamline {
        std::vector<std::array<float, 3>> points;
        std::vector<float> cp;
    };
    std::vector<Streamline> streamlines;
    int cells_x = 0, cells_y = 0, cells_z = 0;
    long steps = 0;
    bool stopped_early = false; // the drag settled before the flow-throughs asked
    bool stable = true;
    double seconds = 0.0;
};

// Runs the analysis. `progress` (0 to 1), when given, is called now and
// then; returning false cancels (the result then says so in `error`).
AeroResult analyze(const AeroInput& input, const std::function<bool(double)>& progress = {});

// Ra of a face, in mm, from its normal's Z part and the layer height: the
// staircase of inclined faces, h |n_z| / 4, at least the bead's rounding
// 0.05 h; walls and flat faces only the rounding.
double face_roughness(double nz, double layer_height);

// The local friction coefficient over [x0, x1] mm from the leading edge (its
// mean there), at speed U m/s, with sand roughness ks mm: laminar
// (Blasius), turbulent smooth or fully rough (Schlichting), turbulent above
// Re_x = 5e5 or when U ks / nu > 300.
double friction_coefficient(double x0, double x1, double speed, double ks, double viscosity);

// The .taero document (cloud.md, rule 10c): format, version, inputs, the
// results and the map per face.
std::string result_json(const AeroResult& r, const AeroInput& in);

} // namespace tisma::aero
