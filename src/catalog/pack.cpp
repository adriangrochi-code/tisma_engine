#include "catalog/pack.hpp"

#include "config/profiles.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <set>

namespace tisma {

namespace {

using json = nlohmann::ordered_json;

json parse_object(std::string_view text, const char* what) {
    json j = json::parse(text.begin(), text.end(), nullptr, false);
    if (!j.is_object())
        throw PackError(std::string(what) + ": not a JSON object");
    return j;
}

std::string text(const json& j, const char* key) {
    return j.contains(key) && j.at(key).is_string() ? j.at(key).get<std::string>() : std::string();
}

PackFile file_of(const json& j) {
    PackFile f;
    if (!j.is_object())
        return f;
    f.name = text(j, "name");
    if (j.contains("size") && j.at("size").is_number_unsigned())
        f.size = j.at("size").get<std::uint64_t>();
    f.sha256 = text(j, "sha256");
    std::transform(f.sha256.begin(), f.sha256.end(), f.sha256.begin(), [](char c) { return char(std::tolower(c)); });
    return f;
}

std::vector<std::string> strings(const json& j, const char* key) {
    std::vector<std::string> out;
    if (j.contains(key) && j.at(key).is_array())
        for (const json& v : j.at(key))
            if (v.is_string())
                out.push_back(v.get<std::string>());
    return out;
}

// The keys the engine knows ("group.key", docs/CONFIG.md), and the printer's
// variables, an object of its own.
const std::set<std::string>& known_keys() {
    static const std::set<std::string> keys = [] {
        std::set<std::string> k;
        for (const auto& [key, value] : describe(Settings{}))
            k.insert(key);
        // The filament's material type, which the settings' file writes only
        // when given (profile_pack_format.md, print profiles, rule 6, PF9).
        k.insert("extrusion.type");
        // A filament's own retraction and travels, objects of their own
        // (retraction_travel.md, rule 10; PF10).
        k.insert("extrusion.retraction");
        k.insert("extrusion.travel");
        k.insert("machine.variables");
        k.insert("machine.macros");
        return k;
    }();
    return keys;
}

// The settings with only the keys the engine knows; a warning for each other.
json known_settings(const json& settings, const std::string& where, std::vector<std::string>& warnings) {
    json out = json::object();
    if (!settings.is_object())
        return out;
    for (const auto& [group, keys] : settings.items()) {
        if (!keys.is_object()) {
            warnings.push_back(where + ": " + group + " is not a group of settings: left out");
            continue;
        }
        for (const auto& [key, value] : keys.items()) {
            if (known_keys().contains(group + "." + key))
                out[group][key] = value;
            else
                warnings.push_back(where + ": unknown setting " + group + "." + key + ": left out");
        }
    }
    return out;
}

std::uint32_t rotr(std::uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }

// FIPS 180-4, 4.2.2 and 5.3.3: the first 32 bits of the fractional parts of
// the cube roots of the first 64 primes, and of the square roots of the first 8.
struct Sha256Constants {
    std::array<std::uint32_t, 64> k{};
    std::array<std::uint32_t, 8> h{};
};
const Sha256Constants& sha256_constants() {
    static const Sha256Constants c = [] {
        Sha256Constants out;
        std::vector<int> primes;
        for (int n = 2; primes.size() < 64; ++n) {
            bool prime = true;
            for (int p : primes)
                if (p * p > n)
                    break;
                else if (n % p == 0)
                    prime = false;
            if (prime)
                primes.push_back(n);
        }
        const auto fraction32 = [](long double x) {
            const long double f = x - std::floor(x);
            return std::uint32_t(std::floor(f * 4294967296.0L));
        };
        for (std::size_t i = 0; i < 64; ++i)
            out.k[i] = fraction32(std::cbrt((long double)primes[i]));
        for (std::size_t i = 0; i < 8; ++i)
            out.h[i] = fraction32(std::sqrt((long double)primes[i]));
        return out;
    }();
    return c;
}

} // namespace

PackIndex parse_pack_index(std::string_view text_in) {
    const json j = parse_object(text_in, "index.json");
    if (text(j, "format") != "tisma-profile-pack")
        throw PackError("index.json: not a profile pack (\"format\" must be \"tisma-profile-pack\")");
    if (!j.contains("version") || !j.at("version").is_number_integer())
        throw PackError("index.json: version must be a whole number");
    PackIndex out;
    out.version = j.at("version").get<int>();
    if (out.version > kPackVersion)
        throw PackError("the profile pack is of a newer version (" + std::to_string(out.version) + "): update Tisma");
    out.pack_version = text(j, "pack_version");
    out.printers = strings(j, "printers");
    if (j.contains("compatible"))
        out.compatible = file_of(j.at("compatible"));
    if (j.contains("community"))
        out.community = file_of(j.at("community"));
    if (j.contains("translation_archive"))
        out.translation_archive = file_of(j.at("translation_archive"));
    if (j.contains("translation") && j.at("translation").is_object())
        for (const auto& [dialect, path] : j.at("translation").items())
            if (path.is_string())
                out.translation[dialect] = path.get<std::string>();
    if (j.contains("vendors") && j.at("vendors").is_array())
        for (const json& v : j.at("vendors")) {
            if (!v.is_object())
                continue;
            PackVendor vendor;
            vendor.id = text(v, "id");
            vendor.name = text(v, "name");
            vendor.license = text(v, "license");
            vendor.origin = text(v, "origin");
            vendor.printers = strings(v, "printers");
            vendor.filaments = strings(v, "filaments");
            vendor.prints = strings(v, "prints");
            vendor.resins = strings(v, "resins");
            vendor.resin_prints = strings(v, "resin_prints");
            if (v.contains("archive"))
                vendor.archive = file_of(v.at("archive"));
            if (v.contains("models") && v.at("models").is_array())
                for (const json& m : v.at("models")) {
                    if (!m.is_object())
                        continue;
                    PackModel model;
                    model.id = text(m, "id");
                    model.name = text(m, "name");
                    model.technology = text(m, "technology");
                    model.family = text(m, "family");
                    model.file = text(m, "file");
                    model.default_variant = text(m, "default_variant");
                    if (m.contains("assets") && m.at("assets").is_object()) {
                        model.thumbnail = text(m.at("assets"), "thumbnail");
                        model.bed_model = text(m.at("assets"), "bed_model");
                        model.bed_texture = text(m.at("assets"), "bed_texture");
                    }
                    if (m.contains("variants") && m.at("variants").is_array())
                        for (const json& x : m.at("variants"))
                            if (x.is_object())
                                model.variants.push_back(
                                    {text(x, "id"), x.contains("nozzle") && x.at("nozzle").is_number() ? x.at("nozzle").get<double>() : 0.0});
                    vendor.models.push_back(std::move(model));
                }
            if (!vendor.id.empty())
                out.vendors.push_back(std::move(vendor));
        }
    return out;
}

std::vector<PackDownload> pack_plan(const PackIndex* index, const std::string& base_url, const std::vector<std::string>& vendors,
                                    const PackIndex* installed) {
    std::string base = base_url.empty() ? std::string(kPackUrl) : base_url;
    if (base.back() != '/')
        base += '/';
    std::vector<PackDownload> out;
    if (!index) {
        out.push_back({base + "index.json", "index.json", 0, ""});
        return out;
    }
    const auto add = [&](const PackFile& f, const std::string& fallback) {
        const std::string name = f.name.empty() ? fallback : f.name;
        out.push_back({base + name, name, f.size, f.sha256});
    };
    add(index->compatible, "compatible.json");
    add(index->community, "community.tar.xz");
    // knowledge.tar.xz (rule 21) is not in the index: no size or SHA-256 to check.
    out.push_back({base + "knowledge.tar.xz", "knowledge.tar.xz", 0, ""});
    // The translation table, when the index names it (project_import.md, rule 11).
    if (!index->translation_archive.name.empty())
        add(index->translation_archive, "translation.tar.xz");
    for (const PackVendor& v : index->vendors) {
        if (!vendors.empty() && std::find(vendors.begin(), vendors.end(), v.id) == vendors.end())
            continue;
        if (installed) { // rule 20: only the vendors whose archive changed
            const auto was = std::find_if(installed->vendors.begin(), installed->vendors.end(),
                                          [&](const PackVendor& w) { return w.id == v.id; });
            if (was != installed->vendors.end() && !was->archive.sha256.empty() && was->archive.sha256 == v.archive.sha256)
                continue;
        }
        add(v.archive, v.id + ".tar.xz");
    }
    return out;
}

std::string sha256_hex(std::string_view data) {
    const Sha256Constants& c = sha256_constants();
    std::array<std::uint32_t, 8> h = c.h;
    std::vector<std::uint8_t> m(data.begin(), data.end());
    const std::uint64_t bits = std::uint64_t(data.size()) * 8u;
    m.push_back(0x80);
    while (m.size() % 64 != 56)
        m.push_back(0);
    for (int i = 7; i >= 0; --i)
        m.push_back(std::uint8_t(bits >> (8 * i)));
    std::array<std::uint32_t, 64> w{};
    for (std::size_t block = 0; block < m.size(); block += 64) {
        for (std::size_t t = 0; t < 16; ++t)
            w[t] = std::uint32_t(m[block + 4 * t]) << 24 | std::uint32_t(m[block + 4 * t + 1]) << 16 |
                   std::uint32_t(m[block + 4 * t + 2]) << 8 | std::uint32_t(m[block + 4 * t + 3]);
        for (std::size_t t = 16; t < 64; ++t) {
            const std::uint32_t s0 = rotr(w[t - 15], 7) ^ rotr(w[t - 15], 18) ^ (w[t - 15] >> 3);
            const std::uint32_t s1 = rotr(w[t - 2], 17) ^ rotr(w[t - 2], 19) ^ (w[t - 2] >> 10);
            w[t] = w[t - 16] + s0 + w[t - 7] + s1;
        }
        std::uint32_t a = h[0], b = h[1], cc = h[2], d = h[3], e = h[4], f = h[5], g = h[6], hh = h[7];
        for (std::size_t t = 0; t < 64; ++t) {
            const std::uint32_t t1 = hh + (rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25)) + ((e & f) ^ (~e & g)) + c.k[t] + w[t];
            const std::uint32_t t2 = (rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22)) + ((a & b) ^ (a & cc) ^ (b & cc));
            hh = g, g = f, f = e, e = d + t1, d = cc, cc = b, b = a, a = t1 + t2;
        }
        h[0] += a, h[1] += b, h[2] += cc, h[3] += d, h[4] += e, h[5] += f, h[6] += g, h[7] += hh;
    }
    std::string out;
    char hex[9];
    for (std::uint32_t v : h) {
        std::snprintf(hex, sizeof hex, "%08x", v);
        out += hex;
    }
    return out;
}

bool safe_pack_path(std::string_view path) {
    if (path.empty() || path.front() == '/' || path.find('\\') != std::string_view::npos || path.find(':') != std::string_view::npos)
        return false;
    const bool under = path.starts_with("community/") || path.starts_with("knowledge/") ||
                       path.starts_with("translation/") ||
                       (path.starts_with("vendors/") && path.find('/', 8) != std::string_view::npos && path.find('/', 8) > 8);
    if (!under)
        return false;
    std::size_t start = 0;
    while (start <= path.size()) {
        const std::size_t end = std::min(path.find('/', start), path.size());
        const std::string_view part = path.substr(start, end - start);
        if (part == ".." || part == ".")
            return false;
        if (end == path.size())
            break;
        start = end + 1;
    }
    return true;
}

Profile pack_printer_profile(std::string_view model_json, const std::string& variant, std::vector<std::string>& warnings,
                             std::string* print_defaults) {
    const json j = parse_object(model_json, "printer model");
    if (text(j, "format") != "tisma-pack-model")
        throw PackError("not a pack printer model (\"format\" must be \"tisma-pack-model\")");
    if (j.contains("version") && j.at("version").is_number_integer() && j.at("version").get<int>() > kPackVersion)
        throw PackError("the printer model is of a newer version: update Tisma");
    const std::string want = variant.empty() ? text(j, "default_variant") : variant;
    const json* chosen = nullptr;
    if (j.contains("variants") && j.at("variants").is_array())
        for (const json& v : j.at("variants"))
            if (v.is_object() && text(v, "id") == want)
                chosen = &v;
    if (!chosen)
        throw PackError("the printer model has no variant '" + want + "'");
    const std::string where = text(j.contains("model") ? j.at("model") : json::object(), "name") + "#" + want;
    json settings = j.contains("common") && j.at("common").is_object() && j.at("common").contains("settings")
                        ? j.at("common").at("settings")
                        : json::object();
    if (!settings.is_object())
        settings = json::object();
    if (chosen->contains("settings") && chosen->at("settings").is_object())
        for (const auto& [group, keys] : chosen->at("settings").items()) {
            if (!keys.is_object()) {
                settings[group] = keys;
                continue;
            }
            for (const auto& [key, value] : keys.items())
                settings[group][key] = value; // the variant's over common, key by key
        }
    // The machine's G-code with macros (profile_pack_format.md, «G-code de la
    // máquina con macros»): the variant's over common.gcode, key by key, and
    // inside templates and values name by name.
    {
        json block = json::object();
        const auto merge = [&](const json& from) {
            if (!from.is_object())
                return;
            for (const auto& [key, value] : from.items()) {
                if ((key == "templates" || key == "values") && value.is_object() && block.contains(key) &&
                    block.at(key).is_object())
                    for (const auto& [name, item] : value.items())
                        block[key][name] = item;
                else
                    block[key] = value;
            }
        };
        if (j.contains("common") && j.at("common").is_object() && j.at("common").contains("gcode"))
            merge(j.at("common").at("gcode"));
        if (chosen->contains("gcode"))
            merge(chosen->at("gcode"));
        if (block.contains("dialect") && block.at("dialect").is_string()) {
            json macros = json::object();
            for (const char* key : {"dialect", "templates", "relative_extrusion", "values"})
                if (block.contains(key))
                    macros[key] = block.at(key);
            settings["machine"]["macros"] = std::move(macros);
        }
    }
    // The model's technology is the printer's, unless its settings say it:
    // an "sla" model is a resin printer (machine.technology "msla").
    const std::string technology = text(j, "technology");
    if ((technology == "sla" || technology == "fff") &&
        !(settings.contains("machine") && settings.at("machine").is_object() && settings.at("machine").contains("technology")))
        settings["machine"]["technology"] = technology == "sla" ? "msla" : "fdm";
    // A printer's purge volumes are a print setting here (PF8): the print's default.
    json carried = json::object();
    if (settings.contains("wipe_tower") && settings.at("wipe_tower").is_object() &&
        settings.at("wipe_tower").contains("purge_volumes")) {
        carried["wipe_tower"]["purge_volumes"] = settings.at("wipe_tower").at("purge_volumes");
        settings["wipe_tower"].erase("purge_volumes");
        if (settings.at("wipe_tower").empty())
            settings.erase("wipe_tower");
    }
    if (print_defaults)
        *print_defaults = carried.dump();
    const json profile{{"format", "tisma-profile"},
                       {"version", 2},
                       {"kind", "printer"},
                       {"name", text(*chosen, "name").empty() ? where : text(*chosen, "name")},
                       {"settings", known_settings(settings, where, warnings)}};
    try {
        return parse_profile(profile.dump());
    } catch (const SettingsError& e) {
        throw PackError(where + ": " + e.what());
    }
}

PackEntry parse_pack_entry(std::string_view json_text, std::vector<std::string>& warnings) {
    const json j = parse_object(json_text, "pack entry");
    if (text(j, "format") != "tisma-pack-entry")
        throw PackError("not a pack entry (\"format\" must be \"tisma-pack-entry\")");
    if (!j.contains("profile") || !j.at("profile").is_object())
        throw PackError("pack entry: no profile");
    PackEntry out;
    json profile = j.at("profile"); // "original" is a field of the entry, never read
    const std::string where = text(profile, "name");
    profile["settings"] = known_settings(profile.contains("settings") ? profile.at("settings") : json::object(), where, warnings);
    const std::string kind = text(profile, "kind");
    // A filament's material (profile_pack_format.md, entries, rule 4): its
    // density and price per kg, when the profile does not give them.
    if (kind == "filament" && j.contains("material") && j.at("material").is_object()) {
        const json& m = j.at("material");
        json& extrusion = profile["settings"]["extrusion"];
        for (const auto& [from, to] : {std::pair<const char*, const char*>{"density", "density"}, {"cost_per_kg", "cost"}})
            if (m.contains(from) && m.at(from).is_number() && m.at(from).get<double>() > 0.0 && !extrusion.contains(to))
                extrusion[to] = m.at(from).get<double>();
        if (extrusion.empty())
            profile["settings"].erase("extrusion");
    }
    // A filament's own G-code with macros (profile_pack_format.md, «G-code de
    // la máquina con macros», rule 1): filament_start and filament_end in
    // place of the simple ones. The engine's language reads every dialect but
    // cura's, whose {name} texts are left to the simple version.
    if (kind == "filament" && j.contains("gcode") && j.at("gcode").is_object()) {
        const json& g = j.at("gcode");
        const std::string dialect = text(g, "dialect");
        if (!dialect.empty() && dialect != "cura" && g.contains("templates") && g.at("templates").is_object())
            for (const auto& [from, to] : {std::pair<const char*, const char*>{"filament_start", "start_gcode"},
                                           {"filament_end", "end_gcode"}})
                if (g.at("templates").contains(from) && g.at("templates").at(from).is_string())
                    profile["settings"]["extrusion"][to] = g.at("templates").at(from);
    }
    // A filament's values in the program it comes from, by that program's
    // names (profile_pack_format.md, source_values): the variables of a
    // machine's own G-code that the engine has no setting for
    // (temperature_vitrification...), as machine.variables, under any the
    // profile already sets.
    if (kind == "filament" && j.contains("source_values") && j.at("source_values").is_object() &&
        j.at("source_values").contains("values") && j.at("source_values").at("values").is_object()) {
        json& variables = profile["settings"]["machine"]["variables"];
        if (!variables.is_object())
            variables = json::object();
        for (const auto& [name, value] : j.at("source_values").at("values").items()) {
            if (variables.contains(name) || name.empty() || name.find_first_of("[]\n") != std::string::npos)
                continue;
            if (value.is_array()) {
                json list = json::array();
                for (const json& item : value)
                    if (item.is_number() || item.is_string())
                        list.push_back(item);
                    else if (item.is_boolean())
                        list.push_back(item.get<bool>() ? "true" : "false");
                if (!list.empty())
                    variables[name] = list;
            } else if (value.is_number() || value.is_string()) {
                variables[name] = value;
            } else if (value.is_boolean()) {
                variables[name] = value.get<bool>() ? "true" : "false";
            }
        }
        if (variables.empty())
            profile["settings"]["machine"].erase("variables");
        if (profile["settings"]["machine"].empty())
            profile["settings"].erase("machine");
    }
    if (kind == "resin" || kind == "resin_print") {
        profile["kind"] = kind == "resin" ? "filament" : "print";
        out.resin = true;
    }
    // The printer's settings the entry carries (a print profile's speeds, as
    // other programs keep them) go to the printer profile, not lost (BH27);
    // another kind's are left out with a warning.
    const std::optional<ProfileKind> own = profile_kind_from_name(text(profile, "kind"));
    json printer_part = json::object(), filament_part = json::object();
    if (own && *own != ProfileKind::Printer) {
        json& settings = profile["settings"];
        for (auto& [group, keys] : settings.items()) {
            std::vector<std::string> moved;
            for (const auto& [key, value] : keys.items()) {
                const auto k = setting_kind(group, key);
                if (!k || *k == *own || (group == "machine" && key == "variables"))
                    continue;
                if (*k == ProfileKind::Printer)
                    printer_part[group][key] = value;
                else if (*own == ProfileKind::Print && ((group == "extrusion" && key == "bridge_flow_ratio") ||
                                                         (group == "resin" && key == "bottom_layers")))
                    filament_part[group][key] = value; // the print's bridges' flow (PF7), a resin print's base (PF11)
                else
                    warnings.push_back(where + ": " + group + "." + key + " is a " + std::string(profile_kind_name(*k)) +
                                       " setting: left out");
                moved.push_back(key);
            }
            for (const std::string& key : moved)
                keys.erase(key);
        }
        for (auto it = settings.begin(); it != settings.end();)
            it = it->is_object() && it->empty() ? settings.erase(it) : std::next(it);
        // Nothing of the printer is left in it: read as this version's profile,
        // so the old version's dropping of the speeds does not apply.
        profile["version"] = 2;
    }
    out.printer_settings = printer_part.dump();
    out.filament_settings = filament_part.dump();
    try {
        out.profile = parse_profile(profile.dump());
    } catch (const SettingsError& e) {
        throw PackError(where + ": " + e.what());
    }
    if (j.contains("compatible") && j.at("compatible").is_number_integer())
        out.compatible = j.at("compatible").get<int>();
    out.generic = j.value("generic", false);
    if (j.contains("compatible_prints") && j.at("compatible_prints").is_array())
        for (const json& p : j.at("compatible_prints"))
            if (p.is_string() && !p.get<std::string>().empty())
                out.compatible_prints.push_back(p.get<std::string>());
    if (j.contains("nozzle") && j.at("nozzle").is_number())
        out.nozzle = j.at("nozzle").get<double>();
    out.license = text(j, "license");
    out.vendor = text(j, "vendor");
    out.origin = text(j, "origin");
    return out;
}

std::vector<std::vector<int>> parse_pack_compatible(std::string_view json_text) {
    const json j = parse_object(json_text, "compatible.json");
    if (text(j, "format") != "tisma-pack-compatible")
        throw PackError("compatible.json: \"format\" must be \"tisma-pack-compatible\"");
    std::vector<std::vector<int>> out;
    if (j.contains("sets") && j.at("sets").is_array())
        for (const json& s : j.at("sets")) {
            std::vector<int> set;
            if (s.is_array())
                for (const json& n : s)
                    if (n.is_number_integer())
                        set.push_back(n.get<int>());
            std::sort(set.begin(), set.end());
            out.push_back(std::move(set));
        }
    return out;
}

bool pack_compatible(const std::optional<int>& set, const std::vector<std::vector<int>>& sets, int printer) {
    if (!set)
        return true;
    if (*set < 0 || std::size_t(*set) >= sets.size())
        return false;
    if (sets[std::size_t(*set)].empty())
        return true; // an empty set is taken as none (rule 12): a pack error, not "no printer"
    return std::binary_search(sets[std::size_t(*set)].begin(), sets[std::size_t(*set)].end(), printer);
}

} // namespace tisma
