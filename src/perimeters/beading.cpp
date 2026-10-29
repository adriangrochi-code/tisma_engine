// Beading schemes. Formulas from Kuipers et al. 2020 (arXiv 2004.13497):
// Section 3.4 (Eq. 4, 5), Section 4 (Definition 4.1, Table 1 a, d, f). The code
// structure is our own; see PROVENANCE.md.

#include "perimeters/beading.hpp"

#include <algorithm>
#include <cmath>

namespace tisma {

namespace {

int unlimited_count(const BeadingParams& p, double d) {
    if (!(d > 0.0))
        return 0;
    const double w = p.preferred_width;
    switch (p.scheme) {
    case BeadingScheme::Uniform:
        // Pairs of preferred-width beads, one from each side.
        return 2 * static_cast<int>(std::floor(d / (2.0 * w) + 0.5));
    case BeadingScheme::Distributed:
    case BeadingScheme::InwardDistributed: {
        int n = static_cast<int>(std::floor(d / w + 0.5));
        if (p.odd_centre > 0.0 && n >= 3 && n % 2 == 1 && d - (n - 1) * w < p.odd_centre * w)
            --n;
        // Thinner than half a bead but not than the smallest detail: one bead.
        return n == 0 && p.min_detail > 0.0 && d >= p.min_detail ? 1 : n;
    }
    }
    return 0;
}

// The beading without the shell: n beads filling thickness d.
Beading fill(const BeadingParams& p, int n, double d);

} // namespace

int bead_count(const BeadingParams& p, double d) {
    const int n = unlimited_count(p, d);
    return p.wall_count > 0 ? std::min(n, 2 * p.wall_count) : n;
}

double outer_bead(const BeadingParams& p) {
    return p.outer_width > 0.0 ? p.outer_width : p.preferred_width;
}

std::optional<ShellRamp> shell_ramp(const BeadingParams& p) {
    if (p.wall_count <= 0 || p.scheme == BeadingScheme::Uniform)
        return std::nullopt;
    const double w = p.preferred_width;
    const double shell = 2.0 * (outer_bead(p) + (p.wall_count - 1) * w); // both sides, at their widths
    // Where the count would reach 2M + 1 by Eq. 5, whatever the centre bead asks.
    const double start = std::max((2 * p.wall_count + 0.5) * w, shell + w / 2.0);
    return ShellRamp{start, start + w / 2.0};
}

double transition_thickness(const BeadingParams& p, int n) {
    const double w = p.preferred_width;
    switch (p.scheme) {
    case BeadingScheme::Uniform:
        // q jumps from 2k to 2k + 2 where d / (2w) + 1/2 reaches k + 1.
        return (2.0 * std::floor(n / 2.0) + 1.0) * w;
    case BeadingScheme::Distributed:
    case BeadingScheme::InwardDistributed:
        // To an odd count from 3 on, once its centre bead is odd_centre w* wide.
        return p.odd_centre > 0.0 && n >= 2 && n % 2 == 0 ? (n + p.odd_centre) * w : (n + 0.5) * w;
    }
    return 0.0;
}

Beading compute_beading(const BeadingParams& p, int n, double d) {
    const std::optional<ShellRamp> shell = shell_ramp(p);
    if (!shell || n < 2 * p.wall_count || d <= shell->start)
        return fill(p, n, d);
    // The shell: the 2M beads of the ramp's start, narrowed towards w* (the
    // outer two towards their own width).
    const int m = 2 * p.wall_count;
    Beading b = fill(p, m, shell->start);
    const double s = std::min(1.0, (d - shell->start) / (shell->end - shell->start));
    for (int i = 0; i < m; ++i) {
        double& w = b.widths[std::size_t(i)];
        w += s * ((i == 0 || i == m - 1 ? outer_bead(p) : p.preferred_width) - w);
    }
    double edge = 0.0;
    for (int i = 0; i < p.wall_count; ++i) {
        const std::size_t k = std::size_t(i);
        b.centres[k] = edge + b.widths[k] / 2.0;
        edge += b.widths[k];
        b.centres[std::size_t(m - 1 - i)] = d - b.centres[k];
    }
    return b;
}

namespace {

Beading fill(const BeadingParams& p, int n, double d) {
    Beading b;
    if (n <= 0)
        return b;
    const double w = p.preferred_width;
    b.widths.assign(static_cast<std::size_t>(n), w);
    switch (p.scheme) {
    case BeadingScheme::Uniform:
        break; // every bead at w*, whatever d is
    case BeadingScheme::Distributed:
        std::fill(b.widths.begin(), b.widths.end(), d / n);
        break;
    case BeadingScheme::InwardDistributed: {
        // E = d - n w* is shared by weight w_i = max(0, 1 - (i - (n - 1)/2)^2 / N^2).
        const double excess = d - n * w;
        std::vector<double> weight(static_cast<std::size_t>(n));
        double total = 0.0;
        for (int i = 0; i < n; ++i) {
            const double off = i - (n - 1) / 2.0;
            weight[std::size_t(i)] = std::max(0.0, 1.0 - off * off / (p.inward_beads * p.inward_beads));
            total += weight[std::size_t(i)];
        }
        for (int i = 0; i < n; ++i)
            b.widths[std::size_t(i)] = w + (total > 0.0 ? excess * weight[std::size_t(i)] / total : excess / n);
        break;
    }
    }
    // Centres from the widths (Definition 4.1): each bead starts where the previous one ends.
    b.centres.resize(b.widths.size());
    double edge = 0.0;
    for (std::size_t i = 0; i < b.widths.size(); ++i) {
        b.centres[i] = edge + b.widths[i] / 2.0;
        edge += b.widths[i];
    }
    if (p.scheme == BeadingScheme::Uniform) {
        // Uniform beads come in from both outlines; the leftover gap stays in the middle.
        for (std::size_t i = b.widths.size() / 2; i < b.widths.size(); ++i)
            b.centres[i] = d - b.centres[b.widths.size() - 1 - i];
    }
    if (n == 1 && p.scheme != BeadingScheme::Uniform) {
        // A wall of one bead: as wide as the wall, never narrower than
        // min_width, on the wall's middle.
        b.widths[0] = std::max(b.widths[0], p.min_width);
        b.centres[0] = d / 2.0;
    }
    return b;
}

} // namespace

HalfBeading half_beading(const Beading& full) {
    HalfBeading h;
    const std::size_t half = (full.widths.size() + 1) / 2;
    h.widths.assign(full.widths.begin(), full.widths.begin() + std::ptrdiff_t(half));
    h.distances.assign(full.centres.begin(), full.centres.begin() + std::ptrdiff_t(half));
    return h;
}

HalfBeading interpolate_beading(const BeadingParams& params, double count, double thickness) {
    const int n = static_cast<int>(std::floor(count));
    const double f = count - n;
    const HalfBeading low = half_beading(compute_beading(params, n, thickness));
    if (f <= 0.0)
        return low;
    return blend_beadings(low, half_beading(compute_beading(params, n + 1, thickness)), f);
}

HalfBeading blend_beadings(const HalfBeading& a, const HalfBeading& b, double f) {
    HalfBeading out;
    const std::size_t common = std::min(a.widths.size(), b.widths.size());
    for (std::size_t i = 0; i < common; ++i) {
        out.widths.push_back(a.widths[i] + f * (b.widths[i] - a.widths[i]));
        out.distances.push_back(a.distances[i] + f * (b.distances[i] - a.distances[i]));
    }
    for (std::size_t i = common; i < b.widths.size(); ++i) {
        out.widths.push_back(f * b.widths[i]);
        out.distances.push_back(b.distances[i]);
    }
    for (std::size_t i = common; i < a.widths.size(); ++i) {
        out.widths.push_back((1.0 - f) * a.widths[i]);
        out.distances.push_back(a.distances[i]);
    }
    return out;
}

} // namespace tisma
