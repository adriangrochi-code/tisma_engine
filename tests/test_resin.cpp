// Resin printing (docs/spec/features/resin.md): the display images, the GOO
// file and the corrections, against the spec's tests RE1 to RE10 (measured
// on Tisma Slicer as a black box).

#include "cli/cli.hpp"
#include "config/settings.hpp"
#include "mesh/indexed_mesh.hpp"
#include "resin/goo.hpp"
#include "resin/resin.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <algorithm>
#include <climits>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <set>
#include <sstream>
#include <string>
#include <vector>

using namespace tisma;
using Catch::Matchers::ContainsSubstring;
using Catch::Matchers::WithinAbs;

namespace {

void add_box(TriangleMesh& m, float x0, float y0, float z0, float x1, float y1, float z1) {
    const Vec3f v[8] = {{x0, y0, z0}, {x1, y0, z0}, {x1, y1, z0}, {x0, y1, z0},
                        {x0, y0, z1}, {x1, y0, z1}, {x1, y1, z1}, {x0, y1, z1}};
    const auto quad = [&](int a, int b, int c, int d) {
        m.add_triangle(v[a], v[b], v[c]);
        m.add_triangle(v[a], v[c], v[d]);
    };
    quad(0, 3, 2, 1);
    quad(4, 5, 6, 7);
    quad(0, 1, 5, 4);
    quad(1, 2, 6, 5);
    quad(2, 3, 7, 6);
    quad(3, 0, 4, 7);
}

// The spec's cube: 20 x 20 x 10 mm, centred on (cx, cy) of the display.
IndexedMesh cube(double cx = 143.43 / 2, double cy = 89.6 / 2) {
    TriangleMesh m;
    add_box(m, float(cx - 10), float(cy - 10), 0, float(cx + 10), float(cy + 10), 10);
    return weld_exact(m);
}

// The spec's base settings: the defaults with 10 bottom layers, the part on
// the platform without supports or pad.
Settings base() {
    Settings s;
    s.resin_bottom_layers = 10;
    s.resin_supports = false;
    s.resin_pad = false;
    return s;
}

struct Lit {
    int c0 = INT_MAX, c1 = -1, r0 = INT_MAX, r1 = -1;
    std::size_t count = 0;
    std::set<int> greys;
};

std::vector<std::uint8_t> pixels(const ResinPrint& p, std::size_t layer, const Settings& s) {
    return decode_goo_image(p.layers.at(layer).image, s.display_pixels_x, s.display_pixels_y);
}

Lit lit_of(const std::vector<std::uint8_t>& px, const Settings& s) {
    Lit l;
    for (int r = 0; r < s.display_pixels_y; ++r)
        for (int c = 0; c < s.display_pixels_x; ++c) {
            const int g = px[std::size_t(r) * std::size_t(s.display_pixels_x) + std::size_t(c)];
            l.greys.insert(g);
            if (g == 0)
                continue;
            ++l.count;
            l.c0 = std::min(l.c0, c), l.c1 = std::max(l.c1, c);
            l.r0 = std::min(l.r0, r), l.r1 = std::max(l.r1, r);
        }
    return l;
}

int at(const std::vector<std::uint8_t>& px, const Settings& s, int col, int row) {
    return px[std::size_t(row) * std::size_t(s.display_pixels_x) + std::size_t(col)];
}

GooFile goo_of(const ResinPrint& p, const Settings& s) {
    std::stringstream file;
    write_goo(file, p, s, "2026-10-27 12:00:00");
    return read_goo(file);
}

} // namespace

TEST_CASE("resin: the run-length coding of an image", "[resin]") {
    // Runs of every length class, greys, and the two ends of a row joined.
    GooImageEncoder e;
    std::vector<std::uint8_t> want;
    auto add = [&](std::uint8_t v, std::size_t n) {
        e.add(v, n);
        want.insert(want.end(), n, v);
    };
    add(0, 15);
    add(255, 16);
    add(7, 4095);
    add(0, 4096);
    add(254, 1);
    add(255, (1u << 20) - 1);
    add(0, 1u << 20);
    add(1, 3);
    const std::size_t total = want.size();
    const std::vector<std::uint8_t> data = e.finish();
    CHECK(data.front() == 0x55);
    CHECK(decode_goo_image(data, int(total), 1) == want);
    // A grey of one pixel takes two bytes: its kind with the length, then its value.
    GooImageEncoder one;
    one.add(183);
    const std::vector<std::uint8_t> grey = one.finish();
    CHECK(grey == std::vector<std::uint8_t>{0x55, 0x41, 183, std::uint8_t(~(0x41 + 183))});
    // The spec's examples of chunks, the differences included: five 0x00, a
    // grey 10, +1, +2 for three pixels, -1, -2 for two, two 0xff.
    std::vector<std::uint8_t> chunks = {0x55, 0x05, 0x41, 0x0A, 0x81, 0x92, 0x03, 0xA1, 0xB2, 0x02, 0xC2};
    std::uint8_t sum = 0;
    for (std::size_t i = 1; i < chunks.size(); ++i)
        sum = std::uint8_t(sum + chunks[i]);
    chunks.push_back(std::uint8_t(~sum));
    CHECK(decode_goo_image(chunks, 15, 1) ==
          std::vector<std::uint8_t>{0, 0, 0, 0, 0, 10, 11, 13, 13, 13, 12, 10, 10, 255, 255});
    chunks[3] ^= 1; // a changed byte fails the checksum
    CHECK_THROWS_AS(decode_goo_image(chunks, 15, 1), ResinError);
    CHECK_THROWS_AS(decode_goo_image(data, int(total) - 1, 1), ResinError);
}

TEST_CASE("resin RE1: the GOO file of a cube", "[resin]") {
    const Settings s = base();
    const ResinPrint p = slice_resin(cube(), s);
    REQUIRE(p.layers.size() == 200);
    CHECK_THAT(p.layers.front().z, WithinAbs(0.05, 1e-9));
    CHECK_THAT(p.layers.back().z, WithinAbs(10.0, 1e-9));
    CHECK_THAT(p.volume, WithinAbs(4000.0, 0.01));
    CHECK_THAT(p.weight, WithinAbs(4.0, 1e-5));
    // An image of 4586 bytes, the checksum 0x03 (the sum 0xfc): Tisma
    // Slicer's, byte for byte.
    const std::vector<std::uint8_t>& image = p.layers[100].image;
    CHECK(image.size() == 4586);
    CHECK(image.back() == 0x03);

    std::stringstream file;
    write_goo(file, p, s, "2026-10-27 12:00:00");
    const std::string bytes = file.str();
    std::size_t expected = kGooHeaderSize + 11;
    for (const ResinLayer& l : p.layers)
        expected += 66 + 4 + l.image.size() + 2;
    CHECK(bytes.size() == expected);
    CHECK(bytes.substr(0, 4) == "V3.0");
    CHECK(bytes.substr(bytes.size() - 11) == std::string("\x00\x00\x00\x07\x00\x00\x00\x44\x4C\x50\x00", 11));
    const GooFile f = read_goo(file);
    const GooHeader& h = f.header;
    CHECK(h.layer_offset == int(kGooHeaderSize));
    CHECK(h.layers == 200);
    CHECK(f.layers.size() == 200);
    CHECK(h.software == "Tisma engine");
    CHECK(h.created == "2026-10-27 12:00:00");
    CHECK(h.printer_name == "MARS 5");
    CHECK(h.printer_type == "MSLA");
    CHECK(h.profile_name.empty());
    CHECK(h.antialiasing == 8);
    CHECK(h.grey_level == 1);
    CHECK(h.blur_level == 0);
    CHECK(h.x_resolution == 4098);
    CHECK(h.y_resolution == 2560);
    CHECK(h.mirror_x);
    CHECK_FALSE(h.mirror_y);
    CHECK(h.size_x == 143.43f);
    CHECK(h.size_y == 89.6f);
    CHECK(h.size_z == 150.0f);
    CHECK(h.layer_height == 0.05f);
    CHECK(h.fixed_waits);
    CHECK(h.off_time == 0.0f);
    CHECK(h.bottom_layers == 10);
    CHECK_FALSE(h.advanced);
    CHECK(h.volume == float(p.volume));
    CHECK(h.price_unit == "$");
    CHECK(h.greyscale);
    CHECK(h.transition_layers == 0);
    CHECK(h.print_seconds == int(std::llround(p.seconds)));
    CHECK(h.small_preview.size() == 116u * 116u);
    CHECK(h.big_preview.size() == 290u * 290u);
    // The preview shows the part: its middle is lit, its corner dark.
    CHECK(h.big_preview[145u * 290u + 145u] != h.big_preview[0]);
    for (std::size_t l = 0; l < f.layers.size(); ++l) {
        CHECK(f.layers[l].pause == 0);
        CHECK(f.layers[l].pause_z == 150.0f);
        CHECK(f.layers[l].z == float(p.layers[l].z));
        CHECK(f.layers[l].image == p.layers[l].image);
    }
}

TEST_CASE("resin RE2: the greys of a cube's edges", "[resin]") {
    const Settings s = base();
    const ResinPrint p = slice_resin(cube(), s);
    const std::vector<std::uint8_t> px = pixels(p, 100, s);
    const Lit l = lit_of(px, s);
    CHECK(l.c0 == 1763);
    CHECK(l.c1 == 2334);
    CHECK(l.r0 == 994);
    CHECK(l.r1 == 1565);
    // 71.4 % of a pixel: 183; of a corner (71.4 % of 71.4 %): 131.
    CHECK(l.greys == std::set<int>{0, 131, 183, 255});
    CHECK(at(px, s, 1762, 1279) == 0);
    CHECK(at(px, s, 1763, 1279) == 183);
    CHECK(at(px, s, 1764, 1279) == 255);
    CHECK(at(px, s, 2333, 1279) == 255);
    CHECK(at(px, s, 2334, 1279) == 183);
    CHECK(at(px, s, 2335, 1279) == 0);
    CHECK(at(px, s, 1763, 994) == 131);
}

TEST_CASE("resin RE3: where the part falls on the image, mirrored and not", "[resin]") {
    Settings s = base();
    const ResinPrint mirrored = slice_resin(cube(30, 20), s);
    const Lit m = lit_of(pixels(mirrored, 100, s), s);
    CHECK(m.c0 == 2955);
    CHECK(m.c1 == 3526);
    CHECK(m.r0 == 1702);
    CHECK(m.r1 == 2274);
    s.display_mirror_x = false;
    const ResinPrint plain = slice_resin(cube(30, 20), s);
    const Lit n = lit_of(pixels(plain, 100, s), s);
    CHECK(n.c0 == 571);
    CHECK(n.c1 == 1142);
    CHECK(n.r0 == 1702);
    CHECK(n.r1 == 2274);
    // Mirrored in Y, the rows turn around.
    s.display_mirror_y = true;
    const Lit y = lit_of(pixels(slice_resin(cube(30, 20), s), 100, s), s);
    CHECK(y.r0 == 2560 - 1 - 2274);
    CHECK(y.r1 == 2560 - 1 - 1702);
    // The centre of the display falls in the centre of the image.
    s = base();
    const Lit c = lit_of(pixels(slice_resin(cube(), s), 100, s), s);
    CHECK(c.c0 + c.c1 == 4097);
    CHECK(c.r0 + c.r1 == 2559);
}

TEST_CASE("resin RE4 and RE7: the bottom layers, the waits and the moves", "[resin]") {
    for (const auto& [bottom, want] : {std::pair{10, 10}, std::pair{5, 5}, std::pair{0, 1}}) {
        Settings s = base();
        s.resin_bottom_layers = bottom;
        const ResinPrint p = slice_resin(cube(), s);
        const GooFile f = goo_of(p, s);
        int long_ones = 0;
        for (const GooLayer& l : f.layers)
            long_ones += l.exposure == 30.0f && l.lift_distance == 7.0f;
        CHECK(long_ones == want);
        CHECK(f.header.bottom_layers == want);
        CHECK(f.layers[std::size_t(want)].exposure == 2.5f);
        CHECK_FALSE(f.header.advanced);
    }
    Settings s = base();
    s.resin_wait_after_cure = 0.5;
    s.resin_wait_after_lift = 0.25;
    s.resin_wait_before_exposure = 1.5;
    s.resin_lift_distance = 6.0;
    s.resin_lift_speed = 2.0;
    s.resin_bottom_lift_distance = 8.0;
    s.resin_bottom_lift_speed = 1.0;
    s.resin_retract_speed = 3.0;
    s.resin_light_pwm = 200;
    s.resin_bottom_light_pwm = 230;
    s.resin_antialiasing = 4;
    const GooFile f = goo_of(slice_resin(cube(), s), s);
    const GooHeader& h = f.header;
    CHECK(h.antialiasing == 4);
    CHECK(h.bottom_before_lift == 0.5f);
    CHECK(h.before_lift == 0.5f);
    CHECK(h.bottom_after_lift == 0.25f);
    CHECK(h.after_lift == 0.25f);
    CHECK(h.bottom_after_retract == 1.5f);
    CHECK(h.after_retract == 1.5f);
    CHECK(h.lift_distance == 6.0f);
    CHECK(h.lift_speed == 120.0f);
    CHECK(h.bottom_lift_distance == 8.0f);
    CHECK(h.bottom_lift_speed == 60.0f);
    CHECK(h.retract_distance == 6.0f);
    CHECK(h.bottom_retract_distance == 8.0f);
    CHECK(h.retract_speed == 180.0f);
    CHECK(h.bottom_retract_speed == 180.0f);
    CHECK(h.pwm == 200);
    CHECK(h.bottom_pwm == 230);
    for (const float v : {h.bottom_second_lift_distance, h.bottom_second_lift_speed, h.second_lift_distance,
                          h.second_lift_speed, h.bottom_second_retract_distance, h.bottom_second_retract_speed,
                          h.second_retract_distance, h.second_retract_speed})
        CHECK(v == 0.0f);
    const GooLayer& bottom = f.layers[0];
    CHECK(bottom.before_lift == 0.5f);
    CHECK(bottom.after_lift == 0.25f);
    CHECK(bottom.after_retract == 1.5f);
    CHECK(bottom.lift_distance == 8.0f);
    CHECK(bottom.lift_speed == 60.0f);
    CHECK(bottom.retract_distance == 8.0f);
    CHECK(bottom.retract_speed == 180.0f);
    CHECK(bottom.pwm == 230);
    const GooLayer& normal = f.layers[100];
    CHECK(normal.lift_distance == 6.0f);
    CHECK(normal.lift_speed == 120.0f);
    CHECK(normal.retract_distance == 6.0f);
    CHECK(normal.retract_speed == 180.0f);
    CHECK(normal.pwm == 200);
    CHECK(normal.second_lift_distance == 0.0f);
    CHECK_FALSE(h.advanced);
}

TEST_CASE("resin RE5: the elephant's foot shrinks the bottom layers less and less", "[resin]") {
    Settings s = base();
    s.resin_elephant_foot = 0.3;
    // The cube's side at x 61.74 falls on a pixel's border (column 2334,
    // mirrored), so the pixels lost on that side are the whole ones.
    const ResinPrint p = slice_resin(cube(71.74, 89.6 / 2), s);
    const int full = lit_of(pixels(p, 100, s), s).c1;
    CHECK(full == 2333);
    const int want[10] = {8, 7, 6, 6, 5, 4, 3, 2, 1, 0};
    for (std::size_t l = 0; l < 10; ++l) {
        INFO("layer " << l);
        CHECK(full - lit_of(pixels(p, l, s), s).c1 == want[l]);
    }
    CHECK(pixels(p, 10, s) == pixels(p, 100, s));
}

TEST_CASE("resin RE6: the scale corrections", "[resin]") {
    Settings s = base();
    s.resin_scale_x = 1.05;
    s.resin_scale_z = 1.1;
    const ResinPrint p = slice_resin(cube(), s);
    CHECK(p.layers.size() == 220);
    CHECK_THAT(p.volume, WithinAbs(4620.0, 0.05));
    const Lit l = lit_of(pixels(p, 100, s), s);
    CHECK(l.c1 - l.c0 + 1 == 600);
    CHECK(l.r1 - l.r0 + 1 == 572);
}

TEST_CASE("resin RE8: the gamma of the edges' greys", "[resin]") {
    Settings s = base();
    s.resin_elephant_foot = 0.0;
    // Placed as in the spec's measure: 28.6 % of the left column and 14.3 %
    // of the right one covered.
    const IndexedMesh part = cube(71.7, 89.6 / 2);
    const std::vector<std::uint8_t> one = pixels(slice_resin(part, s), 100, s);
    const Lit a = lit_of(one, s);
    CHECK(a.c0 == 1763);
    CHECK(a.c1 == 2335);
    CHECK(a.greys.size() == 7);
    CHECK(a.count == 327756);
    CHECK(at(one, s, 1763, 1280) == 73);
    CHECK(at(one, s, 2335, 1280) == 37);
    s.resin_gamma = 2.0;
    const std::vector<std::uint8_t> two = pixels(slice_resin(part, s), 100, s);
    const Lit b = lit_of(two, s);
    CHECK(b.count == a.count);
    CHECK(b.c0 == 1763);
    CHECK(b.c1 == 2335);
    CHECK(at(two, s, 1763, 1280) == 21);
    CHECK(at(two, s, 2335, 1280) == 5);
    s.resin_gamma = 0.0;
    const Lit c = lit_of(pixels(slice_resin(part, s), 100, s), s);
    CHECK(c.c0 == 1764);
    CHECK(c.c1 == 2334);
    CHECK(c.greys == std::set<int>{0, 255});
    CHECK(c.count == 326612);
}

TEST_CASE("resin RE9: the Z correction leaves what hangs dark for its first layers", "[resin]") {
    // supports_more.md's overhang: a column of 10 x 20 x 10 mm with a slab
    // of 20 x 20 x 2 mm on top (Z 8 to 10) reaching 10 mm out in +X.
    TriangleMesh m;
    const float y0 = float(89.6 / 2 - 10), y1 = float(89.6 / 2 + 10);
    add_box(m, 61.7f, y0, 0, 71.7f, y1, 10);
    add_box(m, 61.7f, y0, 8, 81.7f, y1, 10);
    const IndexedMesh part = weld_exact(m);
    Settings s = base();
    const ResinPrint none = slice_resin(part, s);
    const Lit slab = lit_of(pixels(none, 160, s), s); // Z 8.05
    CHECK(slab.count == 327756);
    CHECK(slab.c0 == 1763);
    CHECK(slab.c1 == 2335);
    s.resin_z_correction_layers = 3;
    const ResinPrint corrected = slice_resin(part, s);
    for (std::size_t l : {160u, 161u, 162u}) {
        INFO("layer " << l);
        const Lit column = lit_of(pixels(corrected, l, s), s);
        CHECK(column.count == 164164);
        CHECK(column.c0 == 2049);
        CHECK(column.c1 == 2335);
    }
    CHECK(lit_of(pixels(corrected, 163, s), s).count == 327756); // Z 8.20
    CHECK(pixels(corrected, 100, s) == pixels(none, 100, s));
}

TEST_CASE("resin RE10: the absolute correction grows or shrinks every section", "[resin]") {
    // Tisma Slicer's command line does not apply it (RE10); the engine
    // offsets every section's outline by it (rule 22).
    auto width = [](double correction) {
        Settings s = base();
        s.resin_absolute_correction = correction;
        const std::vector<std::uint8_t> px = pixels(slice_resin(cube(), s), 100, s);
        double sum = 0.0;
        for (int c = 0; c < s.display_pixels_x; ++c)
            sum += at(px, s, c, 1280) / 255.0;
        return sum * 0.035;
    };
    CHECK_THAT(width(0.0), WithinAbs(20.0, 0.01));
    CHECK_THAT(width(0.1), WithinAbs(20.2, 0.01));
    CHECK_THAT(width(-0.1), WithinAbs(19.8, 0.01));
    CHECK_THAT(width(0.5), WithinAbs(21.0, 0.01));
    CHECK_THAT(width(-0.5), WithinAbs(19.0, 0.01));
}

TEST_CASE("resin: what does not fit, and the settings read and written", "[resin]") {
    Settings s = base();
    CHECK_THROWS_WITH(slice_resin(cube(5, 44.8), s), ContainsSubstring("does not fit on the display"));
    s.max_height = 8.0;
    CHECK_THROWS_WITH(slice_resin(cube(), s), ContainsSubstring("machine.max_height"));
    CHECK_THROWS_AS(slice_resin(IndexedMesh{}, s), ResinError);

    const Settings back = parse_settings(R"({"machine": {"display_pixels_x": 3840, "display_pixels_y": 2400,
        "display_width": 153.6, "display_height": 96, "display_mirror_x": false},
        "resin": {"layer_height": 0.03, "exposure": 2, "bottom_layers": 3, "gamma": 0.8, "printer_name": "SATURN",
                  "z_correction_layers": 2, "absolute_correction": -0.02}})");
    CHECK(back.display_pixels_x == 3840);
    CHECK(back.display_width == 153.6);
    CHECK_FALSE(back.display_mirror_x);
    CHECK(back.resin_layer_height == 0.03);
    CHECK(back.resin_bottom_layers == 3);
    CHECK(back.resin_printer_name == "SATURN");
    const Settings again = parse_settings(settings_json(back));
    CHECK(again.display_pixels_y == 2400);
    CHECK(again.resin_gamma == 0.8);
    CHECK(again.resin_z_correction_layers == 2);
    CHECK(again.resin_absolute_correction == -0.02);
    CHECK(again.resin_printer_name == "SATURN");
    CHECK_THROWS_AS(parse_settings(R"({"resin": {"light_pwm": 300}})"), SettingsError);
    CHECK_THROWS_AS(parse_settings(R"({"resin": {"bottom_layers": 2.5}})"), SettingsError);
    CHECK_THROWS_AS(parse_settings(R"({"machine": {"display_pixels_x": 0}})"), SettingsError);
    CHECK_THROWS_AS(parse_settings(R"({"resin": {"printer_name": "a name far longer than thirty-one bytes"}})"),
                    SettingsError);
}

TEST_CASE("resin: a damaged GOO file is refused, never read past its end", "[resin]") {
    // A small display, so that the file is small.
    Settings s = base();
    s.display_pixels_x = 64;
    s.display_pixels_y = 40;
    s.display_width = 64 * 0.5;
    s.display_height = 40 * 0.5;
    TriangleMesh m;
    add_box(m, 10, 6, 0, 22, 14, 1);
    const ResinPrint p = slice_resin(weld_exact(m), s);
    std::stringstream file;
    write_goo(file, p, s, "2026-10-27 12:00:00");
    const std::string good = file.str();
    std::uint64_t state = 0x9e3779b97f4a7c15ull;
    auto next = [&]() {
        state ^= state << 13, state ^= state >> 7, state ^= state << 17;
        return state;
    };
    int refused = 0;
    for (int run = 0; run < 400; ++run) {
        std::string bad = good;
        if (run % 2 == 0) {
            bad.resize(std::size_t(next() % bad.size()));
        } else {
            // Bytes changed past the previews: the counts, the sizes and the images.
            for (int k = 0; k < 3; ++k)
                bad[kGooHeaderSize - 200 + std::size_t(next() % (bad.size() - kGooHeaderSize + 200))] ^=
                    char(1 + next() % 255);
        }
        std::istringstream in(bad);
        try {
            const GooFile f = read_goo(in);
            for (const GooLayer& l : f.layers)
                decode_goo_image(l.image, 64, 40);
        } catch (const ResinError&) {
            ++refused;
        }
    }
    CHECK(refused > 200);
}

namespace {

Polygon rect(double x0, double y0, double x1, double y1) {
    return {{to_units(x0), to_units(y0)}, {to_units(x1), to_units(y0)}, {to_units(x1), to_units(y1)},
            {to_units(x0), to_units(y1)}};
}

Polygons minus(const Polygon& outer, const Polygons& holes) {
    return Clipper2Lib::Difference(Polygons{outer}, holes, Clipper2Lib::FillRule::NonZero);
}

// The spec's cup (VE1): `floor` layers of a 20 x 20 mm floor, with a square
// hole of `drain` mm through it if above 0, then `walls` layers of 2 mm
// walls round a 16 x 16 mm hollow; upside down, the walls come first.
std::vector<Polygons> cup_sections(int floor, int walls, double drain = 0.0, bool upside_down = false) {
    std::vector<Polygons> out;
    const Polygon outer = rect(0, 0, 20, 20);
    const Polygons hollow = {rect(2, 2, 18, 18)};
    const Polygons drain_hole = drain > 0.0 ? Polygons{rect(10 - drain / 2, 10 - drain / 2, 10 + drain / 2, 10 + drain / 2)}
                                            : Polygons{};
    for (int i = 0; i < floor; ++i)
        out.push_back(minus(outer, drain_hole));
    for (int i = 0; i < walls; ++i)
        out.push_back(minus(outer, hollow));
    if (upside_down)
        std::reverse(out.begin(), out.end());
    return out;
}

// A cup turned on a lathe: radius 10, 10 mm tall, a floor 2 mm thick and
// walls 2 mm thick; or the solid cylinder of the same size.
IndexedMesh lathe_cup(bool hollow, double cx, double cy) {
    constexpr int n = 64;
    TriangleMesh m;
    auto at = [&](double r, int i, double z) {
        const double a = 2.0 * 3.14159265358979323846 * (i % n) / n;
        return Vec3f{float(cx + r * std::cos(a)), float(cy + r * std::sin(a)), float(z)};
    };
    const Vec3f bottom{float(cx), float(cy), 0.0f}, top{float(cx), float(cy), 10.0f}, floor{float(cx), float(cy), 2.0f};
    for (int i = 0; i < n; ++i) {
        m.add_triangle(bottom, at(10, i + 1, 0), at(10, i, 0));
        m.add_triangle(at(10, i, 0), at(10, i + 1, 0), at(10, i + 1, 10));
        m.add_triangle(at(10, i, 0), at(10, i + 1, 10), at(10, i, 10));
        if (!hollow) {
            m.add_triangle(top, at(10, i, 10), at(10, i + 1, 10));
            continue;
        }
        m.add_triangle(at(10, i, 10), at(10, i + 1, 10), at(8, i + 1, 10));
        m.add_triangle(at(10, i, 10), at(8, i + 1, 10), at(8, i, 10));
        m.add_triangle(at(8, i, 10), at(8, i + 1, 10), at(8, i + 1, 2));
        m.add_triangle(at(8, i, 10), at(8, i + 1, 2), at(8, i, 2));
        m.add_triangle(floor, at(8, i, 2), at(8, i + 1, 2));
    }
    return weld_exact(m);
}

} // namespace

TEST_CASE("resin VE1 to VE8: cups, layer by layer from the platform", "[resin]") {
    // VE1, on the platform: one cup from the hollow's first layer to the top.
    std::vector<ResinCup> cups = find_cups(cup_sections(5, 20), 0.05, false);
    REQUIRE(cups.size() == 1);
    CHECK(cups[0].first == 5);
    CHECK(cups[0].last == 24);
    CHECK_THAT(cups[0].area, WithinAbs(256.0, 1e-6));
    CHECK_THAT(cups[0].z0, WithinAbs(0.25, 1e-9));
    CHECK_THAT(cups[0].z1, WithinAbs(1.25, 1e-9));
    // VE2, raised on supports: its own floor closes it.
    CHECK(find_cups(cup_sections(5, 20), 0.05, true).size() == 1);
    // VE3, a 4 x 4 mm drain hole, raised: none. VE4, the same on the platform: one.
    CHECK(find_cups(cup_sections(5, 20, 4.0), 0.05, true).empty());
    CHECK(find_cups(cup_sections(5, 20, 4.0), 0.05, false).size() == 1);
    // VE5, a 1 x 1 mm drain hole is too small an exit.
    CHECK(find_cups(cup_sections(5, 20, 1.0), 0.05, true).size() == 1);
    // VE6, a tube: none raised, one on the platform.
    CHECK(find_cups(cup_sections(0, 20), 0.05, true).empty());
    CHECK(find_cups(cup_sections(0, 20), 0.05, false).size() == 1);
    // VE7, the cup upside down (open towards the platform), raised: none.
    CHECK(find_cups(cup_sections(5, 20, 0.0, true), 0.05, true).empty());
    // VE8, a 1 x 1 mm hole through a 10-layer floor on the platform: under 4 mm2, not reported.
    CHECK(find_cups(cup_sections(10, 0, 1.0), 0.05, false).empty());
    // A hollow that opens sideways to the air below it (a step in) is vented.
    std::vector<Polygons> step = cup_sections(5, 0);
    for (int i = 0; i < 10; ++i)
        step.push_back(minus(rect(0, 0, 20, 20), {rect(2, 2, 18, 18)}));
    step[4] = Polygons{rect(0, 0, 20, 10)}; // the floor's last layer covers only half
    CHECK(find_cups(step, 0.05, false).empty());
}

TEST_CASE("resin VE9: a cup turned on a lathe warns, the solid cylinder does not", "[resin]") {
    Settings s = base();
    s.resin_layer_height = 0.1;
    const std::vector<Polygons> cup = resin_sections(lathe_cup(true, 70, 45), s);
    REQUIRE(cup.size() == 100);
    // On the platform, and on supports with a pad (the pad closes it too).
    const std::vector<ResinCup> cups = find_cups(cup, 0.1, false);
    REQUIRE(cups.size() == 1);
    CHECK(cups[0].first == 20);
    CHECK(cups[0].last == 99);
    CHECK_THAT(cups[0].area, WithinAbs(3.14159 * 64, 0.5));
    CHECK(find_cups(resin_sections(lathe_cup(false, 70, 45), s), 0.1, false).empty());
    // The print carries it, and its warning says how to fix it.
    const ResinPrint p = slice_resin(lathe_cup(true, 70, 45), s);
    REQUIRE(p.cups.size() == 1);
    const std::vector<Warning> w = resin_warnings(p, "cup.stl");
    REQUIRE(w.size() == 1);
    CHECK(w[0].kind == "cupping");
    CHECK(w[0].layer == 20);
    CHECK_THAT(w[0].z, WithinAbs(2.0, 1e-9));
    CHECK_THAT(w[0].message, ContainsSubstring("cup.stl"));
    CHECK_THAT(w[0].message, ContainsSubstring("drain hole of 3 to 5 mm"));
    CHECK_THAT(w[0].message, ContainsSubstring("z 2.00 to 10.00 mm"));
    CHECK(slice_resin(lathe_cup(false, 70, 45), s).cups.empty());
}

TEST_CASE("resin CR1: the exposure tower's bands", "[resin]") {
    Settings s = base();
    s.resin_bottom_layers = 5;
    const ResinTower tower; // 1.5 to 3.5 s by 0.25, bands of 2 mm
    CHECK(tower_exposures(tower).size() == 9);
    const ResinPrint p =
        slice_resin(cube(), s, [&](double z) { return tower_exposure(tower, s.resin_exposure, z); });
    REQUIRE(p.layers.size() == 200);
    for (std::size_t l = 0; l < 5; ++l)
        CHECK(p.layers[l].exposure == 30.0);
    for (std::size_t l = 5; l < 40; ++l)
        CHECK(p.layers[l].exposure == 2.5); // the base, to Z 2.0
    const std::pair<std::size_t, double> bands[] = {{40, 1.5}, {80, 1.75}, {120, 2.0}, {160, 2.25}};
    for (const auto& [from, exposure] : bands) {
        INFO("from layer " << from);
        CHECK_THAT(p.layers[from].z, WithinAbs(0.05 * double(from + 1), 1e-9));
        CHECK(p.layers[from].exposure == exposure);
        CHECK(p.layers[from + 39].exposure == exposure);
    }
    // Past the end, the last exposure; the header keeps resin.exposure.
    CHECK(tower_exposure(tower, 2.5, 25.0) == 3.5);
    const GooFile f = goo_of(p, s);
    CHECK(f.header.exposure == 2.5f);
    CHECK(f.layers[40].exposure == 1.5f);
    // The engine turns the advanced mode on, so that the printer takes each
    // layer's exposure (rule 12; Tisma Slicer writes 0).
    CHECK(f.header.advanced);
}

TEST_CASE("resin: the exposure tower's part and the measuring block", "[resin]") {
    const ResinTower tower;
    TriangleMesh m = resin_tower_mesh(tower);
    m.translate(71.7f, 44.8f, 0.0f);
    const BoundingBox box = m.bounding_box();
    CHECK_THAT(box.max.z, WithinAbs(20.0, 1e-5)); // a base and nine bands
    CHECK_THAT(box.max.x - box.min.x, WithinAbs(24.0, 1e-4));
    CHECK_THAT(box.max.y - box.min.y, WithinAbs(10.0, 1e-4)); // 8 mm and the fins
    Settings s = base();
    const std::vector<Polygons> sections = resin_sections(weld_exact(m), s);
    REQUIRE(sections.size() == 400);
    auto pieces = [&](std::size_t layer) {
        Clipper2Lib::PolyTree64 tree;
        Clipper2Lib::Clipper64 c;
        c.AddSubject(sections[layer]);
        c.Execute(Clipper2Lib::ClipType::Union, Clipper2Lib::FillRule::NonZero, tree);
        return tree.Count();
    };
    CHECK(pieces(41) == 1);  // z 2.075: under the holes, before the fins
    CHECK(pieces(59) == 6);  // z 2.975: the five holes cut across it
    CHECK(pieces(10) == 1);  // the base has no holes
    // Fins of 0.15 to 0.5 mm at the back, from 0.25 mm over the band's bottom.
    const Clipper2Lib::Rect64 fins = Clipper2Lib::GetBounds(sections[59]);
    CHECK_THAT(to_mm(fins.bottom) - 44.8, WithinAbs(6.0, 1e-3));
    CHECK_THAT(to_mm(Clipper2Lib::GetBounds(sections[41]).bottom) - 44.8, WithinAbs(4.0, 1e-3));
    // The groove in the front between bands: from 0.15 under the border to 0.15 over it.
    CHECK_THAT(to_mm(Clipper2Lib::GetBounds(sections[40]).top) - 44.8, WithinAbs(-3.5, 1e-3));
    CHECK_THAT(to_mm(Clipper2Lib::GetBounds(sections[44]).top) - 44.8, WithinAbs(-4.0, 1e-3));
    CHECK_THROWS_AS(resin_tower_mesh(ResinTower{1.5, 3.5, 0.25, 1.0}), ResinError);

    const BoundingBox block = resin_measure_block().bounding_box();
    CHECK(block.max.x - block.min.x == 30.0f);
    CHECK(block.max.y - block.min.y == 30.0f);
    CHECK(block.max.z - block.min.z == 10.0f);
    s.resin_scale_x = 1.02;
    const std::array<double, 3> scale = resin_scale_from(s, 29.7, 30.3, 9.9);
    CHECK_THAT(scale[0], WithinAbs(1.02 * 30.0 / 29.7, 1e-12));
    CHECK_THAT(scale[1], WithinAbs(30.0 / 30.3, 1e-12));
    CHECK_THAT(scale[2], WithinAbs(10.0 / 9.9, 1e-12));
    CHECK_THROWS_AS(resin_scale_from(s, 0.0, 30.0, 10.0), ResinError);
}

TEST_CASE("resin RS1: supports and a pad under a cube", "[resin][resin_supports]") {
    Settings s = base();
    s.resin_supports = true;
    s.resin_pad = true;
    const ResinPrint p = slice_resin(cube(), s);
    REQUIRE(p.layers.size() == 340); // 2 + 5 + 10 mm
    // Decoded: the pad from Z 0.05 to 2.0, the pillars from 2.0 to 7.0, the cube from 7.05.
    auto lit = [&](std::size_t l) { return lit_of(pixels(p, l, s), s); };
    const double px = 0.035 * 0.035;
    const Lit pad = lit(20), pillars = lit(100), part = lit(160);
    CHECK(double(pad.count) * px > 300.0);
    // The pad reaches past the supports it holds, by resin.pad_margin.
    CHECK((pad.c1 - pad.c0 + 1) * 0.035 > 16.0 + 2.0 + 2 * 1.6 - 0.2);
    CHECK_THAT(double(pillars.count) * px, WithinAbs(19.6, 3.0)); // 25 pillars of 1 mm
    CHECK_THAT(double(part.count) * px, WithinAbs(400.0, 3.0));
    CHECK(lit(139).count < pad.count);          // Z 7.0: the tips, under the cube
    CHECK(double(lit(140).count) * px > 395.0); // Z 7.05: the cube
    CHECK(p.volume > 4000.0 + 2.0 * 300.0);
    CHECK(p.cups.empty());
    // Without the pad, the pillars stand on the platform and the cube starts at Z 5.05.
    s.resin_pad = false;
    const ResinPrint bare = slice_resin(cube(), s);
    CHECK(bare.layers.size() == 300);
    // The feet, twice the pillar's diameter (less the elephant's foot on the first layer).
    CHECK_THAT(double(lit_of(pixels(bare, 0, s), s).count) * px, WithinAbs(25 * 3.14159 * 0.8 * 0.8, 6.0));
    CHECK_THAT(double(lit_of(pixels(bare, 20, s), s).count) * px, WithinAbs(19.6, 3.0));
}

TEST_CASE("resin RS3: a pad with nothing to hold is an error", "[resin][resin_supports]") {
    Settings s = base();
    s.resin_pad = true;
    CHECK_THROWS_WITH(slice_resin(cube(), s), ContainsSubstring("the pad cannot be made"));
    // Around the part, it stands on the platform with the part.
    s.resin_pad_around_object = true;
    const ResinPrint p = slice_resin(cube(), s);
    CHECK(p.layers.size() == 200);
    CHECK(double(lit_of(pixels(p, 10, s), s).count) * 0.035 * 0.035 > 23.0 * 23.0);
    CHECK_THAT(double(lit_of(pixels(p, 100, s), s).count) * 0.035 * 0.035, WithinAbs(400.0, 3.0));
}

TEST_CASE("resin RS2 and RS4: a hollowed cube", "[resin][resin_supports]") {
    Settings s = base();
    s.resin_hollow = true;
    s.resin_hollow_wall = 2.0;
    s.resin_hollow_closing = 2.0;
    const IndexedMesh part = cube();
    const ResinPrint p = slice_resin(part, s);
    // Solid at the bottom and the top, hollow in the middle with 2 mm walls or more.
    auto area = [&](std::size_t l) { return double(lit_of(pixels(p, l, s), s).count) * 0.035 * 0.035; };
    CHECK_THAT(area(30), WithinAbs(400.0, 3.0));
    CHECK_THAT(area(199), WithinAbs(400.0, 3.0));
    const double middle = area(100);
    CHECK(middle < 400.0 - 15.0 * 15.0);
    CHECK(middle > 400.0 - 16.0 * 16.0 - 2.0);
    CHECK(p.volume > 2300.0);
    CHECK(p.volume < 2800.0);
    // The closed hollow is a cup until it has drain holes (rule 4).
    REQUIRE(p.cups.size() == 1);
    CHECK(p.cups[0].first >= 39);
    CHECK(p.cups[0].last <= 161);
    // RS4: with 3 mm walls the 20 x 20 x 10 mm cube still has room for a hollow.
    s.resin_hollow_wall = 3.0;
    const ResinPrint thick = slice_resin(part, s);
    CHECK(thick.volume < 4000.0 - 400.0);
    CHECK(double(lit_of(pixels(thick, 100, s), s).count) * 0.035 * 0.035 < 400.0 - 13.0 * 13.0);
}

TEST_CASE("resin: the hollow's inner bars and the drain holes", "[resin][resin_supports]") {
    Settings s = base();
    s.resin_hollow = true;
    s.resin_hollow_wall = 2.0;
    s.resin_hollow_infill = 20.0;
    CHECK_THAT(infill_bar_width(s), WithinAbs(1.6, 1e-12)); // 8 · √(0.2 / 5)
    const ResinPrint bars = slice_resin(cube(), s);
    s.resin_hollow_infill = 0.0;
    const ResinPrint empty = slice_resin(cube(), s);
    CHECK(bars.volume > empty.volume + 100.0);
    CHECK(resin_warnings(bars, "cube.stl").size() == 1); // the cup; the bars are wide enough
    s.resin_hollow_infill = 3.0;
    const std::vector<Warning> thin = resin_warnings(slice_resin(cube(), s), "cube.stl");
    CHECK(std::any_of(thin.begin(), thin.end(), [](const Warning& w) { return w.kind == "thin_bars"; }));
    // Raised on supports, a drain hole in the floor opens the hollow: no cup.
    s.resin_hollow_infill = 0.0;
    s.resin_supports = true;
    s.resin_pad = true;
    CHECK(slice_resin(cube(), s).cups.size() == 1);
    const ResinDrain drain{143.43 / 2, 89.6 / 2, 7.0, 0.0, 0.0, -1.0, 3.0, 4.0};
    const ResinPrint drained = slice_resin(cube(), s, {}, {drain});
    CHECK(drained.cups.empty());
    // The hole goes through the floor: the cube's first layer has it (7 mm2).
    const ResinPrint whole = slice_resin(cube(), s);
    const double hole = double(lit_of(pixels(whole, 140, s), s).count - lit_of(pixels(drained, 140, s), s).count);
    CHECK_THAT(hole * 0.035 * 0.035, WithinAbs(7.0, 0.5));
}

TEST_CASE("resin: a .goo output from the command line", "[resin][cli]") {
    const std::filesystem::path dir = std::filesystem::temp_directory_path() / "tisma_resin_cli";
    std::filesystem::create_directories(dir);
    {
        std::ofstream stl(dir / "cube.stl");
        stl << "solid c\n";
        TriangleMesh m;
        add_box(m, -10, -10, 0, 10, 10, 10);
        for (std::size_t t = 0; t < m.triangle_count(); ++t) {
            stl << "facet normal 0 0 0\nouter loop\n";
            for (int k = 0; k < 3; ++k) {
                const Vec3f& v = m.corner(t, k);
                stl << "vertex " << v.x << " " << v.y << " " << v.z << "\n";
            }
            stl << "endloop\nendfacet\n";
        }
        stl << "endsolid c\n";
        std::ofstream(dir / "resin.json") << R"({"resin": {"bottom_layers": 10, "supports": false, "pad": false}})";
    }
    const std::string model = (dir / "cube.stl").string(), config = (dir / "resin.json").string(),
                      goo = (dir / "cube.goo").string();
    std::ostringstream out, err;
    const std::vector<std::string_view> args = {"slice", model, "--config", config, "-o", goo};
    REQUIRE(run_cli(args, out, err) == 0);
    CHECK_THAT(out.str(), ContainsSubstring("layers: 200"));
    std::ifstream in(goo, std::ios::binary);
    const GooFile f = read_goo(in);
    CHECK(f.layers.size() == 200);
    // Centred on the display, as RE1 and RE2.
    CHECK(f.layers[100].image.size() == 4586);
    CHECK(f.layers[100].image.back() == 0x03);
    // A cup on the platform goes into the warnings.
    {
        std::ofstream stl(dir / "cup.stl");
        stl << "solid c\n";
        const IndexedMesh cup = lathe_cup(true, 0, 0);
        for (const auto& t : cup.triangles) {
            stl << "facet normal 0 0 0\nouter loop\n";
            for (const std::uint32_t v : t)
                stl << "vertex " << cup.vertices[v].x << " " << cup.vertices[v].y << " " << cup.vertices[v].z << "\n";
            stl << "endloop\nendfacet\n";
        }
        stl << "endsolid c\n";
    }
    const std::string cup = (dir / "cup.stl").string(), warnings = (dir / "cup.json").string();
    std::ostringstream out2, err2;
    const std::vector<std::string_view> cup_args = {"slice", cup, "--config", config, "-o", goo, "--warnings", warnings};
    REQUIRE(run_cli(cup_args, out2, err2) == 0);
    CHECK_THAT(out2.str(), ContainsSubstring("warnings: 1"));
    std::ifstream report(warnings);
    const std::string json{std::istreambuf_iterator<char>(report), std::istreambuf_iterator<char>()};
    CHECK_THAT(json, ContainsSubstring("\"cupping\""));
    CHECK_THAT(json, ContainsSubstring("cup.stl: a cup from z"));
    std::filesystem::remove_all(dir);
}
