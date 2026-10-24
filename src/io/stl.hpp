#pragma once

#include "mesh/mesh.hpp"

#include <cstddef>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <string_view>

namespace tisma {

class StlError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

enum class StlFormat { Binary, Ascii };

struct StlFile {
    TriangleMesh mesh;
    StlFormat format = StlFormat::Binary;
    std::string name; // ASCII solid name, or the binary header with trailing NULs and spaces removed
};

// Parses an STL file held in memory. The format is detected from the content:
// a buffer whose size matches the binary triangle count is binary even if it
// starts with "solid"; otherwise a buffer starting with "solid" is ASCII.
// Throws StlError on malformed input.
StlFile parse_stl(std::string_view data);

// Reads and parses an STL file. Throws StlError if it cannot be read or parsed.
StlFile read_stl(const std::filesystem::path& path);

// The mesh as a binary STL: the header (the text, cut to 80 bytes and padded
// with zeros; never read back as ASCII, since the size matches the count), the
// triangle count, and per triangle its unit normal from the winding (zero for
// a degenerate one), its three corners and a zero attribute, little-endian.
std::string stl_binary(const TriangleMesh& mesh, std::string_view header = {});

} // namespace tisma
