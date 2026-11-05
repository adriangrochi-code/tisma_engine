// The bed picture of a pack printer as coloured triangles (profile_pack.md,
// rule 14; src/ui/bed_art.hpp).

#include "ui/bed_art.hpp"
#include "ui/icons.hpp"

#include <catch2/catch_approx.hpp>
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

TEST_CASE("bed art: a square picture on a wide bed keeps its shape, centred, counter-clockwise", "[ui][bed_art]") {
    const BedArt art = bed_svg_art(svg("<rect x='0' y='0' width='100' height='100' fill='#ff0000'/>"), 0, 0, 200, 100);
    REQUIRE(art.groups.size() == 1);
    CHECK(art.groups[0].rgb == 0xFF0000);
    CHECK(art.groups[0].count % 3 == 0);
    CHECK_THAT(area(art, art.groups[0]), WithinAbs(100.0 * 100.0, 1e-3)); // positive: facing up
    for (std::size_t i = 0; i < art.vertices.size(); i += 6) {
        CHECK(art.vertices[i] >= 49.9999f); // x from 50 to 150
        CHECK(art.vertices[i] <= 150.0001f);
    }
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

TEST_CASE("bed art: a pack's bed model centred on the bed, its top at height 0 (rule 14)", "[ui][bed_art]") {
    // As the AnkerMake M5's: -127.5..127.5 in X, -147..130 in Y, made around its own centre.
    const double lo[3] = {-127.5, -147.0, -5.0}, hi[3] = {127.5, 130.0, 0.5};
    const BedModelPlace p = bed_model_place(lo, hi, 0.0, 0.0, 235.0, 235.0);
    CHECK(p.dx == Catch::Approx(117.5));
    CHECK(p.dy == Catch::Approx(117.5 + 8.5));
    CHECK(p.dz == Catch::Approx(-0.5));
    CHECK_FALSE(p.small);
    const double tiny_lo[3] = {0, 0, 0}, tiny_hi[3] = {50, 300, 1};
    CHECK(bed_model_place(tiny_lo, tiny_hi, 0.0, 0.0, 235.0, 235.0).small); // under half in X: drawn, logged
}

namespace {

// A picture of w x h pixels, all `rgba`.
tisma::ui::Image picture(int w, int h, std::uint32_t rgba) {
    tisma::ui::Image image;
    image.width = w;
    image.height = h;
    for (int i = 0; i < w * h; ++i)
        image.rgba.insert(image.rgba.end(), {std::uint8_t(rgba >> 24), std::uint8_t(rgba >> 16), std::uint8_t(rgba >> 8),
                                             std::uint8_t(rgba)});
    return image;
}

BedArt art_of(const tisma::ui::Image& i, double min_x, double min_y, double max_x, double max_y, double z = 0.01,
              int cells = 192) {
    return bed_image_art(i.width, i.height, i.rgba, min_x, min_y, max_x, max_y, z, cells);
}

} // namespace

TEST_CASE("bed art: a PNG bed picture, fitted and centred as the SVG (rule 14)", "[ui][bed_art]") {
    // 4 x 2 red on a 100 x 100 bed: 100 x 50, centred in Y.
    const BedArt red = art_of(picture(4, 2, 0xFF0000FF), 0, 0, 100, 100);
    REQUIRE(red.groups.size() == 1);
    CHECK(red.groups[0].rgb == 0xFF0000);
    CHECK(red.groups[0].count == 2 * 6); // a run a row: two rectangles
    CHECK_THAT(area(red, red.groups[0]), WithinAbs(100.0 * 50.0, 1e-3));
    float lo = 1e9f, hi = -1e9f;
    for (std::size_t i = 1; i < red.vertices.size(); i += 6)
        lo = std::min(lo, red.vertices[i]), hi = std::max(hi, red.vertices[i]);
    CHECK(lo == Catch::Approx(25.0f));
    CHECK(hi == Catch::Approx(75.0f));

    // The left half clear, the right half white: only the white is drawn.
    tisma::ui::Image half = picture(4, 4, 0xFFFFFFFF);
    for (int y = 0; y < 4; ++y)
        for (int x = 0; x < 2; ++x)
            half.rgba[(std::size_t(y) * 4 + std::size_t(x)) * 4 + 3] = 0;
    const BedArt white = art_of(half, 0, 0, 100, 100);
    REQUIRE(white.groups.size() == 1);
    CHECK(white.groups[0].rgb == 0xFFFFFF);
    CHECK_THAT(area(white, white.groups[0]), WithinAbs(50.0 * 100.0, 1e-3));

    // A large picture is cut into at most `cells` cells a side; colours rounded to 8 levels.
    const BedArt big = art_of(picture(1000, 500, 0x7F3F00FF), 0, 0, 200, 100, 0.01, 100);
    REQUIRE(big.groups.size() == 1);
    CHECK(big.groups[0].count == 50 * 6); // 100 x 50 cells, one run a row
    CHECK(big.groups[0].rgb == 0x6D4900); // 127 -> level 3 (109), 63 -> level 2 (73)
    CHECK(art_of(tisma::ui::Image{}, 0, 0, 100, 100).groups.empty());
}
