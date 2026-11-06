#pragma once

// G-code thumbnails (gcode_extras.md, rule 8; formats.md, rule 20): the
// `gcode.thumbnails` list, a picture of the part drawn on the CPU without the
// 3D view, its PNG, JPG (gcode/image_write) or QOI file, and the pictures for
// a binary G-code. The form in a text G-code comes later (the comment format is
// still being measured). Own design; QOI from its specification
// (qoiformat.org, version 1.0); see PROVENANCE.md.

#include "gcode/binary.hpp" // GcodeThumbnail
#include "mesh/mesh.hpp"
#include "report/warnings.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace tisma {

enum class ThumbnailFormat { Png, Jpg, Qoi };

struct ThumbnailSize {
    int width = 0, height = 0;
    ThumbnailFormat format = ThumbnailFormat::Png;
    friend bool operator==(const ThumbnailSize&, const ThumbnailSize&) = default;
};

// «16x16/PNG,32x24/JPG,20x20/QOI»: sizes of 1 to 1024 pixels a side; the
// format in any case, PNG when left out («16x16»); spaces and «;» between
// items also taken. Empty text: none. Throws std::invalid_argument saying
// which item is wrong.
std::vector<ThumbnailSize> parse_thumbnails(const std::string& text);
// Back to the text, as parse_thumbnails reads it.
std::string thumbnails_text(const std::vector<ThumbnailSize>& sizes);

// The part seen from the front left and above (an isometric view), as large as
// fits with a small margin, shaded by the light from above the eye; RGBA rows
// from the top, the background clear. Drawn at twice the size and halved, so
// the edges are smooth. An empty mesh gives a clear picture.
std::vector<std::uint8_t> draw_part(const TriangleMesh& mesh, int width, int height);

// The pictures `gcode.thumbnails` asks for, of the part: PNG (compressed), JPG
// (over white) and QOI files. One that cannot be written is named in `skipped`.
std::vector<GcodeThumbnail> make_thumbnails(const TriangleMesh& mesh, const std::vector<ThumbnailSize>& sizes,
                                            std::vector<ThumbnailSize>* skipped = nullptr);

struct Settings;
// The thumbnails of the G-code the settings make: those of gcode.thumbnails in
// a binary G-code. In a text G-code none yet (the comment form is still being
// measured, formats.md rule 20), said in a warning.
std::vector<GcodeThumbnail> gcode_thumbnails(const TriangleMesh& mesh, const Settings& settings,
                                             std::vector<Warning>* warnings = nullptr);

// The QOI file of RGBA rows from the top (4 channels, sRGB with linear alpha).
std::vector<std::uint8_t> encode_qoi(int width, int height, const std::uint8_t* rgba);
// Reads a QOI file back to RGBA rows; false when it is not one.
bool decode_qoi(const std::vector<std::uint8_t>& file, int& width, int& height, std::vector<std::uint8_t>& rgba);

} // namespace tisma
