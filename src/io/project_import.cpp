#include "io/project_import.hpp"

#include "io/xml.hpp"
#include "io/zip.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <functional>
#include <map>
#include <optional>
#include <set>
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
    if (j.contains("plate_temperature") && j.at("plate_temperature").is_object()) {
        const json& p = j.at("plate_temperature");
        PlateTemperature t;
        if (p.contains("plates") && p.at("plates").is_object())
            for (const auto& [name, keys] : p.at("plates").items())
                if (keys.is_array() && keys.size() == 2 && keys[0].is_string() && keys[1].is_string())
                    t.plates[name] = {keys[0].get<std::string>(), keys[1].get<std::string>()};
        const auto strings = [&](const char* name, std::vector<std::string>& into) {
            if (p.contains(name) && p.at(name).is_array())
                for (const json& v : p.at(name))
                    into.push_back(v.is_string() ? v.get<std::string>() : std::string());
        };
        strings("select", t.select), strings("index", t.index), strings("order", t.order);
        out.plate_temperature = std::move(t);
    }
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
                const auto opt = [&](const char* name) -> std::optional<double> {
                    if (c.contains(name) && c.at(name).is_number())
                        return c.at(name).get<double>();
                    return std::nullopt;
                };
                r.min = opt("min"), r.max = opt("max"), r.bare_percent_over = opt("bare_percent_over");
                r.nonnegative = c.value("nonnegative", false) == true;
                r.nonzero = c.value("nonzero", false) == true;
                if (c.contains("rule") && c.at("rule").is_string())
                    r.rule = c.at("rule").get<std::string>();
                if (c.contains("plate") && c.at("plate").is_string())
                    r.plate = c.at("plate").get<std::string>();
            }
            if (row.contains("rule") && row.at("rule").is_string())
                r.rule = row.at("rule").get<std::string>();
            if (row.contains("plate") && row.at("plate").is_string())
                r.plate = row.at("plate").get<std::string>();
            if (row.contains("prusaslicer_values") && row.at("prusaslicer_values").is_object())
                for (const auto& [from, to] : row.at("prusaslicer_values").items())
                    r.renamed[from] = text(to);
            if (row.contains("prusaslicer") && row.at("prusaslicer").is_string())
                r.prusaslicer = row.at("prusaslicer").get<std::string>();
            if (row.contains("percent_of") && row.at("percent_of").is_string())
                r.percent_of = row.at("percent_of").get<std::string>();
            if (row.contains("with") && row.at("with").is_array())
                for (const json& w : row.at("with"))
                    if (w.is_string())
                        r.with.push_back(w.get<std::string>());
            // A row of its own wins over a helper of another row's rule.
            if (const auto had = table.find(key); had == table.end() || !had->second.with_of.empty())
                table[key] = r;
            for (const std::string& w : r.with)
                if (!table.count(w)) {
                    TranslationRow helper = r;
                    helper.with_of = key;
                    helper.with.clear();
                    table[w] = helper;
                }
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
    // The plate whose bed temperatures count (PF14): the one the project
    // names, by name or number, else the first in the table's order with a
    // value.
    std::string plate;
    if (table.plate_temperature) {
        const PlateTemperature& t = *table.plate_temperature;
        const auto value_of = [&](const std::string& key) -> const std::string* {
            for (const auto& [k, v] : settings)
                if (k == key && !trimmed(v).empty())
                    return &v;
            return nullptr;
        };
        for (const std::string& key : t.select) {
            const std::string* named = value_of(key);
            if (!named)
                continue;
            std::string name = trimmed(*named);
            if (const auto n = number_of(name); n && *n >= 0 && std::size_t(*n) < t.index.size())
                name = t.index[std::size_t(*n)];
            if (t.plates.count(name))
                plate = name;
            break; // the first field with a value; "Default Plate": the order
        }
        for (std::size_t i = 0; plate.empty() && i < t.order.size(); ++i) {
            const std::string& o = t.order[i];
            for (const auto& [name, keys] : t.plates)
                if ((o == name && value_of(keys.first)) || (o == keys.first && value_of(o)) || (o == keys.second && value_of(o))) {
                    plate = name;
                    break;
                }
        }
    }
    const auto put = [&](const std::string& key, const std::string& value, const TranslationRow& row) {
        nlohmann::json* node = row.engine.find(',') == std::string::npos ? node_of(root, row.engine) : nullptr;
        // A key the settings' JSON leaves out (at its default, or a machine
        // variable the profile does not have): made in its group, and
        // parse_settings says whether the engine has it.
        if (!node && row.engine.find(',') == std::string::npos)
            if (const std::size_t dot = row.engine.rfind('.'); dot != std::string::npos)
                if (nlohmann::json* group = node_of(root, row.engine.substr(0, dot)); group && group->is_object())
                    node = &((*group)[row.engine.substr(dot + 1)] =
                                 row.engine.starts_with("machine.variables.") ? nlohmann::json::array() : nlohmann::json());
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
        // PrusaSlicer's widths: "N%" of the layer height, as the source computes them.
        if (row.rule == "role_width" && row.percent_of.empty())
            if (const auto n = percent("nozzle")) {
                double h = base.layer_height;
                for (const auto& [k, w] : settings)
                    if (k == "layer_height")
                        if (const auto d = number_of(w); d && *d > 0.0)
                            h = *d;
                std::ostringstream text;
                text << *n / nozzle * h;
                v = text.str();
            }
        if (!row.percent_of.empty())
            if (const auto mm = percent(row.percent_of)) {
                std::ostringstream text;
                text << *mm;
                v = text.str();
            }
        // role_width: a width in mm (after the % above); 0 takes the "with"
        // key's, the same way; both 0, the engine's own (0).
        if (row.rule == "role_width") {
            auto d = number_of(v);
            for (std::size_t i = 0; d && *d == 0.0 && i < row.with.size(); ++i)
                for (const auto& [k, w] : settings)
                    if (k == row.with[i]) {
                        std::string t = trimmed(w);
                        if (!t.empty() && t.back() == '%' && row.percent_of == "nozzle")
                            if (const auto n = number_of(std::string_view(t).substr(0, t.size() - 1))) {
                                d = *n / 100.0 * nozzle;
                                break;
                            }
                        d = number_of(t);
                        break;
                    }
            if (!d || *d < 0.0)
                return false;
            v = [&] {
                std::ostringstream text;
                text << *d;
                return text.str();
            }();
        }
        std::optional<nlohmann::json> to;
        // number and its kin: a value outside the row's limits is not written.
        const auto limited = [&](double d) -> std::optional<nlohmann::json> {
            if ((row.nonnegative && d < 0.0) || (row.min && d < *row.min) || (row.max && d > *row.max))
                return std::nullopt;
            return nlohmann::json(d);
        };
        if (row.how == "number" || row.how.empty() || (row.how == "rule" && (row.rule == "plate_temperature" || row.rule == "role_width"))) {
            if (const auto d = number_of(v))
                to = limited(*d);
            else if (row.how.empty() && node->is_string())
                to = v;
        } else if (row.how == "first_item") {
            // The first extruder's text.
            const std::string first = trimmed(v.substr(0, v.find(',')));
            if (const auto d = number_of(first))
                to = *d;
            else if (!node->is_number())
                to = first;
        } else if (row.how == "at_least") {
            if (const auto d = number_of(v); d && (!row.min || *d >= *row.min))
                to = *d;
        } else if (row.how == "points") {
            // "XxY,XxY,...": at least 3 points.
            nlohmann::json points = nlohmann::json::array();
            bool good = true;
            std::size_t start = 0;
            while (good && start <= v.size()) {
                const std::size_t comma = std::min(v.find(',', start), v.size());
                const std::string p = trimmed(std::string_view(v).substr(start, comma - start));
                const std::size_t x = p.find('x');
                const auto a = x == std::string::npos ? std::nullopt : number_of(std::string_view(p).substr(0, x));
                const auto b = x == std::string::npos ? std::nullopt : number_of(std::string_view(p).substr(x + 1));
                if (a && b)
                    points.push_back({*a, *b});
                else
                    good = false;
                start = comma + 1;
            }
            if (good && points.size() >= 3)
                to = points;
        } else if (row.how == "number_or_percent") {
            const std::string t = trimmed(v);
            if (!t.empty() && t.back() == '%') {
                if (number_of(std::string_view(t).substr(0, t.size() - 1)))
                    to = t;
            } else if (const auto d = number_of(t); d && !(row.nonzero && *d == 0.0)) {
                if (row.bare_percent_over && *d > *row.bare_percent_over) {
                    std::ostringstream text;
                    text << *d << '%';
                    to = text.str();
                } else {
                    to = *d;
                }
            }
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
                to = limited(*d);
        }
        if (!to)
            return false;
        // The engine's own type: a number where a number goes, a list of
        // texts for a variable, and so on.
        const auto text_of = [](const nlohmann::json& n) {
            if (n.is_string())
                return n.get<std::string>();
            std::ostringstream text;
            text << (n.is_number() ? n.get<double>() : 0.0);
            return n.is_number() ? text.str() : n.dump();
        };
        if (node->is_array() && !to->is_array())
            to = nlohmann::json::array({text_of(*to)});
        else if (node->is_number_integer() && to->is_number())
            to = static_cast<long long>(std::llround(to->get<double>()));
        else if (node->is_boolean() && !to->is_boolean())
            return false;
        else if (node->is_number() && !to->is_number())
            return false;
        else if (node->is_string() && !to->is_string())
            to = to->is_number() ? nlohmann::json(text_of(*to)) : *to;
        const bool made = node->is_null() || (node->is_array() && node->empty() && to->is_array());
        const nlohmann::json before = *node;
        *node = *to;
        try {
            out.settings = parse_settings(root.dump());
        } catch (const std::exception&) {
            // The engine refuses it, or has no such key: left out.
            if (made && before.is_null()) {
                const std::size_t dot = row.engine.rfind('.');
                if (nlohmann::json* group = node_of(root, row.engine.substr(0, dot)))
                    group->erase(row.engine.substr(dot + 1));
            } else {
                *node = before;
            }
            return false;
        }
        (void)key;
        return true;
    };
    // Named rules (profile_pack_format.md, «Tabla de traducción»): each one
    // reads the project's keys by their PrusaSlicer name, the one its "with"
    // gives, and writes one or more engine keys.
    std::map<std::string, std::string> source; // PrusaSlicer name -> value
    for (const auto& [key, value] : settings) {
        const TranslationRow* row = row_of(table, key);
        source[row && !row->prusaslicer.empty() ? row->prusaslicer : key] = value;
    }
    const auto raw = [&](const std::string& name) -> std::optional<std::string> {
        const auto f = source.find(name);
        if (f == source.end() || trimmed(f->second).empty())
            return std::nullopt;
        return trimmed(f->second);
    };
    // The first extruder's value ("nil": none).
    const auto first = [&](const std::string& name) -> std::optional<std::string> {
        const auto v = raw(name);
        if (!v)
            return std::nullopt;
        std::string t = trimmed(std::string_view(*v).substr(0, v->find(',')));
        if (t.empty() || t == "nil")
            return std::nullopt;
        return t;
    };
    const auto first_number = [&](const std::string& name) { return first(name) ? number_of(*first(name)) : std::nullopt; };
    const auto flag = [&](const std::string& name) -> std::optional<bool> {
        const auto v = first(name);
        if (!v)
            return std::nullopt;
        const std::string t = lower(*v);
        if (t == "1" || t == "true")
            return true;
        if (t == "0" || t == "false")
            return false;
        return std::nullopt;
    };
    // Writes one engine key; the engine checks it, and a value it refuses
    // leaves the key as it was.
    const auto assign = [&](const std::string& engine, nlohmann::json to) {
        nlohmann::json* node = node_of(root, engine);
        bool made = false;
        if (!node)
            if (const std::size_t dot = engine.rfind('.'); dot != std::string::npos)
                if (nlohmann::json* group = node_of(root, engine.substr(0, dot)); group && group->is_object()) {
                    node = &((*group)[engine.substr(dot + 1)] = nlohmann::json());
                    made = true;
                }
        if (!node)
            return false;
        if (node->is_number_integer() && to.is_number())
            to = static_cast<long long>(std::llround(to.get<double>()));
        const nlohmann::json before = *node;
        *node = std::move(to);
        try {
            out.settings = parse_settings(root.dump());
        } catch (const std::exception&) {
            if (made) {
                const std::size_t dot = engine.rfind('.');
                node_of(root, engine.substr(0, dot))->erase(engine.substr(dot + 1));
            } else {
                *node = before;
            }
            return false;
        }
        return true;
    };
    // A width as the source computes it: mm, or "N%" of the nozzle (when the
    // row says so) or of the layer height; 0 or none: nullopt.
    const auto width_of = [&](const std::string& name) -> std::optional<double> {
        const auto v = first(name);
        if (!v)
            return std::nullopt;
        std::optional<double> mm;
        if (v->back() == '%') {
            const auto n = number_of(std::string_view(*v).substr(0, v->size() - 1));
            const TranslationRow* row = nullptr;
            for (const auto& [key, value] : settings)
                if (const TranslationRow* r = row_of(table, key); r && (r->prusaslicer == name || (r->prusaslicer.empty() && key == name)))
                    row = r;
            double of = row && row->percent_of == "nozzle" ? nozzle : 0.0;
            if (of == 0.0) {
                const auto h = first_number("layer_height");
                of = h ? *h : base.layer_height;
            }
            if (n)
                mm = *n / 100.0 * of;
        } else {
            mm = number_of(*v);
        }
        if (!mm || !(*mm > 0.0))
            return std::nullopt;
        return mm;
    };
    // Each rule: true when it wrote what it had to (or the project leaves it
    // to the engine), false when the engine refused a value.
    const auto rule = [&](const std::string& name) -> std::optional<bool> {
        if (name == "acceleration") {
            // Extruding only when not 0; travel falls back to extruding.
            const auto e = first_number("machine_max_acceleration_extruding");
            auto t = first_number("machine_max_acceleration_travel");
            bool good = true;
            if (e && *e > 0.0)
                good = assign("machine.acceleration", *e) && good;
            if ((!t || *t <= 0.0) && e && *e > 0.0)
                t = e;
            if (t && *t > 0.0)
                good = assign("machine.travel_acceleration", *t) && good;
            return good;
        }
        if (name == "junction_deviation") {
            // Only with marlin2, the one flavor the source programs send it to.
            const auto f = first("gcode_flavor");
            const auto d = first_number("machine_max_junction_deviation");
            if (!d || !f || lower(*f) != "marlin2")
                return true;
            return assign("machine.junction_deviation", *d);
        }
        if (name == "fans") {
            const auto cooling = flag("cooling"), always = flag("fan_always_on");
            if (cooling == false && always != true)
                return assign("cooling.fan_min", 0.0) && assign("cooling.fan_max", 0.0);
            bool good = true;
            if (const auto lo = first_number("min_fan_speed"))
                good = assign("cooling.fan_min", *lo) && good;
            if (const auto hi = first_number("max_fan_speed"))
                good = assign("cooling.fan_max", *hi) && good;
            return good;
        }
        if (name == "gap_fill") {
            const auto speed = first_number("gap_fill_speed");
            const auto on = flag("gap_fill_enabled");
            if (on == false || (speed && *speed == 0.0))
                return assign("walls.gap_fill", false);
            bool good = assign("walls.gap_fill", true);
            if (speed)
                good = assign("speed.gap_fill", *speed) && good;
            return good;
        }
        if (name == "wall_width") {
            auto w = width_of("perimeter_extrusion_width");
            if (!w)
                w = width_of("extrusion_width");
            return assign("walls.width", w ? *w : 1.125 * nozzle);
        }
        if (name == "xy_gap") {
            // In mm, or "N%" of the outer wall width, which walls.outer_width
            // (role_width) already carries as the source computes it.
            const auto v = first("support_material_xy_spacing");
            if (!v)
                return true;
            if (v->back() == '%')
                return number_of(std::string_view(*v).substr(0, v->size() - 1)) ? assign("support.xy_gap", *v) : false;
            const auto d = number_of(*v);
            return d && assign("support.xy_gap", *d);
        }
        if (name == "extruders") {
            // As many extruders as nozzle_diameter has values; the nozzles as a
            // list when they differ; the offsets when one is not 0.
            const auto list = [](const std::string& v) {
                std::vector<std::string> items;
                std::size_t start = 0;
                while (start <= v.size()) {
                    const std::size_t comma = std::min(v.find(',', start), v.size());
                    items.push_back(trimmed(std::string_view(v).substr(start, comma - start)));
                    start = comma + 1;
                }
                return items;
            };
            const auto nozzles = raw("nozzle_diameter");
            if (!nozzles)
                return true;
            std::vector<double> d;
            for (const std::string& n : list(*nozzles))
                if (const auto x = number_of(n); x && *x > 0.0)
                    d.push_back(*x);
                else
                    return false;
            bool good = assign("machine.extruders", double(d.size()));
            if (std::adjacent_find(d.begin(), d.end(), std::not_equal_to<>()) != d.end())
                good = assign("machine.nozzle_diameter", d) && good;
            else
                good = assign("machine.nozzle_diameter", d.front()) && good;
            if (const auto offsets = raw("extruder_offset")) {
                nlohmann::json xy = nlohmann::json::array();
                bool any = false;
                for (const std::string& p : list(*offsets)) {
                    const std::size_t x = p.find('x');
                    const auto a = x == std::string::npos ? std::nullopt : number_of(std::string_view(p).substr(0, x));
                    const auto b = x == std::string::npos ? std::nullopt : number_of(std::string_view(p).substr(x + 1));
                    if (!a || !b)
                        return false;
                    any = any || *a != 0.0 || *b != 0.0;
                    xy.push_back({*a, *b});
                }
                if (any)
                    good = assign("machine.extruder_offset", xy) && good;
            }
            return good;
        }
        if (name == "filament_overrides") {
            // The filament's own retraction and travel values; "nil": the printer's.
            static const std::pair<const char*, const char*> names[] = {
                {"filament_retract_length", "retraction.length"},
                {"filament_retract_speed", "retraction.speed"},
                {"filament_deretract_speed", "retraction.unretract_speed"},
                {"filament_retract_restart_extra", "retraction.restart_extra"},
                {"filament_retract_lift", "retraction.lift"},
                {"filament_retract_lift_above", "retraction.lift_above"},
                {"filament_retract_lift_below", "retraction.lift_below"},
                {"filament_retract_before_travel", "retraction.min_travel"},
                {"filament_retract_layer_change", "retraction.on_layer_change"},
                {"filament_wipe", "retraction.wipe"},
                {"filament_retract_before_wipe", "retraction.before_wipe"},
                {"filament_travel_ramping_lift", "travel.ramping_lift"},
                {"filament_travel_slope", "travel.slope"},
                {"filament_travel_max_lift", "travel.max_lift"}};
            bool good = true;
            for (const auto& [from, to] : names) {
                auto v = first(from);
                if (!v)
                    continue;
                if (v->back() == '%')
                    v->pop_back();
                const std::string field(to);
                const bool is_flag = field == "retraction.on_layer_change" || field == "retraction.wipe" || field == "travel.ramping_lift";
                nlohmann::json value;
                if (is_flag) {
                    const auto b = flag(from);
                    if (!b) {
                        good = false;
                        continue;
                    }
                    value = *b;
                } else if (const auto d = number_of(*v)) {
                    value = *d;
                } else {
                    good = false;
                    continue;
                }
                const std::size_t dot = field.find('.');
                good = assign("extrusion." + field.substr(0, dot) + "." + field.substr(dot + 1), value) && good;
            }
            return good;
        }
        if (name == "thumbnails") {
            // "WxH/FORMAT" list; a size without a format takes thumbnails_format.
            const auto v = raw("thumbnails");
            if (!v)
                return true;
            std::string format = first("thumbnails_format").value_or("");
            for (char& c : format)
                c = char(std::toupper(static_cast<unsigned char>(c)));
            std::string text;
            std::size_t start = 0;
            while (start <= v->size()) {
                const std::size_t comma = std::min(v->find(',', start), v->size());
                std::string item = trimmed(std::string_view(*v).substr(start, comma - start));
                start = comma + 1;
                if (item.empty())
                    continue;
                if (item.find('/') == std::string::npos && !format.empty())
                    item += "/" + format;
                text += (text.empty() ? "" : ",") + item;
            }
            return assign("gcode.thumbnails", text);
        }
        return std::nullopt; // not done yet
    };
    std::set<std::string> ruled;
    std::map<std::string, std::optional<bool>> rule_result;
    // The rows that take an "N%" of another setting last, so they see the
    // project's value of it.
    for (const int pass : {0, 1})
        for (const auto& [key, value] : settings) {
            const TranslationRow* row = row_of(table, key);
            if (!row || row->group == SettingGroup::None || row->engine.empty())
                continue;
            if (row->rule == "plate_temperature" && row->plate != plate)
                continue; // only the project's plate counts
            if (!row->with_of.empty())
                continue; // read by its row's rule
            if (row->how == "rule" && row->rule != "plate_temperature" && row->rule != "role_width") {
                if (pass == 0) {
                    const bool first_time = ruled.insert(row->rule).second;
                    if (first_time)
                        rule_result[row->rule] = rule(row->rule);
                    const std::optional<bool>& r = rule_result[row->rule];
                    if (!r || !*r)
                        out.skipped.push_back(key + " = " + value); // not done yet, or refused
                }
                continue;
            }
            const bool late = row->how == "percent_of" || (!row->percent_of.empty() && row->percent_of != "nozzle");
            if (late != (pass == 1))
                continue;
            if (!put(key, value, *row))
                out.skipped.push_back(key + " = " + value);
        }
    return out;
}

} // namespace tisma

