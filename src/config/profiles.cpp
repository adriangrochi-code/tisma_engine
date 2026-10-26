// Named profiles (docs/PROFILES.md), on nlohmann/json (MIT). Own design; see
// PROVENANCE.md.

#include "config/profiles.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <cstring>
#include <fstream>
#include <sstream>

namespace tisma {
namespace {

using nlohmann::ordered_json;

constexpr int kProfileVersion = 1;

// A path as UTF-8 text for messages (string() can throw on Windows for
// names the code page cannot hold).
std::string text_of(const std::filesystem::path& path) {
    const std::u8string text = path.u8string();
    return std::string(text.begin(), text.end());
}

[[noreturn]] void fail(const std::string& message) { throw SettingsError("profile: " + message); }

ordered_json settings_object(const Settings& s) { return ordered_json::parse(settings_json(s)); }

bool is_temperature(std::string_view variable) { return variable.find("temperature") != std::string_view::npos; }

std::filesystem::path utf8_path(const std::string& text) {
    return std::filesystem::path(std::u8string(reinterpret_cast<const char8_t*>(text.data()), text.size()));
}

// The name as a file name that works on every system: no control or reserved
// characters, no dot or space at the ends, not a reserved device name, and
// at most 100 bytes without cutting a UTF-8 sequence.
std::string file_stem(const std::string& name) {
    std::string out;
    for (const char ch : name) {
        const auto c = static_cast<unsigned char>(ch);
        out += c < 0x20 || std::strchr("<>:\"/\\|?*", c) ? '_' : ch;
    }
    if (out.size() > 100) {
        std::size_t end = 100;
        while (end > 0 && (static_cast<unsigned char>(out[end]) & 0xC0) == 0x80)
            --end;
        out.resize(end);
    }
    while (!out.empty() && (out.back() == ' ' || out.back() == '.'))
        out.pop_back();
    while (!out.empty() && (out.front() == ' ' || out.front() == '.'))
        out.erase(out.begin());
    // Windows reserves the device names also with an extension ("nul.txt").
    std::string upper;
    for (const char c : out.substr(0, out.find('.')))
        upper += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    while (!upper.empty() && upper.back() == ' ')
        upper.pop_back();
    const bool device = upper == "CON" || upper == "PRN" || upper == "AUX" || upper == "NUL" ||
                        (upper.size() == 4 && (upper.starts_with("COM") || upper.starts_with("LPT")) &&
                         upper[3] >= '1' && upper[3] <= '9');
    if (out.empty())
        out = "profile";
    else if (device)
        out.insert(0, "_");
    return out;
}

std::string read_file(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in)
        fail("cannot open '" + text_of(path) + "'");
    std::ostringstream text;
    text << in.rdbuf();
    return text.str();
}

// The profile files of a kind, with the profile each one holds.
std::vector<std::pair<std::filesystem::path, Profile>> profile_files(const std::filesystem::path& root, ProfileKind kind,
                                                                     std::vector<std::string>* errors) {
    std::vector<std::pair<std::filesystem::path, Profile>> out;
    const std::filesystem::path folder = root / std::string(profile_kind_name(kind));
    std::error_code ec;
    if (!std::filesystem::is_directory(folder, ec))
        return out;
    for (const auto& entry : std::filesystem::directory_iterator(folder, ec)) {
        if (!entry.is_regular_file(ec) || entry.path().extension() != ".json")
            continue;
        try {
            Profile p = parse_profile(read_file(entry.path()));
            if (p.kind != kind)
                fail("a " + std::string(profile_kind_name(p.kind)) + " profile in the " +
                     std::string(profile_kind_name(kind)) + " folder");
            out.emplace_back(entry.path(), std::move(p));
        } catch (const std::exception& e) {
            if (errors)
                errors->push_back(text_of(entry.path()) + ": " + e.what());
        }
    }
    std::sort(out.begin(), out.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
    return out;
}

} // namespace

std::string_view profile_kind_name(ProfileKind kind) {
    switch (kind) {
    case ProfileKind::Printer:
        return "printer";
    case ProfileKind::Filament:
        return "filament";
    case ProfileKind::Print:
        return "print";
    }
    return "print";
}

std::optional<ProfileKind> profile_kind_from_name(std::string_view name) {
    for (const ProfileKind kind : kProfileKinds)
        if (profile_kind_name(kind) == name)
            return kind;
    return std::nullopt;
}

std::optional<ProfileKind> setting_kind(std::string_view group, std::string_view key, std::string_view variable) {
    if (group == "machine") {
        if (key == "filament_diameter")
            return ProfileKind::Filament;
        if (key == "variables" && is_temperature(variable))
            return ProfileKind::Filament;
        return ProfileKind::Printer;
    }
    if (group == "retraction" || group == "gcode")
        return ProfileKind::Printer;
    if (group == "cooling" || group == "extrusion" || group == "compensation")
        return ProfileKind::Filament;
    if (group == "layers" || group == "slicing" || group == "walls" || group == "surfaces" || group == "infill" ||
        group == "speed" || group == "adhesion" || group == "support" || group == "compute" || group == "zaa" ||
        group == "fuzzy")
        return ProfileKind::Print;
    return std::nullopt;
}

std::string profile_settings_json(ProfileKind kind, const Settings& settings) {
    const ordered_json all = settings_object(settings);
    ordered_json out = ordered_json::object();
    for (const auto& [group, keys] : all.items()) {
        if (!keys.is_object())
            continue;
        for (const auto& [key, value] : keys.items()) {
            if (group == "machine" && key == "variables") {
                if (kind == ProfileKind::Print)
                    continue;
                ordered_json variables = ordered_json::object();
                for (const auto& [name, v] : value.items())
                    if (setting_kind(group, key, name) == kind)
                        variables[name] = v;
                out[group][key] = std::move(variables);
            } else if (setting_kind(group, key) == kind) {
                out[group][key] = value;
            }
        }
    }
    return out.dump(2) + "\n";
}

Settings with_profile(const Settings& settings, ProfileKind kind, const Settings& from) {
    ordered_json out = settings_object(settings);
    const ordered_json source = settings_object(from);
    for (auto& [group, keys] : out.items()) {
        if (!keys.is_object())
            continue;
        for (auto& [key, value] : keys.items()) {
            if (group == "machine" && key == "variables") {
                ordered_json variables = ordered_json::object();
                for (const auto& [name, v] : value.items())
                    if (setting_kind(group, key, name) != kind)
                        variables[name] = v;
                for (const auto& [name, v] : source.at(group).at(key).items())
                    if (setting_kind(group, key, name) == kind)
                        variables[name] = v;
                value = std::move(variables);
            } else if (setting_kind(group, key) == kind) {
                value = source.at(group).at(key);
            }
        }
    }
    Settings result = parse_settings(out.dump());
    result.ignored_groups = settings.ignored_groups;
    return result;
}

bool same_profile(ProfileKind kind, const Settings& a, const Settings& b) {
    return profile_settings_json(kind, a) == profile_settings_json(kind, b);
}

std::string profile_file_json(const Profile& profile) {
    const ordered_json root = {
        {"format", "tisma-profile"},
        {"version", kProfileVersion},
        {"kind", std::string(profile_kind_name(profile.kind))},
        {"name", profile.name},
        {"settings", ordered_json::parse(profile_settings_json(profile.kind, profile.settings))},
    };
    return root.dump(2) + "\n";
}

static Profile parse_profile_checked(std::string_view text) {
    ordered_json root;
    try {
        root = ordered_json::parse(text.begin(), text.end());
    } catch (const ordered_json::parse_error& e) {
        fail(std::string("not valid JSON: ") + e.what());
    }
    if (!root.is_object() || root.value("format", std::string()) != "tisma-profile")
        fail("not a profile (\"format\" must be \"tisma-profile\")");
    if (!root.contains("version") || !root.at("version").is_number_integer())
        fail("version: must be an integer");
    if (root.at("version").get<int>() > kProfileVersion)
        fail("version " + std::to_string(root.at("version").get<int>()) + " is newer than this program reads");
    Profile p;
    const auto kind = profile_kind_from_name(root.value("kind", std::string()));
    if (!kind)
        fail("kind: must be printer, filament or print");
    p.kind = *kind;
    if (!root.contains("name") || !root.at("name").is_string() || root.at("name").get<std::string>().empty())
        fail("name: must be a text that is not empty");
    p.name = root.at("name").get<std::string>();
    if (!root.contains("settings") || !root.at("settings").is_object())
        fail("settings: must be an object");
    const ordered_json& settings = root.at("settings");
    const std::string kind_name(profile_kind_name(p.kind));
    for (const auto& [group, keys] : settings.items()) {
        if (group == "changes")
            fail("changes: height changes belong to a calibration or a project, not a profile");
        if (!keys.is_object())
            continue; // parse_settings names the error
        for (const auto& [key, value] : keys.items()) {
            if (group == "machine" && key == "variables" && value.is_object()) {
                for (const auto& [name, v] : value.items())
                    if (setting_kind(group, key, name) != p.kind)
                        fail(group + "." + key + "." + name + ": not a " + kind_name + " setting");
            } else if (const auto k = setting_kind(group, key); k && *k != p.kind) {
                fail(group + "." + key + ": a " + std::string(profile_kind_name(*k)) + " setting, not a " + kind_name +
                     " one");
            }
        }
    }
    p.settings = parse_settings(settings.dump());
    return p;
}

Profile parse_profile(std::string_view text) {
    if (!json_depth_ok(text))
        fail("nested more than 64 levels deep");
    try {
        return parse_profile_checked(text);
    } catch (const nlohmann::json::exception& e) {
        fail(std::string("not a valid profile: ") + e.what());
    }
}

std::vector<Profile> load_profiles(const std::filesystem::path& root, ProfileKind kind, std::vector<std::string>* errors) {
    std::vector<Profile> out;
    for (auto& [path, profile] : profile_files(root, kind, errors)) {
        const bool repeated = std::any_of(out.begin(), out.end(), [&](const Profile& p) { return p.name == profile.name; });
        if (repeated) {
            if (errors)
                errors->push_back(text_of(path) + ": profile: another file has the name '" + profile.name + "'");
            continue;
        }
        out.push_back(std::move(profile));
    }
    std::sort(out.begin(), out.end(), [](const Profile& a, const Profile& b) {
        return std::lexicographical_compare(a.name.begin(), a.name.end(), b.name.begin(), b.name.end(), [](char x, char y) {
            return std::tolower(static_cast<unsigned char>(x)) < std::tolower(static_cast<unsigned char>(y));
        });
    });
    return out;
}

void save_profile(const std::filesystem::path& root, const Profile& profile) {
    if (profile.name.empty())
        fail("name: must be a text that is not empty");
    const std::filesystem::path folder = root / std::string(profile_kind_name(profile.kind));
    std::error_code ec;
    std::filesystem::create_directories(folder, ec);
    // Over the file that holds this name, or a new file.
    std::filesystem::path path;
    for (const auto& [file, p] : profile_files(root, profile.kind, nullptr))
        if (p.name == profile.name)
            path = file;
    if (path.empty()) {
        const std::string stem = file_stem(profile.name);
        path = folder / utf8_path(stem + ".json");
        for (int i = 2; std::filesystem::exists(path, ec); ++i)
            path = folder / utf8_path(stem + " (" + std::to_string(i) + ").json");
    }
    // Written aside and then moved over, so a failed write leaves the old file.
    std::filesystem::path temporary = path;
    temporary += ".tmp";
    {
        std::ofstream out(temporary, std::ios::binary);
        out << profile_file_json(profile);
        if (!out)
            fail("cannot write '" + text_of(temporary) + "'");
    }
    std::filesystem::rename(temporary, path, ec);
    if (ec) {
        std::filesystem::remove(temporary, ec);
        fail("cannot write '" + text_of(path) + "'");
    }
}

bool remove_profile(const std::filesystem::path& root, ProfileKind kind, const std::string& name) {
    for (const auto& [file, p] : profile_files(root, kind, nullptr)) {
        if (p.name == name) {
            std::error_code ec;
            return std::filesystem::remove(file, ec);
        }
    }
    return false;
}

} // namespace tisma
