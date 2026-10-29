// STL reader and writer written from the public description of the format: the 3D Systems
// "StereoLithography Interface Specification" (1989) and its common binary
// variant (80-byte header, little-endian uint32 count, 50 bytes per facet).
// See PROVENANCE.md.

#include "io/stl.hpp"

#include <oneapi/tbb/blocked_range.h>
#include <oneapi/tbb/parallel_for.h>

#include <algorithm>
#include <atomic>
#include <bit>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <limits>
#include <locale>
#include <sstream>

namespace tisma {
namespace {

constexpr std::size_t kHeaderSize = 80;
constexpr std::size_t kPreambleSize = kHeaderSize + 4;
constexpr std::size_t kFacetSize = 50; // normal + 3 vertices (12 floats) + uint16 attribute

std::uint32_t load_le_u32(const char* p) {
    const auto* b = reinterpret_cast<const unsigned char*>(p);
    return std::uint32_t(b[0]) | (std::uint32_t(b[1]) << 8) | (std::uint32_t(b[2]) << 16) |
           (std::uint32_t(b[3]) << 24);
}

float load_le_f32(const char* p) { return std::bit_cast<float>(load_le_u32(p)); }

bool is_space(char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v'; }

char to_lower(char c) { return (c >= 'A' && c <= 'Z') ? char(c - 'A' + 'a') : c; }

bool iequals(std::string_view a, std::string_view b) {
    return a.size() == b.size() &&
           std::equal(a.begin(), a.end(), b.begin(), [](char x, char y) { return to_lower(x) == to_lower(y); });
}

std::string_view trim(std::string_view s) {
    while (!s.empty() && (is_space(s.front()) || s.front() == '\0'))
        s.remove_prefix(1);
    while (!s.empty() && (is_space(s.back()) || s.back() == '\0'))
        s.remove_suffix(1);
    return s;
}

// An ASCII STL starts with the keyword "solid" and, being text, has no NUL
// bytes. Binary files nearly always have some (the attribute word is usually 0).
bool looks_like_ascii(std::string_view data) {
    if (std::memchr(data.data(), '\0', data.size()) != nullptr)
        return false;
    std::size_t i = 0;
    while (i < data.size() && is_space(data[i]))
        ++i;
    return data.size() - i >= 5 && iequals(data.substr(i, 5), "solid") &&
           (data.size() - i == 5 || is_space(data[i + 5]));
}

StlFile parse_binary(std::string_view data, std::uint32_t count) {
    StlFile out;
    out.format = StlFormat::Binary;
    out.name = std::string(trim(data.substr(0, kHeaderSize)));
    // The facets over the threads, each into its own place (a model of
    // millions of triangles); the first facet with a non-finite coordinate,
    // whichever thread finds it, is the one named.
    std::vector<Vec3f> corners(std::size_t(count) * 3);
    std::atomic<std::uint32_t> bad{std::numeric_limits<std::uint32_t>::max()};
    const char* const facets = data.data() + kPreambleSize;
    tbb::parallel_for(tbb::blocked_range<std::uint32_t>(0, count, 1u << 14),
                      [&](const tbb::blocked_range<std::uint32_t>& range) {
                          for (std::uint32_t i = range.begin(); i != range.end(); ++i) {
                              const char* p = facets + std::size_t(i) * kFacetSize;
                              for (int k = 0; k < 3; ++k) {
                                  const char* q = p + 12 + 12 * k; // skip the facet normal
                                  const Vec3f v{load_le_f32(q), load_le_f32(q + 4), load_le_f32(q + 8)};
                                  if (!std::isfinite(v.x) || !std::isfinite(v.y) || !std::isfinite(v.z)) {
                                      std::uint32_t seen = bad.load();
                                      while (i < seen && !bad.compare_exchange_weak(seen, i)) {
                                      }
                                  }
                                  corners[std::size_t(i) * 3 + std::size_t(k)] = v;
                              }
                          }
                      });
    if (const std::uint32_t i = bad.load(); i != std::numeric_limits<std::uint32_t>::max())
        throw StlError("binary STL: facet " + std::to_string(i) + " has a non-finite coordinate");
    out.mesh = TriangleMesh(std::move(corners));
    return out;
}

class AsciiParser {
public:
    explicit AsciiParser(std::string_view data) : data_(data) {}

    StlFile parse() {
        StlFile out;
        out.format = StlFormat::Ascii;
        out.mesh.reserve(data_.size() / 250); // a typical facet takes about 250 bytes
        bool first_solid = true;
        while (true) {
            std::string_view tok = next();
            if (tok.empty())
                break;
            if (!iequals(tok, "solid"))
                fail("expected 'solid', found '" + std::string(tok) + "'");
            std::string_view name = rest_of_line();
            if (first_solid) {
                out.name = std::string(name);
                first_solid = false;
            }
            if (!parse_facets(out.mesh))
                break; // end of file without 'endsolid': accepted
        }
        if (first_solid)
            fail("empty file");
        return out;
    }

private:
    // Parses facets until 'endsolid'. Returns false if the data ends first.
    bool parse_facets(TriangleMesh& mesh) {
        while (true) {
            std::string_view tok = next();
            if (tok.empty())
                return false;
            if (iequals(tok, "endsolid")) {
                rest_of_line();
                return true;
            }
            if (!iequals(tok, "facet"))
                fail("expected 'facet' or 'endsolid', found '" + std::string(tok) + "'");
            expect("normal");
            for (int k = 0; k < 3; ++k)
                number(); // normals are recomputed from the winding
            expect("outer");
            expect("loop");
            Vec3f v[3];
            for (Vec3f& p : v) {
                expect("vertex");
                p = {number(), number(), number()};
            }
            expect("endloop");
            expect("endfacet");
            mesh.add_triangle(v[0], v[1], v[2]);
        }
    }

    std::string_view next() {
        while (pos_ < data_.size() && is_space(data_[pos_]))
            ++pos_;
        const std::size_t start = pos_;
        while (pos_ < data_.size() && !is_space(data_[pos_]))
            ++pos_;
        token_start_ = start;
        return data_.substr(start, pos_ - start);
    }

    std::string_view rest_of_line() {
        const std::size_t start = pos_;
        while (pos_ < data_.size() && data_[pos_] != '\n')
            ++pos_;
        return trim(data_.substr(start, pos_ - start));
    }

    void expect(std::string_view keyword) {
        std::string_view tok = next();
        if (!iequals(tok, keyword))
            fail("expected '" + std::string(keyword) + "', found '" + std::string(tok) + "'");
    }

    float number() {
        std::string_view tok = next();
        std::string_view digits = tok;
        if (!digits.empty() && digits.front() == '+')
            digits.remove_prefix(1); // from_chars does not accept a leading '+'
        float value = 0.0f;
        if (digits.empty() || !parse_float(digits, value) || !std::isfinite(value))
            fail("expected a number, found '" + std::string(tok) + "'");
        return value;
    }

    static bool parse_float(std::string_view s, float& value) {
#if defined(__cpp_lib_to_chars) && __cpp_lib_to_chars >= 201611L
        const auto [end, ec] = std::from_chars(s.data(), s.data() + s.size(), value);
        return ec == std::errc() && end == s.data() + s.size();
#else
        // Standard libraries without floating-point from_chars: locale-independent stream.
        std::istringstream in{std::string(s)};
        in.imbue(std::locale::classic());
        in >> value;
        return !in.fail() && in.peek() == std::char_traits<char>::eof();
#endif
    }

    [[noreturn]] void fail(const std::string& message) const {
        const std::size_t line = 1 + std::size_t(std::count(data_.begin(), data_.begin() + std::ptrdiff_t(token_start_), '\n'));
        throw StlError("ASCII STL, line " + std::to_string(line) + ": " + message);
    }

    std::string_view data_;
    std::size_t pos_ = 0;
    std::size_t token_start_ = 0;
};

} // namespace

StlFile parse_stl(std::string_view data) {
    // An ASCII file saved with a UTF-8 byte order mark: text all the same
    // (the spec team's BH17, read as binary before).
    if (data.starts_with("\xEF\xBB\xBF") && looks_like_ascii(data.substr(3)))
        return AsciiParser(data.substr(3)).parse();
    if (data.size() >= kPreambleSize) {
        const std::uint32_t count = load_le_u32(data.data() + kHeaderSize);
        const std::uint64_t expected = kPreambleSize + std::uint64_t(count) * kFacetSize;
        if (expected == data.size())
            return parse_binary(data, count);
        if (looks_like_ascii(data)) {
            try {
                return AsciiParser(data).parse();
            } catch (const StlError&) {
                if (expected > data.size())
                    throw;
                // A binary file whose header starts with "solid" and has trailing bytes.
            }
        }
        if (expected < data.size())
            return parse_binary(data, count); // some exporters append bytes after the last facet
        throw StlError("binary STL is truncated: header announces " + std::to_string(count) + " facets (" +
                       std::to_string(expected) + " bytes) but the file has " + std::to_string(data.size()) +
                       " bytes");
    }
    if (looks_like_ascii(data))
        return AsciiParser(data).parse();
    throw StlError("file too short to be an STL (" + std::to_string(data.size()) + " bytes)");
}

StlFile read_stl(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary | std::ios::ate);
    if (!in)
        throw StlError("cannot open '" + path.string() + "'");
    const std::streamoff size = in.tellg();
    if (size < 0)
        throw StlError("cannot read '" + path.string() + "'");
    std::string buffer(static_cast<std::size_t>(size), '\0');
    in.seekg(0);
    if (!in.read(buffer.data(), size))
        throw StlError("cannot read '" + path.string() + "'");
    return parse_stl(buffer);
}

std::string stl_binary(const TriangleMesh& mesh, std::string_view header) {
    std::string out(kPreambleSize, '\0');
    if (!header.empty())
        std::memcpy(out.data(), header.data(), std::min(header.size(), kHeaderSize));
    const auto count = static_cast<std::uint32_t>(mesh.triangle_count());
    for (std::size_t i = 0; i < 4; ++i)
        out[kHeaderSize + i] = static_cast<char>((count >> (8 * i)) & 0xFF);
    out.reserve(kPreambleSize + kFacetSize * mesh.triangle_count());
    const auto put = [&out](float v) {
        const auto bits = std::bit_cast<std::uint32_t>(v);
        for (int i = 0; i < 4; ++i)
            out += static_cast<char>((bits >> (8 * i)) & 0xFF);
    };
    for (std::size_t t = 0; t < mesh.triangle_count(); ++t) {
        const Vec3f& a = mesh.corner(t, 0);
        const Vec3f& b = mesh.corner(t, 1);
        const Vec3f& c = mesh.corner(t, 2);
        const double ux = double(b.x) - a.x, uy = double(b.y) - a.y, uz = double(b.z) - a.z;
        const double vx = double(c.x) - a.x, vy = double(c.y) - a.y, vz = double(c.z) - a.z;
        double nx = uy * vz - uz * vy, ny = uz * vx - ux * vz, nz = ux * vy - uy * vx;
        const double length = std::sqrt(nx * nx + ny * ny + nz * nz);
        if (length > 0.0)
            nx /= length, ny /= length, nz /= length;
        for (const double v : {nx, ny, nz})
            put(static_cast<float>(v));
        for (const Vec3f* p : {&a, &b, &c}) {
            put(p->x);
            put(p->y);
            put(p->z);
        }
        out += '\0';
        out += '\0';
    }
    return out;
}

} // namespace tisma
