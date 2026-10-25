// Wavy nonplanar infill, after ZigZagZ (Allum et al. 2021); own design. See
// PROVENANCE.md.

#include "infill/nonplanar.hpp"

#include "infill/patterns.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace tisma {

std::optional<WaveParams> wave_params(const Settings& s) {
    if (!s.infill_nonplanar || !(s.infill_density > 0.0) || !(s.infill_nonplanar_amplitude > 0.0))
        return std::nullopt;
    const double w = s.wall_width, density = s.infill_density / 100.0;
    double spacing = 0.0; // between neighbouring lines of the pattern
    switch (s.infill_pattern) {
    case InfillPattern::AlignedRectilinear:
    case InfillPattern::Concentric:
    case InfillPattern::Hilbert:
    case InfillPattern::Archimedean:
    case InfillPattern::Octagram:
        spacing = w / density;
        break;
    case InfillPattern::Grid:
        spacing = 2.0 * w / density;
        break;
    case InfillPattern::Triangles:
    case InfillPattern::Stars:
        spacing = 3.0 * w / density;
        break;
    case InfillPattern::Honeycomb:
        spacing = 0.5 * std::numbers::sqrt3 * honeycomb_side(w, density);
        break;
    default:
        return std::nullopt; // the lines move from layer to layer: nothing to nest on
    }
    WaveParams p;
    p.amplitude = std::min(s.layer_height * s.infill_nonplanar_amplitude / 100.0,
                           std::max(0.0, spacing - 1.25 * s.nozzle_diameter - 0.5 * w));
    if (!(p.amplitude > 0.0))
        return std::nullopt;
    p.wavelength = s.infill_nonplanar_wavelength;
    p.direction = s.infill_angle + 45.0;
    p.cell = std::clamp(p.amplitude, 0.25, 0.5);
    p.fade = 2.0 * p.amplitude;
    p.margin = 0.5 * p.amplitude + 0.5 * w + 0.5 * p.cell;
    p.step = std::min(0.25, p.wavelength / 8.0);
    return p;
}

double wave_height(const WaveParams& params, Point2d p) {
    if (!(params.wavelength > 0.0))
        return 0.0;
    const double a = params.direction * std::numbers::pi / 180.0;
    // Lines 45 degrees off the waves' direction see them a wavelength long.
    const double t = (p.x * std::cos(a) + p.y * std::sin(a)) / (params.wavelength / std::numbers::sqrt2);
    const double triangle = 4.0 * std::abs(t - std::floor(t + 0.5)) - 1.0; // -1 at whole t, +1 halfway
    return 0.5 * params.amplitude * triangle;
}

double wave_share(const WaveParams& params, double depth) {
    if (!(params.fade > 0.0))
        return depth > params.margin ? 1.0 : 0.0;
    return std::clamp((depth - params.margin) / params.fade, 0.0, 1.0);
}

void make_wavy(std::vector<Toolpath>& paths, const DepthField& depth, double z, const double* z_below,
               const WaveParams& params) {
    const auto lift = [&](Point2d p, double at) { return wave_share(params, depth.at(p.x, p.y, at)) * wave_height(params, p); };
    for (Toolpath& t : paths) {
        if (t.points.size() < 2)
            continue;
        std::vector<ToolpathPoint> fine;
        const std::size_t n = t.points.size() + (t.closed ? 1 : 0);
        for (std::size_t i = 0; i + 1 < n; ++i) {
            const ToolpathPoint& a = t.points[i];
            const ToolpathPoint& b = t.points[(i + 1) % t.points.size()];
            const double length = std::hypot(b.p.x - a.p.x, b.p.y - a.p.y);
            const int pieces = std::max(1, int(std::ceil(length / params.step)));
            for (int k = 0; k < pieces; ++k) {
                const double f = double(k) / double(pieces);
                fine.push_back({{a.p.x + f * (b.p.x - a.p.x), a.p.y + f * (b.p.y - a.p.y)},
                                a.width + f * (b.width - a.width)});
            }
        }
        if (!t.closed)
            fine.push_back(t.points.back());
        for (ToolpathPoint& q : fine) {
            q.dz = lift(q.p, z);
            q.below = z_below != nullptr ? lift(q.p, *z_below) : 0.0;
        }
        // The triangle wave is straight between crests and troughs: the cuts in
        // between, on the line through their neighbours in X, Y, Z and below,
        // add nothing.
        std::vector<ToolpathPoint> kept;
        kept.reserve(fine.size());
        for (std::size_t i = 0; i < fine.size(); ++i) {
            if (!kept.empty() && i + 1 < fine.size()) {
                const ToolpathPoint& a = kept.back();
                const ToolpathPoint& b = fine[i];
                const ToolpathPoint& c = fine[i + 1];
                const double ab = std::hypot(b.p.x - a.p.x, b.p.y - a.p.y);
                const double ac = std::hypot(c.p.x - a.p.x, c.p.y - a.p.y);
                if (ac > 0.0) {
                    const double f = ab / ac;
                    const double off = std::abs((c.p.x - a.p.x) * (b.p.y - a.p.y) - (c.p.y - a.p.y) * (b.p.x - a.p.x)) / ac;
                    if (off < 1e-4 && std::abs(a.dz + f * (c.dz - a.dz) - b.dz) < 1e-4 &&
                        std::abs(a.below + f * (c.below - a.below) - b.below) < 1e-4 &&
                        std::abs(a.width + f * (c.width - a.width) - b.width) < 1e-6)
                        continue;
                }
            }
            kept.push_back(fine[i]);
        }
        t.points = std::move(kept);
        t.wavy = true;
    }
}

} // namespace tisma
