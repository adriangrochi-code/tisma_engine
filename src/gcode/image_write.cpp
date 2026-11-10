// JPG and PNG through stb_image_write. Own code; see image_write.hpp.

#include "gcode/image_write.hpp"

#include <algorithm>
#include <cstddef>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STB_IMAGE_WRITE_STATIC
#define STBI_WRITE_NO_STDIO
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wshadow"
#pragma GCC diagnostic ignored "-Wdouble-promotion"
#pragma GCC diagnostic ignored "-Wcast-qual"
#pragma GCC diagnostic ignored "-Wold-style-cast"
#pragma GCC diagnostic ignored "-Wuseless-cast"
#pragma GCC diagnostic ignored "-Wimplicit-fallthrough"
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#pragma GCC diagnostic ignored "-Wpragmas"
#elif defined(_MSC_VER)
#pragma warning(push, 0)
#endif
#include <stb_image_write.h>
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#elif defined(_MSC_VER)
#pragma warning(pop)
#endif

namespace tisma {
namespace {

void append(void* context, void* data, int size) {
    auto* out = static_cast<std::vector<std::uint8_t>*>(context);
    const auto* bytes = static_cast<const std::uint8_t*>(data);
    out->insert(out->end(), bytes, bytes + std::max(0, size));
}

} // namespace

std::vector<std::uint8_t> encode_png_compressed(int width, int height, const std::uint8_t* rgba) {
    std::vector<std::uint8_t> out;
    if (width <= 0 || height <= 0)
        return out;
    if (!stbi_write_png_to_func(append, &out, width, height, 4, rgba, width * 4))
        out.clear();
    return out;
}

std::vector<std::uint8_t> encode_jpg(int width, int height, const std::uint8_t* rgba, int quality,
                                     std::uint32_t background) {
    std::vector<std::uint8_t> out;
    if (width <= 0 || height <= 0)
        return out;
    // Over the background: c * a + bg * (1 - a).
    std::vector<std::uint8_t> rgb(std::size_t(width) * std::size_t(height) * 3);
    const int bg[3] = {int(background >> 16 & 0xFF), int(background >> 8 & 0xFF), int(background & 0xFF)};
    for (std::size_t i = 0; i < std::size_t(width) * std::size_t(height); ++i) {
        const int a = rgba[4 * i + 3];
        for (int c = 0; c < 3; ++c)
            rgb[3 * i + std::size_t(c)] = std::uint8_t((rgba[4 * i + std::size_t(c)] * a + bg[c] * (255 - a) + 127) / 255);
    }
    if (!stbi_write_jpg_to_func(append, &out, width, height, 3, rgb.data(), std::clamp(quality, 1, 100)))
        out.clear();
    return out;
}

} // namespace tisma
