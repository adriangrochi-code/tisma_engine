// The USB camera's format choice and frame pacing (printer_connections.md,
// «Cámara USB de la computadora», rules 2 and 3), without a camera.
#include "device/usb_camera.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace tisma::device;

TEST_CASE("USB camera: the largest format up to 1280 x 720 at 10 fps or more, MJPEG first (rule 2)", "[device][camera]") {
    const std::vector<CameraFormat> formats{
        {1920, 1080, 30, true}, // too large
        {1280, 720, 10, false}, {1280, 720, 30, true}, {1280, 720, 5, true}, // MJPEG wins at the same size
        {640, 480, 30, true},   {800, 600, 8, true}, // 8 fps is too slow
    };
    const auto f = choose_camera_format(formats);
    REQUIRE(f);
    CHECK(f->width == 1280);
    CHECK(f->height == 720);
    CHECK(f->mjpeg);
    CHECK(f->fps == 30);
    CHECK(camera_format_text(*f) == "1280 \xC3\x97 720 MJPEG, 30 fps");

    // Uncompressed 1280 x 720 only: it still is the largest that fits.
    const auto raw = choose_camera_format({{1280, 720, 10, false}, {640, 480, 30, true}});
    REQUIRE(raw);
    CHECK_FALSE(raw->mjpeg);
    CHECK(raw->width == 1280);

    // None at 10 fps: the fastest, then the smallest.
    const auto slow = choose_camera_format({{1920, 1080, 5, true}, {640, 480, 8, false}, {320, 240, 8, false}});
    REQUIRE(slow);
    CHECK(slow->width == 320);
    CHECK_FALSE(choose_camera_format({}));
}

TEST_CASE("USB camera: at most 15 frames a second, the late ones skipped, 10 s without frames warned (rule 3)",
          "[device][camera]") {
    FramePacer pacer;
    CHECK_FALSE(pacer.due(0.0)); // nothing yet
    CHECK_FALSE(pacer.stalled(5.0));
    // 30 frames a second for one second, drawn at 60 Hz: 15 shown, the rest skipped.
    std::size_t shown = 0, frame = 0;
    for (int tick = 0; tick < 60; ++tick) {
        const double now = tick / 60.0;
        if (tick % 2 == 0)
            pacer.arrived(frame++, now);
        if (const auto f = pacer.due(now)) {
            ++shown;
            CHECK(*f == frame - 1); // always the newest
        }
    }
    CHECK(shown == 15);
    CHECK(pacer.skipped() + shown + 1 == frame); // the last one still waits
    const auto waiting = pacer.due(1.0); // the one that waited, then nothing: each frame once
    REQUIRE(waiting);
    CHECK(*waiting == frame - 1);
    CHECK_FALSE(pacer.due(1.2));
    CHECK_FALSE(pacer.stalled(1.0));
    CHECK(pacer.stalled(11.0)); // 10 s without a frame
    pacer.arrived(frame++, 11.5);
    CHECK_FALSE(pacer.stalled(11.6));
    pacer.disconnected();
    CHECK(pacer.stalled(11.7));
    // Never a frame: warned 10 s after it was first asked for one.
    FramePacer none;
    CHECK_FALSE(none.due(100.0));
    CHECK_FALSE(none.stalled(105.0));
    CHECK(none.stalled(110.0));
}

TEST_CASE("USB camera: listing the connected cameras never fails, and each one has formats (rule 1)", "[device][camera]") {
    // No camera on the test machines: an empty list, no error. With one, each has a name and its formats.
    for (const UsbCamera& cam : list_usb_cameras()) {
        CHECK_FALSE(cam.id.empty());
        CHECK_FALSE(cam.formats.empty());
        CHECK(choose_camera_format(cam.formats));
    }
}


#include "device/camera_frame.hpp"
#include "gcode/image_write.hpp"
#include "ui/image_decode.hpp"

#include <string>

namespace {

// A JPEG's segments without its Huffman tables, as many UVC cameras send them.
std::string without_dht(const std::string& jpeg) {
    std::string out = jpeg.substr(0, 2);
    std::size_t at = 2;
    while (at + 4 <= jpeg.size()) {
        const unsigned char marker = static_cast<unsigned char>(jpeg[at + 1]);
        if (marker == 0xDA) {
            out += jpeg.substr(at);
            return out;
        }
        const std::size_t length = (std::size_t(static_cast<unsigned char>(jpeg[at + 2])) << 8) |
                                   static_cast<unsigned char>(jpeg[at + 3]);
        if (marker != 0xC4)
            out += jpeg.substr(at, 2 + length);
        at += 2 + length;
    }
    return out;
}

} // namespace

TEST_CASE("USB camera: an MJPEG frame without Huffman tables gets the standard ones (T.81 Annex K.3)",
          "[device][camera]") {
    // A 16 x 16 gradient written as a baseline JPEG with the standard tables.
    std::vector<std::uint8_t> rgba(16 * 16 * 4);
    for (int i = 0; i < 16 * 16; ++i) {
        rgba[std::size_t(i) * 4 + 0] = std::uint8_t(i % 16 * 16);
        rgba[std::size_t(i) * 4 + 1] = std::uint8_t(i / 16 * 16);
        rgba[std::size_t(i) * 4 + 2] = 128;
        rgba[std::size_t(i) * 4 + 3] = 255;
    }
    const std::vector<std::uint8_t> jpg = tisma::encode_jpg(16, 16, rgba.data(), 90);
    const std::string jpeg(jpg.begin(), jpg.end());
    const tisma::ui::Image original = tisma::ui::decode_image(jpeg);
    REQUIRE(original.width == 16);
    const std::string bare = without_dht(jpeg);
    REQUIRE(bare.size() < jpeg.size());
    CHECK(tisma::ui::decode_image(bare).rgba != original.rgba); // without the tables the picture comes out wrong
    const std::string fixed = mjpeg_with_huffman_tables(bare);
    const tisma::ui::Image back = tisma::ui::decode_image(fixed);
    REQUIRE(back.width == 16);
    CHECK(back.rgba == original.rgba);
    // With its tables, or not a JPEG: unchanged.
    CHECK(mjpeg_with_huffman_tables(jpeg) == jpeg);
    CHECK(mjpeg_with_huffman_tables("not a jpeg") == "not a jpeg");
}

TEST_CASE("USB camera: YUYV to RGBA by BT.601", "[device][camera]") {
    // Two pixels: Y 235 and Y 16, with neutral colour: white and black.
    const std::string grey{'\xEB', '\x80', '\x10', '\x80'};
    const std::vector<std::uint8_t> rgba = yuyv_to_rgba(grey, 2, 1);
    REQUIRE(rgba.size() == 8);
    CHECK(rgba == std::vector<std::uint8_t>{255, 255, 255, 255, 0, 0, 0, 255});
    // Pure red in BT.601 studio range: Y 81, Cb 90, Cr 240.
    const std::vector<std::uint8_t> red = yuyv_to_rgba(std::string{'\x51', '\x5A', '\x51', '\xF0'}, 2, 1);
    REQUIRE(red.size() == 8);
    CHECK(red[0] >= 250);
    CHECK(red[1] <= 5);
    CHECK(red[2] <= 5);
    CHECK(yuyv_to_rgba(grey, 4, 1).empty()); // size mismatch
}
