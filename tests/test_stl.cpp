#include "io/stl.hpp"
#include "test_temp.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <array>
#include <bit>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>

using namespace tisma;
using Catch::Matchers::ContainsSubstring;
using Catch::Matchers::WithinAbs;

namespace {

void put_u32(std::string& s, std::uint32_t v) {
    for (int i = 0; i < 4; ++i)
        s.push_back(char((v >> (8 * i)) & 0xFF));
}

void put_f32(std::string& s, float f) { put_u32(s, std::bit_cast<std::uint32_t>(f)); }

using Tri = std::array<Vec3f, 3>;

// Builds a binary STL in memory, byte by byte in little-endian order.
std::string make_binary(const std::vector<Tri>& tris, std::string header = "tisma test",
                        std::uint32_t announced = std::numeric_limits<std::uint32_t>::max()) {
    header.resize(80, '\0');
    std::string s = header;
    put_u32(s, announced == std::numeric_limits<std::uint32_t>::max() ? std::uint32_t(tris.size()) : announced);
    for (const Tri& t : tris) {
        put_f32(s, 0.0f);
        put_f32(s, 0.0f);
        put_f32(s, 1.0f);
        for (const Vec3f& p : t) {
            put_f32(s, p.x);
            put_f32(s, p.y);
            put_f32(s, p.z);
        }
        s.push_back('\0');
        s.push_back('\0');
    }
    return s;
}

const std::vector<Tri> kTwoTris = {
    Tri{Vec3f{0, 0, 0}, Vec3f{1, 0, 0}, Vec3f{0, 1, 0}},
    Tri{Vec3f{1.5f, -2.25f, 3}, Vec3f{4, 5, 6}, Vec3f{-7, 8.125f, 9}},
};

std::filesystem::path data_file(const char* name) { return std::filesystem::path(TISMA_TEST_DATA_DIR) / name; }

} // namespace

TEST_CASE("binary STL is read exactly", "[stl]") {
    const StlFile f = parse_stl(make_binary(kTwoTris));
    CHECK(f.format == StlFormat::Binary);
    CHECK(f.name == "tisma test");
    REQUIRE(f.mesh.triangle_count() == 2);
    for (std::size_t i = 0; i < 2; ++i)
        for (int k = 0; k < 3; ++k)
            CHECK(f.mesh.corner(i, k) == kTwoTris[i][std::size_t(k)]);
}

TEST_CASE("binary STL with zero facets is an empty mesh", "[stl]") {
    const StlFile f = parse_stl(make_binary({}));
    CHECK(f.format == StlFormat::Binary);
    CHECK(f.mesh.empty());
}

TEST_CASE("binary STL whose header starts with 'solid' is still binary", "[stl]") {
    const StlFile f = parse_stl(make_binary(kTwoTris, "solid exported by some CAD"));
    CHECK(f.format == StlFormat::Binary);
    CHECK(f.mesh.triangle_count() == 2);
}

TEST_CASE("binary STL with trailing bytes is accepted", "[stl]") {
    std::string data = make_binary(kTwoTris, "solid x");
    data += "trailing";
    const StlFile f = parse_stl(data);
    CHECK(f.format == StlFormat::Binary);
    CHECK(f.mesh.triangle_count() == 2);
}

TEST_CASE("truncated binary STL is rejected", "[stl]") {
    std::string data = make_binary(kTwoTris);
    data.resize(data.size() - 10);
    CHECK_THROWS_WITH(parse_stl(data), ContainsSubstring("truncated"));
}

TEST_CASE("binary STL with a NaN coordinate is rejected", "[stl]") {
    std::vector<Tri> tris = kTwoTris;
    tris[1][2].y = std::numeric_limits<float>::quiet_NaN();
    CHECK_THROWS_WITH(parse_stl(make_binary(tris)), ContainsSubstring("non-finite"));
    // Read over the threads, it still names the first bad facet.
    std::vector<Tri> many;
    for (int i = 0; i < 100000; ++i)
        many.push_back(kTwoTris[std::size_t(i % 2)]);
    many[90000][0].x = std::numeric_limits<float>::infinity();
    many[70001][1].z = std::numeric_limits<float>::quiet_NaN();
    CHECK_THROWS_WITH(parse_stl(make_binary(many)), ContainsSubstring("facet 70001 has a non-finite"));
    many[70001][1].z = 0.0f;
    many[90000][0].x = 0.0f;
    const StlFile f = parse_stl(make_binary(many));
    REQUIRE(f.mesh.triangle_count() == 100000);
    CHECK(f.mesh.corner(99999, 2).x == kTwoTris[1][2].x);
}

TEST_CASE("ASCII STL is read", "[stl]") {
    const std::string text = "solid  my part \n"
                             "facet normal 0 0 1\n"
                             " outer loop\n"
                             "  vertex 0 0 0\n"
                             "  vertex 1.5e1 0 0\n"
                             "  vertex 0 +2.5 -3\n"
                             " endloop\n"
                             "endfacet\n"
                             "endsolid my part\n";
    const StlFile f = parse_stl(text);
    CHECK(f.format == StlFormat::Ascii);
    CHECK(f.name == "my part");
    REQUIRE(f.mesh.triangle_count() == 1);
    CHECK(f.mesh.corner(0, 1) == Vec3f{15.0f, 0.0f, 0.0f});
    CHECK(f.mesh.corner(0, 2) == Vec3f{0.0f, 2.5f, -3.0f});
}

TEST_CASE("ASCII STL accepts upper case, CRLF, several solids and a missing endsolid", "[stl]") {
    const std::string text = "SOLID a\r\n"
                             "FACET NORMAL 0 0 1\r\nOUTER LOOP\r\nVERTEX 0 0 0\r\nVERTEX 1 0 0\r\nVERTEX 0 1 0\r\n"
                             "ENDLOOP\r\nENDFACET\r\nENDSOLID a\r\n"
                             "solid b\n"
                             "facet normal 0 0 1 outer loop vertex 0 0 1 vertex 1 0 1 vertex 0 1 1 endloop endfacet\n";
    const StlFile f = parse_stl(text);
    CHECK(f.name == "a");
    REQUIRE(f.mesh.triangle_count() == 2);
    CHECK(f.mesh.corner(1, 0) == Vec3f{0, 0, 1});
}

TEST_CASE("ASCII STL after a UTF-8 byte order mark is text (the spec team's BH17)", "[stl]") {
    const StlFile f = parse_stl("\xEF\xBB\xBFsolid a\nfacet normal 0 0 1 outer loop vertex 0 0 0 vertex 1 0 0 vertex 0 1 0 "
                                "endloop endfacet\nendsolid a\n");
    CHECK(f.format == StlFormat::Ascii);
    CHECK(f.name == "a");
    CHECK(f.mesh.triangle_count() == 1);
}

TEST_CASE("ASCII STL with 'solid' and no facets is an empty mesh", "[stl]") {
    const StlFile f = parse_stl("solid empty\nendsolid empty\n");
    CHECK(f.format == StlFormat::Ascii);
    CHECK(f.mesh.empty());
}

TEST_CASE("malformed ASCII STL reports the line", "[stl]") {
    const std::string text = "solid bad\n"
                             "facet normal 0 0 1\n"
                             " outer loop\n"
                             "  vertex 0 0 0\n"
                             "  vertex 1 zero 0\n";
    CHECK_THROWS_WITH(parse_stl(text), ContainsSubstring("line 5") && ContainsSubstring("zero"));
}

TEST_CASE("ASCII facet with four vertices is rejected", "[stl]") {
    const std::string text = "solid q\nfacet normal 0 0 1\nouter loop\n"
                             "vertex 0 0 0\nvertex 1 0 0\nvertex 1 1 0\nvertex 0 1 0\nendloop\nendfacet\nendsolid q\n";
    CHECK_THROWS_WITH(parse_stl(text), ContainsSubstring("expected 'endloop'"));
}

TEST_CASE("ASCII STL rejects nan coordinates", "[stl]") {
    const std::string text = "solid q\nfacet normal 0 0 1\nouter loop\n"
                             "vertex nan 0 0\nvertex 1 0 0\nvertex 1 1 0\nendloop\nendfacet\nendsolid q\n";
    CHECK_THROWS_AS(parse_stl(text), StlError);
}

TEST_CASE("garbage and tiny files are rejected", "[stl]") {
    CHECK_THROWS_AS(parse_stl(""), StlError);
    CHECK_THROWS_AS(parse_stl("hello"), StlError);
    CHECK_THROWS_AS(parse_stl("solidify"), StlError);
}

TEST_CASE("ASCII and binary files of the same cube agree", "[stl]") {
    const StlFile ascii = read_stl(data_file("cube_ascii.stl"));
    REQUIRE(ascii.format == StlFormat::Ascii);
    REQUIRE(ascii.mesh.triangle_count() == 12);

    std::vector<Tri> tris;
    for (std::size_t i = 0; i < ascii.mesh.triangle_count(); ++i)
        tris.push_back({ascii.mesh.corner(i, 0), ascii.mesh.corner(i, 1), ascii.mesh.corner(i, 2)});
    const auto path = test_temp_dir() / "tisma_test_cube_binary.stl";
    {
        std::ofstream out(path, std::ios::binary);
        const std::string bytes = make_binary(tris);
        out.write(bytes.data(), std::streamsize(bytes.size()));
    }
    const StlFile binary = read_stl(path);
    std::filesystem::remove(path);
    CHECK(binary.format == StlFormat::Binary);
    CHECK(binary.mesh.corners() == ascii.mesh.corners());
    CHECK_THAT(binary.mesh.signed_volume(), WithinAbs(1000.0, 1e-9));
}

TEST_CASE("missing file is reported", "[stl]") {
    CHECK_THROWS_WITH(read_stl(data_file("does_not_exist.stl")), ContainsSubstring("cannot open"));
}
