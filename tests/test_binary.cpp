#include "config/settings.hpp"
#include "gcode/binary.hpp"
#include "gcode/export.hpp"
#include "gcode/output.hpp"
#include "gcode/writer.hpp"
#include "mesh/indexed_mesh.hpp"
#include "slice/pipeline.hpp"
#include "test_printer.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

using namespace tisma;
using Catch::Matchers::ContainsSubstring;

namespace {

IndexedMesh cube() {
    IndexedMesh m;
    for (float z : {0.0f, 10.0f})
        for (const auto& p : {std::array<float, 2>{90, 90}, {110, 90}, {110, 110}, {90, 110}})
            m.vertices.push_back({p[0], p[1], z});
    for (const auto& f : std::vector<std::array<std::uint32_t, 4>>{{0, 3, 2, 1}, {4, 5, 6, 7}, {0, 1, 5, 4}, {1, 2, 6, 5}, {2, 3, 7, 6}, {3, 0, 4, 7}}) {
        m.triangles.push_back({f[0], f[1], f[2]});
        m.triangles.push_back({f[0], f[2], f[3]});
    }
    return m;
}

Settings printer() {
    Settings s = test_printer();
    s.variables["temperature"] = {"200"};
    s.variables["filament_type"] = {"PLA"};
    s.filament_density = 1.24;
    return s;
}

std::uint32_t u(const std::string& b, std::size_t at, int size) {
    std::uint32_t v = 0;
    for (int i = 0; i < size; ++i)
        v |= std::uint32_t(static_cast<unsigned char>(b[at + std::size_t(i)])) << (8 * i);
    return v;
}

std::string value_of(const std::vector<std::pair<std::string, std::string>>& pairs, const std::string& key) {
    for (const auto& [k, v] : pairs)
        if (k == key)
            return v;
    return "(none)";
}

} // namespace

TEST_CASE("binary G-code: the CRC-32 of IEEE 802.3", "[binary]") {
    CHECK(crc32("123456789") == 0xCBF43926u); // the standard check value
    CHECK(crc32("") == 0u);
    CHECK(crc32("6789", crc32("12345")) == crc32("123456789")); // continued
}

TEST_CASE("binary G-code: header, blocks in order, read back (gcode_extras.md, rule 9, X8)", "[binary]") {
    const Settings s = printer();
    const SlicedModel model = slice_model(cube(), s);
    std::ostringstream out;
    write_gcode(out, model, s);
    const std::string text = out.str();
    const std::string b = binary_gcode(text, s);
    // GCDE, version 1, CRC32.
    REQUIRE(b.size() > 10);
    CHECK(b.substr(0, 4) == "GCDE");
    CHECK(u(b, 4, 4) == 1);
    CHECK(u(b, 8, 2) == 1);
    // The blocks: file, printer, print and slicer metadata, then G-code.
    std::vector<std::uint32_t> types;
    std::size_t at = 10, largest = 0;
    while (at < b.size()) {
        const std::uint32_t type = u(b, at, 2), size = u(b, at + 4, 4);
        CHECK(u(b, at + 2, 2) == 0); // not compressed
        types.push_back(type);
        if (type == 1)
            largest = std::max<std::size_t>(largest, size);
        at += 8 + 2 + size + 4;
    }
    CHECK(at == b.size());
    REQUIRE(types.size() > 5);
    CHECK(std::vector<std::uint32_t>(types.begin(), types.begin() + 5) == std::vector<std::uint32_t>{0, 3, 4, 2, 1});
    CHECK(largest <= 65536);
    const BinaryGcode read = read_binary_gcode(b);
    CHECK_THAT(value_of(read.file, "Producer"), ContainsSubstring("Tisma engine"));
    CHECK(value_of(read.printer, "filament_type") == "PLA");
    CHECK(value_of(read.printer, "nozzle_diameter") == "0.4");
    CHECK(value_of(read.print, "estimated printing time (normal mode)") != "(none)");
    CHECK(value_of(read.print, "filament used [g]") != "(none)");
    CHECK(value_of(read.slicer, "layers.height") == "0.2");
    // The G-code is the text's, up to its closing summary, and whole lines in each block.
    CHECK(text.starts_with(read.gcode));
    CHECK(read.gcode.ends_with("\n"));
    CHECK(read.gcode.find("; estimated printing time") == std::string::npos);
    CHECK(text.size() - read.gcode.size() < 40000); // only the summary and the settings are left out
    // A changed byte breaks its block's CRC.
    std::string damaged = b;
    damaged[b.size() / 2] = char(damaged[b.size() / 2] ^ 0x20);
    CHECK_THROWS_WITH(read_binary_gcode(damaged), ContainsSubstring("CRC32"));
    CHECK_THROWS_WITH(read_binary_gcode("GCDX"), ContainsSubstring("GCDE"));
    CHECK_THROWS_WITH(read_binary_gcode(b.substr(0, b.size() - 3)), ContainsSubstring("past the end"));
}

TEST_CASE("binary G-code: exported with gcode.binary, named .bgcode", "[binary]") {
    Settings s = printer();
    s.gcode_binary = true;
    s.input_name = "cube20";
    const SlicedModel model = slice_model(cube(), s);
    CHECK(output_filename(s, model, std::nullopt) == "cube20.bgcode");
    s.filename_format = "{input_filename_base}.{default_output_extension}";
    CHECK(output_filename(s, model, std::nullopt) == "cube20..bgcode");
    const std::filesystem::path path = std::filesystem::temp_directory_path() / "tisma_binary.bgcode";
    const GcodeExport exported = export_gcode(path, model, s);
    REQUIRE(exported.written);
    CHECK(exported.check.ok());
    std::ifstream in(path, std::ios::binary);
    const std::string bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    in.close();
    CHECK(bytes.substr(0, 4) == "GCDE");
    CHECK(exported.bytes == bytes.size());
    CHECK(read_binary_gcode(bytes).gcode.find("G1 ") != std::string::npos);
    const std::optional<GcodeTotals> totals = read_gcode_totals(path);
    REQUIRE(totals);
    CHECK(totals->print_seconds > 60.0);
    CHECK(totals->grams > 1.0);
    std::filesystem::remove(path);
}
