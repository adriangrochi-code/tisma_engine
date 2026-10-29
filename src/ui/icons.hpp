#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

namespace tisma::ui {

// An RGBA image, 8 bits per channel, rows top to bottom, alpha not
// premultiplied.
struct Image {
    int width = 0, height = 0;
    std::vector<std::uint8_t> rgba;
};

// Rasterizes SVG text into a size x size image, scaled to fit and centred.
// The gray #808080 that the interface icons are drawn in (assets/brand/README.md)
// becomes `gray` (0xRRGGBB), so an icon can take the theme's text colour; other
// colours are kept. An empty image if the SVG cannot be read. Uses nanosvg (Zlib).
Image rasterize_svg(std::string_view svg, int size, std::uint32_t gray = 0x808080);

} // namespace tisma::ui
