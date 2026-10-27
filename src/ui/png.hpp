#pragma once

#include "io/checksum.hpp"

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

// Reads back a PNG as encode_png writes it (8-bit RGBA, stored blocks,
// filter type 0): the screenshots, for comparing them. False for any other
// PNG, or a damaged one.
bool decode_png(const std::vector<std::uint8_t>& png, int& width, int& height, std::vector<std::uint8_t>& rgba);
bool read_png(const std::filesystem::path& path, int& width, int& height, std::vector<std::uint8_t>& rgba);

// How two pictures of the same size differ inside a rectangle (x, y, w, h
// in pixels from the top left): the pixels that differ at all, those that
// differ by more than `tolerance` in some channel (0 to 255) and the largest
// difference. Alpha is not compared.
struct PictureDifference {
    std::size_t pixels = 0;
    std::size_t differing = 0;
    std::size_t over = 0;
    int largest = 0;
};
PictureDifference compare_pictures(const std::uint8_t* a, const std::uint8_t* b, int width, int height, int x, int y,
                                   int w, int h, int tolerance);

// CRC-32 of the PNG specification (ISO 3309), and Adler-32 of RFC 1950
// (io/checksum.hpp).
using tisma::adler32;
using tisma::crc32;

} // namespace tisma::ui
