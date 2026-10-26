#pragma once

#include <map>
#include <string>
#include <string_view>

// The interface's help: what each setting does, read from docs/CONFIG.md (the
// interface carries a copy), for the info button beside each setting. Own
// design; see PROVENANCE.md.
namespace tisma::ui {

struct SettingDoc {
    std::string key; // "group.key"
    std::string type, default_value, range, meaning; // plain text; empty when the table has no such column
};

// Every row of the settings tables of the markdown: a "### `group`" heading,
// then a table whose header names the columns (Clave, Tipo, Por defecto,
// Rango, Significado) and whose rows start with "| `key` |" (or several:
// "| `a`, `b` |"). Marks are taken out of the text (see plain_text).
std::map<std::string, SettingDoc> parse_setting_docs(std::string_view markdown);

// Markdown's inline marks taken out: `code` and **bold** keep their text,
// [text](link) keeps the text.
std::string plain_text(std::string_view markdown);

} // namespace tisma::ui
