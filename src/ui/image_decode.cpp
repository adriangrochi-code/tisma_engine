// JPEG and PNG through stb_image, WebP through libwebp's decoder. Own code;
// see image_decode.hpp.

#include "ui/image_decode.hpp"

#include <webp/decode.h>

#include <climits>
#include <cstdint>
#include <cstring>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#define STBI_NO_LINEAR
#define STBI_NO_HDR
#define STBI_MAX_DIMENSIONS 8192
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
#pragma GCC diagnostic ignored "-Wpragmas"
#elif defined(_MSC_VER)
#pragma warning(push, 0)
#endif
#include <stb_image.h>
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#elif defined(_MSC_VER)
#pragma warning(pop)
#endif

namespace tisma::ui {

Image decode_image(std::string_view bytes) {
    Image out;
    // WebP: «RIFF», its size, «WEBP» (the library's thumbnails).
    if (bytes.size() >= 12 && bytes.substr(0, 4) == "RIFF" && bytes.substr(8, 4) == "WEBP") {
        const auto* data = reinterpret_cast<const std::uint8_t*>(bytes.data());
        int w = 0, h = 0;
        if (!WebPGetInfo(data, bytes.size(), &w, &h) || w <= 0 || h <= 0 || w > 8192 || h > 8192)
            return out;
        out.rgba.resize(std::size_t(w) * std::size_t(h) * 4);
        if (!WebPDecodeRGBAInto(data, bytes.size(), out.rgba.data(), out.rgba.size(), w * 4)) {
            out.rgba.clear();
            return out;
        }
        out.width = w, out.height = h;
        return out;
    }
    if (bytes.empty() || bytes.size() > std::size_t(INT_MAX))
        return out;
    int w = 0, h = 0, channels = 0;
    unsigned char* pixels = stbi_load_from_memory(reinterpret_cast<const unsigned char*>(bytes.data()), int(bytes.size()),
                                                  &w, &h, &channels, 4);
    if (!pixels)
        return out;
    out.width = w;
    out.height = h;
    out.rgba.assign(pixels, pixels + std::size_t(w) * std::size_t(h) * 4);
    stbi_image_free(pixels);
    return out;
}

} // namespace tisma::ui
