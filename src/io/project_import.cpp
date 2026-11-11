#include "io/project_import.hpp"

#include "io/xml.hpp"
#include "io/zip.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <optional>
#include <sstream>
#include <utility>

namespace tisma {

namespace {

std::string lower(std::string_view text) {
    std::string out(text);
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) { return char(std::tolower(c)); });
    return out;
}

bool starts_with_folder(std::string_view name, std::string_view folder) {
    if (!name.empty() && name.front() == '/')
        name.remove_prefix(1);
    return name.size() > folder.size() && lower(name.substr(0, folder.size())) == folder;
}

// The value of <metadata name="Application"> in the model, looked for where
// the metadata is: before the resources, in the first part of the file; and
// the number of its build items.
std::string application_of(const ZipArchive& zip, int& items) {
    const ZipArchive::Entry* model = zip.find("3D/3dmodel.model");
    if (!model)
        for (const ZipArchive::Entry& e : zip.entries())
            if (starts_with_folder(e.name, "3d/") && lower(e.name).ends_with(".model")) {
                model = &e;
                break;
            }
    if (!model)
        return {};
    const std::string text = zip.read(*model);
    for (std::size_t at = text.find("<item"); at != std::string::npos; at = text.find("<item", at + 5))
        if (at + 5 < text.size() && (text[at + 5] == ' ' || text[at + 5] == '\t' || text[at + 5] == '\n' || text[at + 5] == '/'))
            ++items;
    const std::string_view head = std::string_view(text).substr(0, 1 << 16);
    const std::size_t key = head.find("name=\"Application\"");
    if (key == std::string_view::npos)
        return {};
    const std::size_t open = head.find('>', key), close = head.find('<', key);
    if (open == std::string_view::npos || close == std::string_view::npos || close < open)
        return {};
    return XmlReader::decode(head.substr(open + 1, close - open - 1));
}

} // namespace

ProjectSource project_source(std::string_view package) {
    const ZipArchive zip(package);
    ProjectSource out;
    if (zip.find("Metadata/Slic3r_PE.config") || zip.find("Metadata/Slic3r_PE_model.config"))
        out.family = ProjectFamily::PrusaSlicer;
    else if (zip.find("Metadata/project_settings.config") || zip.find("Metadata/model_settings.config"))
        out.family = ProjectFamily::OrcaSlicer;
    else if (std::any_of(zip.entries().begin(), zip.entries().end(),
                         [](const ZipArchive::Entry& e) { return starts_with_folder(e.name, "cura/"); }))
        out.family = ProjectFamily::Cura;
    out.application = application_of(zip, out.items);
    // A known program's name in the metadata, as the dialog shows it.
    static constexpr std::array<std::pair<std::string_view, std::string_view>, 9> known = {{
        {"tisma", "Tisma Slicer"},
        {"prusaslicer", "PrusaSlicer"},
        {"orcaslicer", "OrcaSlicer"},
        {"bambustudio", "Bambu Studio"},
        {"bambu studio", "Bambu Studio"},
        {"creality", "Creality Print"},
        {"elegoo", "Elegoo Slicer"},
        {"anycubic", "Anycubic Slicer Next"},
        {"cura", "Cura"},
    }};
    const std::string app = lower(out.application);
    for (const auto& [needle, name] : known)
        if (app.find(needle) != std::string::npos) {
            out.program = std::string(name);
            break;
        }
    if (out.program.empty())
        switch (out.family) {
        case ProjectFamily::PrusaSlicer: out.program = "PrusaSlicer"; break;
        case ProjectFamily::OrcaSlicer: out.program = "OrcaSlicer"; break;
        case ProjectFamily::Cura: out.program = "Cura"; break;
        case ProjectFamily::Standard: break;
        }
    return out;
}

TranslationTable parse_translation(std::string_view text) {
    using json = nlohmann::json;
    json j;
    try {
        j = json::parse(text);
    } catch (const json::exception& e) {
        throw TranslationError(std::string("translation table: ") + e.what());
    }
    if (!j.is_object() || j.value("format", "") != "tisma-pack-translation")
        throw TranslationError("translation table: \"format\" must be \"tisma-pack-translation\"");
    if (!j.contains("version") || !j.at("version").is_number_integer() || j.at("version").get<int>() > 1)
        throw TranslationError("translation table: a version this program does not read");
    TranslationTable out;
    out.dialect = j.value("dialect", "");
    if (!j.contains("kinds") || !j.at("kinds").is_object())
        return out;
    for (const auto& [kind, rows] : j.at("kinds").items()) {
        if (!rows.is_object())
            continue;
        auto& table = out.kinds[kind];
        for (const auto& [key, row] : rows.items()) {
            if (!row.is_object())
                continue;
            TranslationRow r;
            if (row.contains("engine") && row.at("engine").is_string())
                r.engine = row.at("engine").get<std::string>();
            const std::string group = row.contains("group") && row.at("group").is_string() ? row.at("group").get<std::string>() : "";
            r.group = group == "translated" ? SettingGroup::Translated
                      : group == "similar"  ? SettingGroup::Similar
                                            : SettingGroup::None;
            if (r.engine.empty())
                r.group = SettingGroup::None;
            const auto text = [](const json& v) {
                return v.is_string() ? v.get<std::string>() : v.is_null() ? std::string() : v.dump();
            };
            if (row.contains("conversion") && row.at("conversion").is_object()) {
                const json& c = row.at("conversion");
                r.how = c.contains("how") && c.at("how").is_string() ? c.at("how").get<std::string>() : "";
                if (c.contains("factor") && c.at("factor").is_number())
                    r.factor = c.at("factor").get<double>();
                if (c.contains("base") && c.at("base").is_string())
                    r.base = c.at("base").get<std::string>();
                if (c.contains("values") && c.at("values").is_object())
                    for (const auto& [from, to] : c.at("values").items())
                        r.values[from] = text(to);
            }
            if (row.contains("prusaslicer_values") && row.at("prusaslicer_values").is_object())
                for (const auto& [from, to] : row.at("prusaslicer_values").items())
                    r.renamed[from] = text(to);
            if (row.contains("percent_of") && row.at("percent_of").is_string())
                r.percent_of = row.at("percent_of").get<std::string>();
            table[key] = r;
            if (row.contains("with") && row.at("with").is_array())
                for (const json& w : row.at("with"))
                    if (w.is_string() && !table.count(w.get<std::string>()))
                        table[w.get<std::string>()] = r;
        }
    }
    return out;
}

std::vector<std::pair<std::string, std::string>> project_settings(std::string_view package) {
    const ZipArchive zip(package);
    std::vector<std::pair<std::string, std::string>> out;
    // The OrcaSlicer family: a JSON object of strings and lists of strings.
    if (const ZipArchive::Entry* orca = zip.find("Metadata/project_settings.config")) {
        const nlohmann::json j = nlohmann::json::parse(zip.read(*orca), nullptr, false);
        if (j.is_object())
            for (const auto& [key, value] : j.items()) {
                if (value.is_string()) {
                    out.emplace_back(key, value.get<std::string>());
                } else if (value.is_array()) {
                    std::string joined;
                    for (const nlohmann::json& v : value)
                        if (v.is_string() || v.is_number())
                            joined += (joined.empty() ? "" : ",") + (v.is_string() ? v.get<std::string>() : v.dump());
                    out.emplace_back(key, joined);
                } else if (value.is_number() || value.is_boolean()) {
                    out.emplace_back(key, value.dump());
                }
            }
        return out;
    }
    const ZipArchive::Entry* config = zip.find("Metadata/Slic3r_PE.config");
    if (!config)
        return out;
    const std::string text = zip.read(*config);
    std::size_t start = 0;
    while (start < text.size()) {
        std::size_t end = text.find('\n', start);
        if (end == std::string::npos)
            end = text.size();
        std::string_view line(text.data() + start, end - start);
        start = end + 1;
        if (!line.empty() && line.back() == '\r')
            line.remove_suffix(1);
        // "; key = value": a key of lowercase letters, digits and underscores.
        if (!line.starts_with("; "))
            continue;
        line.remove_prefix(2);
        const std::size_t eq = line.find(" = ");
        const std::size_t bare = line.size() >= 2 && line.ends_with(" =") ? line.size() - 2 : std::string_view::npos;
        const std::size_t cut = eq != std::string_view::npos ? eq : bare;
        if (cut == std::string_view::npos || cut == 0)
            continue;
        const std::string_view key = line.substr(0, cut);
        if (!std::all_of(key.begin(), key.end(), [](char c) { return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_'; }))
            continue;
        out.emplace_back(std::string(key), cut + 3 <= line.size() ? std::string(line.substr(cut + 3)) : std::string());
    }
    return out;
}

std::string translation_dialect(const ProjectSource& source) {
    const std::string app = lower(source.application + " " + source.program);
    switch (source.family) {
    case ProjectFamily::PrusaSlicer:
        return "prusaslicer";
    case ProjectFamily::OrcaSlicer:
        if (app.find("creality") != std::string::npos)
            return "crealityprint";
        if (app.find("elegoo") != std::string::npos)
            return "elegooslicer";
        return "orcaslicer";
    case ProjectFamily::Cura:
        return "cura";
    case ProjectFamily::Standard:
        break;
    }
    return "";
}

SettingSort sort_settings(const TranslationTable& table, const std::vector<std::pair<std::string, std::string>>& settings) {
    SettingSort out;
    // A project's settings are one list: look the key up in each kind, the
    // print's first.
    static constexpr std::array<std::string_view, 6> kOrder = {"print", "filament", "printer",
                                                               "resin_print", "resin", "resin_printer"};
    for (const auto& [key, value] : settings) {
        SettingGroup group = SettingGroup::None;
        bool found = false;
        for (const std::string_view kind : kOrder) {
            const auto k = table.kinds.find(std::string(kind));
            if (k == table.kinds.end())
                continue;
            if (const auto row = k->second.find(key); row != k->second.end()) {
                group = row->second.group, found = true;
                break;
            }
        }
        if (!found)
            for (const auto& [kind, rows] : table.kinds)
                if (const auto row = rows.find(key); row != rows.end()) {
                    group = row->second.group;
                    break;
                }
        (group == SettingGroup::Translated ? out.translated : group == SettingGroup::Similar ? out.similar : out.none) += 1;
        out.detail.emplace_back(key, group);
    }
    return out;
}

namespace {

const TranslationRow* row_of(const TranslationTable& table, const std::string& key) {
    static constexpr std::array<std::string_view, 6> kOrder = {"print", "filament", "printer",
                                                               "resin_print", "resin", "resin_printer"};
    for (const std::string_view kind : kOrder)
        if (const auto k = table.kinds.find(std::string(kind)); k != table.kinds.end())
            if (const auto row = k->second.find(key); row != k->second.end())
                return &row->second;
    for (const auto& [kind, rows] : table.kinds)
        if (const auto row = rows.find(key); row != rows.end())
            return &row->second;
    return nullptr;
}

std::string trimmed(std::string_view v) {
    while (!v.empty() && std::isspace(static_cast<unsigned char>(v.front())))
        v.remove_prefix(1);
    while (!v.empty() && std::isspace(static_cast<unsigned char>(v.back())))
        v.remove_suffix(1);
    return std::string(v);
}

std::optional<double> number_of(std::string_view v) {
    const std::string t = trimmed(v);
    if (t.empty())
        return std::nullopt;
    char* end = nullptr;
    const double d = std::strtod(t.c_str(), &end);
    if (end != t.c_str() + t.size() || !std::isfinite(d))
        return std::nullopt;
    return d;
}

// The JSON node of a dotted engine key in the settings' JSON, or null.
nlohmann::json* node_of(nlohmann::json& root, const std::string& key) {
    nlohmann::json* at = &root;
    std::size_t start = 0;
    while (true) {
        const std::size_t dot = key.find('.', start);
        const std::string part = key.substr(start, dot == std::string::npos ? std::string::npos : dot - start);
        if (!at->is_object() || !at->contains(part))
            return nullptr;
        at = &(*at)[part];
        if (dot == std::string::npos)
            return at;
        start = dot + 1;
    }
}

} // namespace

ProjectSettings translate_settings(const TranslationTable& table,
                                   const std::vector<std::pair<std::string, std::string>>& settings, const Settings& base) {
    ProjectSettings out{base, {}};
    nlohmann::json root = nlohmann::json::parse(settings_json(base));
    // The nozzle: the project's (its first extruder's), else the base's.
    double nozzle = base.nozzle_diameter;
    for (const auto& [key, value] : settings)
        if (key == "nozzle_diameter")
            if (const auto d = number_of(value.substr(0, value.find(','))); d && *d > 0.0)
                nozzle = *d;
    const auto put = [&](const std::string& key, const std::string& value, const TranslationRow& row) {
        nlohmann::json* node = row.engine.find(',') == std::string::npos ? node_of(root, row.engine) : nullptr;
        if (!node)
            return false;
        std::string v = value;
        if (const auto r = row.renamed.find(v); r != row.renamed.end())
            v = r->second;
        // An "N%" of the nozzle or of another setting first (PF12, PF13).
        const auto percent = [&](const std::string& of) -> std::optional<double> {
            const std::string t = trimmed(v);
            if (t.empty() || t.back() != '%')
                return std::nullopt;
            const auto n = number_of(std::string_view(t).substr(0, t.size() - 1));
            if (!n)
                return std::nullopt;
            if (of == "nozzle")
                return *n / 100.0 * nozzle;
            const nlohmann::json* b = node_of(root, of);
            if (!b || !b->is_number())
                return std::nullopt;
            return *n / 100.0 * b->get<double>();
        };
        if (!row.percent_of.empty())
            if (const auto mm = percent(row.percent_of)) {
                std::ostringstream text;
                text << *mm;
                v = text.str();
            }
        std::optional<nlohmann::json> to;
        if (row.how == "number" || row.how.empty()) {
            if (const auto d = number_of(v))
                to = *d;
            else if (row.how.empty() && node->is_string())
                to = v;
        } else if (row.how == "first_item") {
            if (const auto d = number_of(v.substr(0, v.find_first_of(",;"))))
                to = *d;
        } else if (row.how == "flag") {
            const std::string t = lower(trimmed(v));
            if (t == "1" || t == "true")
                to = true;
            else if (t == "0" || t == "false")
                to = false;
        } else if (row.how == "factor") {
            if (const auto d = number_of(v))
                to = *d * row.factor;
        } else if (row.how == "map") {
            const auto m = row.values.find(trimmed(v));
            to = m != row.values.end() ? m->second : trimmed(v);
        } else if (row.how == "percent_of") {
            const std::string t = trimmed(v);
            if (!t.empty() && t.back() == '%') {
                const auto n = number_of(std::string_view(t).substr(0, t.size() - 1));
                const nlohmann::json* b = node_of(root, row.base);
                if (n && b && b->is_number())
                    to = *n / 100.0 * b->get<double>();
            } else if (const auto d = number_of(t)) {
                to = *d;
            }
        } else if (row.how == "absolute_only") {
            if (const auto d = number_of(v))
                to = *d; // an "N%" is not an absolute value
        } else if (row.how == "number_strip_percent") {
            std::string t = trimmed(v);
            if (!t.empty() && t.back() == '%')
                t.pop_back();
            if (const auto d = number_of(t))
                to = *d;
        }
        if (!to)
            return false;
        // The engine's own type: a number where a number goes, and so on.
        if (node->is_number_integer() && to->is_number())
            to = static_cast<long long>(std::llround(to->get<double>()));
        else if (node->is_boolean() && !to->is_boolean())
            return false;
        else if (node->is_number() && !to->is_number())
            return false;
        else if (node->is_string() && !to->is_string())
            to = to->is_number() ? nlohmann::json(to->dump()) : *to;
        const nlohmann::json before = *node;
        *node = *to;
        try {
            out.settings = parse_settings(root.dump());
        } catch (const std::exception&) {
            *node = before; // the engine refuses it: left out
            return false;
        }
        (void)key;
        return true;
    };
    // The rows that take an "N%" of another setting last, so they see the
    // project's value of it.
    for (const int pass : {0, 1})
        for (const auto& [key, value] : settings) {
            const TranslationRow* row = row_of(table, key);
            if (!row || row->group == SettingGroup::None || row->engine.empty())
                continue;
            const bool late = row->how == "percent_of" || (!row->percent_of.empty() && row->percent_of != "nozzle");
            if (late != (pass == 1))
                continue;
            if (!put(key, value, *row))
                out.skipped.push_back(key + " = " + value);
        }
    return out;
}

} // namespace tisma

