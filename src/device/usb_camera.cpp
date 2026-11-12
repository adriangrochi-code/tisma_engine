// The USB camera's format choice and frame pacing (printer_connections.md,
// «Cámara USB de la computadora», rules 2 and 3). Own design; see
// PROVENANCE.md.

#include "device/usb_camera.hpp"

#include <cstdint>

#include <cstdio>

#if defined(__linux__)
#include <fcntl.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <filesystem>
#include <poll.h>
#include <sys/mman.h>
#endif

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

#if defined(__linux__)
namespace {

int query(int fd, unsigned long request, void* arg) {
    int r;
    do
        r = ioctl(fd, request, arg);
    while (r == -1 && errno == EINTR);
    return r;
}

// The frame rates of one size, as frames per second.
std::vector<double> rates(int fd, std::uint32_t format, std::uint32_t width, std::uint32_t height) {
    std::vector<double> out;
    v4l2_frmivalenum iv{};
    iv.pixel_format = format;
    iv.width = width;
    iv.height = height;
    for (iv.index = 0; query(fd, VIDIOC_ENUM_FRAMEINTERVALS, &iv) == 0; ++iv.index) {
        if (iv.type == V4L2_FRMIVAL_TYPE_DISCRETE) {
            if (iv.discrete.numerator > 0)
                out.push_back(double(iv.discrete.denominator) / double(iv.discrete.numerator));
        } else { // stepwise or continuous: its fastest
            if (iv.stepwise.min.numerator > 0)
                out.push_back(double(iv.stepwise.min.denominator) / double(iv.stepwise.min.numerator));
            break;
        }
    }
    return out;
}

} // namespace

std::vector<UsbCamera> list_usb_cameras() {
    std::vector<UsbCamera> out;
    std::error_code ec;
    std::vector<std::filesystem::path> nodes;
    for (const auto& entry : std::filesystem::directory_iterator("/dev", ec))
        if (entry.path().filename().string().rfind("video", 0) == 0)
            nodes.push_back(entry.path());
    std::sort(nodes.begin(), nodes.end());
    for (const std::filesystem::path& node : nodes) {
        const int fd = open(node.c_str(), O_RDWR | O_NONBLOCK);
        if (fd < 0)
            continue;
        v4l2_capability cap{};
        const bool capture = query(fd, VIDIOC_QUERYCAP, &cap) == 0 &&
                             ((cap.capabilities & V4L2_CAP_DEVICE_CAPS) ? cap.device_caps : cap.capabilities) &
                                 V4L2_CAP_VIDEO_CAPTURE;
        if (capture) {
            UsbCamera cam;
            cam.id = node.string();
            cam.name = reinterpret_cast<const char*>(cap.card);
            v4l2_fmtdesc fmt{};
            fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            for (fmt.index = 0; query(fd, VIDIOC_ENUM_FMT, &fmt) == 0; ++fmt.index) {
                if (fmt.pixelformat != V4L2_PIX_FMT_MJPEG && fmt.pixelformat != V4L2_PIX_FMT_YUYV)
                    continue;
                v4l2_frmsizeenum size{};
                size.pixel_format = fmt.pixelformat;
                for (size.index = 0; query(fd, VIDIOC_ENUM_FRAMESIZES, &size) == 0; ++size.index) {
                    if (size.type != V4L2_FRMSIZE_TYPE_DISCRETE)
                        break; // UVC cameras list discrete sizes
                    for (const double fps : rates(fd, fmt.pixelformat, size.discrete.width, size.discrete.height))
                        cam.formats.push_back({int(size.discrete.width), int(size.discrete.height), fps,
                                               fmt.pixelformat == V4L2_PIX_FMT_MJPEG});
                }
            }
            // A metadata node of the same camera has no formats: it is not a camera.
            if (!cam.formats.empty())
                out.push_back(std::move(cam));
        }
        close(fd);
    }
    return out;
}

struct UsbCapture::Impl {
    int fd = -1;
    struct Buffer {
        void* data = MAP_FAILED;
        std::size_t length = 0;
    };
    std::vector<Buffer> buffers;
    CameraFormat format;
    bool gone = false;
};

bool UsbCapture::open(const std::string& id, const CameraFormat& format, std::string& error) {
    close();
    Impl& d = *impl_;
    d.fd = ::open(id.c_str(), O_RDWR | O_NONBLOCK);
    if (d.fd < 0) {
        error = "cannot open the camera " + id + ": " + std::strerror(errno);
        return false;
    }
    const auto fail = [&](const char* what) {
        error = std::string("the camera ") + id + " " + what + ": " + std::strerror(errno);
        close();
        return false;
    };
    v4l2_format f{};
    f.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    f.fmt.pix.width = std::uint32_t(format.width);
    f.fmt.pix.height = std::uint32_t(format.height);
    f.fmt.pix.pixelformat = format.mjpeg ? V4L2_PIX_FMT_MJPEG : V4L2_PIX_FMT_YUYV;
    f.fmt.pix.field = V4L2_FIELD_NONE;
    if (query(d.fd, VIDIOC_S_FMT, &f) != 0)
        return fail("refused the format");
    d.format = {int(f.fmt.pix.width), int(f.fmt.pix.height), format.fps,
                f.fmt.pix.pixelformat == V4L2_PIX_FMT_MJPEG};
    if (format.fps > 0.0) { // the rate is a request: the camera may keep its own
        v4l2_streamparm parm{};
        parm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        parm.parm.capture.timeperframe.numerator = 1000;
        parm.parm.capture.timeperframe.denominator = std::uint32_t(format.fps * 1000.0 + 0.5);
        if (query(d.fd, VIDIOC_S_PARM, &parm) == 0 && parm.parm.capture.timeperframe.numerator > 0)
            d.format.fps = double(parm.parm.capture.timeperframe.denominator) /
                           double(parm.parm.capture.timeperframe.numerator);
    }
    v4l2_requestbuffers req{};
    req.count = 4;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    if (query(d.fd, VIDIOC_REQBUFS, &req) != 0 || req.count < 2)
        return fail("gave no buffers");
    for (std::uint32_t i = 0; i < req.count; ++i) {
        v4l2_buffer b{};
        b.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        b.memory = V4L2_MEMORY_MMAP;
        b.index = i;
        if (query(d.fd, VIDIOC_QUERYBUF, &b) != 0)
            return fail("did not describe its buffers");
        Impl::Buffer buf;
        buf.length = b.length;
        buf.data = mmap(nullptr, b.length, PROT_READ | PROT_WRITE, MAP_SHARED, d.fd, b.m.offset);
        if (buf.data == MAP_FAILED)
            return fail("could not share its buffers");
        d.buffers.push_back(buf);
        if (query(d.fd, VIDIOC_QBUF, &b) != 0)
            return fail("did not take its buffers");
    }
    v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (query(d.fd, VIDIOC_STREAMON, &type) != 0)
        return fail("did not start");
    return true;
}

bool UsbCapture::read(std::string& frame, double timeout_s) {
    Impl& d = *impl_;
    if (d.fd < 0 || d.gone)
        return false;
    pollfd p{d.fd, POLLIN, 0};
    const int waited = ::poll(&p, 1, int(timeout_s * 1000.0));
    if (waited <= 0)
        return false;
    if (p.revents & (POLLERR | POLLHUP)) {
        d.gone = true;
        return false;
    }
    // Every frame ready now; keep the newest, give the others back.
    bool got = false;
    for (;;) {
        v4l2_buffer b{};
        b.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        b.memory = V4L2_MEMORY_MMAP;
        if (query(d.fd, VIDIOC_DQBUF, &b) != 0) {
            if (errno == ENODEV || errno == EIO)
                d.gone = true;
            break;
        }
        if (b.index < d.buffers.size() && !(b.flags & V4L2_BUF_FLAG_ERROR)) {
            frame.assign(static_cast<const char*>(d.buffers[b.index].data),
                         std::min<std::size_t>(b.bytesused, d.buffers[b.index].length));
            got = true;
        }
        query(d.fd, VIDIOC_QBUF, &b);
    }
    return got;
}

void UsbCapture::close() {
    Impl& d = *impl_;
    if (d.fd >= 0) {
        v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        query(d.fd, VIDIOC_STREAMOFF, &type);
    }
    for (const Impl::Buffer& b : d.buffers)
        if (b.data != MAP_FAILED)
            munmap(b.data, b.length);
    d.buffers.clear();
    if (d.fd >= 0)
        ::close(d.fd);
    d.fd = -1;
    d.gone = false;
}
#else
std::vector<UsbCamera> list_usb_cameras() { return {}; }

struct UsbCapture::Impl {
    CameraFormat format;
};

bool UsbCapture::open(const std::string&, const CameraFormat&, std::string& error) {
    error = "USB cameras are not supported on this system yet";
    return false;
}
bool UsbCapture::read(std::string&, double) { return false; }
void UsbCapture::close() {}
#endif

UsbCapture::UsbCapture() : impl_(std::make_unique<Impl>()) {}
UsbCapture::~UsbCapture() { close(); }
const CameraFormat& UsbCapture::format() const { return impl_->format; }
#if defined(__linux__)
bool UsbCapture::disconnected() const { return impl_->gone; }
#else
bool UsbCapture::disconnected() const { return false; }
#endif

} // namespace tisma::device
