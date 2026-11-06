#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace tisma {

struct Settings;

// A picture for the G-code (gcode_extras.md, rule 8): its image file's bytes.
struct GcodeThumbnail {
    int format = 0; // 0 PNG, 1 JPG, 2 QOI: the binary format's numbers
    int width = 0, height = 0;
    std::string data;
    friend bool operator==(const GcodeThumbnail&, const GcodeThumbnail&) = default;
};

// The CRC-32 of IEEE 802.3 (reflected polynomial 0xEDB88320, starting and
// ending with all bits flipped), continued from `crc` when given.
std::uint32_t crc32(std::string_view bytes, std::uint32_t crc = 0);

// The binary G-code (docs/spec/features/gcode_extras.md, rule 9, test X8),
// from the public description of the format: little-endian; the header
// "GCDE", version 1 and checksum type 1 (CRC32); then blocks, each a type,
// a compression (0: none) and its data's size, its parameters, its data and
// the CRC32 of the three. In this order: the file's metadata (Producer), the
// printer's (printer_model, filament_type, nozzle_diameter...), the print's
// (the closing summary: filament used, estimated times), the slicer's (every
// "; key = value" setting line), then the G-code in blocks of 64 KiB at most,
// cut at line ends. Metadata is INI text (key=value lines, parameter 0), the
// G-code is not encoded (parameter 0). The thumbnails go after the printer's
// metadata, one block each, with three 16-bit parameters: the format, the width
// and the height. `text` is a G-code as write_gcode makes it. Own design.
std::string binary_gcode(std::string_view text, const Settings& settings,
                         const std::vector<GcodeThumbnail>& thumbnails = {});

// A binary G-code read back: its metadata and its G-code joined.
struct BinaryGcode {
    std::vector<std::pair<std::string, std::string>> file, printer, print, slicer;
    std::string gcode;
    std::vector<GcodeThumbnail> thumbnails;
};

// Reads a binary G-code; throws std::runtime_error on a wrong header, an
// unknown or compressed block, a size past the end or a CRC that does not match.
BinaryGcode read_binary_gcode(std::string_view bytes);

} // namespace tisma
