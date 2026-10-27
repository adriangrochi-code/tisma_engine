#include "support/support.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using namespace tisma;
using Catch::Matchers::WithinAbs;

namespace {

Polygons square(double x0, double y0, double x1, double y1) {
    return {{{to_units(x0), to_units(y0)}, {to_units(x1), to_units(y0)}, {to_units(x1), to_units(y1)}, {to_units(x0), to_units(y1)}}};
}

} // namespace

TEST_CASE("support: a T's cap is held up from the bed, outside the stem", "[support]") {
    // Layers 0-9: a 2 x 2 stem; layers 10-14: a 10 x 10 cap.
    std::vector<Polygons> regions;
    for (int l = 0; l < 15; ++l)
        regions.push_back(l < 10 ? square(4, 4, 6, 6) : square(0, 0, 10, 10));
    const std::vector<double> heights(15, 0.2);
    SupportParams p; // automatic angle: half a bead (0.2 mm) per layer
    p.z_gap_layers = 1;
    p.xy_gap = 0.6;
    const std::vector<Polygons> over = overhangs(regions, heights, p);
    CHECK(over[0].empty());
    CHECK(over[5].empty());
    // The stem grown by 0.2 holds the rest (and 1 um more: a step of exactly half a bead is held).
    CHECK_THAT(area_mm2(over[10]), WithinAbs(100.0 - 2.402 * 2.402, 1e-6));
    CHECK(over[11].empty());
    const std::vector<Polygons> support = support_areas(regions, heights, p);
    // 0.6 away from the stem, exactly; the cap's outline, from a grid of an
    // eighth of a bead (0.05 mm), within a pixel and a half outwards.
    const double pixel = p.bead / 8.0;
    for (int l = 0; l <= 8; ++l) {
        const double area = area_mm2(support[std::size_t(l)]);
        CHECK(area >= 100.0 - 3.2 * 3.2 - 1e-6);
        CHECK(area <= (10.0 + 3.0 * pixel) * (10.0 + 3.0 * pixel) - 3.2 * 3.2);
        CHECK_FALSE(region_contains(support[std::size_t(l)], 5.0, 5.0));
        CHECK_FALSE(region_contains(support[std::size_t(l)], 3.41, 5.0)); // 0.59 from the stem
        CHECK(region_contains(support[std::size_t(l)], 3.39, 5.0));
    }
    CHECK(support[9].empty()); // the gap under the cap
    for (int l = 10; l < 15; ++l)
        CHECK(support[std::size_t(l)].empty());
}

TEST_CASE("support: the threshold angle decides whether a 45 degree slope needs it", "[support]") {
    // An upside-down pyramid: every layer reaches out 0.2 mm (one layer height) further: 45 degrees.
    std::vector<Polygons> regions;
    for (int l = 0; l < 10; ++l)
        regions.push_back(square(-2.0 - 0.2 * l, -2.0 - 0.2 * l, 2.0 + 0.2 * l, 2.0 + 0.2 * l));
    const std::vector<double> heights(10, 0.2);
    SupportParams steep;
    steep.threshold_angle = 40.0; // faces steeper than 40 degrees from the horizontal print without
    for (const Polygons& o : overhangs(regions, heights, steep))
        CHECK(area_mm2(o) < 1e-9);
    SupportParams flat;
    flat.threshold_angle = 50.0; // a 45 degree face is flatter than that
    const std::vector<Polygons> over = overhangs(regions, heights, flat);
    CHECK(area_mm2(over[5]) > 0.1);
    CHECK(area_mm2(support_areas(regions, heights, flat)[0]) > 1.0);
}

TEST_CASE("support: automatic, a 40 degree wedge is held and a 45 degree one is not (supports_skirt.md, S1)", "[support]") {
    // Layers 0.2 high and beads 0.4 wide: a face may reach out half a bead a
    // layer, so the limit is 45 degrees, and overhanging means MORE than that.
    const std::vector<double> heights(20, 0.2);
    SupportParams automatic; // threshold 0: half a bead
    for (const double degrees : {20.0, 30.0, 40.0, 45.0, 50.0, 60.0}) {
        CAPTURE(degrees);
        const double step = 0.2 / std::tan(degrees * 3.14159265358979 / 180.0);
        std::vector<Polygons> regions;
        for (int l = 0; l < 20; ++l)
            regions.push_back(square(0.0, 0.0, 10.0 + step * l, 10.0));
        double held = 0.0;
        for (const Polygons& a : support_areas(regions, heights, automatic))
            held += area_mm2(a);
        if (degrees < 45.0)
            CHECK(held > 0.1);
        else
            CHECK(held == 0.0);
    }
}

TEST_CASE("support: a part standing on another stops the column there", "[support]") {
    // A 10 x 10 slab at layers 0-2, nothing at 3-5 above its right half... a shelf over the slab:
    // layers 0-2 slab, 3-6 a 2 x 2 post at the left, 7-9 a 10 x 10 shelf.
    std::vector<Polygons> regions;
    for (int l = 0; l < 10; ++l)
        regions.push_back(l < 3 ? square(0, 0, 10, 10) : l < 7 ? square(0, 0, 2, 2) : square(0, 0, 10, 10));
    const std::vector<double> heights(10, 0.2);
    SupportParams p;
    p.z_gap_layers = 0;
    const std::vector<Polygons> support = support_areas(regions, heights, p);
    CHECK(area_mm2(support[5]) > 50.0); // under the shelf, beside the post
    CHECK(area_mm2(support[3]) > 50.0);
    for (int l = 0; l < 3; ++l)
        CHECK(support[std::size_t(l)].empty()); // it stands on the slab
}
