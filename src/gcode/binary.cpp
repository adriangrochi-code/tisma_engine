// The binary G-code (docs/spec/features/gcode_extras.md, rule 9). Own design
// from the public description of the format; the CRC-32 from its definition
// (IEEE 802.3). See PROVENANCE.md.

#include "gcode/binary.hpp"

#include "config/settings.hpp"

#include <array>
#include <stdexcept>

#ifndef TISMA_VERSION
#define TISMA_VERSION "dev"
#endif

namespace tisma {

namespace {

constexpr std::array<std::uint32_t, 256> crc_table() {
    std::array<std::uint32_t, 256> table{};
    for (std::uint32_t i = 0; i < 256; ++i) {
        std::uint32_t c = i;
        for (int k = 0; k < 8; ++k)
            c = c & 1u ? 0xEDB88320u ^ (c >> 1) : c >> 1;
        table[i] = c;
    }
    return table;
}
constexpr std::array<std::uint32_t, 256> kCrcTable = crc_table();

// The block types and the size of their parameters.
enum : std::uint16_t { kFileMetadata = 0, kGcode = 1, kSlicerMetadata = 2, kPrinterMetadata = 3, kPrintMetadata = 4, kThumbnail = 5 };
constexpr std::size_t kMaxGcodeBlock = 65536;

void put16(std::string& out, std::uint16_t v) {
    out += char(v & 0xFF);
    out += char(v >> 8);
}

void put32(std::string& out, std::uint32_t v) {
    for (int i = 0; i < 4; ++i)
        out += char((v >> (8 * i)) & 0xFF);
}

std::uint32_t get(std::string_view bytes, std::size_t at, int size) {
    std::uint32_t v = 0;
    for (int i = 0; i < size; ++i)
        v |= std::uint32_t(static_cast<unsigned char>(bytes[at + std::size_t(i)])) << (8 * i);
    return v;
}

// One uncompressed block with its CRC32 over header, parameters and data.
void block(std::string& out, std::uint16_t type, std::string_view data) {
    std::string head;
    put16(head, type);
    put16(head, 0); // no compression
    put32(head, std::uint32_t(data.size()));
    put16(head, 0); // the encoding: INI for metadata, none for G-code
    std::uint32_t crc = crc32(head);
    crc = crc32(data, crc);
    out += head;
    out += data;
    put32(out, crc);
}

using Pairs = std::vector<std::pair<std::string, std::string>>;

std::string ini(const Pairs& pairs) {
    std::string out;
    for (const auto& [key, value] : pairs)
        out += key + "=" + value + "\n";
    return out;
}

Pairs read_ini(std::string_view text) {
    Pairs out;
    std::size_t at = 0;
    while (at < text.size()) {
        const std::size_t end = std::min(text.find('\n', at), text.size());
        const std::string_view line = text.substr(at, end - at);
        if (const std::size_t eq = line.find('='); eq != std::string_view::npos)
            out.push_back({std::string(line.substr(0, eq)), std::string(line.substr(eq + 1))});
        at = end + 1;
    }
    return out;
}

} // namespace

std::uint32_t crc32(std::string_view bytes, std::uint32_t crc) {
    crc = ~crc;
    for (const char b : bytes)
        crc = kCrcTable[(crc ^ static_cast<unsigned char>(b)) & 0xFFu] ^ (crc >> 8);
    return ~crc;
}

std::string binary_gcode(std::string_view text, const Settings& s) {
    // The G-code up to the closing summary; the summary and the settings after it.
    const std::string_view summary_mark = "\n; estimated printing time = ";
    const std::size_t summary = text.find(summary_mark);
    const std::string_view body = summary == std::string_view::npos ? text : text.substr(0, summary + 1);
    Pairs print, slicer;
    if (summary != std::string_view::npos) {
        std::size_t at = summary + 1;
        while (at < text.size()) {
            const std::size_t end = std::min(text.find('\n', at), text.size());
            const std::string_view line = text.substr(at, end - at);
            at = end + 1;
            const std::size_t eq = line.find(" = ");
            if (!line.starts_with("; ") || eq == std::string_view::npos)
                continue;
            const std::string key(line.substr(2, eq - 2));
            // A setting's name has its group: "layers.height".
            (key.find('.') != std::string::npos ? slicer : print).push_back({key, std::string(line.substr(eq + 3))});
        }
    }
    const auto variable = [&](const char* name) {
        const auto it = s.variables.find(name);
        return it == s.variables.end() || it->second.empty() ? std::string() : it->second.front();
    };
    const auto summary_value = [&](const char* key) {
        for (const auto& [k, v] : print)
            if (k == key)
                return v;
        return std::string();
    };
    const auto number = [](double v) {
        std::string t = std::to_string(v);
        t.erase(t.find_last_not_of('0') + 1);
        if (!t.empty() && t.back() == '.')
            t.pop_back();
        return t;
    };
    // machine.high_flow_nozzle, by extruder (multi_extruder.md): 0 or 1, comma separated.
    std::string high_flow;
    for (std::size_t i = 0; i < s.high_flow_nozzle.size(); ++i)
        high_flow += (i ? "," : "") + std::string(s.high_flow_nozzle[i] ? "1" : "0");
    const Pairs printer = {{"printer_model", variable("printer_model")},
                           {"filament_type", variable("filament_type")},
                           {"nozzle_diameter", number(s.nozzle_diameter)},
                           {"high_flow_nozzle", high_flow},
                           {"layer_height", number(s.layer_height)},
                           {"temperature", variable("temperature")},
                           {"bed_temperature", variable("bed_temperature")},
                           {"brim_width", number(s.brim_width)},
                           {"fill_density", number(s.infill_density) + "%"},
                           {"support_material", s.support ? "1" : "0"},
                           {"ironing", s.ironing ? "1" : "0"},
                           {"filament used [mm]", summary_value("filament used [mm]")},
                           {"filament used [g]", summary_value("filament used [g]")},
                           {"estimated printing time (normal mode)", summary_value("estimated printing time (normal mode)")}};
    std::string out = "GCDE";
    put32(out, 1); // version
    put16(out, 1); // CRC32
    block(out, kFileMetadata, ini({{"Producer", std::string("Tisma engine ") + TISMA_VERSION}}));
    block(out, kPrinterMetadata, ini(printer));
    block(out, kPrintMetadata, ini(print));
    block(out, kSlicerMetadata, ini(slicer));
    // The G-code, in blocks of whole lines.
    std::size_t at = 0;
    while (at < body.size()) {
        std::size_t end = std::min(at + kMaxGcodeBlock, body.size());
        if (end < body.size()) {
            const std::size_t line_end = body.rfind('\n', end - 1);
            end = line_end != std::string_view::npos && line_end >= at ? line_end + 1 : end;
        }
        block(out, kGcode, body.substr(at, end - at));
        at = end;
    }
    return out;
}

BinaryGcode read_binary_gcode(std::string_view bytes) {
    if (bytes.size() < 10 || bytes.substr(0, 4) != "GCDE")
        throw std::runtime_error("not a binary G-code: no GCDE header");
    if (get(bytes, 4, 4) != 1)
        throw std::runtime_error("binary G-code version " + std::to_string(get(bytes, 4, 4)) + " is not 1");
    const std::uint32_t checksum = get(bytes, 8, 2);
    if (checksum > 1)
        throw std::runtime_error("unknown checksum type " + std::to_string(checksum));
    BinaryGcode out;
    std::size_t at = 10;
    while (at < bytes.size()) {
        if (bytes.size() - at < 8)
            throw std::runtime_error("a block header cut short");
        const std::uint32_t type = get(bytes, at, 2), compression = get(bytes, at + 2, 2), size = get(bytes, at + 4, 4);
        if (compression != 0)
            throw std::runtime_error("compressed blocks are not read");
        const std::size_t parameters = type == kThumbnail ? 6 : 2;
        if (type > kThumbnail)
            throw std::runtime_error("unknown block type " + std::to_string(type));
        const std::size_t whole = 8 + parameters + size + (checksum ? 4 : 0);
        if (bytes.size() - at < whole)
            throw std::runtime_error("a block runs past the end of the file");
        if (checksum && crc32(bytes.substr(at, whole - 4)) != get(bytes, at + whole - 4, 4))
            throw std::runtime_error("a block's CRC32 does not match");
        const std::string_view data = bytes.substr(at + 8 + parameters, size);
        switch (type) {
        case kFileMetadata:
            out.file = read_ini(data);
            break;
        case kPrinterMetadata:
            out.printer = read_ini(data);
            break;
        case kPrintMetadata:
            out.print = read_ini(data);
            break;
        case kSlicerMetadata:
            out.slicer = read_ini(data);
            break;
        case kGcode:
            out.gcode += data;
            break;
        default: // thumbnails: not read here
            break;
        }
        at += whole;
    }
    return out;
}

} // namespace tisma
