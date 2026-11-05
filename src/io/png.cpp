// PNG writer with stored (uncompressed) zlib blocks, in the core so that the
// G-code thumbnails (src/gcode/thumbnail.hpp) can use it without the
// interface. Own design from the PNG specification and RFC 1950/1951; see
// PROVENANCE.md.

#include "io/png.hpp"

#include "io/checksum.hpp"

#include <algorithm>
#include <cstddef>

namespace tisma {
namespace {

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

} // namespace tisma
