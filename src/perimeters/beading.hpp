#pragma once

#include <vector>

namespace tisma {

// Beading schemes from Kuipers et al. 2020, Section 4 and Table 1: how many
// beads fill a local thickness d (twice the medial-axis radius) and how wide
// each one is. All lengths in mm.
enum class BeadingScheme {
    Uniform,           // Table 1a: every bead at the preferred width; an even count
    Distributed,       // Table 1d: the nearest bead count, d split evenly
    InwardDistributed, // Table 1f: the nearest bead count, the difference given mostly to the inner beads
};

struct BeadingParams {
    BeadingScheme scheme = BeadingScheme::Distributed;
    double preferred_width = 0.4; // w*, usually the nozzle diameter
    double inward_beads = 1.5;    // N in Table 1f: how many inner beads take the difference
};

// The beads across a thickness, from one outline to the other: widths[i] and
// the distance of each bead's centre line from the first outline. Symmetric:
// bead i and bead n-1-i have the same width.
struct Beading {
    std::vector<double> widths;
    std::vector<double> centres;
};

// Quantization q(d) (Section 3.4, Eq. 4, and Table 1): the bead count for a
// thickness d. Zero for features too thin to print with this scheme.
int bead_count(const BeadingParams& params, double thickness);

// The thickness at which bead_count changes from n to the next count (the
// inverse q^-1(n) of Eq. 5): transitions are anchored there (Eq. 6).
double transition_thickness(const BeadingParams& params, int n);

// The beading B(n, d) for n beads across thickness d (Definition 4.1). Bead
// centres follow from the widths, so beads do not overlap and an odd count
// puts one bead exactly in the middle.
Beading compute_beading(const BeadingParams& params, int n, double thickness);

} // namespace tisma
