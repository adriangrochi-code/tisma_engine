// The bed picture of a pack printer as coloured triangles (profile_pack.md,
// rule 14; src/ui/bed_art.hpp).

#include "ui/bed_art.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <string>

using namespace tisma::ui;
using Catch::Matchers::WithinAbs;

namespace {

// The area of a group's triangles, in mm².
double area(const BedArt& art, const BedArt::Group& g) {
    double sum = 0.0;
    for (std::uint32_t v = g.first; v + 2 < g.first + g.count; v += 3) {
        const float* a = &art.vertices[6 * v];
        const float* b = a + 6;
        const float* c = a + 12;
        sum += 0.5 * ((b[0] - a[0]) * (c[1] - a[1]) - (c[0] - a[0]) * (b[1] - a[1]));
    }
    return sum;
}

std::string svg(const std::string& body, const char* box = "0 0 100 100") {
    return std::string("<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"100\" height=\"100\" viewBox=\"") + box + "\">" +
           body + "</svg>";
}

} // namespace

TEST_CASE("bed art: a full rectangle covers the bed's box in its colour, counter-clockwise", "[ui][bed_art]") {
    const BedArt art = bed_svg_art(svg("<rect x='0' y='0' width='100' height='100' fill='#ff0000'/>"), 0, 0, 200, 100);
    REQUIRE(art.groups.size() == 1);
    CHECK(art.groups[0].rgb == 0xFF0000);
    CHECK(art.groups[0].count % 3 == 0);
    CHECK_THAT(area(art, art.groups[0]), WithinAbs(200.0 * 100.0, 1e-3)); // positive: facing up
    for (std::size_t i = 0; i < art.vertices.size(); i += 6) {
        CHECK(art.vertices[i + 2] == 0.01f);
        CHECK(art.vertices[i + 5] == 1.0f); // facing up
    }
}

TEST_CASE("bed art: the picture's top left is the bed's back left (y flipped)", "[ui][bed_art]") {
    const BedArt art = bed_svg_art(svg("<rect x='0' y='0' width='10' height='10' fill='#0000ff'/>"), 0, 0, 100, 100, 0.5);
    REQUIRE(art.groups.size() == 1);
    CHECK(art.groups[0].rgb == 0x0000FF);
    for (std::size_t i = 0; i < art.vertices.size(); i += 6) {
        CHECK(art.vertices[i] >= -1e-4f);
        CHECK(art.vertices[i] <= 10.0001f);
        CHECK(art.vertices[i + 1] >= 89.9999f);
        CHECK(art.vertices[i + 1] <= 100.0001f);
        CHECK(art.vertices[i + 2] == 0.5f);
    }
}

TEST_CASE("bed art: holes by the fill rule, strokes widened, colours kept in order", "[ui][bed_art]") {
    // A square with a square hole (even-odd), then a 2-wide stroke of 80 on top.
    const BedArt art = bed_svg_art(
        svg("<path fill-rule='evenodd' fill='#00ff00' d='M10 10 H90 V90 H10 Z M30 30 H70 V70 H30 Z'/>"
            "<line x1='10' y1='50' x2='90' y2='50' stroke='#112233' stroke-width='2'/>"),
        0, 0, 100, 100);
    REQUIRE(art.groups.size() == 2);
    CHECK(art.groups[0].rgb == 0x00FF00);
    // The stroke crosses the ring twice, 20 mm each: those 2 × 40 mm² are the stroke's.
    CHECK_THAT(area(art, art.groups[0]), WithinAbs(80.0 * 80.0 - 40.0 * 40.0 - 2.0 * 40.0, 1e-2));
    CHECK(art.groups[1].rgb == 0x112233);
    CHECK_THAT(area(art, art.groups[1]), WithinAbs(80.0 * 2.0, 1e-2));
    CHECK(art.groups[1].first == art.groups[0].count);
    // A curve is followed to about a tenth of a millimetre: a circle of radius 40.
    const BedArt round = bed_svg_art(svg("<circle cx='50' cy='50' r='40' fill='#808080'/>"), 0, 0, 100, 100);
    REQUIRE(round.groups.size() == 1);
    CHECK_THAT(area(round, round.groups[0]), WithinAbs(std::numbers::pi * 40.0 * 40.0, std::numbers::pi * 40.0 * 40.0 * 0.01));
}

TEST_CASE("bed art: a shape painted over another takes its place: the triangles never overlap", "[ui][bed_art]") {
    // A 100 × 100 square, then a 20 × 20 one over its middle.
    const BedArt art = bed_svg_art(svg("<rect width='100' height='100' fill='#000000'/>"
                                       "<rect x='40' y='40' width='20' height='20' fill='#ffffff'/>"),
                                   0, 0, 100, 100);
    REQUIRE(art.groups.size() == 2);
    CHECK_THAT(area(art, art.groups[0]), WithinAbs(100.0 * 100.0 - 20.0 * 20.0, 1e-2));
    CHECK_THAT(area(art, art.groups[1]), WithinAbs(20.0 * 20.0, 1e-2));
}

TEST_CASE("bed art: nothing drawn, clear shapes, gradients and bad input give nothing", "[ui][bed_art]") {
    CHECK(bed_svg_art("not an svg", 0, 0, 100, 100).groups.empty());
    CHECK(bed_svg_art(svg(""), 0, 0, 100, 100).vertices.empty());
    CHECK(bed_svg_art(svg("<rect width='50' height='50' fill='none'/>"), 0, 0, 100, 100).groups.empty());
    CHECK(bed_svg_art(svg("<rect width='50' height='50' fill='#ff0000' opacity='0'/>"), 0, 0, 100, 100).groups.empty());
    CHECK(bed_svg_art(svg("<rect width='50' height='50' fill='#ff0000'/>"), 0, 0, 0, 100).groups.empty()); // no box
    // Two shapes of the same colour are one group.
    CHECK(bed_svg_art(svg("<rect width='10' height='10' fill='#ff0000'/><rect x='20' width='10' height='10' fill='#ff0000'/>"),
                      0, 0, 100, 100)
              .groups.size() == 1);
}
