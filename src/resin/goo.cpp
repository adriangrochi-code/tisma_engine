#include "resin/goo.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <ctime>
#include <iterator>

namespace tisma {
namespace {

constexpr std::array<std::uint8_t, 8> kMagic = {0x07, 0x00, 0x00, 0x00, 0x44, 0x4C, 0x50, 0x00};
constexpr std::array<std::uint8_t, 11> kEnding = {0x00, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00, 0x44, 0x4C, 0x50, 0x00};
constexpr int kSmall = 116, kBig = 290;

class BigEndian {
public:
    explicit BigEndian(std::ostream& out) : out_(out) {}
    void u8(unsigned v) { out_.put(char(std::uint8_t(v))); }
    void u16(unsigned v) {
        u8(v >> 8);
        u8(v);
    }
    void i32(std::int64_t v) {
        const auto u = std::uint32_t(v);
        u16(u >> 16);
        u16(u & 0xffff);
    }
    void f32(double v) { i32(std::bit_cast<std::uint32_t>(float(v))); }
    void flag(bool v) { u8(v ? 1 : 0); }
    // A text field of n bytes, filled with zeros.
    void text(std::string_view s, std::size_t n) {
        for (std::size_t i = 0; i < n; ++i)
            u8(i < s.size() ? std::uint8_t(s[i]) : 0);
    }
    void bytes(const std::uint8_t* p, std::size_t n) { out_.write(reinterpret_cast<const char*>(p), std::streamsize(n)); }
    void line_end() {
        u8(0x0d);
        u8(0x0a);
    }

private:
    std::ostream& out_;
};

class Reader {
public:
    explicit Reader(const std::vector<std::uint8_t>& data) : data_(data) {}
    std::size_t pos = 0;
    const std::uint8_t* take(std::size_t n, const char* what) {
        if (n > data_.size() - pos)
            throw ResinError(std::string("GOO file: cut short in ") + what);
        const std::uint8_t* p = data_.data() + pos;
        pos += n;
        return p;
    }
    unsigned u8(const char* what) { return *take(1, what); }
    unsigned u16(const char* what) {
        const std::uint8_t* p = take(2, what);
        return unsigned(p[0]) << 8 | p[1];
    }
    std::uint32_t u32(const char* what) {
        const std::uint8_t* p = take(4, what);
        return std::uint32_t(p[0]) << 24 | std::uint32_t(p[1]) << 16 | std::uint32_t(p[2]) << 8 | p[3];
    }
    int i16(const char* what) { return int(std::int16_t(u16(what))); }
    int i32(const char* what) { return int(std::int32_t(u32(what))); }
    float f32(const char* what) { return std::bit_cast<float>(u32(what)); }
    bool flag(const char* what) { return u8(what) != 0; }
    std::string text(std::size_t n, const char* what) {
        const std::uint8_t* p = take(n, what);
        std::string s(reinterpret_cast<const char*>(p), n);
        s.resize(s.find('\0') == std::string::npos ? n : s.find('\0'));
        return s;
    }
    void line_end(const char* what) {
        const std::uint8_t* p = take(2, what);
        if (p[0] != 0x0d || p[1] != 0x0a)
            throw ResinError(std::string("GOO file: no 0D 0A after ") + what);
    }

private:
    const std::vector<std::uint8_t>& data_;
};

void preview(BigEndian& out, const std::vector<std::uint16_t>& image, int side) {
    for (std::size_t i = 0; i < std::size_t(side) * std::size_t(side); ++i)
        out.u16(i < image.size() ? image[i] : 0);
}

} // namespace

void write_goo(std::ostream& stream, const ResinPrint& print, const Settings& s, std::string_view created) {
    BigEndian out(stream);
    const double max_height = print.max_height;
    // What the header gives a layer of each kind (rule 7); a layer that
    // differs (the exposure tower) needs the advanced mode (rule 12).
    auto plain = [&](const ResinLayer& l) {
        const bool b = l.bottom;
        return float(l.exposure) == float(b ? s.resin_bottom_exposure : s.resin_exposure) &&
               float(l.lift_distance) == float(b ? s.resin_bottom_lift_distance : s.resin_lift_distance) &&
               float(l.lift_speed) == float(b ? s.resin_bottom_lift_speed : s.resin_lift_speed) &&
               float(l.retract_distance) == float(l.lift_distance) &&
               float(l.retract_speed) == float(s.resin_retract_speed) &&
               float(l.wait_after_cure) == float(s.resin_wait_after_cure) &&
               float(l.wait_after_lift) == float(s.resin_wait_after_lift) &&
               float(l.wait_before_exposure) == float(s.resin_wait_before_exposure) &&
               l.pwm == (b ? s.resin_bottom_light_pwm : s.resin_light_pwm);
    };
    bool advanced = false;
    for (const ResinLayer& l : print.layers)
        advanced = advanced || !plain(l);

    out.text("V3.0", 4);
    out.bytes(kMagic.data(), kMagic.size());
    out.text("Tisma engine", 32);
    out.text(TISMA_VERSION, 24);
    out.text(created, 24);
    out.text(s.resin_printer_name, 32);
    out.text("MSLA", 32);
    out.text(s.resin_profile_name, 32);
    out.u16(unsigned(s.resin_antialiasing));
    out.u16(1); // grey level
    out.u16(0); // blur level
    preview(out, print.small_preview, kSmall);
    out.line_end();
    preview(out, print.big_preview, kBig);
    out.line_end();
    out.i32(std::int64_t(print.layers.size()));
    out.u16(unsigned(s.display_pixels_x));
    out.u16(unsigned(s.display_pixels_y));
    out.flag(s.display_mirror_x);
    out.flag(s.display_mirror_y);
    out.f32(s.display_width);
    out.f32(s.display_height);
    out.f32(max_height);
    out.f32(s.resin_layer_height);
    out.f32(s.resin_exposure);
    out.flag(true); // the waits as given
    out.f32(0.0);   // turn-off time
    for (int kind = 0; kind < 2; ++kind) { // bottom layers, then the rest
        out.f32(s.resin_wait_after_cure);
        out.f32(s.resin_wait_after_lift);
        out.f32(s.resin_wait_before_exposure);
    }
    out.f32(s.resin_bottom_exposure);
    out.i32(print.bottom_layers);
    out.f32(s.resin_bottom_lift_distance);
    out.f32(s.resin_bottom_lift_speed * 60.0);
    out.f32(s.resin_lift_distance);
    out.f32(s.resin_lift_speed * 60.0);
    out.f32(s.resin_bottom_lift_distance);
    out.f32(s.resin_retract_speed * 60.0);
    out.f32(s.resin_lift_distance);
    out.f32(s.resin_retract_speed * 60.0);
    for (int i = 0; i < 8; ++i)
        out.f32(0.0); // no second stage
    out.u16(unsigned(s.resin_bottom_light_pwm));
    out.u16(unsigned(s.resin_light_pwm));
    out.flag(advanced);
    out.i32(std::llround(print.seconds));
    out.f32(print.volume);
    out.f32(print.weight);
    out.f32(0.0); // price
    out.text("$", 8);
    out.i32(std::int64_t(kGooHeaderSize));
    out.flag(true); // greys from 0 to 255
    out.u16(0);     // transition layers

    for (const ResinLayer& l : print.layers) {
        out.u16(0); // no pause
        out.f32(max_height);
        out.f32(l.z);
        out.f32(l.exposure);
        out.f32(0.0); // turn-off time
        out.f32(l.wait_after_cure);
        out.f32(l.wait_after_lift);
        out.f32(l.wait_before_exposure);
        out.f32(l.lift_distance);
        out.f32(l.lift_speed * 60.0);
        out.f32(0.0);
        out.f32(0.0);
        out.f32(l.retract_distance);
        out.f32(l.retract_speed * 60.0);
        out.f32(0.0);
        out.f32(0.0);
        out.u16(unsigned(l.pwm));
        out.line_end();
        out.i32(std::int64_t(l.image.size()));
        out.bytes(l.image.data(), l.image.size());
        out.line_end();
    }
    out.bytes(kEnding.data(), kEnding.size());
}

GooFile read_goo(std::istream& in) {
    const std::vector<std::uint8_t> data{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
    Reader r(data);
    GooFile f;
    GooHeader& h = f.header;
    h.version = r.text(4, "the version");
    const std::uint8_t* magic = r.take(kMagic.size(), "the magic tag");
    if (!std::equal(kMagic.begin(), kMagic.end(), magic))
        throw ResinError("GOO file: wrong magic tag");
    h.software = r.text(32, "the software");
    h.software_version = r.text(24, "the software version");
    h.created = r.text(24, "the file time");
    h.printer_name = r.text(32, "the printer name");
    h.printer_type = r.text(32, "the printer type");
    h.profile_name = r.text(32, "the profile name");
    h.antialiasing = r.i16("the anti-aliasing level");
    h.grey_level = r.i16("the grey level");
    h.blur_level = r.i16("the blur level");
    auto image = [&](int side, const char* what) {
        std::vector<std::uint16_t> px(std::size_t(side) * std::size_t(side));
        for (std::uint16_t& p : px)
            p = std::uint16_t(r.u16(what));
        r.line_end(what);
        return px;
    };
    h.small_preview = image(kSmall, "the small preview");
    h.big_preview = image(kBig, "the big preview");
    h.layers = r.i32("the layer count");
    h.x_resolution = r.i16("the x resolution");
    h.y_resolution = r.i16("the y resolution");
    h.mirror_x = r.flag("the x mirror");
    h.mirror_y = r.flag("the y mirror");
    h.size_x = r.f32("the platform's x size");
    h.size_y = r.f32("the platform's y size");
    h.size_z = r.f32("the platform's z size");
    h.layer_height = r.f32("the layer thickness");
    h.exposure = r.f32("the exposure");
    h.fixed_waits = r.flag("the exposure delay mode");
    h.off_time = r.f32("the turn-off time");
    for (float* v : {&h.bottom_before_lift, &h.bottom_after_lift, &h.bottom_after_retract, &h.before_lift, &h.after_lift,
                     &h.after_retract})
        *v = r.f32("the waits");
    h.bottom_exposure = r.f32("the bottom exposure");
    h.bottom_layers = r.i32("the bottom layers");
    for (float* v : {&h.bottom_lift_distance, &h.bottom_lift_speed, &h.lift_distance, &h.lift_speed,
                     &h.bottom_retract_distance, &h.bottom_retract_speed, &h.retract_distance, &h.retract_speed,
                     &h.bottom_second_lift_distance, &h.bottom_second_lift_speed, &h.second_lift_distance,
                     &h.second_lift_speed, &h.bottom_second_retract_distance, &h.bottom_second_retract_speed,
                     &h.second_retract_distance, &h.second_retract_speed})
        *v = r.f32("the lifts and retracts");
    h.bottom_pwm = r.i16("the bottom light PWM");
    h.pwm = r.i16("the light PWM");
    h.advanced = r.flag("the advance mode");
    h.print_seconds = r.i32("the printing time");
    h.volume = r.f32("the volume");
    h.weight = r.f32("the weight");
    h.price = r.f32("the price");
    h.price_unit = r.text(8, "the price unit");
    h.layer_offset = r.i32("the layer offset");
    h.greyscale = r.flag("the grey scale level");
    h.transition_layers = r.i16("the transition layers");
    if (r.pos != kGooHeaderSize)
        throw ResinError("GOO file: the header is not " + std::to_string(kGooHeaderSize) + " bytes");
    if (h.layers < 0 || h.layer_offset < 0 || std::size_t(h.layer_offset) > data.size())
        throw ResinError("GOO file: wrong layer count or offset");
    r.pos = std::size_t(h.layer_offset);
    for (int i = 0; i < h.layers; ++i) {
        GooLayer l;
        l.pause = r.i16("a layer's pause flag");
        for (float* v : {&l.pause_z, &l.z, &l.exposure, &l.off_time, &l.before_lift, &l.after_lift, &l.after_retract,
                         &l.lift_distance, &l.lift_speed, &l.second_lift_distance, &l.second_lift_speed,
                         &l.retract_distance, &l.retract_speed, &l.second_retract_distance, &l.second_retract_speed})
            *v = r.f32("a layer's definition");
        l.pwm = r.i16("a layer's light PWM");
        r.line_end("a layer's definition");
        const std::uint32_t size = r.u32("a layer's data size");
        const std::uint8_t* p = r.take(size, "a layer's image");
        l.image.assign(p, p + size);
        r.line_end("a layer's image");
        f.layers.push_back(std::move(l));
    }
    const std::uint8_t* end = r.take(kEnding.size(), "the ending");
    if (!std::equal(kEnding.begin(), kEnding.end(), end) || r.pos != data.size())
        throw ResinError("GOO file: wrong ending");
    return f;
}

std::string goo_time_now() {
    const std::time_t t = std::time(nullptr);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    char text[32];
    std::strftime(text, sizeof text, "%Y-%m-%d %H:%M:%S", &tm);
    return text;
}

} // namespace tisma
