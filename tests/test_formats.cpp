#include "io/model.hpp"
#include "io/xml.hpp"
#include "io/zip.hpp"
#include "slice/placement.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <random>
#include <string>

using namespace tisma;
using Catch::Matchers::ContainsSubstring;
using Catch::Matchers::WithinAbs;

namespace {

const std::filesystem::path kFormats = std::filesystem::path(TISMA_TEST_DATA_DIR) / "formats";

std::string file(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    REQUIRE(in);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

// The generator of tests/data/formats/make.py, to make the deflated data again.
struct Lcg {
    std::uint64_t x;
    std::uint32_t next() {
        x = (x * 1103515245u + 12345u) % 2147483648u;
        return std::uint32_t(x >> 16);
    }
};

std::string words(std::size_t n) {
    static const char* const kWords[] = {"tisma ", "layer ", "nozzle ", "bed ", "wall ", "infill ", "support ", "seam ",
                                         "bridge ", "skirt ", "brim ", "speed ", "flow ", "fan ", "tree ", "branch "};
    Lcg g{1};
    std::string out;
    while (out.size() < n)
        out += kWords[g.next() % 16];
    out.resize(n);
    return out;
}

std::string noise(std::size_t n, std::uint64_t seed) {
    Lcg g{seed};
    std::string out;
    for (std::size_t i = 0; i < n; ++i)
        out.push_back(char(g.next() & 0xFF));
    return out;
}

void check_box(const ModelFile& m, double x0, double y0, double size = 20.0, double height = 10.0) {
    REQUIRE_FALSE(m.mesh.empty());
    const BoundingBox box = m.mesh.bounding_box();
    CHECK_THAT(box.min.x, WithinAbs(x0, 1e-4));
    CHECK_THAT(box.min.y, WithinAbs(y0, 1e-4));
    CHECK_THAT(box.min.z, WithinAbs(0.0, 1e-4));
    CHECK_THAT(box.max.x - box.min.x, WithinAbs(size, 1e-3));
    CHECK_THAT(box.max.z - box.min.z, WithinAbs(height, 1e-3));
    CHECK_THAT(m.mesh.signed_volume(), WithinAbs(size * size * height, 1e-3 * size * size * height));
}

} // namespace

TEST_CASE("formats: DEFLATE streams of every block type inflate to their data", "[formats]") {
    std::string far = noise(20000, 7);
    far += words(3000) + far.substr(0, 12000);
    const std::pair<const char*, std::string> cases[] = {
        {"words", words(30000)}, {"fixed", words(5000)}, {"stored", noise(3000, 3)},
        {"rle", words(8000)},    {"far", far},           {"empty", std::string()}};
    for (const auto& [name, expected] : cases) {
        CAPTURE(name);
        const std::string deflated = file(kFormats / (std::string(name) + ".deflate"));
        CHECK(inflate(deflated, expected.size()) == expected);
        // A limit below the size, or the stream cut short, is an error.
        if (!expected.empty()) {
            CHECK_THROWS_AS(inflate(deflated, expected.size() - 1), ZipError);
            CHECK_THROWS_AS(inflate(deflated.substr(0, deflated.size() / 2), expected.size()), ZipError);
        }
    }
    CHECK_THROWS_AS(inflate(std::string("\x07", 1), 10), ZipError); // block type 3
    CHECK_THROWS_AS(inflate(std::string(), 10), ZipError);
}

TEST_CASE("formats: OBJ, the box with quads, normals and negative indices (FM2)", "[formats]") {
    const ModelFile m = read_model(kFormats / "box.obj");
    CHECK(m.format == ModelFormat::Obj);
    CHECK(m.mesh.triangle_count() == 12);
    check_box(m, 0.0, 0.0);
    // Line continuations and comments; the errors say the line.
    CHECK(parse_obj("v 0 0 0\nv 1 0 0 # x\nv 0 \\\n 1 0\nf 1 2 3\n").mesh.triangle_count() == 1);
    CHECK_THROWS_WITH(parse_obj("v 0 0 0\nv 1 0 0\nf 1 2 3\n"), ContainsSubstring("line 3"));
    CHECK_THROWS_WITH(parse_obj("v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2\n"), ContainsSubstring("three vertices"));
    CHECK_THROWS_WITH(parse_obj("v 0 zero 0\n"), ContainsSubstring("coordinate"));
    CHECK_THROWS_WITH(parse_obj("v 0 0 0\n"), ContainsSubstring("no faces"));
    CHECK_THROWS_AS(parse_obj("v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 -4\n"), ModelError);
}

TEST_CASE("formats: 3MF packages of the box (FM3, FM9)", "[formats]") {
    for (const char* name : {"box.3mf", "box_stored.3mf", "box_support.3mf", "box_production.3mf"}) {
        CAPTURE(name);
        const ModelFile m = read_model(kFormats / name);
        CHECK(m.format == ModelFormat::ThreeMf);
        CHECK(m.objects == 1);
        CHECK(m.parts == 1);
        CHECK(m.mesh.triangle_count() == 12); // the support object is left out
        check_box(m, 90.0, 90.0);             // the build item's transform; mirrored, still outward
    }
    // In inches: 25.4 times (FM3).
    check_box(read_model(kFormats / "box_inch.3mf"), 90.0 * 25.4, 90.0 * 25.4, 508.0, 254.0);
    // An object of two parts, as components (FM9).
    const ModelFile parts = read_model(kFormats / "box_parts.3mf");
    CHECK(parts.parts == 2);
    CHECK(parts.mesh.triangle_count() == 24);
    check_box(parts, 90.0, 90.0);
    // A slicer project's modifier volume is not printed.
    const ModelFile modifier = read_model(kFormats / "box_modifier.3mf");
    CHECK(modifier.mesh.triangle_count() == 12);
    CHECK(modifier.parts == 1);
}

TEST_CASE("formats: damaged 3MF packages are refused, never crash", "[formats]") {
    const std::string good = file(kFormats / "box.3mf");
    CHECK_THROWS_AS(parse_3mf(good.substr(0, good.size() - 30)), ModelError);
    CHECK_THROWS_AS(parse_model("not a package", ".3mf"), ModelError);
    std::mt19937 rng(5);
    int refused = 0;
    for (int run = 0; run < 400; ++run) {
        std::string bad = good;
        for (int k = 0; k < 1 + run % 4; ++k)
            bad[rng() % bad.size()] = char(rng() & 0xFF);
        try {
            const ModelFile m = parse_3mf(bad);
            CHECK(m.mesh.triangle_count() > 0);
        } catch (const ModelError&) {
            ++refused;
        }
    }
    CHECK(refused > 0);
    // An entry that says it inflates to 4 GB: refused before anything is allocated.
    std::string huge = good;
    for (std::size_t at = huge.find("PK\x01\x02"); at != std::string::npos; at = huge.find("PK\x01\x02", at + 4))
        if (huge.compare(at + 46, 16, "3D/3dmodel.model") == 0)
            for (int k = 0; k < 4; ++k)
                huge[at + 24 + std::size_t(k)] = char(k == 0 ? 0xF0 : 0xFF);
    CHECK_THROWS_WITH(parse_3mf(huge), ContainsSubstring("more than deflate can hold"));
}

TEST_CASE("formats: STL, OBJ or 3MF by the content and the extension", "[formats]") {
    const std::string cube = file(std::filesystem::path(TISMA_TEST_DATA_DIR) / "cube_ascii.stl");
    CHECK(parse_model(cube, ".stl").format == ModelFormat::StlAscii);
    CHECK(parse_model(cube, "").format == ModelFormat::StlAscii);
    CHECK(parse_model(file(kFormats / "box.3mf"), ".stl").format == ModelFormat::ThreeMf); // a ZIP is a 3MF
    CHECK(parse_model(file(kFormats / "box.obj"), ".OBJ").format == ModelFormat::Obj);
    CHECK_THROWS_AS(read_model(kFormats / "missing.3mf"), ModelError);
    CHECK(std::string(format_name(ModelFormat::ThreeMf)) == "3MF");
    // The formats not read yet say so, not that an STL is broken (the spec team's BH9).
    CHECK_THROWS_WITH(parse_model("<?xml version=\"1.0\"?><amf unit=\"millimeter\"></amf>", ".amf"),
                      ContainsSubstring("AMF files are not supported yet"));
    CHECK_THROWS_WITH(parse_model(file(kFormats / "box.3mf"), ".AMF"), ContainsSubstring("AMF files")); // zipped
    CHECK_THROWS_WITH(parse_model("ISO-10303-21;", ".step"), ContainsSubstring("STEP files are not supported yet"));
    CHECK_THROWS_WITH(parse_model("ISO-10303-21;", ".stp"), ContainsSubstring("STEP files"));
}

TEST_CASE("formats: XML elements, attributes and references", "[formats]") {
    XmlReader xml("<?xml version=\"1.0\"?><!-- c --><!DOCTYPE m [<!ENTITY x \"y\">]>"
                  "<a p:k='1 &amp; 2'><b v=\"&#65;&#x42;\"/><![CDATA[<c>]]></a>");
    REQUIRE(xml.next() == XmlReader::Event::Start);
    CHECK(xml.name() == "a");
    CHECK(XmlReader::decode(*xml.attribute("k")) == "1 & 2");
    REQUIRE(xml.next() == XmlReader::Event::Start);
    CHECK(XmlReader::decode(*xml.attribute("v")) == "AB");
    CHECK(xml.next() == XmlReader::Event::End);
    CHECK(xml.next() == XmlReader::Event::End);
    CHECK(xml.next() == XmlReader::Event::Done);
    for (const char* bad : {"<a>", "<a b>", "<a b=c/>", "</a>", "<a b='1/>"}) {
        CAPTURE(bad);
        XmlReader r(bad);
        CHECK_THROWS_AS([&] { while (r.next() != XmlReader::Event::Done) {} }(), XmlError);
    }
    CHECK_THROWS_AS(XmlReader::decode("&nope;"), XmlError);
}

TEST_CASE("placement: the model's box centred on the bed, or on placement.center (PL1, PL2, PL6)", "[formats]") {
    ModelFile m = read_model(kFormats / "box.obj"); // 0 to 20
    Settings s;
    place_on_bed(m.mesh, s); // no bed: stays
    CHECK(m.mesh.bounding_box().min.x == 0.0f);
    s.bed = {{0, 0}, {200, 0}, {200, 200}, {0, 200}};
    place_on_bed(m.mesh, s);
    CHECK_THAT(m.mesh.bounding_box().min.x, WithinAbs(90.0, 1e-4));
    CHECK_THAT(m.mesh.bounding_box().min.y, WithinAbs(90.0, 1e-4));
    CHECK(m.mesh.bounding_box().min.z == 0.0f); // Z is the slicer's
    s.placement_center = std::array<double, 2>{50.0, 60.0};
    place_on_bed(m.mesh, s);
    CHECK_THAT(m.mesh.bounding_box().min.x, WithinAbs(40.0, 1e-4));
    CHECK_THAT(m.mesh.bounding_box().max.y, WithinAbs(70.0, 1e-4));
    s.placement_arrange = false; // stays where it is (PL3)
    s.placement_center.reset();
    place_on_bed(m.mesh, s);
    CHECK_THAT(m.mesh.bounding_box().min.x, WithinAbs(40.0, 1e-4));
}

TEST_CASE("placement: without a bed it stays, unless a centre is given (PL15)", "[formats]") {
    ModelFile m = read_model(kFormats / "box.obj"); // 20 mm, from 0 to 20
    m.mesh.translate(10.0f, 10.0f, 0.0f);            // 10 to 30, as in the test
    Settings s;                                      // no machine.bed
    place_on_bed(m.mesh, s);
    CHECK_THAT(m.mesh.bounding_box().min.x, WithinAbs(10.0, 1e-4)); // Tisma Slicer: 90, on its default bed
    s.placement_center = std::array<double, 2>{50.0, 50.0};
    s.placement_arrange = false;
    place_on_bed(m.mesh, s);
    CHECK_THAT(m.mesh.bounding_box().min.x, WithinAbs(10.0, 1e-4)); // placement.arrange "no"
    s.placement_arrange = true;
    place_on_bed(m.mesh, s);
    CHECK_THAT(m.mesh.bounding_box().min.x, WithinAbs(40.0, 1e-4));
    CHECK_THAT(m.mesh.bounding_box().max.y, WithinAbs(60.0, 1e-4));
}
