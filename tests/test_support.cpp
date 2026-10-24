#include "support/support.hpp"

#include <catch2/catch_test_macros.hpp>
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
    CHECK_THAT(area_mm2(over[10]), WithinAbs(100.0 - 2.4 * 2.4, 1e-6)); // the stem grown by 0.2 holds the rest
    CHECK(over[11].empty());
    const std::vector<Polygons> support = support_areas(regions, heights, p);
    for (int l = 0; l <= 8; ++l)
        CHECK_THAT(area_mm2(support[std::size_t(l)]), WithinAbs(100.0 - 3.2 * 3.2, 1e-6)); // 0.6 away from the stem
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
