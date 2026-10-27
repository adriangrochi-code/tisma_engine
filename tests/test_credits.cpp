#include "ui/credits.hpp"

#include "ui/settings_layout.hpp"
#include "ui/text.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>
#include <string>

using namespace tisma::ui;

namespace {

const std::filesystem::path kSource = TISMA_SOURCE_DIR;

std::string read_file(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    REQUIRE(in);
    std::ostringstream text;
    text << in.rdbuf();
    return text.str();
}

const Credits& credits() {
    static const Credits c = parse_credits(read_file(kSource / "resources" / "credits.json"));
    return c;
}

} // namespace

TEST_CASE("credits: the data file reads, and a card shows its sources", "[ui][credits]") {
    const Credits& c = credits();
    CHECK(c.features.size() >= 11);
    CHECK(c.libraries.size() >= 10);
    const std::vector<const FeatureCredit*> curved = credits_for_card(c, "curved");
    REQUIRE(curved.size() == 1);
    CHECK(curved.front()->source.starts_with("CurviSlicer"));
    CHECK(credits_for_card(c, "seam").empty());
    // Every card named is one the panel has.
    std::set<std::string> cards;
    for (const SettingsTab tab : {SettingsTab::Print, SettingsTab::Material, SettingsTab::Printer})
        for (const SectionLayout& section : settings_layout(tab))
            for (const SubsectionLayout& sub : section.subsections)
                cards.insert(std::string(sub.id));
    for (const FeatureCredit& f : c.features) {
        INFO(f.source);
        CHECK(f.url.starts_with("https://"));
        CHECK_FALSE(f.feature.es.empty());
        CHECK_FALSE(f.feature.en.empty());
        for (const std::string& card : f.cards)
            CHECK(cards.contains(card));
    }
}

TEST_CASE("credits: every spec's «Basado en» has its credit (CR3)", "[ui][credits]") {
    const Credits& c = credits();
    const auto has_spec = [&](const std::string& spec) {
        return std::any_of(c.features.begin(), c.features.end(), [&](const FeatureCredit& f) { return f.spec == spec; });
    };
    int lines = 0;
    for (const auto& entry : std::filesystem::directory_iterator(kSource / "docs" / "spec" / "features")) {
        const std::string name = entry.path().filename().string();
        if (entry.path().extension() != ".md" || name == "credits.md")
            continue;
        const std::vector<std::string> urls = based_on_urls(read_file(entry.path()));
        if (urls.empty())
            continue;
        ++lines;
        INFO("spec " << name << " has a «Basado en» line without its credit in resources/credits.json");
        CHECK(has_spec(name));
        for (const std::string& url : urls)
            CHECK(std::any_of(c.features.begin(), c.features.end(),
                              [&](const FeatureCredit& f) { return f.spec == name && f.url == url; }));
    }
    CHECK(lines >= 1); // support_fins.md
    // The credits.md table's rows, each one by its spec.
    const std::string table = read_file(kSource / "docs" / "spec" / "features" / "credits.md");
    std::istringstream in(table);
    int rows = 0;
    for (std::string line; std::getline(in, line);) {
        // | Función | `spec.md` (...) | ...: the second cell starts with the spec.
        const std::size_t cell = line.find(" | `");
        if (!line.starts_with("| ") || cell == std::string::npos || line.find('|', 2) != cell + 1)
            continue;
        const std::size_t open = cell + 3;
        const std::string spec = line.substr(open + 1, line.find('`', open + 1) - open - 1);
        if (!spec.ends_with(".md"))
            continue;
        INFO("credits.md names " << spec << " without its credit in resources/credits.json");
        CHECK(has_spec(spec));
        ++rows;
    }
    CHECK(rows >= 11);
}

TEST_CASE("credits: every library the program carries has its license and notice (CR2)", "[ui][credits]") {
    const Credits& c = credits();
    const std::vector<ThirdPartyRow> rows = third_party_rows(read_file(kSource / "THIRD_PARTY.md"));
    REQUIRE(rows.size() >= 11);
    CHECK_FALSE(std::find_if(rows.begin(), rows.end(), [](const ThirdPartyRow& r) { return r.name == "Catch2"; })->distributed);
    for (const ThirdPartyRow& row : rows) {
        INFO("THIRD_PARTY.md: " << row.name);
        const auto lib = std::find_if(c.libraries.begin(), c.libraries.end(),
                                      [&](const LibraryCredit& l) { return l.name == row.name; });
        if (!row.distributed) {
            CHECK(lib == c.libraries.end());
            continue;
        }
        REQUIRE(lib != c.libraries.end());
        CHECK(lib->license == row.license);
        CHECK(lib->url == row.url);
        CHECK(row.version.find(lib->version) != std::string::npos);
        REQUIRE_FALSE(lib->notices.empty());
        for (const std::string& notice : lib->notices) {
            INFO(notice);
            CHECK(std::filesystem::is_regular_file(kSource / "resources" / "licenses" / notice));
        }
    }
    for (const LibraryCredit& lib : c.libraries) {
        INFO(lib.name << " is in resources/credits.json but not in THIRD_PARTY.md");
        CHECK(std::any_of(rows.begin(), rows.end(), [&](const ThirdPartyRow& r) { return r.name == lib.name; }));
    }
}

TEST_CASE("credits: «Basado en» in each language (CR1, CR4)", "[ui][credits]") {
    const Language before = language();
    const auto fins = std::find_if(credits().features.begin(), credits().features.end(),
                                   [](const FeatureCredit& f) { return f.spec == "support_fins.md"; });
    REQUIRE(fins != credits().features.end());
    set_language(Language::Spanish);
    CHECK(based_on(*fins) == "Basado en Support Fins, de gittrahan");
    CHECK(fins->feature.now() == "Aletas de soporte");
    set_language(Language::English);
    CHECK(based_on(*fins) == "Based on Support Fins, by gittrahan");
    CHECK(fins->feature.now() == "Support fins");
    const auto song = std::find_if(credits().features.begin(), credits().features.end(),
                                   [](const FeatureCredit& f) { return f.spec == "z_antialiasing.md"; });
    REQUIRE(song != credits().features.end());
    CHECK(based_on(*song) == "Based on FFF anti-aliasing (2017), by Song et al.");
    CHECK(card_source(*fins) == "Support Fins");
    FeatureCredit anonymous;
    anonymous.source = "Something";
    CHECK(based_on(anonymous) == "Based on Something");
    set_language(before);
}

TEST_CASE("credits: what is not a credits file", "[ui][credits]") {
    CHECK_THROWS_AS(parse_credits("not json"), CreditsError);
    CHECK_THROWS_AS(parse_credits("{}"), CreditsError);
    CHECK_THROWS_AS(parse_credits(R"({"format": "tisma-credits", "version": 2})"), CreditsError);
    CHECK_THROWS_AS(parse_credits(R"({"format": "tisma-credits", "version": 1, "features": [{"source": 3}]})"), CreditsError);
    CHECK_THROWS_AS(parse_credits(R"({"format": "tisma-credits", "version": 1, "features": [{"url": "x"}]})"), CreditsError);
    const Credits empty = parse_credits(R"({"format": "tisma-credits", "version": 1})");
    CHECK(empty.features.empty());
    CHECK(empty.libraries.empty());
}

TEST_CASE("credits: THIRD_PARTY.md's table and a spec's «Basado en» line read", "[ui][credits]") {
    const std::vector<ThirdPartyRow> rows = third_party_rows("# T\n\n| Biblioteca | CMake | Versión | Licencia | Leída en | URL | Uso |\n"
                                                             "|---|---|---|---|---|---|---|\n"
                                                             "| A | a | 1.0 (commit x) | MIT | LICENSE | https://a | Solo pruebas. |\n"
                                                             "| B | b | 2 | Zlib | LICENSE | https://b | Todo. |\n\n"
                                                             "| Herramienta | Versión |\n|---|---|\n| C | 3 |\n");
    REQUIRE(rows.size() == 2);
    CHECK_FALSE(rows[0].distributed);
    CHECK(rows[1].name == "B");
    CHECK(rows[1].version == "2");
    CHECK(rows[1].license == "Zlib");
    CHECK(rows[1].url == "https://b");
    CHECK(rows[1].distributed);
    CHECK(based_on_urls("# X\n\n**Basado en:** Y, de Z (MIT), `https://y.org/z`. Más\ntexto https://w.org.\n\nOtro https://no.org") ==
          std::vector<std::string>{"https://y.org/z", "https://w.org"});
    CHECK(based_on_urls("# X\n\n**Fuentes:** https://a.org\n").empty());
}
