// Beading schemes. Formulas from Kuipers et al. 2020 (arXiv 2004.13497):
// Section 3.4 (Eq. 4, 5), Section 4 (Definition 4.1, Table 1 a, d, f). The code
// structure is our own; see PROVENANCE.md.

#include "perimeters/beading.hpp"

#include <algorithm>
#include <cmath>

namespace tisma {

int bead_count(const BeadingParams& p, double d) {
    if (!(d > 0.0))
        return 0;
    const double w = p.preferred_width;
    switch (p.scheme) {
    case BeadingScheme::Uniform:
        // Pairs of preferred-width beads, one from each side.
        return 2 * static_cast<int>(std::floor(d / (2.0 * w) + 0.5));
    case BeadingScheme::Distributed:
    case BeadingScheme::InwardDistributed:
        return static_cast<int>(std::floor(d / w + 0.5));
    }
    return 0;
}

double transition_thickness(const BeadingParams& p, int n) {
    const double w = p.preferred_width;
    switch (p.scheme) {
    case BeadingScheme::Uniform:
        // q jumps from 2k to 2k + 2 where d / (2w) + 1/2 reaches k + 1.
        return (2.0 * std::floor(n / 2.0) + 1.0) * w;
    case BeadingScheme::Distributed:
    case BeadingScheme::InwardDistributed:
        return (n + 0.5) * w;
    }
    return 0.0;
}

Beading compute_beading(const BeadingParams& p, int n, double d) {
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
    return b;
}

} // namespace tisma
