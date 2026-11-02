// Involute gears for printing. Own design; see PROVENANCE.md.
//
// The outline of one tooth in polar coordinates around the gear's axis: at a
// radius rho between the base circle and the tip, the involute flank lies at
// half-angle psi(rho) = s / (2 r) + inv(alpha) - inv(alpha_rho) from the
// tooth's middle, where s is the tooth's thickness on the pitch circle of
// radius r, inv(x) = tan x - x, and cos(alpha_rho) = rb / rho. Inside the
// base circle the flank goes straight to the root circle. The outline is a
// single radius for each angle, so the gear is a solid of rings
// (cad/lathe.hpp) with the angles where the outline has its corners.

#include "cad/gears.hpp"

#include "cad/lathe.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>
#include <vector>

namespace tisma::cad {
namespace {

constexpr double kPi = std::numbers::pi;

double radians(double degrees) {
    return degrees * kPi / 180.0;
}

double involute(double a) {
    return std::tan(a) - a;
}

struct Tooth {
    double r = 0, rb = 0, ra = 0, rf = 0, half_pitch = 0; // radii; half the angle of a tooth pitch
    double s = 0;                                          // thickness on the pitch circle
    double alpha = 0;
    double flank_low = 0; // the flank's lowest radius on the involute: max(rb, rf)
    double tip = 0;       // the tip's radius, lowered if the tooth would be pointed
    // The flank's half-angle at radius rho (flank_low <= rho <= tip).
    double psi(double rho) const {
        const double a_rho = std::acos(std::clamp(rb / rho, -1.0, 1.0));
        return s / (2.0 * r) + involute(alpha) - involute(a_rho);
    }
};

Tooth tooth_of(const GearSpec& g) {
    Tooth t;
    t.alpha = radians(g.pressure_angle);
    t.r = g.module * g.teeth / 2.0;
    t.rb = t.r * std::cos(t.alpha);
    t.ra = t.r + g.module;
    t.rf = t.r - 1.25 * g.module;
    t.half_pitch = kPi / g.teeth;
    t.s = kPi * g.module / 2.0 - g.backlash;
    t.flank_low = std::max(t.rb, t.rf);
    t.tip = t.ra;
    // A pointed tooth: its tip comes down to where it keeps a little land.
    if (t.psi(t.tip) < 0.02 / t.tip) {
        double lo = t.flank_low, hi = t.tip;
        for (int i = 0; i < 60; ++i) {
            const double mid = 0.5 * (lo + hi);
            (t.psi(mid) < 0.02 / mid ? hi : lo) = mid;
        }
        t.tip = lo;
    }
    return t;
}

// The radius at a flank half-angle: psi falls as the radius grows.
double flank_radius(const Tooth& t, double half_angle) {
    double lo = t.flank_low, hi = t.tip;
    for (int i = 0; i < 60; ++i) {
        const double mid = 0.5 * (lo + hi);
        (t.psi(mid) > half_angle ? lo : hi) = mid;
    }
    return 0.5 * (lo + hi);
}

} // namespace

GearDims gear_dims(const GearSpec& g) {
    const Tooth t = tooth_of(g);
    GearDims d;
    d.pitch_diameter = 2.0 * t.r;
    d.outside_diameter = 2.0 * t.tip;
    d.root_diameter = 2.0 * t.rf;
    d.base_diameter = 2.0 * t.rb;
    d.circular_pitch = kPi * g.module;
    d.tooth_thickness = t.s;
    return d;
}

double center_distance(const GearSpec& a, int other_teeth) {
    // Helical gears mesh at the transverse module m / cos(beta).
    return a.module * (a.teeth + other_teeth) / 2.0 / std::cos(radians(a.helix_angle));
}

std::string check_gear(const GearSpec& g) {
    if (g.teeth < 6 || g.teeth > 400)
        return "the teeth must be between 6 and 400";
    if (!(g.module >= 0.3 && g.module <= 20.0))
        return "the module must be between 0.3 and 20 mm";
    if (!(g.pressure_angle >= 14.5 && g.pressure_angle <= 30.0))
        return "the pressure angle must be between 14.5 and 30 degrees";
    if (!(g.width >= 0.5 && g.width <= 500.0))
        return "the width must be between 0.5 and 500 mm";
    if (!(g.helix_angle >= 0.0 && g.helix_angle <= 45.0))
        return "the helix angle must be between 0 and 45 degrees";
    if (!(g.backlash >= 0.0 && g.backlash < kPi * g.module / 4.0))
        return "the backlash must be between 0 and a quarter of the circular pitch";
    const double rf = g.module * g.teeth / 2.0 - 1.25 * g.module;
    if (!(g.bore >= 0.0 && g.bore / 2.0 <= rf - std::max(1.0, g.module)))
        return "the bore must leave at least a module (and 1 mm) of material under the teeth";
    return {};
}

double gear_outline_radius(const GearSpec& g, double angle) {
    const Tooth t = tooth_of(g);
    double a = std::fmod(std::abs(angle), 2.0 * t.half_pitch);
    if (a > t.half_pitch)
        a = 2.0 * t.half_pitch - a; // symmetric about the tooth's middle
    if (a <= t.psi(t.tip))
        return t.tip;
    if (a <= t.psi(t.flank_low))
        return flank_radius(t, a);
    return t.rf; // the root, and the straight flank under the base circle
}

TriangleMesh make_gear(const GearSpec& g) {
    if (const std::string why = check_gear(g); !why.empty())
        throw std::invalid_argument(why);
    const Tooth t = tooth_of(g);
    // The angles of one tooth pitch, from the middle of a tooth: the tip,
    // each flank sampled by radius, the straight flank and the root.
    constexpr int kFlank = 10, kTip = 3, kRoot = 4;
    std::vector<std::pair<double, double>> half; // (angle, radius) from the middle outwards
    const double tip_half = t.psi(t.tip), low_half = t.psi(t.flank_low);
    for (int i = 0; i < kTip; ++i)
        half.push_back({tip_half * i / kTip, t.tip});
    for (int i = 0; i <= kFlank; ++i) {
        const double rho = t.tip - (t.tip - t.flank_low) * i / kFlank;
        half.push_back({t.psi(rho), rho});
    }
    const bool straight = t.rf < t.flank_low - 1e-9;
    const double root_from = low_half + (straight ? 1e-6 : 0.0); // the straight flank: the same angle, twice
    for (int i = straight ? 0 : 1; i <= kRoot; ++i)
        half.push_back({root_from + (t.half_pitch - root_from) * i / kRoot, t.rf});
    // The whole pitch: the half mirrored, the middle of the gap shared.
    std::vector<std::pair<double, double>> pitch;
    for (auto it = half.rbegin(); it != half.rend(); ++it)
        if (it->first > 0.0)
            pitch.push_back({-it->first, it->second});
    for (const auto& p : half)
        if (p.first < t.half_pitch - 1e-12)
            pitch.push_back(p);
    std::vector<double> angles, radii;
    for (int k = 0; k < g.teeth; ++k)
        for (const auto& [a, r] : pitch) {
            double at = a + 2.0 * t.half_pitch * k;
            if (at < 0.0)
                at += 2.0 * kPi;
            angles.push_back(at);
            radii.push_back(r);
        }
    // Increasing angles, the radius going with each.
    std::vector<std::size_t> order(angles.size());
    for (std::size_t i = 0; i < order.size(); ++i)
        order[i] = i;
    std::sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) { return angles[a] < angles[b]; });
    std::vector<double> sorted_angles, outline;
    for (const std::size_t i : order)
        sorted_angles.push_back(angles[i]), outline.push_back(radii[i]);
    const Lathe lathe(sorted_angles);

    // The twist: a helix of angle beta turns the outline by z tan(beta) / r;
    // a herringbone turns back from mid width.
    const double rate = (g.left_hand ? -1.0 : 1.0) * std::tan(radians(g.helix_angle)) / t.r;
    const auto turn = [&](double z) {
        return g.herringbone ? rate * (g.width / 2.0 - std::abs(z - g.width / 2.0)) : rate * z;
    };
    std::vector<double> zs;
    const double twist = std::abs(rate) * (g.herringbone ? g.width / 2.0 : g.width);
    // Each step turns less than a fifth of a tooth's tip.
    const int step_count = rate == 0.0 ? 1 : std::max(1, int(std::ceil(twist / std::max(tip_half * 0.4, 1e-3))));
    const int count = g.herringbone ? 2 * ((step_count + 1) / 2) : step_count;
    for (int i = 0; i <= count; ++i)
        zs.push_back(g.width * i / count);
    std::vector<Ring> rings;
    for (const double z : zs)
        rings.push_back({z, outline, turn(z)});
    if (g.bore > 0.0) { // down the bore, a loop
        for (auto it = zs.rbegin(); it != zs.rend(); ++it)
            rings.push_back(lathe.ring(*it, [&](double) { return g.bore / 2.0; }));
        return outward(lathe.mesh(rings, true));
    }
    return outward(lathe.mesh(rings, false));
}

std::string gear_json(const GearSpec& g) {
    nlohmann::json j;
    j["format"] = "tisma-gear-1";
    j["teeth"] = g.teeth;
    j["module"] = g.module;
    j["pressure_angle"] = g.pressure_angle;
    j["width"] = g.width;
    j["bore"] = g.bore;
    j["helix_angle"] = g.helix_angle;
    j["herringbone"] = g.herringbone;
    j["backlash"] = g.backlash;
    j["left_hand"] = g.left_hand;
    return j.dump();
}

GearSpec parse_gear(std::string_view text) {
    try {
        const nlohmann::json j = nlohmann::json::parse(text);
        if (j.value("format", "") != "tisma-gear-1")
            throw std::runtime_error("not a gear");
        GearSpec g;
        g.teeth = j.value("teeth", g.teeth);
        g.module = j.value("module", g.module);
        g.pressure_angle = j.value("pressure_angle", g.pressure_angle);
        g.width = j.value("width", g.width);
        g.bore = j.value("bore", g.bore);
        g.helix_angle = j.value("helix_angle", g.helix_angle);
        g.herringbone = j.value("herringbone", g.herringbone);
        g.backlash = j.value("backlash", g.backlash);
        g.left_hand = j.value("left_hand", g.left_hand);
        if (const std::string why = check_gear(g); !why.empty())
            throw std::runtime_error(why);
        return g;
    } catch (const nlohmann::json::exception& e) {
        throw std::runtime_error(std::string("gear: ") + e.what());
    }
}

} // namespace tisma::cad
