#pragma once

// A USB camera's frames as the interface can show them (printer_connections.md,
// «Cámara USB de la computadora»): YUYV to RGBA, and MJPEG frames completed
// with the standard Huffman tables. Many UVC cameras leave the tables out of
// each MJPEG frame (the UVC payload format assumes them); a plain JPEG reader
// needs them. Own design from the JPEG standard (ITU-T T.81 | ISO/IEC 10918-1,
// Annex K.3, the typical tables) and ITU-R BT.601 for the colour; see
// PROVENANCE.md.

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace tisma::device {

// YUYV 4:2:2 (Y0 U Y1 V per two pixels), BT.601 studio range, to RGBA rows.
// Empty when the size does not match.
std::vector<std::uint8_t> yuyv_to_rgba(std::string_view yuyv, int width, int height);

// The MJPEG frame with the standard Huffman tables put before its scan when it
// has none; unchanged when it has them or is not a JPEG.
std::string mjpeg_with_huffman_tables(std::string_view frame);

} // namespace tisma::device
