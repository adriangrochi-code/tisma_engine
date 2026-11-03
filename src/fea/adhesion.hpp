#pragma once

// How well the printed lines stick together: between layers (axis 3) and
// between neighbouring lines in a layer (axis 2), from the material, the
// nozzle temperature, the layer height against the line width and, when
// known, the layer time, the fan and the chamber (the owner's request,
// 2026-11-03). The model is docs/research/interlayer_adhesion.md §5 (level
// A) and docs/research/bead_adhesion.md §5.2 (the bond quality's
// temperature factor); every constant there is an estimate fitted to the
// measured data and data sheets it names. The materials' constants are
// measured at their sources' settings (a 0.2 mm layer, a line of about
// 0.42 mm, the recommended temperature), so the factors here are relative to
// those settings: 1 there. The result is an approximation and carries its
// uncertainty. Own design; see PROVENANCE.md.

#include "fea/material.hpp"

#include <limits>
#include <string_view>

namespace tisma {

struct MaterialFamily;

// What the analysis knows of how the part is printed. NaN: not known (the
// factor for it is then 1).
struct PrintConditions {
    double nozzle_temperature = std::numeric_limits<double>::quiet_NaN(); // °C
    double layer_height = 0.2;                                           // mm
    double line_width = 0.42;                                            // mm
    double layer_time = std::numeric_limits<double>::quiet_NaN();         // s
    double fan = std::numeric_limits<double>::quiet_NaN();                // 0 to 1
    double chamber = std::numeric_limits<double>::quiet_NaN();            // °C
    // Single walls and perimeter-only zones: their grooves are not filled by
    // the neighbouring lines, so the layer height counts more.
    bool single_walls = false;
};

struct Adhesion {
    bool modelled = false; // false: no data for the family; every factor 1
    // Times the material's constants at its reference settings.
    double z_strength = 1.0;      // tension and shear across the layers
    double z_stiffness = 1.0;     // E3, G13, G23
    double across_strength = 1.0; // tension across the lines in a layer, and in-layer shear
    // Strength across the layers over strength in the layer, as predicted.
    double z_ratio = 0.0;
    // The 1 sigma relative uncertainty of the strengths (0.25: ±25 %).
    double z_uncertainty = 0.5, across_uncertainty = 0.5;
    // The reference nozzle temperature the factors are relative to (°C) and
    // the temperature used (the reference when not known).
    double reference_temperature = 0.0, temperature = 0.0;
    // The temperature is outside what the family prints well at: below
    // where the layers bond, or above where the polymer degrades.
    bool too_cold = false, too_hot = false;
};

// The factors for a material family (null or unknown: not modelled).
Adhesion adhesion(const MaterialFamily* family, const PrintConditions& print);

// The material with the factors applied: zt, s13, s23 by z_strength; e3,
// g13, g23 by z_stiffness; yt, s12 by across_strength. Compression is left
// as it is (it closes the bond rather than opening it).
void apply_adhesion(OrthotropicMaterial& m, const Adhesion& a);

// The smallest safety factor's band from the strengths' uncertainty when the
// part fails at a bond (between layers or between lines): ±1 sigma of the
// governing strength. For a failure in the line itself the material's own
// scatter, ±10 %.
struct SafetyBand {
    double low = 0.0, high = 0.0;
};
SafetyBand safety_band(double min_safety, FailureMode mode, const Adhesion& a);

} // namespace tisma
