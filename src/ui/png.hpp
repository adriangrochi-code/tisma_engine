#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace tisma::ui {

// A PNG file of an RGBA image (8 bits per channel, rows top to bottom), from
// the PNG specification (W3C, third edition) with zlib "stored" blocks
// (RFC 1950 and 1951): no compression, so the writer stays a few lines. For
// screenshots of the interface. Own design; see PROVENANCE.md.
std::vector<std::uint8_t> encode_png(int width, int height, const std::uint8_t* rgba);

// Writes it; false if the file cannot be written.
bool write_png(const std::filesystem::path& path, int width, int height, const std::uint8_t* rgba);

// CRC-32 of the PNG specification (ISO 3309), and Adler-32 of RFC 1950.
std::uint32_t crc32(const std::uint8_t* data, std::size_t size, std::uint32_t crc = 0);
std::uint32_t adler32(const std::uint8_t* data, std::size_t size, std::uint32_t adler = 1);

} // namespace tisma::ui
