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

