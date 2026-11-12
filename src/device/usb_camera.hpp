#pragma once

// A USB (UVC) camera of the computer as a printer's camera
// (printer_connections.md, «Cámara USB de la computadora»): the choice of
// format (rule 2) and the pacing of the frames on screen (rule 3). The capture
// itself comes from the system (DirectShow, AVFoundation, V4L2) through a
// library recorded in THIRD_PARTY.md; this part needs no camera and is the
// same on every system. Own design; see PROVENANCE.md.

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace tisma::device {

// What a camera can give.
struct CameraFormat {
    int width = 0, height = 0;
    double fps = 0.0;
    bool mjpeg = false; // else uncompressed
};

// Rule 2: the largest up to 1280 × 720 that gives at least 10 frames per
// second, MJPEG before uncompressed at the same size (less USB bandwidth, so a
// second camera fits on the same port). When none reaches that, the fastest,
// then the smallest. None for an empty list.
std::optional<CameraFormat> choose_camera_format(const std::vector<CameraFormat>& formats);

// «1280 × 720 MJPEG, 30 fps» for the line beside the choice (rule 1).
std::string camera_format_text(const CameraFormat& format);

// Rule 3: at most 15 frames per second on screen; frames the interface does
// not draw in time are skipped, not queued (only the newest is kept); 10 s
// without a frame, or a disconnection, is a warning in the view. Times are in
// seconds from any steady clock.
class FramePacer {
public:
    static constexpr double kMaxFps = 15.0;
    static constexpr double kStaleSeconds = 10.0;

    // A frame arrived from the camera (its number in arrival order).
    void arrived(std::size_t frame, double now);
    // The frame to draw now, if one is due: the newest that arrived, at most
    // kMaxFps a second. Each frame is handed at most once.
    std::optional<std::size_t> due(double now);
    void disconnected() { disconnected_ = true; }
    // The warning for the view: disconnected, or no frame for kStaleSeconds.
    bool stalled(double now) const;
    // Frames that arrived and were never handed (skipped, not queued).
    std::size_t skipped() const { return skipped_; }

private:
    std::optional<std::size_t> newest_;
    double last_arrival_ = -1.0, last_shown_ = -1e9, started_ = -1.0;
    std::size_t skipped_ = 0;
    bool disconnected_ = false;
};

// A camera the computer sees, with every format it offers.
struct UsbCamera {
    std::string id;   // what opens it again: on Linux its /dev/videoN
    std::string name; // what the person sees: «HD Webcam C270»
    std::vector<CameraFormat> formats;
};

// The cameras connected now (rule 1). On Linux through V4L2 (the kernel's
// Video for Linux 2 API: VIDIOC_QUERYCAP, VIDIOC_ENUM_FMT,
// VIDIOC_ENUM_FRAMESIZES, VIDIOC_ENUM_FRAMEINTERVALS), only MJPEG and YUYV,
// the two the engine can show; elsewhere empty until its capture is built.
std::vector<UsbCamera> list_usb_cameras();

} // namespace tisma::device
