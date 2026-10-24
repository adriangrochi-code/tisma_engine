#pragma once

#include "fea/material.hpp"

#include <limits>
#include <span>
#include <string_view>

namespace tisma {

// Printed materials as their sources published them: the literature,
// manufacturers' data sheets and published black-box tests (docs/MATERIALS.md
// lists every source with its link, its printing conditions and what it does
// not say). A record holds only the numbers its source gives; as_printed
// fills the rest of the orthotropic constants by the documented rules.
inline constexpr double kNotGiven = std::numeric_limits<double>::quiet_NaN();

struct MaterialRecord {
    std::string_view id;     // for the settings, e.g. "bambu-pla-basic"
    std::string_view name;   // the material and who measured it
    std::string_view source; // short citation; the full one is in docs/MATERIALS.md
    // Another record whose moduli this one borrows when its source gives none.
    std::string_view moduli_from = {};
    // Tensile moduli (MPa): along the lines, across them in the layer, in the
    // layer for a mixed raster (data sheets' X-Y), across the layers (Z).
    double e_line = kNotGiven, e_across = kNotGiven, e_xy = kNotGiven, e_z = kNotGiven;
    double flex_xy = kNotGiven, flex_z = kNotGiven; // flexural moduli (MPa)
    double nu12 = kNotGiven, nu23 = kNotGiven;
    double g12 = kNotGiven; // shear modulus in the layer (MPa)
    double g_z = kNotGiven; // shear modulus on the layer's plane (MPa)
    // Tensile strengths (MPa), the same directions as the moduli.
    double t_line = kNotGiven, t_across = kNotGiven, t_xy = kNotGiven, t_z = kNotGiven;
    double c_line = kNotGiven, c_across = kNotGiven; // compressive strengths (MPa)
    double shear_xy = kNotGiven, shear_z = kNotGiven; // shear strengths (MPa)
};

std::span<const MaterialRecord> material_records();

// The record with this id, or null.
const MaterialRecord* find_material(std::string_view id);

// The orthotropic constants of a record (axes as in OrthotropicMaterial), the
// values its source does not give filled by these rules, in this order:
// - e1: along the lines, else the mixed raster's, else the flexural modulus;
//   e2: across the lines, else e1 (Song et al. 2017: E22 within 2 % of E11 in
//   dense prints); e3: Z, else the flexural Z, else e2 (Cantrell et al. 2017:
//   the upright modulus equals the flat one).
// - nu12: given, else 0.35 (the sources span 0.33 to 0.38); nu13 = nu12;
//   nu23: given, else nu12.
// - g12: given, else e1 / (2 (1 + nu12)) (Song 2017 and Cantrell 2017 within
//   3 %); g13 = g23: given, else min(e2, e3) / (2 (1 + nu23)).
// - xt: along the lines, else the mixed raster's (all a data sheet gives);
//   yt: across the lines, else
//   kAcrossLines xt but not below a given zt; zt: Z, else kBetweenLayers xt
//   but not above yt (Song 2017: across the layers is "the weakest and most
//   brittle" direction).
// - Compression: given, else equal to the tension (on the safe side: Song
//   2017 measured nearly twice); zc = max(zt, kCompressionAcrossLayers xc).
// - s12: given, else kShearInLayer yt; s13 = s23: given, else
//   kShearBetweenLayers zt.
OrthotropicMaterial as_printed(const MaterialRecord& record);

// The rules' ratios and their sources.
inline constexpr double kAcrossLines = 46.24 / 54.89;      // Song 2017, Table 4: PLA 90 / 0 degrees
inline constexpr double kBetweenLayers = 39.0 / 60.0;      // CNC Kitchen: PLA at 210 C, standing / lying
inline constexpr double kCompressionAcrossLayers = 0.85;   // Ahn et al. 2002: ABS built upright, 15 % lower
inline constexpr double kShearInLayer = 28.8 / 32.8;       // Cantrell 2017, Tables II and IV: ABS flat
inline constexpr double kShearBetweenLayers = 25.8 / 30.0; // Cantrell 2017: ABS upright
inline constexpr double kPoisson = 0.35;

} // namespace tisma
