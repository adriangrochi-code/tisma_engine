// Printed materials from published measurements. Own design; the numbers come
// from the sources named in each record (full citations and links in
// docs/MATERIALS.md); see PROVENANCE.md.

#include "fea/materials.hpp"

#include <algorithm>
#include <cmath>
#include <initializer_list>

namespace tisma {
namespace {

// Data sheets: 100 % infill, X-Y bars lying and Z bars upright (ISO 527).
constexpr std::string_view kBambu = "Bambu Lab technical data sheet";
constexpr std::string_view kEsun = "eSUN technical data sheet";
constexpr std::string_view kCncKitchen = "CNC Kitchen (Stefan Hermann), tensile tests";

constexpr MaterialRecord kRecords[] = {
    // The literature.
    {.id = "song2017-pla",
     .name = "PLA (IMAKR), unidirectional, 0.2 mm layers, 220 C",
     .source = "Song et al. 2017, Materials & Design 123, Tables 4 and 6",
     .e_line = 3980.0, .e_across = 4040.0, .nu12 = 0.34, .nu23 = 0.37, .g12 = 1500.0, .g_z = 1470.0,
     .t_line = 54.89, .t_across = 46.24, .c_line = 98.40, .c_across = 98.08},
    {.id = "casavola2016-pla",
     .name = "PLA, one layer, 0.35 mm layers, 0.7 mm lines, 200 C",
     .source = "Casavola et al. 2016, Materials & Design 90",
     .e_line = 3120.0, .e_across = 2770.0, .nu12 = 0.330, .g12 = 1246.57, .t_line = 50.23, .t_across = 22.49},
    {.id = "casavola2016-abs",
     .name = "ABS, one layer, 0.35 mm layers, 0.7 mm lines, 225 C",
     .source = "Casavola et al. 2016, Materials & Design 90",
     .e_line = 1790.0, .e_across = 1150.0, .nu12 = 0.344, .g12 = 808.58, .t_line = 26.11, .t_across = 6.71},
    {.id = "cantrell2017-abs",
     .name = "ABS, [+45/-45] raster, 0.1 mm layers, 235 C",
     .source = "Cantrell et al. 2017, Rapid Prototyping Journal 23(4), Tables II and IV",
     .e_xy = 1960.0, .e_z = 2040.0, .nu12 = 0.36, .nu23 = 0.36, .g12 = 740.0, .g_z = 670.0, .t_xy = 32.8,
     .t_z = 30.0, .shear_xy = 28.8, .shear_z = 25.8},

    // Manufacturers.
    {.id = "bambu-pla-basic", .name = "Bambu Lab PLA Basic (sheet V3.0)", .source = kBambu,
     .e_xy = 2580.0, .e_z = 2060.0, .t_xy = 35.0, .t_z = 31.0},
    {.id = "bambu-pla-matte", .name = "Bambu Lab PLA Matte (sheet V3.0)", .source = kBambu,
     .e_xy = 1960.0, .e_z = 1670.0, .t_xy = 30.0, .t_z = 22.0},
    {.id = "bambu-pla-tough-plus", .name = "Bambu Lab PLA Tough+ (sheet V3.0)", .source = kBambu,
     .e_xy = 1860.0, .e_z = 1920.0, .t_xy = 34.9, .t_z = 20.9},
    {.id = "bambu-petg-basic", .name = "Bambu Lab PETG Basic (sheet V3.0)", .source = kBambu,
     .e_xy = 2780.0, .e_z = 2550.0, .t_xy = 51.0, .t_z = 35.0},
    {.id = "bambu-petg-hf", .name = "Bambu Lab PETG HF (sheet V1.0)", .source = kBambu,
     .e_xy = 1810.0, .e_z = 1540.0, .t_xy = 34.0, .t_z = 23.0},
    {.id = "bambu-abs", .name = "Bambu Lab ABS (sheet V3.0)", .source = kBambu,
     .e_xy = 2200.0, .e_z = 1960.0, .t_xy = 33.0, .t_z = 28.0},
    {.id = "bambu-asa", .name = "Bambu Lab ASA (sheet V3.0)", .source = kBambu,
     .e_xy = 2450.0, .e_z = 2120.0, .t_xy = 37.0, .t_z = 31.0},
    {.id = "bambu-pla-cf", .name = "Bambu Lab PLA-CF (sheet V2.0)", .source = kBambu,
     .e_xy = 2790.0, .e_z = 2160.0, .t_xy = 38.0, .t_z = 26.0},
    {.id = "bambu-petg-cf", .name = "Bambu Lab PETG-CF (sheet V3.0)", .source = kBambu,
     .e_xy = 2460.0, .e_z = 1340.0, .t_xy = 35.0, .t_z = 29.0},
    {.id = "bambu-pa6-cf", .name = "Bambu Lab PA6-CF (sheet V3.0)", .source = kBambu,
     .e_xy = 4430.0, .e_z = 2170.0, .t_xy = 102.0, .t_z = 48.0},
    {.id = "bambu-paht-cf", .name = "Bambu Lab PAHT-CF, dry (sheet V3.0)", .source = kBambu,
     .e_xy = 3860.0, .e_z = 2180.0, .t_xy = 92.0, .t_z = 47.0},
    {.id = "esun-pla-plus", .name = "eSUN PLA+ (sheet V1.0, 2026)", .source = kEsun,
     .flex_xy = 3259.39, .flex_z = 2873.94, .t_xy = 70.37, .t_z = 36.36},
    {.id = "esun-petg-basic", .name = "eSUN PETG Basic (sheets V1.0 and V2.0)", .source = kEsun,
     .flex_xy = 2636.66, .flex_z = 2207.6, .t_xy = 68.41, .t_z = 35.84},

    // Published black-box tests: the strength of the bond between layers
    // against the printing conditions. Moduli from the same author's
    // three-point bending of the same polymer, or from the literature.
    {.id = "cnckitchen-pla-210", .name = "PLA (dasFilament) at 210 C", .source = kCncKitchen,
     .flex_xy = 3300.0, .t_xy = 60.0, .t_z = 39.0},
    {.id = "cnckitchen-petg-245", .name = "PETG (dasFilament) at 245 C", .source = kCncKitchen,
     .flex_xy = 1900.0, .t_xy = 55.0, .t_z = 32.0},
    {.id = "cnckitchen-petg-clear", .name = "PETG, 0.12 mm layers, 0.5 mm lines, 15 mm/s, no fan",
     .source = kCncKitchen, .flex_xy = 1900.0, .t_xy = 59.0, .t_z = 44.0},
    {.id = "cnckitchen-abs-open", .name = "ABS (eSUN), chamber open", .source = kCncKitchen,
     .moduli_from = "cantrell2017-abs", .t_xy = 39.0, .t_z = 21.0},
    {.id = "cnckitchen-abs-chamber-65", .name = "ABS (eSUN), chamber at 65 C", .source = kCncKitchen,
     .moduli_from = "cantrell2017-abs", .t_xy = 39.0, .t_z = 29.0},
};

// The first value given.
double first(std::initializer_list<double> values) {
    for (double v : values)
        if (!std::isnan(v))
            return v;
    return kNotGiven;
}

bool has_moduli(const MaterialRecord& r) {
    return !std::isnan(first({r.e_line, r.e_across, r.e_xy, r.e_z, r.flex_xy, r.flex_z}));
}

} // namespace

std::span<const MaterialRecord> material_records() { return kRecords; }

const MaterialRecord* find_material(std::string_view id) {
    for (const MaterialRecord& r : kRecords)
        if (r.id == id)
            return &r;
    return nullptr;
}

OrthotropicMaterial as_printed(const MaterialRecord& r) {
    const MaterialRecord* source = &r;
    if (!has_moduli(r) && !r.moduli_from.empty())
        if (const MaterialRecord* other = find_material(r.moduli_from))
            source = other;
    const MaterialRecord& e = *source;
    OrthotropicMaterial m;
    m.e1 = first({e.e_line, e.e_xy, e.flex_xy});
    m.e2 = first({e.e_across, m.e1});
    m.e3 = first({e.e_z, e.flex_z, m.e2});
    m.nu12 = first({e.nu12, kPoisson});
    m.nu13 = m.nu12;
    m.nu23 = first({e.nu23, m.nu12});
    m.g12 = first({e.g12, m.e1 / (2.0 * (1.0 + m.nu12))});
    m.g13 = m.g23 = first({e.g_z, std::min(m.e2, m.e3) / (2.0 * (1.0 + m.nu23))});

    m.xt = first({r.t_line, r.t_xy});
    m.yt = first({r.t_across, std::max(kAcrossLines * m.xt, first({r.t_z, 0.0}))});
    m.zt = first({r.t_z, std::min(kBetweenLayers * m.xt, m.yt)});
    m.xc = first({r.c_line, m.xt});
    m.yc = first({r.c_across, m.yt});
    m.zc = std::max(m.zt, kCompressionAcrossLayers * m.xc);
    m.s12 = first({r.shear_xy, kShearInLayer * m.yt});
    m.s13 = m.s23 = first({r.shear_z, kShearBetweenLayers * m.zt});
    return m;
}

} // namespace tisma
