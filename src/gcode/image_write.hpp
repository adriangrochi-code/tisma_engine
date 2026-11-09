#pragma once

// JPG and compressed PNG files of RGBA pictures, for the G-code thumbnails
// (gcode_extras.md, rule 8), through stb_image_write (MIT; THIRD_PARTY.md),
// to memory, no files. Own wrapper; see PROVENANCE.md.

#include <cstdint>
#include <vector>

namespace tisma {

// A PNG with deflate compression (smaller than io/png's stored blocks).
std::vector<std::uint8_t> encode_png_compressed(int width, int height, const std::uint8_t* rgba);
// A JPG (baseline, `quality` 1 to 100). JPG has no alpha: the clear parts are
// laid over `background` (0xRRGGBB).
std::vector<std::uint8_t> encode_jpg(int width, int height, const std::uint8_t* rgba, int quality = 90,
                                     std::uint32_t background = 0xFFFFFF);

} // namespace tisma
