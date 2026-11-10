// JPEG and PNG through stb_image. Own code; see image_decode.hpp.

#include "ui/image_decode.hpp"

#include <climits>
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
