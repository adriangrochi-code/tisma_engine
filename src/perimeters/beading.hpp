#pragma once

#include <optional>
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
    int wall_count = 0;           // M walls from each outline at most; 0: fill the whole region
};

// The beads across a thickness, from one outline to the other: widths[i] and
// the distance of each bead's centre line from the first outline. Symmetric:
// bead i and bead n-1-i have the same width.
struct Beading {
    std::vector<double> widths;
    std::vector<double> centres;
};

// Quantization q(d) (Section 3.4, Eq. 4, and Table 1): the bead count for a
// thickness d. Zero for features too thin to print with this scheme. With a
// wall limit M the count stops at 2M (Section 5.1, Eq. 8: q'(d) = min(M, q(d))
// with M counted across the whole thickness there).
int bead_count(const BeadingParams& params, double thickness);

// The thickness at which bead_count changes from n to the next count (the
// inverse q^-1(n) of Eq. 5): transitions are anchored there (Eq. 6).
double transition_thickness(const BeadingParams& params, int n);

// The shell (Section 5.1): with a wall limit M, parts too thick for 2M beads
// keep M walls from each outline and leave the middle for infill. Our own form
// of it, continuous in the thickness: from `start`, where the unlimited count
// would reach 2M + 1, the 2M beads narrow linearly to w* until `end`, w*/2
// thicker, so the middle gap opens from 0 to w*; beyond `end` the beads stay at
// w*. The uniform scheme already leaves its gap in the middle and needs no
// ramp. Empty without a limit.
struct ShellRamp {
    double start = 0.0, end = 0.0; // thicknesses
};
std::optional<ShellRamp> shell_ramp(const BeadingParams& params);

// The beading B(n, d) for n beads across thickness d (Definition 4.1). Bead
// centres follow from the widths, so beads do not overlap and an odd count
// puts one bead exactly in the middle. Within the shell, the beads come in
// from both outlines and the gap stays in the middle.
Beading compute_beading(const BeadingParams& params, int n, double thickness);

// One side of a beading, as Kuipers et al. 2020 use it along the medial axis
// (Section 3.5): the ceil(n/2) beads from one outline up to the middle, each
// with its width and its distance from that outline. With an odd count the
// last bead sits exactly in the middle (distance = thickness / 2).
struct HalfBeading {
    std::vector<double> widths;
    std::vector<double> distances;
};

HalfBeading half_beading(const Beading& full);

// Beading for a fractional bead count inside a transition ramp (Section 3.5,
// "beading interpolation"): bead i from the outline is interpolated between
// the beadings for floor(count) and floor(count) + 1 at the same thickness.
// A bead that only the larger beading has (its middle bead) fades in: its
// width is scaled by the fraction and its distance is that of the larger
// beading. That last rule is our own; the paper only shows it in a figure.
HalfBeading interpolate_beading(const BeadingParams& params, double count, double thickness);

} // namespace tisma
