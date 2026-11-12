// The USB camera's format choice and frame pacing (printer_connections.md,
// «Cámara USB de la computadora», rules 2 and 3). Own design; see
// PROVENANCE.md.

#include "device/usb_camera.hpp"

#include <cstdio>

namespace tisma::device {

std::optional<CameraFormat> choose_camera_format(const std::vector<CameraFormat>& formats) {
    const auto area = [](const CameraFormat& f) { return static_cast<long long>(f.width) * f.height; };
    const auto fits = [](const CameraFormat& f) { return f.width <= 1280 && f.height <= 720 && f.fps >= 10.0; };
    std::optional<CameraFormat> best;
    for (const CameraFormat& f : formats) {
        if (!fits(f))
            continue;
        if (!best || area(f) > area(*best) || (area(f) == area(*best) && f.mjpeg && !best->mjpeg) ||
            (area(f) == area(*best) && f.mjpeg == best->mjpeg && f.fps > best->fps))
            best = f;
    }
    if (best)
        return best;
    for (const CameraFormat& f : formats)
        if (!best || f.fps > best->fps || (f.fps == best->fps && area(f) < area(*best)) ||
            (f.fps == best->fps && area(f) == area(*best) && f.mjpeg && !best->mjpeg))
            best = f;
    return best;
}

std::string camera_format_text(const CameraFormat& format) {
    char text[64];
    std::snprintf(text, sizeof text, "%d \xC3\x97 %d %s, %.0f fps", format.width, format.height,
                  format.mjpeg ? "MJPEG" : "YUV", format.fps);
    return text;
}

void FramePacer::arrived(std::size_t frame, double now) {
    if (newest_)
        ++skipped_; // the one waiting is never drawn: the newer replaces it
    newest_ = frame;
    last_arrival_ = now;
    disconnected_ = false;
}

std::optional<std::size_t> FramePacer::due(double now) {
    if (started_ < 0.0)
        started_ = now;
    // 1 ms of slack: a display's ticks jitter around 1/15 s.
    if (!newest_ || now - last_shown_ < 1.0 / kMaxFps - 1e-3)
        return std::nullopt;
    last_shown_ = now;
    const std::size_t frame = *newest_;
    newest_.reset();
    return frame;
}

bool FramePacer::stalled(double now) const {
    if (disconnected_)
        return true;
    const double since = last_arrival_ >= 0.0 ? last_arrival_ : started_;
    return since >= 0.0 && now - since >= kStaleSeconds;
}

} // namespace tisma::device
