#pragma once

// JPEG and PNG pictures to RGBA rows: the frames of a Bambu Lab printer's
// camera (printer_connections.md, «Bambu Lab en red local», rule 6) and PNG
// pictures. Through stb_image (MIT; THIRD_PARTY.md), JPEG and PNG only, from
// memory, at most 8192 pixels a side. Own wrapper; see PROVENANCE.md.

#include "ui/icons.hpp" // Image

#include <string_view>

namespace tisma::ui {

// The picture as RGBA rows, top row first; empty when the bytes are not a
// JPEG or PNG it can read.
Image decode_image(std::string_view bytes);

} // namespace tisma::ui
