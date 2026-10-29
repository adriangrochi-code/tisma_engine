#include "ui/credits.hpp"

#include "ui/text.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>

namespace tisma::ui {
namespace {

using nlohmann::json;

std::string text_of(const json& object, const char* key) {
    const auto it = object.find(key);
    if (it == object.end())
        return {};
    if (!it->is_string())
        throw CreditsError(std::string("\"") + key + "\" is not a text");
    return it->get<std::string>();
}

Localized localized_of(const json& object, const char* key) {
    const auto it = object.find(key);
    if (it == object.end())
        return {};
    if (it->is_string())
        return {it->get<std::string>(), it->get<std::string>()};
    if (!it->is_object())
        throw CreditsError(std::string("\"") + key + "\" is neither a text nor {\"es\", \"en\"}");
    Localized out{text_of(*it, "es"), text_of(*it, "en")};
    if (out.en.empty())
        out.en = out.es;
    return out;
}

std::vector<std::string> texts_of(const json& object, const char* key) {
    std::vector<std::string> out;
    const auto it = object.find(key);
    if (it == object.end())
        return out;
    if (!it->is_array())
        throw CreditsError(std::string("\"") + key + "\" is not a list");
    for (const json& item : *it) {
        if (!item.is_string())
            throw CreditsError(std::string("\"") + key + "\" holds something that is not a text");
        out.push_back(item.get<std::string>());
    }
    return out;
}

std::string_view trim(std::string_view s) {
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t'))
        s.remove_prefix(1);
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r'))
        s.remove_suffix(1);
    return s;
}

// The cells of a Markdown table's row, trimmed.
std::vector<std::string> cells_of(std::string_view line) {
    std::vector<std::string> cells;
    line = trim(line);
    if (line.starts_with('|'))
        line.remove_prefix(1);
    if (line.ends_with('|'))
        line.remove_suffix(1);
    std::size_t start = 0;
    for (std::size_t i = 0; i <= line.size(); ++i)
        if (i == line.size() || line[i] == '|') {
            cells.emplace_back(trim(line.substr(start, i - start)));
            start = i + 1;
        }
    return cells;
}

} // namespace

const std::string& Localized::now() const { return language() == Language::English && !en.empty() ? en : es; }

Credits parse_credits(std::string_view text) {
    json root;
    try {
        root = json::parse(text);
    } catch (const json::exception&) {
        throw CreditsError("not a JSON file");
    }
    if (!root.is_object() || !root.contains("format") || root.at("format") != "tisma-credits")
        throw CreditsError("not a credits file (format tisma-credits)");
    if (!root.contains("version") || !root.at("version").is_number_integer() || root.at("version").get<long>() != 1)
        throw CreditsError("credits of another version");
    Credits credits;
    try {
        for (const json& f : root.value("features", json::array())) {
            FeatureCredit c;
            c.feature = localized_of(f, "feature");
            c.spec = text_of(f, "spec");
            c.source = text_of(f, "source");
            c.short_source = text_of(f, "short");
            c.author = text_of(f, "author");
            c.license = localized_of(f, "license");
            c.url = text_of(f, "url");
            c.cards = texts_of(f, "cards");
            if (c.source.empty())
                throw CreditsError("a function's credit without its source");
            credits.features.push_back(std::move(c));
        }
        for (const json& l : root.value("libraries", json::array())) {
            LibraryCredit c;
            c.name = text_of(l, "name");
            c.version = text_of(l, "version");
            c.license = text_of(l, "license");
            c.url = text_of(l, "url");
            c.notices = texts_of(l, "notices");
            if (c.name.empty())
                throw CreditsError("a library without its name");
            credits.libraries.push_back(std::move(c));
        }
    } catch (const json::exception& e) {
        throw CreditsError(e.what());
    }
    return credits;
}

std::vector<const FeatureCredit*> credits_for_card(const Credits& credits, std::string_view card) {
    std::vector<const FeatureCredit*> out;
    for (const FeatureCredit& c : credits.features)
        if (std::find(c.cards.begin(), c.cards.end(), card) != c.cards.end())
            out.push_back(&c);
    return out;
}

const std::string& card_source(const FeatureCredit& credit) {
    return credit.short_source.empty() ? credit.source : credit.short_source;
}

std::string based_on(const FeatureCredit& credit) {
    std::string out = std::string(tr(T::BasedOn)) + " " + card_source(credit);
    if (!credit.author.empty())
        out += std::string(tr(T::BasedOnBy)) + credit.author;
    return out;
}

std::vector<ThirdPartyRow> third_party_rows(std::string_view markdown) {
    std::vector<ThirdPartyRow> rows;
    bool in_table = false;
    std::size_t start = 0;
    while (start <= markdown.size()) {
        std::size_t end = markdown.find('\n', start);
        if (end == std::string_view::npos)
            end = markdown.size();
        const std::string_view line = trim(markdown.substr(start, end - start));
        start = end + 1;
        if (!in_table) {
            in_table = line.starts_with("| Biblioteca |");
            continue;
        }
        if (!line.starts_with('|'))
            break; // the table ended
        const std::vector<std::string> cells = cells_of(line);
        if (cells.size() < 7 || cells[0].starts_with("---"))
            continue;
        rows.push_back({cells[0], cells[2], cells[3], cells[5], !cells[6].starts_with("Solo pruebas")});
    }
    return rows;
}

std::vector<std::string> based_on_urls(std::string_view spec) {
    std::vector<std::string> urls;
    const std::size_t at = spec.find("**Basado en:**");
    if (at == std::string_view::npos)
        return urls;
    // The paragraph: up to the first blank line.
    std::size_t end = spec.find("\n\n", at);
    if (end == std::string_view::npos)
        end = spec.size();
    const std::string_view paragraph = spec.substr(at, end - at);
    for (std::size_t i = paragraph.find("http"); i != std::string_view::npos; i = paragraph.find("http", i)) {
        std::size_t j = i;
        while (j < paragraph.size() && paragraph[j] != ' ' && paragraph[j] != '\n' && paragraph[j] != '`' &&
               paragraph[j] != ')' && paragraph[j] != '>')
            ++j;
        std::string_view url = paragraph.substr(i, j - i);
        while (!url.empty() && (url.back() == '.' || url.back() == ','))
            url.remove_suffix(1);
        if (url.starts_with("http://") || url.starts_with("https://"))
            urls.emplace_back(url);
        i = j;
    }
    return urls;
}

} // namespace tisma::ui
