#include "config/settings.hpp"
#include "ui/help.hpp"

#include <catch2/catch_test_macros.hpp>

#include <fstream>
#include <sstream>

using namespace tisma;
using namespace tisma::ui;

TEST_CASE("help: every setting the engine writes has its explanation in docs/CONFIG.md", "[ui][help]") {
    std::ifstream in(std::string(TISMA_DOCS_DIR) + "/CONFIG.md", std::ios::binary);
    REQUIRE(in);
    std::ostringstream text;
    text << in.rdbuf();
    const auto docs = parse_setting_docs(text.str());
    CHECK(docs.size() > 60);
    for (const auto& [key, value] : describe(Settings{})) {
        CAPTURE(key);
        if (key.rfind("changes", 0) == 0 || key.rfind("machine.variables", 0) == 0)
            continue; // lists, documented apart
        const auto it = docs.find(key);
        REQUIRE(it != docs.end());
        CHECK_FALSE(it->second.meaning.empty());
    }
    const SettingDoc& h = docs.at("layers.height");
    CHECK(h.type == "número");
    CHECK_FALSE(h.default_value.empty());
}

TEST_CASE("help: tables and marks are read as plain text", "[ui][help]") {
    const char* md = "# Ajustes\n\n### `walls`\n\nTexto.\n\n"
                     "| Clave | Tipo | Por defecto | Rango | Significado |\n|---|---|---|---|---|\n"
                     "| `count` | entero | 3 | 0 a 10 | Paredes **desde** cada borde (`docs/X.md`, [ficha](https://x)) |\n"
                     "| `seam`, `side` | texto | `\"aligned\"` | a \\| b | Dónde empieza |\n\n"
                     "### `speed`\n\n| Clave | Tipo | Por defecto | Significado |\n|---|---|---|---|\n"
                     "| `walls` | número | 40 | Paredes |\n\n## Otra cosa\n\n| `nope` | x | y | z |\n";
    const auto docs = parse_setting_docs(md);
    REQUIRE(docs.size() == 4);
    CHECK(docs.at("walls.side").meaning == "Dónde empieza");
    CHECK(docs.at("walls.count").meaning == "Paredes desde cada borde (docs/X.md, ficha)");
    CHECK(docs.at("walls.count").range == "0 a 10");
    CHECK(docs.at("walls.seam").default_value == "\"aligned\"");
    CHECK(docs.at("walls.seam").range == "a | b");
    CHECK(docs.at("speed.walls").range.empty());
    CHECK(docs.at("speed.walls").default_value == "40");
    CHECK(docs.find("nope") == docs.end());
}
