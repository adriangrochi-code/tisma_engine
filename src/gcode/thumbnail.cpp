// G-code thumbnails. Own code; see thumbnail.hpp and PROVENANCE.md.

#include "gcode/thumbnail.hpp"

#include "config/settings.hpp"
#include "io/png.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <limits>
#include <numbers>
#include <stdexcept>

namespace tisma {
namespace {

std::string upper(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return char(std::toupper(c)); });
    return s;
}

int side(const std::string& digits, const std::string& item) {
    if (digits.empty() || digits.size() > 4 || !std::all_of(digits.begin(), digits.end(), [](unsigned char c) {
            return std::isdigit(c) != 0;
        }))
        throw std::invalid_argument("«" + item + "» is not WIDTHxHEIGHT/FORMAT");
    const int v = std::stoi(digits);
    if (v < 1 || v > 1024)
        throw std::invalid_argument("«" + item + "»: each side goes from 1 to 1024 pixels");
    return v;
}

} // namespace

std::vector<ThumbnailSize> parse_thumbnails(const std::string& text) {
    std::vector<ThumbnailSize> out;
    std::string item;
    const auto take = [&] {
        const auto a = item.find_first_not_of(" \t");
        const auto b = item.find_last_not_of(" \t");
        const std::string t = a == std::string::npos ? std::string() : item.substr(a, b - a + 1);
        item.clear();
        if (t.empty())
            return;
        ThumbnailSize s;
        const auto slash = t.find('/');
        const std::string size = t.substr(0, slash);
        if (slash != std::string::npos) {
            const std::string f = upper(t.substr(slash + 1));
            if (f == "PNG")
                s.format = ThumbnailFormat::Png;
            else if (f == "JPG" || f == "JPEG")
                s.format = ThumbnailFormat::Jpg;
            else if (f == "QOI")
                s.format = ThumbnailFormat::Qoi;
            else
                throw std::invalid_argument("«" + t + "»: the format is PNG, JPG or QOI");
        }
        const auto x = size.find_first_of("xX");
        if (x == std::string::npos)
            throw std::invalid_argument("«" + t + "» is not WIDTHxHEIGHT/FORMAT");
        s.width = side(size.substr(0, x), t);
        s.height = side(size.substr(x + 1), t);
        out.push_back(s);
    };
    for (const char c : text) {
        if (c == ',' || c == ';' || c == '\n')
            take();
        else
            item.push_back(c);
    }
    take();
    return out;
}

std::string thumbnails_text(const std::vector<ThumbnailSize>& sizes) {
    std::string out;
    for (const ThumbnailSize& s : sizes) {
        if (!out.empty())
            out += ',';
        out += std::to_string(s.width) + "x" + std::to_string(s.height) + "/" +
               (s.format == ThumbnailFormat::Jpg ? "JPG" : s.format == ThumbnailFormat::Qoi ? "QOI" : "PNG");
    }
    return out;
}

std::vector<std::uint8_t> draw_part(const TriangleMesh& mesh, int width, int height) {
    std::vector<std::uint8_t> out(std::size_t(std::max(0, width)) * std::size_t(std::max(0, height)) * 4, 0);
    if (mesh.empty() || width <= 0 || height <= 0)
        return out;
    // The view: turned 45° about Z (the front left corner nearest), then
    // tilted so that the eye looks down about 35° (isometric).
    const double yaw = -std::numbers::pi / 4.0, pitch = std::atan(1.0 / std::sqrt(2.0));
    const double cy = std::cos(yaw), sy = std::sin(yaw), cp = std::cos(pitch), sp = std::sin(pitch);
    // Screen x to the right, y up, depth towards the eye.
    const auto view = [&](const Vec3f& p) {
        const double x = cy * p.x - sy * p.y, y = sy * p.x + cy * p.y;
        return std::array<double, 3>{x, cp * p.z + sp * y, -sp * p.z + cp * y};
    };
    const int w = 2 * width, h = 2 * height;
    std::vector<std::array<double, 3>> v;
    v.reserve(mesh.corners().size());
    double lo[2] = {1e300, 1e300}, hi[2] = {-1e300, -1e300};
    for (const Vec3f& c : mesh.corners()) {
        v.push_back(view(c));
        for (int a = 0; a < 2; ++a)
            lo[a] = std::min(lo[a], v.back()[a]), hi[a] = std::max(hi[a], v.back()[a]);
    }
    const double margin = 0.06;
    const double scale = std::min(w * (1 - 2 * margin) / std::max(hi[0] - lo[0], 1e-9),
                                  h * (1 - 2 * margin) / std::max(hi[1] - lo[1], 1e-9));
    const double ox = 0.5 * w - 0.5 * (lo[0] + hi[0]) * scale, oy = 0.5 * h + 0.5 * (lo[1] + hi[1]) * scale;
    for (auto& p : v)
        p = {ox + p[0] * scale, oy - p[1] * scale, p[2] * scale}; // pixels, y down; depth in pixels too
    std::vector<double> depth(std::size_t(w) * h, -std::numeric_limits<double>::infinity());
    std::vector<std::array<float, 3>> colour(std::size_t(w) * h);
    // The part's colour, a warm grey, lit from above the eye.
    const double base[3] = {0.80, 0.78, 0.74};
    for (std::size_t t = 0; t + 2 < v.size(); t += 3) {
        const auto& a = v[t];
        const auto& b = v[t + 1];
        const auto& c = v[t + 2];
        // The normal in view space (the eye looks along -depth).
        const double ux = b[0] - a[0], uy = -(b[1] - a[1]), uz = b[2] - a[2];
        const double vx = c[0] - a[0], vy = -(c[1] - a[1]), vz = c[2] - a[2];
        double nx = uy * vz - uz * vy, ny = uz * vx - ux * vz, nz = ux * vy - uy * vx;
        const double len = std::sqrt(nx * nx + ny * ny + nz * nz);
        if (len <= 0)
            continue;
        nx /= len, ny /= len, nz /= len;
        // Both sides lit the same, so a mesh with turned faces still looks whole.
        const double light = 0.35 + 0.65 * std::abs(0.3 * nx + 0.55 * ny + 0.78 * nz);
        const double area = (b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0]);
        if (std::abs(area) < 1e-12)
            continue;
        const int x0 = std::max(0, int(std::floor(std::min({a[0], b[0], c[0]}))));
        const int x1 = std::min(w - 1, int(std::ceil(std::max({a[0], b[0], c[0]}))));
        const int y0 = std::max(0, int(std::floor(std::min({a[1], b[1], c[1]}))));
        const int y1 = std::min(h - 1, int(std::ceil(std::max({a[1], b[1], c[1]}))));
        for (int y = y0; y <= y1; ++y)
            for (int x = x0; x <= x1; ++x) {
                const double px = x + 0.5, py = y + 0.5;
                const double w0 = ((b[0] - px) * (c[1] - py) - (b[1] - py) * (c[0] - px)) / area;
                const double w1 = ((c[0] - px) * (a[1] - py) - (c[1] - py) * (a[0] - px)) / area;
                const double w2 = 1.0 - w0 - w1;
                if (w0 < 0 || w1 < 0 || w2 < 0)
                    continue;
                const double z = w0 * a[2] + w1 * b[2] + w2 * c[2];
                const std::size_t i = std::size_t(y) * w + x;
                if (z <= depth[i])
                    continue;
                depth[i] = z;
                colour[i] = {float(base[0] * light), float(base[1] * light), float(base[2] * light)};
            }
    }
    // Halved: each pixel the mean of four, its alpha the share covered.
    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x) {
            double sum[3] = {0, 0, 0};
            int n = 0;
            for (int dy = 0; dy < 2; ++dy)
                for (int dx = 0; dx < 2; ++dx) {
                    const std::size_t i = std::size_t(2 * y + dy) * w + std::size_t(2 * x + dx);
                    if (depth[i] == -std::numeric_limits<double>::infinity())
                        continue;
                    for (int k = 0; k < 3; ++k)
                        sum[k] += colour[i][k];
                    ++n;
                }
            std::uint8_t* px = &out[(std::size_t(y) * width + x) * 4];
            if (n == 0)
                continue;
            for (int k = 0; k < 3; ++k)
                px[k] = std::uint8_t(std::clamp(std::lround(sum[k] / n * 255.0), 0L, 255L));
            px[3] = std::uint8_t(n * 255 / 4);
        }
    return out;
}

std::vector<GcodeThumbnail> make_thumbnails(const TriangleMesh& mesh, const std::vector<ThumbnailSize>& sizes,
                                            std::vector<ThumbnailSize>* skipped) {
    std::vector<GcodeThumbnail> out;
    for (const ThumbnailSize& s : sizes) {
        if (s.format == ThumbnailFormat::Jpg) {
            if (skipped)
                skipped->push_back(s);
            continue;
        }
        const std::vector<std::uint8_t> rgba = draw_part(mesh, s.width, s.height);
        const std::vector<std::uint8_t> file = s.format == ThumbnailFormat::Qoi
                                                   ? encode_qoi(s.width, s.height, rgba.data())
                                                   : encode_png(s.width, s.height, rgba.data());
        out.push_back({s.format == ThumbnailFormat::Qoi ? 2 : 0, s.width, s.height, std::string(file.begin(), file.end())});
    }
    return out;
}

std::vector<GcodeThumbnail> gcode_thumbnails(const TriangleMesh& mesh, const Settings& settings,
                                             std::vector<Warning>* warnings) {
    if (settings.gcode_thumbnails.empty())
        return {};
    std::vector<ThumbnailSize> sizes;
    try {
        sizes = parse_thumbnails(settings.gcode_thumbnails);
    } catch (const std::invalid_argument&) {
        return {}; // refused when the settings were read
    }
    if (!binary_output(settings)) {
        if (warnings && !sizes.empty())
            warnings->push_back({"thumbnails", -1, 0.0,
                                 "gcode.thumbnails: the pictures go only in a binary G-code for now", 0.0});
        return {};
    }
    std::vector<ThumbnailSize> skipped;
    std::vector<GcodeThumbnail> out = make_thumbnails(mesh, sizes, &skipped);
    if (warnings && !skipped.empty())
        warnings->push_back({"thumbnails", -1, 0.0, "gcode.thumbnails: " + thumbnails_text(skipped) +
                                                        " left out: JPG pictures are not written yet", 0.0});
    return out;
}

namespace {

struct Px {
    std::uint8_t r = 0, g = 0, b = 0, a = 255;
    friend bool operator==(const Px&, const Px&) = default;
};
int qoi_hash(const Px& p) { return (p.r * 3 + p.g * 5 + p.b * 7 + p.a * 11) % 64; }
void put32(std::vector<std::uint8_t>& out, std::uint32_t v) {
    for (int s = 24; s >= 0; s -= 8)
        out.push_back(std::uint8_t(v >> s));
}

} // namespace

std::vector<std::uint8_t> encode_qoi(int width, int height, const std::uint8_t* rgba) {
    std::vector<std::uint8_t> out = {'q', 'o', 'i', 'f'};
    put32(out, std::uint32_t(width));
    put32(out, std::uint32_t(height));
    out.push_back(4); // channels
    out.push_back(0); // sRGB with linear alpha
    std::array<Px, 64> seen{};
    for (Px& p : seen)
        p = Px{0, 0, 0, 0};
    Px prev;
    int run = 0;
    const std::size_t count = std::size_t(std::max(0, width)) * std::size_t(std::max(0, height));
    for (std::size_t i = 0; i < count; ++i) {
        const Px p{rgba[4 * i], rgba[4 * i + 1], rgba[4 * i + 2], rgba[4 * i + 3]};
        if (p == prev) {
            if (++run == 62) {
                out.push_back(std::uint8_t(0xC0 | (run - 1)));
                run = 0;
            }
            continue;
        }
        if (run > 0) {
            out.push_back(std::uint8_t(0xC0 | (run - 1)));
            run = 0;
        }
        const int h = qoi_hash(p);
        if (seen[std::size_t(h)] == p) {
            out.push_back(std::uint8_t(h)); // QOI_OP_INDEX
        } else {
            seen[std::size_t(h)] = p;
            if (p.a == prev.a) {
                const int dr = std::int8_t(p.r - prev.r), dg = std::int8_t(p.g - prev.g), db = std::int8_t(p.b - prev.b);
                const int dr_dg = dr - dg, db_dg = db - dg;
                if (dr >= -2 && dr <= 1 && dg >= -2 && dg <= 1 && db >= -2 && db <= 1) {
                    out.push_back(std::uint8_t(0x40 | ((dr + 2) << 4) | ((dg + 2) << 2) | (db + 2))); // DIFF
                } else if (dg >= -32 && dg <= 31 && dr_dg >= -8 && dr_dg <= 7 && db_dg >= -8 && db_dg <= 7) {
                    out.push_back(std::uint8_t(0x80 | (dg + 32))); // LUMA
                    out.push_back(std::uint8_t(((dr_dg + 8) << 4) | (db_dg + 8)));
                } else {
                    out.insert(out.end(), {0xFE, p.r, p.g, p.b}); // RGB
                }
            } else {
                out.insert(out.end(), {0xFF, p.r, p.g, p.b, p.a}); // RGBA
            }
        }
        prev = p;
    }
    if (run > 0)
        out.push_back(std::uint8_t(0xC0 | (run - 1)));
    out.insert(out.end(), {0, 0, 0, 0, 0, 0, 0, 1}); // the end mark
    return out;
}

bool decode_qoi(const std::vector<std::uint8_t>& file, int& width, int& height, std::vector<std::uint8_t>& rgba) {
    if (file.size() < 14 + 8 || file[0] != 'q' || file[1] != 'o' || file[2] != 'i' || file[3] != 'f')
        return false;
    const auto get32 = [&](std::size_t at) {
        return std::uint32_t(file[at]) << 24 | std::uint32_t(file[at + 1]) << 16 | std::uint32_t(file[at + 2]) << 8 |
               std::uint32_t(file[at + 3]);
    };
    const std::uint32_t w = get32(4), h = get32(8);
    if (w == 0 || h == 0 || w > 16384 || h > 16384 || (file[12] != 3 && file[12] != 4))
        return false;
    width = int(w), height = int(h);
    const std::size_t count = std::size_t(w) * h;
    rgba.assign(count * 4, 0);
    std::array<Px, 64> seen{};
    for (Px& p : seen)
        p = Px{0, 0, 0, 0};
    Px p;
    std::size_t at = 14;
    const std::size_t end = file.size() - 8;
    int run = 0;
    for (std::size_t i = 0; i < count; ++i) {
        if (run > 0) {
            --run;
        } else {
            if (at >= end)
                return false;
            const std::uint8_t op = file[at++];
            if (op == 0xFE) {
                if (at + 3 > end)
                    return false;
                p.r = file[at], p.g = file[at + 1], p.b = file[at + 2];
                at += 3;
            } else if (op == 0xFF) {
                if (at + 4 > end)
                    return false;
                p.r = file[at], p.g = file[at + 1], p.b = file[at + 2], p.a = file[at + 3];
                at += 4;
            } else if ((op & 0xC0) == 0x00) {
                p = seen[op];
            } else if ((op & 0xC0) == 0x40) {
                p.r = std::uint8_t(p.r + ((op >> 4) & 3) - 2);
                p.g = std::uint8_t(p.g + ((op >> 2) & 3) - 2);
                p.b = std::uint8_t(p.b + (op & 3) - 2);
            } else if ((op & 0xC0) == 0x80) {
                if (at >= end)
                    return false;
                const int dg = (op & 0x3F) - 32;
                const std::uint8_t b2 = file[at++];
                p.r = std::uint8_t(p.r + dg + ((b2 >> 4) & 0x0F) - 8);
                p.g = std::uint8_t(p.g + dg);
                p.b = std::uint8_t(p.b + dg + (b2 & 0x0F) - 8);
            } else {
                run = op & 0x3F;
            }
            seen[std::size_t(qoi_hash(p))] = p;
        }
        rgba[4 * i] = p.r, rgba[4 * i + 1] = p.g, rgba[4 * i + 2] = p.b, rgba[4 * i + 3] = p.a;
    }
    return true;
}

} // namespace tisma
