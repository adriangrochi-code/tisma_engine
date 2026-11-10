// The adhesion between layers and between lines; see adhesion.hpp. Own
// design over docs/research/interlayer_adhesion.md §5 and
// docs/research/bead_adhesion.md §5.2; see PROVENANCE.md.

#include "fea/adhesion.hpp"

#include "fea/verdict.hpp"

#include <algorithm>
#include <cmath>

namespace tisma {

namespace {

// interlayer_adhesion.md §5.2 (and §5.3 for delta, k_fan; §5.5 for the
// uncertainty; bead_adhesion.md §5.8 for the uncertainty across the lines).
struct Constants {
    std::string_view family;
    double r_ref, r_min, r_max;  // strength across the layers over in the layer
    double t_low, t_opt, p;      // the healing rise of M_T (°C, °C, exponent)
    double t_hot, beta;          // the loss above t_hot (1/°C)
    double tg;                   // °C (0: none given)
    double delta;                // chamber gain (0: none, or a semicrystalline polymer)
    double k_fan, fan_ref;       // fan loss, and the fan of the reference specimens (0 to 1)
    double chamber_ref;          // °C of the reference specimens
    double sigma_z, sigma_across;
    bool semicrystalline;        // no layer time or chamber law (§5.3)
};

constexpr Constants kConstants[] = {
    {"PLA", 0.80, 0.20, 0.95, 175, 230, 0.25, 245, 0.008, 60, 0.0, 0.10, 1.0, 25, 0.25, 0.30, false},
    {"PETG", 0.75, 0.25, 0.90, 190, 245, 0.35, 248, 0.02, 80, 0.3, 0.25, 0.5, 25, 0.25, 0.30, false},
    {"ABS", 0.70, 0.35, 0.95, 215, 255, 0.30, 275, 0.01, 105, 0.75, 0.25, 0.0, 47.5, 0.25, 0.30, false},
    {"ASA", 0.72, 0.35, 0.90, 225, 260, 0.30, 280, 0.01, 100, 0.75, 0.25, 0.0, 47.5, 0.25, 0.30, false},
    {"PC", 0.65, 0.30, 1.00, 245, 275, 0.30, 300, 0.01, 145, 0.55, 0.25, 0.0, 47.5, 0.25, 0.50, false},
    {"PA", 0.65, 0.30, 0.90, 230, 270, 0.50, 300, 0.01, 60, 0.0, 0.25, 0.0, 25, 0.25, 0.50, true},
    {"PA-CF", 0.55, 0.30, 0.80, 260, 290, 0.50, 310, 0.01, 60, 0.0, 0.25, 0.0, 25, 0.25, 0.50, true},
    // No row of its own: PC's temperatures, the fibre grades' ratio
    // (PET-CF and PA-CF rows), and the wider band.
    {"PC-CF", 0.55, 0.30, 0.80, 255, 285, 0.30, 305, 0.01, 145, 0.55, 0.25, 0.0, 47.5, 0.50, 0.50, false},
    {"PEI", 0.60, 0.35, 0.80, 340, 370, 0.30, 400, 0.01, 186, 0.0, 0.25, 0.0, 25, 0.50, 0.50, false},
    {"PEEK", 0.20, 0.07, 0.30, 360, 410, 0.30, 440, 0.01, 143, 0.0, 0.25, 0.0, 25, 0.50, 0.50, true},
    {"TPU", 0.75, 0.40, 0.85, 205, 230, 0.25, 250, 0.01, 0, 0.0, 0.10, 1.0, 25, 0.30, 0.30, false},
};

const Constants* constants_of(std::string_view family) {
    for (const Constants& c : kConstants)
        if (c.family == family)
            return &c;
    return nullptr;
}

// §5.3, nozzle temperature: the healing rise, then the loss when too hot.
double m_temperature(const Constants& c, double t) {
    const double rise = std::pow(std::clamp((t - c.t_low) / (c.t_opt - c.t_low), 0.0, 1.0), c.p);
    return rise * std::max(0.0, 1.0 - c.beta * std::max(0.0, t - c.t_hot));
}

// §5.3, layer time and fan (amorphous polymers).
double m_layer_time(const Constants& c, double layer_time, double fan) {
    if (c.semicrystalline || !std::isfinite(layer_time))
        return 1.0;
    const double f = std::isfinite(fan) ? std::clamp(fan, 0.0, 1.0) : c.fan_ref;
    const double critical = 15.0 - 7.0 * f;
    return layer_time <= critical ? 1.0 : std::max(0.6, 1.0 - 0.25 * std::log(layer_time / critical));
}

double m_fan(const Constants& c, double fan) { return 1.0 - c.k_fan * std::clamp(fan, 0.0, 1.0); }

// §5.3, chamber below Tg (amorphous polymers).
double m_chamber(const Constants& c, double chamber) {
    if (c.semicrystalline || c.delta <= 0.0 || c.tg <= 25.0 || !std::isfinite(chamber))
        return 1.0;
    const auto x = [&](double t) { return std::clamp((t - 25.0) / (c.tg - 25.0), 0.0, 1.0); };
    return (1.0 + c.delta * x(chamber)) / (1.0 + c.delta * x(c.chamber_ref));
}

// §5.3, layer height over line width.
double m_geometry(double h_over_w, bool single_walls) {
    const double kappa = single_walls ? 0.78 : 0.25;
    return std::clamp(1.0 - kappa * std::pow(std::max(h_over_w, 0.0), 1.5), 0.2, 1.0);
}

// bead_adhesion.md §5.2, the bond quality's temperature factor f_T: 1 at
// the reference, 1.5 % less per °C below it down to 0.6, flat for 20 °C
// above, then 1 % less per °C.
double across_temperature(double t, double reference) {
    const double d = t - reference;
    if (d < 0.0)
        return std::max(0.6, 1.0 + 0.015 * d);
    if (d <= 20.0)
        return 1.0;
    return std::max(0.3, 1.0 - 0.01 * (d - 20.0));
}

} // namespace

Adhesion adhesion(const MaterialFamily* family, const PrintConditions& print) {
    Adhesion a;
    const Constants* found = family ? constants_of(family->name) : nullptr;
    if (!found)
        return a;
    Constants c = *found;
    // PEEK in a chamber at 200 °C or more: the other row of §5.2.
    if (c.family == "PEEK" && std::isfinite(print.chamber) && print.chamber >= 200.0) {
        c.r_ref = 0.5;
        c.r_min = 0.2;
        c.r_max = 0.9;
    }
    a.modelled = true;
    // The reference temperature: the middle of the family's nozzle range in
    // the spec's table, where its data sheets print.
    a.reference_temperature = 0.5 * (family->nozzle_min + family->nozzle_max);
    a.temperature = std::isfinite(print.nozzle_temperature) && print.nozzle_temperature > 0.0 ? print.nozzle_temperature
                                                                                              : a.reference_temperature;
    a.too_cold = a.temperature < c.t_low;
    a.too_hot = a.temperature > c.t_hot;

    const double fan = std::isfinite(print.fan) ? print.fan : c.fan_ref;
    const double healing = m_temperature(c, a.temperature) * m_layer_time(c, print.layer_time, print.fan) * m_fan(c, fan) *
                           m_chamber(c, print.chamber);
    const double healing_ref = m_temperature(c, a.reference_temperature) * m_fan(c, c.fan_ref);
    const double width = print.line_width > 0.0 ? print.line_width : 0.42;
    const double height = print.layer_height > 0.0 ? print.layer_height : 0.2;
    const double geometry = m_geometry(height / width, print.single_walls) / m_geometry(0.2 / 0.42, false);

    const double relative = healing_ref > 0.0 ? healing / healing_ref * geometry : 0.0;
    a.z_ratio = std::clamp(c.r_ref * relative, c.r_min, c.r_max);
    a.z_strength = a.z_ratio / c.r_ref;
    // §5.1: E_z keeps 85 % without any healing; the bond's contact width
    // lowers it as its square root (bead_adhesion.md §5.3, n_E = 0.5).
    const double stiff = (0.85 + 0.15 * std::min(1.0, healing)) / (0.85 + 0.15 * std::min(1.0, healing_ref));
    a.z_stiffness = stiff * std::sqrt(geometry);
    a.across_strength = across_temperature(a.temperature, a.reference_temperature);
    a.z_uncertainty = c.sigma_z;
    a.across_uncertainty = c.sigma_across;
    if (a.too_cold || a.too_hot) {
        a.z_uncertainty = std::max(a.z_uncertainty, 0.5);
        a.across_uncertainty = std::max(a.across_uncertainty, 0.5);
    }
    return a;
}

void apply_adhesion(OrthotropicMaterial& m, const Adhesion& a) {
    if (!a.modelled)
        return;
    m.zt *= a.z_strength;
    m.s13 *= a.z_strength;
    m.s23 *= a.z_strength;
    m.e3 *= a.z_stiffness;
    m.g13 *= a.z_stiffness;
    m.g23 *= a.z_stiffness;
    m.yt *= a.across_strength;
    m.s12 *= a.across_strength;
}

SafetyBand safety_band(double min_safety, FailureMode mode, const Adhesion& a, double discretization) {
    double sigma = 0.0;
    switch (mode) {
    case FailureMode::BetweenLayers:
        sigma = a.modelled ? a.z_uncertainty : 0.5;
        break;
    case FailureMode::BetweenLines:
        sigma = a.modelled ? a.across_uncertainty : 0.5;
        break;
    case FailureMode::Line:
        sigma = 0.10;
        break;
    case FailureMode::None:
        break;
    }
    if (mode != FailureMode::None && std::isfinite(discretization))
        sigma = std::sqrt(sigma * sigma + discretization * discretization);
    sigma = std::min(sigma, 0.9);
    return {min_safety * (1.0 - sigma), min_safety * (1.0 + sigma), sigma};
}

} // namespace tisma
