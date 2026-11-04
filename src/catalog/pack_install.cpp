// Installing the profile pack from a folder of downloaded files
// (profile_pack.md, rules 4, 5, 7, 8, 20 and 21). Own design.
#include "catalog/pack.hpp"
#include "catalog/xz.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <system_error>

namespace tisma {

namespace fs = std::filesystem;

namespace {

// A UTF-8 path from the pack (tar names, community files), the same on
// Windows, where a narrow string is the ANSI code page.
fs::path utf8_path(const std::string& text) {
    return fs::path(std::u8string(reinterpret_cast<const char8_t*>(text.data()), text.size()));
}

constexpr std::uint64_t kMaxEntry = 50ull << 20;   // rule 5: a file over 50 MB discards the archive
constexpr std::size_t kMaxArchive = std::size_t(1) << 30; // what one archive may unpack to

bool read_all(const fs::path& path, std::string& data) {
    std::ifstream in(path, std::ios::binary);
    if (!in)
        return false;
    std::ostringstream s;
    s << in.rdbuf();
    data = s.str();
    return bool(in) || in.eof();
}

bool write_all(const fs::path& path, std::string_view data) {
    std::error_code ec;
    fs::create_directories(path.parent_path(), ec);
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out.write(data.data(), std::streamsize(data.size()));
    return bool(out);
}

// Whether a downloaded file is the one the index names (rule 4).
std::string check_file(const std::string& data, const PackFile& expected) {
    if (expected.size != 0 && data.size() != expected.size)
        return "size " + std::to_string(data.size()) + " instead of " + std::to_string(expected.size);
    if (!expected.sha256.empty()) {
        std::string want = expected.sha256;
        std::transform(want.begin(), want.end(), want.begin(), [](unsigned char c) { return char(std::tolower(c)); });
        if (sha256_hex(data) != want)
            return "SHA-256 does not match the index";
    }
    return {};
}

// Unpacks one archive in memory and checks every entry before anything is
// written (rule 5): all paths safe and under `prefix`, no links, none over
// 50 MB. Empty on success, else why the whole archive is refused.
std::string unpack(const std::string& packed, const std::string& prefix, std::vector<TarEntry>& files) {
    std::string tar, error;
    if (!xz_decompress(packed, tar, error, kMaxArchive))
        return error;
    std::vector<std::string> skipped;
    if (!tar_read(tar, files, skipped, error))
        return error;
    if (!skipped.empty())
        return "a link or special file inside: " + skipped.front();
    for (const TarEntry& f : files) {
        if (!safe_pack_path(f.path) || !f.path.starts_with(prefix))
            return "an unsafe path inside: " + f.path;
        if (f.data.size() > kMaxEntry)
            return "a file over 50 MB inside: " + f.path;
    }
    return {};
}

// The archives installed and their SHA-256 (installed.json), so an update
// downloads again the ones that were refused (rule 20).
std::map<std::string, std::string> read_installed(const fs::path& pack_dir) {
    std::map<std::string, std::string> out;
    std::string text;
    if (!read_all(pack_dir / "installed.json", text))
        return out;
    try {
        const nlohmann::json j = nlohmann::json::parse(text);
        if (j.contains("archives") && j.at("archives").is_object())
            for (const auto& [name, sha] : j.at("archives").items())
                if (sha.is_string())
                    out[name] = sha.get<std::string>();
    } catch (const nlohmann::json::exception&) {
    }
    return out;
}

} // namespace

PackIndex pack_installed(const fs::path& pack_dir) {
    std::string text;
    if (!read_all(pack_dir / "index.json", text))
        throw PackError("no profile pack installed in " + pack_dir.string());
    PackIndex index = parse_pack_index(text);
    const auto archives = read_installed(pack_dir);
    for (PackVendor& v : index.vendors) {
        const std::string name = v.archive.name.empty() ? v.id + ".tar.xz" : v.archive.name;
        const auto it = archives.find(name);
        v.archive.sha256 = it == archives.end() ? std::string() : it->second;
    }
    return index;
}

PackInstallReport pack_install(const fs::path& pack_dir, const fs::path& staging,
                               const std::function<void(const std::string&)>& progress) {
    const auto say = [&](const std::string& line) {
        if (progress)
            progress(line);
    };
    std::string index_text;
    if (!read_all(staging / "index.json", index_text))
        throw PackError("no index.json in " + staging.string());
    const PackIndex index = parse_pack_index(index_text); // PF1: a newer version stops here, nothing changed

    PackInstallReport report;
    report.pack_version = index.pack_version;
    const fs::path fresh = fs::path(pack_dir.string() + ".new");
    const fs::path old = fs::path(pack_dir.string() + ".old");
    std::error_code ec;
    fs::remove_all(fresh, ec);
    fs::create_directories(fresh, ec);
    if (ec)
        throw PackError("cannot write " + fresh.string() + ": " + ec.message());
    const bool had = fs::exists(pack_dir / "index.json");
    const auto was = had ? read_installed(pack_dir) : std::map<std::string, std::string>();
    std::map<std::string, std::string> now;

    const auto refuse = [&](const std::string& name, const std::string& why) {
        report.discarded.push_back(name);
        report.warnings.push_back(name + ": " + why);
        say("discarded " + name + ": " + why);
    };
    // Keeps what the earlier install had for `name` (its files under `sub`).
    const auto keep = [&](const std::string& name, const fs::path& sub) {
        if (!had || !fs::exists(pack_dir / sub))
            return;
        fs::create_directories((fresh / sub).parent_path(), ec);
        fs::copy(pack_dir / sub, fresh / sub, fs::copy_options::recursive, ec);
        if (ec) {
            report.warnings.push_back(name + ": the earlier files could not be kept: " + ec.message());
            return;
        }
        report.kept.push_back(name);
        const auto it = was.find(name);
        if (it != was.end())
            now[name] = it->second;
    };

    // compatible.json
    {
        const std::string name = index.compatible.name.empty() ? "compatible.json" : index.compatible.name;
        std::string data;
        if (read_all(staging / name, data)) {
            std::string why = check_file(data, index.compatible);
            if (why.empty()) {
                try {
                    (void)parse_pack_compatible(data);
                } catch (const std::exception& e) {
                    why = std::string("not valid: ") + e.what();
                }
            }
            if (why.empty() && write_all(fresh / "compatible.json", data)) {
                report.installed.push_back(name);
                now[name] = sha256_hex(data);
                say("installed " + name);
            } else {
                refuse(name, why.empty() ? "cannot be written" : why);
                keep(name, "compatible.json");
            }
        } else {
            keep(name, "compatible.json");
        }
    }

    // The archives: community, knowledge and each vendor.
    struct Archive {
        std::string name;
        PackFile expected;
        std::string prefix;
    };
    std::vector<Archive> archives;
    archives.push_back({index.community.name.empty() ? "community.tar.xz" : index.community.name, index.community, "community/"});
    archives.push_back({"knowledge.tar.xz", PackFile{}, "knowledge/"}); // rule 21: not in the index
    for (const PackVendor& v : index.vendors)
        archives.push_back({v.archive.name.empty() ? v.id + ".tar.xz" : v.archive.name, v.archive, "vendors/" + v.id + "/"});
    for (const Archive& a : archives) {
        const fs::path sub = fs::path(a.prefix.substr(0, a.prefix.size() - 1));
        std::string data;
        if (!read_all(staging / a.name, data)) {
            keep(a.name, sub); // not downloaded this time: unchanged or not chosen
            continue;
        }
        std::string why = check_file(data, a.expected);
        std::vector<TarEntry> files;
        if (why.empty())
            why = unpack(data, a.prefix, files);
        if (why.empty()) {
            for (const TarEntry& f : files)
                if (!write_all(fresh / utf8_path(f.path), f.data)) {
                    why = "cannot write " + f.path;
                    break;
                }
        }
        if (!why.empty()) {
            fs::remove_all(fresh / sub, ec);
            refuse(a.name, why);
            keep(a.name, sub);
            continue;
        }
        report.installed.push_back(a.name);
        now[a.name] = sha256_hex(data);
        say("installed " + a.name + " (" + std::to_string(files.size()) + " files)");
    }

    // The index as published, and what was really installed.
    nlohmann::json state = {{"pack_version", index.pack_version}, {"archives", nlohmann::json::object()}};
    for (const auto& [name, sha] : now)
        state["archives"][name] = sha;
    if (!write_all(fresh / "index.json", index_text) || !write_all(fresh / "installed.json", state.dump(1))) {
        fs::remove_all(fresh, ec);
        throw PackError("cannot write " + fresh.string());
    }

    // Rule 7: one switch, so a cut install leaves the earlier pack whole.
    fs::remove_all(old, ec);
    if (fs::exists(pack_dir)) {
        fs::rename(pack_dir, old, ec);
        if (ec) {
            fs::remove_all(fresh, ec);
            throw PackError("cannot replace " + pack_dir.string() + ": " + ec.message());
        }
    }
    fs::rename(fresh, pack_dir, ec);
    if (ec) {
        std::error_code back;
        fs::rename(old, pack_dir, back);
        throw PackError("cannot replace " + pack_dir.string() + ": " + ec.message());
    }
    fs::remove_all(old, ec);
    say("pack " + index.pack_version + " ready");
    return report;
}

PackProfiles pack_profiles(const fs::path& pack_dir, const PackIndex& index, const std::string& printer,
                           const std::string& filament, const std::string& print) {
    PackProfiles out;
    const auto hash = printer.rfind('#');
    if (hash == std::string::npos)
        throw PackError("a pack printer is \"<model file>#<variant>\", not \"" + printer + "\"");
    const std::string file = printer.substr(0, hash), variant = printer.substr(hash + 1);
    const auto number = std::find(index.printers.begin(), index.printers.end(), printer);
    if (number == index.printers.end())
        throw PackError("the pack has no printer \"" + printer + "\"");
    out.number = int(number - index.printers.begin());
    // A file of the pack, under vendors/ when the index names it without.
    const auto read_pack = [&](const std::string& path, std::string& data) {
        if (safe_pack_path(path) && read_all(pack_dir / utf8_path(path), data))
            return true;
        return safe_pack_path("vendors/" + path) && read_all(pack_dir / utf8_path("vendors/" + path), data);
    };
    std::string model_text;
    if (!read_pack(file, model_text))
        throw PackError("the printer's model file is not installed: " + file);
    out.printer = pack_printer_profile(model_text, variant, out.warnings);
    // Rule 13: the variant's first filament and its print profile by default
    // (on a resin printer, its first resin and its resin print profile).
    out.filament_path = filament;
    out.print_path = print;
    const nlohmann::json model = nlohmann::json::parse(model_text, nullptr, false);
    out.resin = model.is_object() && model.value("technology", "") == "sla";
    const char* material_key = out.resin ? "resin" : "filament";
    const char* print_key = out.resin ? "resin_print" : "print";
    // A path, or a list whose first one is chosen.
    const auto first_path = [](const nlohmann::json& d, const char* key) {
        if (!d.contains(key))
            return std::string();
        const nlohmann::json& x = d.at(key);
        if (x.is_string())
            return x.get<std::string>();
        if (x.is_array() && !x.empty() && x.front().is_string())
            return x.front().get<std::string>();
        return std::string();
    };
    if (model.is_object() && model.contains("variants") && model.at("variants").is_array())
        for (const nlohmann::json& v : model.at("variants"))
            if (v.is_object() && v.value("id", "") == variant && v.contains("defaults") && v.at("defaults").is_object()) {
                const nlohmann::json& d = v.at("defaults");
                if (out.filament_path.empty())
                    out.filament_path = first_path(d, material_key);
                if (out.print_path.empty())
                    out.print_path = first_path(d, print_key);
            }
    std::vector<std::vector<int>> sets;
    std::string sets_text;
    if (read_all(pack_dir / "compatible.json", sets_text))
        sets = parse_pack_compatible(sets_text);
    for (const auto& [path, kind] : {std::pair{out.filament_path, ProfileKind::Filament}, std::pair{out.print_path, ProfileKind::Print}}) {
        if (path.empty())
            continue;
        std::string text;
        if (!read_pack(path, text))
            throw PackError("not in the installed pack: " + path);
        PackEntry entry = parse_pack_entry(text, out.warnings);
        if (entry.profile.kind != kind)
            throw PackError(path + " is not a " + (kind == ProfileKind::Filament ? "filament" : "print") + " profile");
        if (!pack_compatible(entry.compatible, sets, out.number)) // rule 12, PK7
            throw PackError(entry.profile.name + " is not for the " + out.printer.name);
        if (entry.resin != out.resin)
            throw PackError(entry.profile.name + (out.resin ? " is not a resin profile, and the " : " is a resin profile, and the ") +
                            out.printer.name + (out.resin ? " is a resin printer" : " is not a resin printer"));
        if (kind == ProfileKind::Filament)
            out.filament_vendor = entry.vendor;
        (kind == ProfileKind::Filament ? out.filament : out.print) = std::move(entry.profile);
    }
    return out;
}

std::vector<PackModelRef> find_pack_models(const PackIndex& index, const std::string& query) {
    const auto lower = [](std::string t) {
        std::transform(t.begin(), t.end(), t.begin(), [](unsigned char c) { return char(std::tolower(c)); });
        return t;
    };
    std::vector<std::string> words;
    {
        std::istringstream s(lower(query));
        for (std::string w; s >> w;)
            words.push_back(w);
    }
    std::vector<PackModelRef> out;
    for (const PackVendor& v : index.vendors)
        for (const PackModel& m : v.models) {
            const std::string text = lower(v.name + " " + v.id + " " + m.name);
            if (std::all_of(words.begin(), words.end(), [&](const std::string& w) { return text.find(w) != std::string::npos; }))
                out.push_back({&v, &m});
        }
    return out;
}

bool pack_printer_is_resin(const PackIndex& index, int printer) {
    if (printer < 0 || printer >= int(index.printers.size()))
        return false;
    const std::string& name = index.printers[std::size_t(printer)];
    const std::string file = name.substr(0, name.rfind('#'));
    for (const PackVendor& v : index.vendors)
        for (const PackModel& m : v.models)
            if (m.file == file || "vendors/" + m.file == file || m.file == "vendors/" + file)
                return m.technology == "sla";
    return false;
}

std::vector<PackFilamentRef> pack_filaments_for(const fs::path& pack_dir, const PackIndex& index, int printer) {
    std::vector<std::vector<int>> sets;
    std::string sets_text;
    if (read_all(pack_dir / "compatible.json", sets_text)) {
        try {
            sets = parse_pack_compatible(sets_text);
        } catch (const PackError&) {
        }
    }
    const bool resin = pack_printer_is_resin(index, printer);
    std::vector<PackFilamentRef> out;
    for (const PackVendor& v : index.vendors) {
        if (v.archive.sha256.empty())
            continue; // not installed
        for (const std::string& path : resin ? v.resins : v.filaments) {
            std::string text;
            if (!safe_pack_path(path) || !read_all(pack_dir / utf8_path(path), text))
                continue;
            try {
                std::vector<std::string> ignored;
                const PackEntry e = parse_pack_entry(text, ignored);
                if (e.profile.kind == ProfileKind::Filament && e.resin == resin && pack_compatible(e.compatible, sets, printer))
                    out.push_back({path, e.profile.name, v.name});
            } catch (const std::exception&) {
            }
        }
    }
    return out;
}

PackEntry read_pack_entry(const fs::path& pack_dir, const std::string& path, std::vector<std::string>& warnings) {
    std::string text;
    if (!safe_pack_path(path) || !read_all(pack_dir / utf8_path(path), text))
        throw PackError("not in the installed pack: " + path);
    return parse_pack_entry(text, warnings);
}

Profile pack_entry_profile(const fs::path& pack_dir, const std::string& path, std::vector<std::string>& warnings) {
    return read_pack_entry(pack_dir, path, warnings).profile;
}

namespace {
// The engine's own setting keys ("walls.count"...).
bool engine_setting(const std::string& key) {
    static const std::set<std::string> keys = [] {
        std::set<std::string> out;
        for (const auto& [k, v] : describe(Settings{}))
            out.insert(k);
        return out;
    }();
    return keys.count(key) > 0;
}
} // namespace

bool community_key_known(const std::string& key) {
    static const std::set<std::string> own = {
        "measured.max_flow", "measured.acceleration", "measured.speed", "input_shaper.frequency_x",
        "input_shaper.frequency_y", "input_shaper.type", "head.clearance_radius", "head.gantry_height",
        "head.nozzle_to_fan_height", "bed.mesh_points", "firmware.version", "filament.tensile_strength_xy",
        "filament.tensile_strength_z", "filament.tensile_modulus_xy", "filament.tensile_modulus_z", "filament.elongation_xy",
        "filament.elongation_z", "filament.impact_strength", "filament.hdt", "filament.glass_transition",
        "filament.shrinkage_xy", "filament.shrinkage_z", "filament.drying_temperature", "filament.drying_hours",
        "filament.price_per_kg", "resin.exposure", "resin.bottom_exposure", "resin.viscosity"};
    return own.count(key) > 0 || engine_setting(key);
}

std::vector<CommunityValue> parse_community(std::string_view text) {
    std::vector<CommunityValue> out;
    const nlohmann::json j = nlohmann::json::parse(text, nullptr, false);
    if (!j.is_object() || !j.contains("data") || !j.at("data").is_array())
        return out;
    const auto str = [](const nlohmann::json& o, const char* k) {
        return o.contains(k) && o.at(k).is_string() ? o.at(k).get<std::string>()
               : o.contains(k) && o.at(k).is_number() ? o.at(k).dump()
                                                      : std::string();
    };
    for (const nlohmann::json& d : j.at("data")) {
        if (!d.is_object())
            continue;
        CommunityValue v;
        v.key = str(d, "key");
        if (!community_key_known(v.key) || !d.contains("value"))
            continue;
        const std::string kind = str(d, "kind");
        if (kind == "official")
            v.kind = CommunityKind::Official;
        else if (kind == "measured")
            v.kind = CommunityKind::Measured;
        else if (kind == "community")
            v.kind = CommunityKind::Community;
        else
            continue;
        v.value = d.at("value").dump();
        v.unit = str(d, "unit");
        v.conditions = d.contains("conditions") && !d.at("conditions").is_null()
                           ? (d.at("conditions").is_string() ? d.at("conditions").get<std::string>() : d.at("conditions").dump())
                           : std::string();
        if (d.contains("source") && d.at("source").is_object()) {
            const nlohmann::json& s = d.at("source");
            v.source = {str(s, "type"), str(s, "url"), str(s, "title"), str(s, "author"), str(s, "published"), str(s, "at"),
                        str(s, "accessed")};
        }
        out.push_back(std::move(v));
    }
    return out;
}

std::vector<CommunityValue> pack_community(const fs::path& pack_dir, const std::string& group, const std::string& vendor,
                                           const std::string& name) {
    if (group != "printers" && group != "filaments" && group != "resins")
        return {};
    const std::string path = "community/" + group + "/" + vendor + "/" + name + ".json";
    std::string text;
    if (vendor.find('/') != std::string::npos || name.find('/') != std::string::npos || !safe_pack_path(path) ||
        !read_all(pack_dir / utf8_path(path), text))
        return {};
    return parse_community(text);
}

bool community_fills_alone(const CommunityValue& v, bool setting_empty) {
    return v.kind == CommunityKind::Official && setting_empty && engine_setting(v.key);
}

} // namespace tisma
