// The settings' help from docs/CONFIG.md. Own design; see PROVENANCE.md.

#include "ui/help.hpp"

#include <algorithm>
#include <vector>

namespace tisma::ui {

namespace {

std::string_view trim(std::string_view s) {
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t'))
        s.remove_prefix(1);
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r'))
        s.remove_suffix(1);
    return s;
}

// The cells of a table row "| a | b |" (a "\|" inside a cell is a bar).
std::vector<std::string> cells(std::string_view line) {
    std::vector<std::string> out;
    line = trim(line);
    if (line.empty() || line.front() != '|')
        return out;
    line.remove_prefix(1);
    std::string cell;
    for (std::size_t i = 0; i < line.size(); ++i) {
        if (line[i] == '\\' && i + 1 < line.size() && line[i + 1] == '|') {
            cell += '|';
            ++i;
        } else if (line[i] == '|') {
            out.emplace_back(trim(cell));
            cell.clear();
        } else {
            cell += line[i];
        }
    }
    return out;
}

} // namespace

std::string plain_text(std::string_view md) {
    std::string out;
    for (std::size_t i = 0; i < md.size(); ++i) {
        const char c = md[i];
        if (c == '`')
            continue;
        if (c == '*' && i + 1 < md.size() && md[i + 1] == '*') {
            ++i;
            continue;
        }
        if (c == '[') { // [text](link): the text
            const std::size_t close = md.find("](", i);
            const std::size_t end = close == std::string_view::npos ? close : md.find(')', close);
            if (end != std::string_view::npos) {
                out += plain_text(md.substr(i + 1, close - i - 1));
                i = end;
                continue;
            }
        }
        out += c;
    }
    return out;
}

std::map<std::string, SettingDoc> parse_setting_docs(std::string_view md) {
    std::map<std::string, SettingDoc> out;
    std::string group;
    std::vector<std::string> header;
    std::size_t at = 0;
    while (at <= md.size()) {
        const std::size_t end = std::min(md.find('\n', at), md.size());
        const std::string_view line = md.substr(at, end - at);
        at = end + 1;
        if (line.rfind("#", 0) == 0) {
            // "### `group`" names the group of the tables under it; other headings end it.
            const std::size_t open = line.find('`'), close = open == std::string_view::npos ? open : line.find('`', open + 1);
            group = line.rfind("### ", 0) == 0 && close != std::string_view::npos
                        ? std::string(line.substr(open + 1, close - open - 1))
                        : std::string();
            header.clear();
            continue;
        }
        const std::vector<std::string> row = cells(line);
        if (row.empty()) {
            header.clear();
            continue;
        }
        if (row[0] == "Clave") {
            header = row;
            continue;
        }
        if (header.empty() || group.empty() || row[0].size() < 3 || row[0].front() != '`' || row[0].back() != '`')
            continue; // the separator row, or a table of something else
        SettingDoc doc;
        for (std::size_t k = 1; k < row.size() && k < header.size(); ++k) {
            const std::string text = plain_text(row[k]);
            if (header[k] == "Tipo")
                doc.type = text;
            else if (header[k] == "Por defecto")
                doc.default_value = text;
            else if (header[k] == "Rango")
                doc.range = text;
            else if (header[k] == "Significado")
                doc.meaning = text;
        }
        // A row may name several keys: "`start_gcode`, `end_gcode`".
        for (std::size_t open = row[0].find('`'); open != std::string::npos;) {
            const std::size_t close = row[0].find('`', open + 1);
            if (close == std::string::npos)
                break;
            doc.key = group + "." + row[0].substr(open + 1, close - open - 1);
            out[doc.key] = doc;
            open = row[0].find('`', close + 1);
        }
    }
    return out;
}

} // namespace tisma::ui
