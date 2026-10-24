#include "perimeters/beading.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <numeric>

using namespace tisma;
using Catch::Matchers::WithinAbs;

namespace {

double sum(const std::vector<double>& v) { return std::accumulate(v.begin(), v.end(), 0.0); }

BeadingParams with(BeadingScheme s) {
    BeadingParams p;
    p.scheme = s;
    p.preferred_width = 0.4;
    return p;
}

} // namespace

TEST_CASE("distributed scheme: nearest bead count, thickness split evenly", "[beading]") {
    const BeadingParams p = with(BeadingScheme::Distributed);
    CHECK(bead_count(p, 0.19) == 0);
    CHECK(bead_count(p, 0.2) == 1);
    CHECK(bead_count(p, 0.9) == 2);
    CHECK(bead_count(p, 1.0) == 3);
    const Beading b = compute_beading(p, 2, 0.9);
    REQUIRE(b.widths.size() == 2);
    CHECK_THAT(b.widths[0], WithinAbs(0.45, 1e-12));
    CHECK_THAT(b.centres[0], WithinAbs(0.225, 1e-12));
    CHECK_THAT(b.centres[1], WithinAbs(0.675, 1e-12));
    // The count jumps from n to n + 1 at (n + 1/2) w*.
    CHECK_THAT(transition_thickness(p, 2), WithinAbs(1.0, 1e-12));
    CHECK(bead_count(p, transition_thickness(p, 2) - 1e-9) == 2);
    CHECK(bead_count(p, transition_thickness(p, 2) + 1e-9) == 3);
}

TEST_CASE("uniform scheme: preferred width, pairs from both outlines", "[beading]") {
    const BeadingParams p = with(BeadingScheme::Uniform);
    CHECK(bead_count(p, 0.39) == 0);
    CHECK(bead_count(p, 0.41) == 2);
    CHECK(bead_count(p, 1.0) == 2);
    CHECK(bead_count(p, 1.21) == 4);
    const Beading b = compute_beading(p, 2, 1.0);
    CHECK(b.widths == std::vector<double>{0.4, 0.4});
    CHECK_THAT(b.centres[0], WithinAbs(0.2, 1e-12));
    CHECK_THAT(b.centres[1], WithinAbs(0.8, 1e-12)); // the 0.2 mm gap stays in the middle
    CHECK_THAT(transition_thickness(p, 2), WithinAbs(1.2, 1e-12));
}

TEST_CASE("inward distributed scheme: the difference goes to the inner beads", "[beading]") {
    BeadingParams p = with(BeadingScheme::InwardDistributed);
    p.inward_beads = 1.5;
    // d = 1.0, n = 3: E = -0.2; weights 5/9, 1, 5/9.
    const Beading b = compute_beading(p, 3, 1.0);
    REQUIRE(b.widths.size() == 3);
    const double total_weight = 5.0 / 9.0 + 1.0 + 5.0 / 9.0;
    CHECK_THAT(b.widths[0], WithinAbs(0.4 - 0.2 * (5.0 / 9.0) / total_weight, 1e-12));
    CHECK_THAT(b.widths[1], WithinAbs(0.4 - 0.2 / total_weight, 1e-12));
    CHECK_THAT(b.widths[2], WithinAbs(b.widths[0], 1e-12));
    CHECK_THAT(b.centres[1], WithinAbs(0.5, 1e-12)); // an odd count puts a bead in the middle
    // With many beads the outer ones keep the preferred width.
    const Beading wide = compute_beading(p, 9, 9 * 0.4 + 0.3);
    CHECK_THAT(wide.widths.front(), WithinAbs(0.4, 1e-12));
    CHECK(wide.widths[4] > 0.4);
}

TEST_CASE("distributed beadings fill the thickness exactly and are symmetric", "[beading]") {
    for (BeadingScheme s : {BeadingScheme::Distributed, BeadingScheme::InwardDistributed}) {
        const BeadingParams p = with(s);
        for (double d = 0.2; d < 6.0; d += 0.037) {
            const int n = bead_count(p, d);
            const Beading b = compute_beading(p, n, d);
            REQUIRE(int(b.widths.size()) == n);
            CHECK_THAT(sum(b.widths), WithinAbs(d, 1e-9));
            for (int i = 0; i < n; ++i) {
                CHECK(b.widths[std::size_t(i)] > 0.0);
                CHECK_THAT(b.widths[std::size_t(i)], WithinAbs(b.widths[std::size_t(n - 1 - i)], 1e-9));
                CHECK_THAT(b.centres[std::size_t(i)] + b.centres[std::size_t(n - 1 - i)], WithinAbs(d, 1e-9));
            }
        }
    }
}

TEST_CASE("distributed widths stay between half and one and a half preferred widths", "[beading]") {
    // With n = round(d / w*), d / n lies in [w* (n - 1/2) / n, w* (n + 1/2) / n],
    // which is widest for n = 1: [w*/2, 3w*/2].
    const BeadingParams p = with(BeadingScheme::Distributed);
    for (double d = 0.2; d < 6.0; d += 0.013) {
        const Beading b = compute_beading(p, bead_count(p, d), d);
        for (double w : b.widths) {
            CHECK(w >= 0.2 - 1e-12);
            CHECK(w <= 0.6 + 1e-12);
        }
    }
}

TEST_CASE("half beadings run from the outline to the middle", "[beading]") {
    const BeadingParams p = with(BeadingScheme::Distributed);
    const HalfBeading even = half_beading(compute_beading(p, 2, 0.9));
    CHECK(even.widths.size() == 1);
    CHECK_THAT(even.distances[0], WithinAbs(0.225, 1e-12));
    const HalfBeading odd = half_beading(compute_beading(p, 3, 1.2));
    REQUIRE(odd.widths.size() == 2);
    CHECK_THAT(odd.distances[1], WithinAbs(0.6, 1e-12)); // the middle bead sits at thickness / 2
}

TEST_CASE("beading interpolation inside a ramp", "[beading]") {
    const BeadingParams p = with(BeadingScheme::Distributed);
    // At the ends of the ramp the interpolation gives the plain beadings.
    const HalfBeading at2 = interpolate_beading(p, 2.0, 1.0);
    const HalfBeading b2 = half_beading(compute_beading(p, 2, 1.0));
    CHECK(at2.widths == b2.widths);
    CHECK(at2.distances == b2.distances);
    // Two thirds of the way from 2 to 3 beads (as in Fig. 8 of the paper).
    const HalfBeading x = interpolate_beading(p, 2.0 + 2.0 / 3.0, 1.0);
    const HalfBeading b3 = half_beading(compute_beading(p, 3, 1.0));
    REQUIRE(x.widths.size() == 2);
    CHECK_THAT(x.widths[0], WithinAbs(b2.widths[0] + 2.0 / 3.0 * (b3.widths[0] - b2.widths[0]), 1e-12));
    CHECK_THAT(x.distances[0], WithinAbs(b2.distances[0] + 2.0 / 3.0 * (b3.distances[0] - b2.distances[0]), 1e-12));
    // The middle bead of the 3-bead beading fades in.
    CHECK_THAT(x.widths[1], WithinAbs(2.0 / 3.0 * b3.widths[1], 1e-12));
    CHECK_THAT(x.distances[1], WithinAbs(0.5, 1e-12));
}
