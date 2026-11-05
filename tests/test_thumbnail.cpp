// Tests of the G-code thumbnails (src/gcode/thumbnail.hpp; gcode_extras.md, rule 8).

#include "config/settings.hpp"
#include "gcode/binary.hpp"
#include "gcode/image_write.hpp"
#include "gcode/thumbnail.hpp"
#include "io/png.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <stdexcept>
#include <string>

using namespace tisma;

namespace {

TriangleMesh box(float x, float y, float z) {
    TriangleMesh m;
    const Vec3f p[8] = {{0, 0, 0}, {x, 0, 0}, {x, y, 0}, {0, y, 0}, {0, 0, z}, {x, 0, z}, {x, y, z}, {0, y, z}};
    const int f[12][3] = {{0, 2, 1}, {0, 3, 2}, {4, 5, 6}, {4, 6, 7}, {0, 1, 5}, {0, 5, 4},
                          {1, 2, 6}, {1, 6, 5}, {2, 3, 7}, {2, 7, 6}, {3, 0, 4}, {3, 4, 7}};
    for (const auto& t : f)
        m.add_triangle(p[t[0]], p[t[1]], p[t[2]]);
    return m;
}

} // namespace

TEST_CASE("gcode.thumbnails: the list of sizes and formats (X7)", "[gcode][thumbnail]") {
    const std::vector<ThumbnailSize> s = parse_thumbnails("16x16/PNG,32x24/jpg; 20X20/QOI , 300x300");
    REQUIRE(s.size() == 4);
    CHECK(s[0] == ThumbnailSize{16, 16, ThumbnailFormat::Png});
    CHECK(s[1] == ThumbnailSize{32, 24, ThumbnailFormat::Jpg});
    CHECK(s[2] == ThumbnailSize{20, 20, ThumbnailFormat::Qoi});
    CHECK(s[3] == ThumbnailSize{300, 300, ThumbnailFormat::Png});
    CHECK(thumbnails_text(s) == "16x16/PNG,32x24/JPG,20x20/QOI,300x300/PNG");
    CHECK(parse_thumbnails("").empty());
    CHECK(parse_thumbnails(" , ").empty());
    CHECK_THROWS_AS(parse_thumbnails("16x16/GIF"), std::invalid_argument);
    CHECK_THROWS_AS(parse_thumbnails("16/PNG"), std::invalid_argument);
    CHECK_THROWS_AS(parse_thumbnails("0x16/PNG"), std::invalid_argument);
    CHECK_THROWS_AS(parse_thumbnails("2000x16"), std::invalid_argument);
    CHECK_THROWS_AS(parse_thumbnails("ax16"), std::invalid_argument);

    // In the settings: kept as written, refused when wrong.
    Settings settings;
    CHECK(settings.gcode_thumbnails.empty());
    CHECK_THROWS(parse_settings(R"({"gcode": {"thumbnails": "16x16/BMP"}})"));
    CHECK(parse_settings(R"({"gcode": {"thumbnails": "16x16/PNG,32x24/JPG"}})").gcode_thumbnails ==
          "16x16/PNG,32x24/JPG");
}

TEST_CASE("thumbnails: the part drawn on the CPU, centred, the background clear", "[gcode][thumbnail]") {
    const std::vector<std::uint8_t> p = draw_part(box(20, 20, 20), 64, 48);
    REQUIRE(p.size() == std::size_t(64 * 48 * 4));
    // The corners are clear, the middle is the part.
    CHECK(p[3] == 0);
    CHECK(p[(std::size_t(47) * 64 + 63) * 4 + 3] == 0);
    CHECK(p[(std::size_t(24) * 64 + 32) * 4 + 3] == 255);
    // As large as fits: the part reaches near the top and bottom edges (6 % margin).
    int top = 48, bottom = -1;
    for (int y = 0; y < 48; ++y)
        for (int x = 0; x < 64; ++x)
            if (p[(std::size_t(y) * 64 + x) * 4 + 3] > 0)
                top = std::min(top, y), bottom = std::max(bottom, y);
    CHECK(top <= 4);
    CHECK(bottom >= 43);
    // Three faces seen, each its own shade.
    CHECK(p[(std::size_t(10) * 64 + 32) * 4] != p[(std::size_t(34) * 64 + 24) * 4]);
    // An empty mesh: all clear.
    const std::vector<std::uint8_t> none = draw_part(TriangleMesh{}, 8, 8);
    CHECK(std::all_of(none.begin(), none.end(), [](std::uint8_t v) { return v == 0; }));
}

TEST_CASE("thumbnails: QOI files written and read back the same", "[gcode][thumbnail]") {
    const std::vector<std::uint8_t> p = draw_part(box(30, 10, 15), 40, 30);
    const std::vector<std::uint8_t> file = encode_qoi(40, 30, p.data());
    CHECK(std::string(file.begin(), file.begin() + 4) == "qoif");
    CHECK(file.size() < p.size()); // the runs of clear background pack
    int w = 0, h = 0;
    std::vector<std::uint8_t> back;
    REQUIRE(decode_qoi(file, w, h, back));
    CHECK(w == 40);
    CHECK(h == 30);
    CHECK(back == p);
    // Every kind of step: noise, small steps, alpha changes, long runs.
    std::vector<std::uint8_t> mixed;
    unsigned seed = 7;
    for (int i = 0; i < 500; ++i) {
        seed = seed * 1103515245u + 12345u;
        const int kind = i / 100;
        const std::uint8_t r = kind == 0 ? std::uint8_t(seed >> 16) : std::uint8_t(100 + (i % 3));
        const std::uint8_t a = kind == 3 ? std::uint8_t(seed >> 8) : 255;
        mixed.insert(mixed.end(), {r, std::uint8_t(kind == 1 ? 100 + i % 40 : 50), std::uint8_t(kind == 4 ? 9 : 200), a});
    }
    REQUIRE(decode_qoi(encode_qoi(25, 20, mixed.data()), w, h, back));
    CHECK(back == mixed);
    CHECK_FALSE(decode_qoi({'n', 'o', 'p', 'e'}, w, h, back));
}

TEST_CASE("thumbnails: one block each in a binary G-code, after the printer's metadata", "[gcode][thumbnail][binary]") {
    Settings s;
    s.gcode_binary = true;
    s.gcode_thumbnails = "16x16/PNG,32x24/JPG,20x20/QOI";
    std::vector<Warning> warnings;
    const std::vector<GcodeThumbnail> t = gcode_thumbnails(box(20, 10, 5), s, &warnings);
    REQUIRE(t.size() == 3);
    CHECK(warnings.empty());
    CHECK(t[0].format == 0);
    CHECK(t[0].data.substr(1, 3) == "PNG");
    CHECK(t[1].format == 1);
    CHECK(t[1].data.substr(0, 3) == "\xFF\xD8\xFF"); // a JPEG's start
    CHECK(t[1].width == 32);
    CHECK(t[1].height == 24);
    CHECK(t[2].format == 2);
    CHECK(t[2].data.substr(0, 4) == "qoif");

    const std::string text = "G28\nG1 X10 Y10\n; estimated printing time = 1m\n; layers.height = 0.2\n";
    const std::string bin = binary_gcode(text, s, t);
    const BinaryGcode back = read_binary_gcode(bin);
    CHECK(back.thumbnails == t);
    CHECK(back.gcode == "G28\nG1 X10 Y10\n");
    // The order: file, printer, the thumbnails, print, slicer, G-code.
    const std::size_t printer = bin.find("printer_model="), picture = bin.find("qoif"),
                      print = bin.find("estimated printing time=");
    CHECK(printer < picture);
    CHECK(picture < print);
    // Each block's parameters: format, width, height (16 bits, little-endian).
    const std::size_t qoi_block = picture - 14;
    CHECK(bin[qoi_block] == 5);                 // the thumbnail type
    CHECK(bin[qoi_block + 8] == 2);             // QOI
    CHECK(bin[qoi_block + 10] == 20);           // width
    CHECK(bin[qoi_block + 12] == 20);           // height

    // A text G-code: none yet, said once.
    s.gcode_binary = false;
    warnings.clear();
    CHECK(gcode_thumbnails(box(20, 10, 5), s, &warnings).empty());
    CHECK(warnings.size() == 1);
    // Nothing asked: nothing, no warning.
    s.gcode_thumbnails.clear();
    warnings.clear();
    CHECK(gcode_thumbnails(box(20, 10, 5), s, &warnings).empty());
    CHECK(warnings.empty());
}

TEST_CASE("thumbnails: compressed PNG and JPG, read back by an image reader", "[gcode][thumbnail]") {
    const std::vector<std::uint8_t> p = draw_part(box(30, 20, 10), 220, 124);
    const std::vector<std::uint8_t> png = encode_png_compressed(220, 124, p.data());
    // Deflated: far below the stored PNG of io/png (about 110 kB).
    CHECK(png.size() < encode_png(220, 124, p.data()).size() / 4);
    const std::vector<std::uint8_t> jpg = encode_jpg(220, 124, p.data());
    REQUIRE(jpg.size() > 4);
    CHECK(jpg[0] == 0xFF);
    CHECK(jpg[1] == 0xD8);
    CHECK(jpg[jpg.size() - 2] == 0xFF);
    CHECK(jpg[jpg.size() - 1] == 0xD9);
    CHECK(encode_jpg(0, 10, p.data()).empty());
}
