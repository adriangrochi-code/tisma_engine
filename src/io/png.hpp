#pragma once

#include <cstdint>
#include <vector>

namespace tisma {

// A PNG file of an RGBA image (8 bits per channel, rows top to bottom), from
// the PNG specification (W3C, third edition) with zlib "stored" blocks
// (RFC 1950 and 1951): no compression, so the writer stays a few lines.
// Own design; see PROVENANCE.md.
std::vector<std::uint8_t> encode_png(int width, int height, const std::uint8_t* rgba);

} // namespace tisma
