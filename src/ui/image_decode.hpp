#pragma once

// JPEG, PNG and WebP pictures to RGBA rows: the frames of a Bambu Lab
// printer's camera (printer_connections.md, «Bambu Lab en red local», rule 6),
// PNG pictures and the library's WebP thumbnails (p2p_library.md, rule 3 ter).
// Through stb_image (MIT) and libwebp's decoder (BSD-3-Clause; THIRD_PARTY.md),
// from memory, at most 8192 pixels a side. Own wrapper; see PROVENANCE.md.

#include "ui/icons.hpp" // Image

#include <string_view>

namespace tisma::ui {

// The picture as RGBA rows, top row first; empty when the bytes are not a
// JPEG, PNG or WebP it can read.
Image decode_image(std::string_view bytes);

} // namespace tisma::ui
