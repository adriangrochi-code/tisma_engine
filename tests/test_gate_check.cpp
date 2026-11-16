#include "gcode/gate_check.hpp"
#include "gcode/preview.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <string>

using namespace tisma;
using Catch::Matchers::WithinAbs;

namespace {

// A layer of G-code at z with the moves given, of one kind.
std::string layer(double z, const char* type, const std::string& moves) {
    return ";LAYER_CHANGE\n;Z:" + std::to_string(z) + "\n;HEIGHT:0.2\nG1 Z" + std::to_string(z) + "\n;TYPE:" + type +
           "\n;WIDTH:0.4\n" + moves;
}

} // namespace

TEST_CASE("the gate's part size counts the beads' width and leaves the support out", "[gate]") {
    std::string g = "G90\nM83\n";
    for (int l = 1; l <= 5; ++l)
        g += layer(0.2 * l, "External perimeter",
                   "G1 X0.2 Y0.2 F3000\nG1 X9.8 Y0.2 E1\nG1 X9.8 Y9.8 E1\nG1 X0.2 Y9.8 E1\nG1 X0.2 Y0.2 E1\n") +
             ";TYPE:Support material\nG1 X20 Y20 F3000\nG1 X30 Y20 E1\n";
    const auto size = part_extent(read_gcode_preview(g));
    CHECK_THAT(size[0], WithinAbs(10.0, 1e-4));
    CHECK_THAT(size[1], WithinAbs(10.0, 1e-4));
    CHECK_THAT(size[2], WithinAbs(1.0, 1e-4));
}

TEST_CASE("the gate finds support with nothing under it", "[gate]") {
    // A column of support from the bed, and from the third layer a second
    // piece 10 mm away with nothing under it.
    std::string g = "G90\nM83\n";
    for (int l = 1; l <= 4; ++l) {
        std::string moves = "G1 X0 Y0 F3000\nG1 X10 Y0 E1\n";
        if (l >= 3)
            moves += "G1 X0 Y10 F3000\nG1 X10 Y10 E1\n";
        g += layer(0.2 * l, "Support material", moves);
    }
    const GcodePreview p = read_gcode_preview(g);
    std::vector<std::array<float, 3>> where;
    // 50 mm of support above the first layer, 10 of them (the new piece's
    // first layer) on nothing.
    CHECK_THAT(floating_support_share(p, 1.0, &where), WithinAbs(10.0 / 50.0, 0.01));
    REQUIRE(!where.empty());
    CHECK_THAT(where.front()[1], WithinAbs(10.0, 1e-4));
    CHECK_THAT(where.front()[2], WithinAbs(0.6, 1e-4));
}

TEST_CASE("the gate finds overhangs with no support under them", "[gate]") {
    std::string g = "G90\nM83\n";
    for (int l = 1; l <= 5; ++l)
        g += layer(0.2 * l, "Support material", "G1 X0 Y0 F3000\nG1 X10 Y0 E1\n");
    const GcodePreview p = read_gcode_preview(g);
    const std::vector<OverhangPoint> points{{5, 1, 1.2f, 1, 0}, {5, 8, 1.2f, 1, 1}, {5, 1, 9.0f, 1, 2}};
    const auto bare = overhangs_without_support(p, points);
    REQUIRE(bare.size() == 2);
    CHECK(bare[0].region == 1); // too far sideways
    CHECK(bare[1].region == 2); // too far above
}
