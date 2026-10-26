// PNG writer with stored (uncompressed) zlib blocks. Own design from the PNG
// specification and RFC 1950/1951; see PROVENANCE.md.

#include "ui/png.hpp"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <string>

namespace tisma::ui {
namespace {

std::array<std::uint32_t, 256> crc_table() {
    std::array<std::uint32_t, 256> table{};
    for (std::uint32_t n = 0; n < 256; ++n) {
        std::uint32_t c = n;
        for (int k = 0; k < 8; ++k)
            c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
        table[n] = c;
    }
    return table;
}

void put32(std::vector<std::uint8_t>& out, std::uint32_t v) {
    for (int shift = 24; shift >= 0; shift -= 8)
        out.push_back(static_cast<std::uint8_t>(v >> shift));
}

// A chunk: length, type, data and the CRC of type and data.
void chunk(std::vector<std::uint8_t>& out, const char* type, const std::vector<std::uint8_t>& data) {
    put32(out, static_cast<std::uint32_t>(data.size()));
    const std::size_t start = out.size();
    out.insert(out.end(), type, type + 4);
    out.insert(out.end(), data.begin(), data.end());
    put32(out, crc32(out.data() + start, out.size() - start));
}

} // namespace

std::uint32_t crc32(const std::uint8_t* data, std::size_t size, std::uint32_t crc) {
    static const std::array<std::uint32_t, 256> table = crc_table();
    crc = ~crc;
    for (std::size_t i = 0; i < size; ++i)
        crc = table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    return ~crc;
}

std::uint32_t adler32(const std::uint8_t* data, std::size_t size, std::uint32_t adler) {
    constexpr std::uint32_t kMod = 65521;
    std::uint32_t a = adler & 0xFFFF, b = adler >> 16;
    for (std::size_t i = 0; i < size; ++i) {
        a = (a + data[i]) % kMod;
        b = (b + a) % kMod;
    }
    return (b << 16) | a;
}

std::vector<std::uint8_t> encode_png(int width, int height, const std::uint8_t* rgba) {
    const auto w = static_cast<std::size_t>(width), h = static_cast<std::size_t>(height);
    // Scanlines, each led by filter type 0 (none).
    std::vector<std::uint8_t> raw;
    raw.reserve(h * (1 + 4 * w));
    for (std::size_t y = 0; y < h; ++y) {
        raw.push_back(0);
        raw.insert(raw.end(), rgba + y * 4 * w, rgba + (y + 1) * 4 * w);
    }

    // zlib: header (deflate, 32K window, no preset dictionary; 0x7801 is a
    // multiple of 31), stored blocks of at most 65535 bytes, Adler-32.
    std::vector<std::uint8_t> z = {0x78, 0x01};
    constexpr std::size_t kBlock = 65535;
    std::size_t at = 0;
    do {
        const std::size_t n = std::min(kBlock, raw.size() - at);
        const bool last = at + n == raw.size();
        z.push_back(last ? 1 : 0); // BFINAL, BTYPE = 00 (stored)
        const auto len = static_cast<std::uint16_t>(n);
        const auto nlen = static_cast<std::uint16_t>(~len);
        z.push_back(static_cast<std::uint8_t>(len & 0xFF));
        z.push_back(static_cast<std::uint8_t>(len >> 8));
        z.push_back(static_cast<std::uint8_t>(nlen & 0xFF));
        z.push_back(static_cast<std::uint8_t>(nlen >> 8));
        z.insert(z.end(), raw.begin() + static_cast<std::ptrdiff_t>(at), raw.begin() + static_cast<std::ptrdiff_t>(at + n));
        at += n;
    } while (at < raw.size());
    put32(z, adler32(raw.data(), raw.size()));

    std::vector<std::uint8_t> out = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
    std::vector<std::uint8_t> header;
    put32(header, static_cast<std::uint32_t>(width));
    put32(header, static_cast<std::uint32_t>(height));
    header.insert(header.end(), {8, 6, 0, 0, 0}); // 8 bits, RGBA, deflate, adaptive filters, no interlace
    chunk(out, "IHDR", header);
    chunk(out, "IDAT", z);
    chunk(out, "IEND", {});
    return out;
}

bool write_png(const std::filesystem::path& path, int width, int height, const std::uint8_t* rgba) {
    const std::vector<std::uint8_t> png = encode_png(width, height, rgba);
    std::ofstream file(path, std::ios::binary);
    file.write(reinterpret_cast<const char*>(png.data()), static_cast<std::streamsize>(png.size()));
    return static_cast<bool>(file);
}

namespace {

std::uint32_t get32(const std::vector<std::uint8_t>& in, std::size_t at) {
    return (std::uint32_t(in[at]) << 24) | (std::uint32_t(in[at + 1]) << 16) | (std::uint32_t(in[at + 2]) << 8) |
           std::uint32_t(in[at + 3]);
}

} // namespace

bool decode_png(const std::vector<std::uint8_t>& png, int& width, int& height, std::vector<std::uint8_t>& rgba) {
    static const std::uint8_t kSignature[8] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
    if (png.size() < 8 || !std::equal(kSignature, kSignature + 8, png.begin()))
        return false;
    std::vector<std::uint8_t> z;
    std::uint32_t w = 0, h = 0;
    bool header = false, end = false;
    for (std::size_t at = 8; !end;) {
        if (at + 12 > png.size())
            return false;
        const std::uint32_t n = get32(png, at);
        if (n > png.size() - at - 12)
            return false;
        if (crc32(&png[at + 4], std::size_t(n) + 4) != get32(png, at + 8 + n))
            return false;
        const std::string type(reinterpret_cast<const char*>(&png[at + 4]), 4);
        const std::size_t data = at + 8;
        if (type == "IHDR") {
            // 8 bits, RGBA, deflate, adaptive filters, no interlace.
            if (n != 13 || png[data + 8] != 8 || png[data + 9] != 6 || png[data + 10] != 0 || png[data + 11] != 0 ||
                png[data + 12] != 0)
                return false;
            w = get32(png, data);
            h = get32(png, data + 4);
            header = true;
        } else if (type == "IDAT") {
            z.insert(z.end(), png.begin() + std::ptrdiff_t(data), png.begin() + std::ptrdiff_t(data + n));
        } else if (type == "IEND") {
            end = true;
        }
        at += 12 + std::size_t(n);
    }
    if (!header || w == 0 || h == 0 || w > 1u << 15 || h > 1u << 15 || z.size() < 6 || (z[0] * 256 + z[1]) % 31 != 0 ||
        (z[0] & 0x0F) != 8)
        return false;
    // The stored blocks, one after the other.
    std::vector<std::uint8_t> raw;
    std::size_t at = 2;
    for (bool last = false; !last;) {
        if (at + 5 > z.size() || (z[at] & 0x06) != 0) // BTYPE 00 only
            return false;
        last = z[at] & 1;
        const std::size_t len = std::size_t(z[at + 1]) | (std::size_t(z[at + 2]) << 8);
        const std::size_t nlen = std::size_t(z[at + 3]) | (std::size_t(z[at + 4]) << 8);
        if ((len ^ 0xFFFF) != nlen || at + 5 + len > z.size())
            return false;
        raw.insert(raw.end(), z.begin() + std::ptrdiff_t(at + 5), z.begin() + std::ptrdiff_t(at + 5 + len));
        at += 5 + len;
    }
    if (at + 4 > z.size() || adler32(raw.data(), raw.size()) != get32(z, at))
        return false;
    const std::size_t row = 4 * std::size_t(w);
    if (raw.size() != std::size_t(h) * (row + 1))
        return false;
    rgba.resize(std::size_t(h) * row);
    for (std::size_t y = 0; y < h; ++y) {
        if (raw[y * (row + 1)] != 0) // filter type 0 only
            return false;
        std::copy_n(raw.begin() + std::ptrdiff_t(y * (row + 1) + 1), row, rgba.begin() + std::ptrdiff_t(y * row));
    }
    width = int(w), height = int(h);
    return true;
}

bool read_png(const std::filesystem::path& path, int& width, int& height, std::vector<std::uint8_t>& rgba) {
    std::ifstream file(path, std::ios::binary);
    if (!file)
        return false;
    const std::vector<std::uint8_t> png((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    return decode_png(png, width, height, rgba);
}

PictureDifference compare_pictures(const std::uint8_t* a, const std::uint8_t* b, int width, int height, int x, int y,
                                   int w, int h, int tolerance) {
    PictureDifference d;
    const int x0 = std::clamp(x, 0, width), x1 = std::clamp(x + w, 0, width);
    const int y0 = std::clamp(y, 0, height), y1 = std::clamp(y + h, 0, height);
    for (int yy = y0; yy < y1; ++yy)
        for (int xx = x0; xx < x1; ++xx) {
            const std::size_t i = (std::size_t(yy) * std::size_t(width) + std::size_t(xx)) * 4;
            int most = 0;
            for (std::size_t c = 0; c < 3; ++c)
                most = std::max(most, std::abs(int(a[i + c]) - int(b[i + c])));
            ++d.pixels;
            d.differing += most > 0;
            d.over += most > tolerance;
            d.largest = std::max(d.largest, most);
        }
    return d;
}

} // namespace tisma::ui
