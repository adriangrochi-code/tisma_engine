// .xz and tar reading for the profile pack. The files in tests/data/xz were
// made from invented data with the xz command (tests/data/xz/make_xz_data.py).
#include "catalog/pack.hpp"
#include "catalog/xz.hpp"
#include "gcode/binary.hpp"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <sstream>

using namespace tisma;

namespace {

std::string load(const char* name) {
    std::ifstream in(std::filesystem::path(TISMA_TEST_DATA_DIR) / "xz" / name, std::ios::binary);
    std::ostringstream s;
    s << in.rdbuf();
    return s.str();
}

struct Case {
    const char* name;
    std::size_t size;
    const char* sha256;
};

const Case kCases[] = {
    {"hello.xz", 13, "ef991afb5194c4d213aef4ec2d292946c266b5b821b13dcba7587f4ef3dc2d1a"},
    {"words.xz", 200000, "6a1ea61139a200e4f510efedd8d0536be47d527908373f6cd9e6625d739e8a35"},
    {"words_lp2.xz", 200000, "6a1ea61139a200e4f510efedd8d0536be47d527908373f6cd9e6625d739e8a35"},
    {"words_blocks.xz", 200000, "6a1ea61139a200e4f510efedd8d0536be47d527908373f6cd9e6625d739e8a35"},
    {"random.xz", 70000, "302b6a5ffdf70ade85e18e8e59593405dee4cbea1e9f22ac9321c8ce09d63bac"},
    {"multi.xz", 5013, "589a6e69cf39101001e0668ec9dfd16de410d94dd677550dadc6846050b6d750"},
    {"pax.tar.xz", 10240, "1732516a24ef79c7c2e277e22c323b8110b7e9f768cb9a59b1c0de79026d7e87"},
    {"gnu.tar.xz", 10240, "d09a88e726e180ce488d5131d2a12e76431cee6f990ddaad1d5c64241c610393"},
};

} // namespace

TEST_CASE("xz: CRC-32 and CRC-64 check values", "[xz]") {
    CHECK(crc32("123456789") == 0xCBF43926u);
    CHECK(crc64("123456789") == 0x995DC9BBDF1939FAull);
}

TEST_CASE("xz: files from the xz command decompress to the same bytes", "[xz]") {
    for (const Case& c : kCases) {
        INFO(c.name);
        const std::string packed = load(c.name);
        REQUIRE(!packed.empty());
        std::string out, error;
        REQUIRE(xz_decompress(packed, out, error));
        CHECK(error.empty());
        CHECK(out.size() == c.size);
        CHECK(sha256_hex(out) == c.sha256);
    }
    std::string out, error;
    REQUIRE(xz_decompress(load("hello.xz"), out, error));
    CHECK(out == "hello, tisma\n");
}

TEST_CASE("xz: damaged, cut or too large data stops with an error", "[xz]") {
    const std::string good = load("words.xz");
    std::string out, error;
    // A byte changed in the middle of the compressed data.
    std::string bad = good;
    bad[good.size() / 2] = char(bad[good.size() / 2] ^ 0x5A);
    CHECK_FALSE(xz_decompress(bad, out, error));
    CHECK(!error.empty());
    // Cut short.
    error.clear();
    CHECK_FALSE(xz_decompress(std::string_view(good).substr(0, good.size() - 20), out, error));
    CHECK(!error.empty());
    // Not xz at all.
    CHECK_FALSE(xz_decompress("{\"format\": 1}", out, error));
    // More output than allowed.
    error.clear();
    CHECK_FALSE(xz_decompress(good, out, error, 1000));
    CHECK(error == "uncompressed data larger than allowed");
    // Stream padding that is not a multiple of four.
    CHECK_FALSE(xz_decompress(load("hello.xz") + std::string(3, '\0'), out, error));
    CHECK(xz_decompress(load("hello.xz") + std::string(4, '\0'), out, error));
}

TEST_CASE("tar: pax and GNU archives give the files, long names whole, links skipped", "[xz]") {
    for (const char* name : {"pax.tar.xz", "gnu.tar.xz"}) {
        INFO(name);
        std::string tar, error;
        REQUIRE(xz_decompress(load(name), tar, error));
        std::vector<TarEntry> files;
        std::vector<std::string> skipped;
        REQUIRE(tar_read(tar, files, skipped, error));
        REQUIRE(files.size() == 3);
        CHECK(files[0].path == "vendors/acme/models/one.json");
        CHECK(files[0].data == "{\"a\": 1}\n");
        std::string long_name = "vendors/acme/filaments/";
        for (int i = 0; i < 12; ++i) long_name += "long_name_";
        CHECK(files[1].path == long_name + ".json");
        CHECK(files[1].data == "long\n");
        CHECK(files[2].path == "community/printers/Acme/One.json");
        CHECK(files[2].data.empty());
        REQUIRE(skipped.size() == 1);
        CHECK(skipped[0] == "vendors/acme/link");
        // A changed header byte breaks its checksum.
        std::string bad = tar;
        bad[10] = char(bad[10] ^ 1);
        files.clear();
        CHECK_FALSE(tar_read(bad, files, skipped, error));
    }
}

TEST_CASE("xz and tar: any changed byte gives an error or other bytes, never a crash", "[xz]") {
    for (const char* name : {"words_lp2.xz", "random.xz", "pax.tar.xz"}) {
        const std::string good = load(name);
        std::uint32_t x = 99;
        for (int round = 0; round < 300; ++round) {
            x = x * 1664525u + 1013904223u;
            std::string bad = good;
            const std::size_t at = (x >> 8) % bad.size();
            bad[at] = char(bad[at] ^ (1 + (x & 0x7F)));
            std::string out, error;
            if (!xz_decompress(bad, out, error)) {
                CHECK(!error.empty());
                continue;
            }
            std::vector<TarEntry> files;
            std::vector<std::string> skipped;
            (void)tar_read(out, files, skipped, error);
        }
    }
}
