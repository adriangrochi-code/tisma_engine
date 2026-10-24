// SVG icons to images, with nanosvg (Zlib, see THIRD_PARTY.md). Own code
// around it; see PROVENANCE.md.

#include "ui/icons.hpp"

#include <algorithm>
#include <string>

#define NANOSVG_IMPLEMENTATION
#define NANOSVGRAST_IMPLEMENTATION
#include <nanosvg.h>
#include <nanosvgrast.h>

namespace tisma::ui {
namespace {

constexpr std::uint32_t kIconGray = 0x808080; // as 0xRRGGBB, the same in nanosvg's 0xAABBGGRR

// nanosvg keeps colours as 0xAABBGGRR.
std::uint32_t abgr(std::uint32_t rgb, std::uint32_t alpha) {
    return (alpha << 24) | ((rgb & 0xFF) << 16) | (rgb & 0xFF00) | ((rgb >> 16) & 0xFF);
}

void recolor(NSVGpaint& paint, std::uint32_t gray) {
    if (paint.type == NSVG_PAINT_COLOR && (paint.color & 0xFFFFFFu) == kIconGray)
        paint.color = abgr(gray, paint.color >> 24);
}

} // namespace

Image rasterize_svg(std::string_view svg, int size, std::uint32_t gray) {
    Image out;
    if (size <= 0)
        return out;
    std::string text(svg); // nanosvg parses in place
    NSVGimage* image = nsvgParse(text.data(), "px", 96.0f);
    if (!image)
        return out;
    if (image->width <= 0 || image->height <= 0) {
        nsvgDelete(image);
        return out;
    }
    for (NSVGshape* shape = image->shapes; shape; shape = shape->next) {
        recolor(shape->fill, gray);
        recolor(shape->stroke, gray);
    }
    const float scale = float(size) / std::max(image->width, image->height);
    const float tx = (float(size) - image->width * scale) * 0.5f;
    const float ty = (float(size) - image->height * scale) * 0.5f;
    out.width = out.height = size;
    out.rgba.assign(std::size_t(size) * std::size_t(size) * 4, 0);
    NSVGrasterizer* raster = nsvgCreateRasterizer();
    if (raster) {
        nsvgRasterize(raster, image, tx, ty, scale, out.rgba.data(), size, size, size * 4);
        nsvgDeleteRasterizer(raster);
    } else {
        out = {};
    }
    nsvgDelete(image);
    return out;
}

} // namespace tisma::ui
