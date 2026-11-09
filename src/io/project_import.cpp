#include "io/project_import.hpp"

#include "io/xml.hpp"
#include "io/zip.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <cctype>
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
            table[key] = r;
            if (row.contains("with") && row.at("with").is_array())
                for (const json& w : row.at("with"))
                    if (w.is_string() && !table.count(w.get<std::string>()))
                        table[w.get<std::string>()] = r;
        }
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

} // namespace tisma
