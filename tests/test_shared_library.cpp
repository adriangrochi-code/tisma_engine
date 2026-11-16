// The shared library's signed files (src/library/shared_index): the library
// key certified by the project's key, the block list and the index signed by
// it (tests/data/library, made by its make.sh with OpenSSL 3), the search
// without accents, and what is refused (p2p_library.md, rules 3 bis to 4).
#include "library/shared_index.hpp"
#include "net/ed25519.hpp"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <sstream>

using namespace tisma::library;

namespace {
std::string file(const std::string& name) {
    std::ifstream in(std::filesystem::path(TISMA_TEST_DATA_DIR) / "library" / name, std::ios::binary);
    std::stringstream s;
    s << in.rdbuf();
    return s.str();
}
std::vector<std::uint8_t> raw(const std::string& s) { return {s.begin(), s.end()}; }
} // namespace

TEST_CASE("shared library: the key, the block list and the index, signed and checked", "[library][ed25519]") {
    const auto project = tisma::net::key_bytes(file("project_key.hex"));
    REQUIRE(project.size() == 32);
    const auto key = read_library_key(file("key.json"), project);
    REQUIRE(key.size() == 32);
    // Another project key: the certificate does not match.
    auto other = project;
    other[0] ^= 1;
    CHECK_THROWS_AS(read_library_key(file("key.json"), other), SharedError);

    const BlockList list = read_block_list(file("blocklist.json"), raw(file("blocklist.json.sig")), key);
    CHECK(list.sequence == 3);
    CHECK(list.blocks_file("CC33"));
    // A replayed older list, and a changed one.
    CHECK_THROWS_AS(read_block_list(file("blocklist.json"), raw(file("blocklist.json.sig")), key, 4), SharedError);
    std::string changed = file("blocklist.json");
    changed[changed.find("copyright")] = 'C';
    CHECK_THROWS_AS(read_block_list(changed, raw(file("blocklist.json.sig")), key), SharedError);

    const SharedIndex index = read_index(file("index.json"), raw(file("index.json.sig")), key);
    CHECK(index.sequence == 7);
    REQUIRE(index.models.size() == 4);
    CHECK(index.models[0].author == "Ana");
    REQUIRE(index.models[0].project() != nullptr);
    CHECK(index.models[0].project()->sha256 == "aa11");
    CHECK(index.models[3].has_gcode());
    CHECK_THROWS_AS(read_index(file("index.json"), raw(file("index.json.sig")), key, 8), SharedError);
    CHECK_THROWS_AS(read_index(file("index.json"), raw(file("blocklist.json.sig")), key), SharedError);

    // The search: every word, anywhere, without accents or capitals; the
    // blocked file's model and the one with G-code are never offered.
    CHECK(index.search("", &list) == std::vector<std::size_t>{0, 1});
    CHECK(index.search("MECANICA", &list) == std::vector<std::size_t>{0});
    CHECK(index.search("telefono", &list) == std::vector<std::size_t>{1});
    CHECK(index.search("soporte bruno", &list) == std::vector<std::size_t>{1});
    CHECK(index.search("soporte ana", &list).empty());
    CHECK(index.search("bloqueada", &list).empty());
    CHECK(index.search("bloqueada", nullptr) == std::vector<std::size_t>{2});
    CHECK(fold("Ñandú ÁRBOL") == "nandu arbol");
}
