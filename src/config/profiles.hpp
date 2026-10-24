#pragma once

#include "config/settings.hpp"

#include <array>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// Named profiles (docs/PROFILES.md): the settings split in three parts, the
// printer, the filament and the print, each saved and chosen on its own.
// Own design; see PROVENANCE.md.
namespace tisma {

enum class ProfileKind { Printer, Filament, Print };
inline constexpr std::array<ProfileKind, 3> kProfileKinds{ProfileKind::Printer, ProfileKind::Filament,
                                                          ProfileKind::Print};

// "printer", "filament", "print".
std::string_view profile_kind_name(ProfileKind kind);
std::optional<ProfileKind> profile_kind_from_name(std::string_view name);

// The kind a setting belongs to, by its group and key in the settings JSON
// (docs/CONFIG.md); for machine.variables, by the variable's name: those
// with "temperature" in it belong to the filament, the rest to the printer.
// None for the height changes, which belong to a calibration, not a profile.
std::optional<ProfileKind> setting_kind(std::string_view group, std::string_view key,
                                        std::string_view variable = {});

struct Profile {
    ProfileKind kind = ProfileKind::Print;
    std::string name;
    Settings settings; // only the settings of `kind` are the profile's; the rest are defaults
};

// The settings of one kind, as settings JSON with only those keys.
std::string profile_settings_json(ProfileKind kind, const Settings& settings);

// `settings` with every setting of `kind` taken from `from`, the rest kept.
Settings with_profile(const Settings& settings, ProfileKind kind, const Settings& from);

// Whether a and b agree in every setting of `kind`.
bool same_profile(ProfileKind kind, const Settings& a, const Settings& b);

// A profile file: {"format": "tisma-profile", "version": 1, "kind", "name",
// "settings": {only the settings of that kind}}.
std::string profile_file_json(const Profile& profile);
// Throws SettingsError naming the key, also for a setting of another kind.
Profile parse_profile(std::string_view json);

// User profiles are files under a root folder, one folder per kind:
// <root>/<kind>/<name>.json, the name made safe for a file name; the file
// keeps the real name. Sorted by name; unreadable files are skipped and
// described in `errors` when given.
std::vector<Profile> load_profiles(const std::filesystem::path& root, ProfileKind kind,
                                   std::vector<std::string>* errors = nullptr);
// Saves over the profile of the same kind and name, or in a new file. Throws
// SettingsError if it cannot be written.
void save_profile(const std::filesystem::path& root, const Profile& profile);
// Removes the profile of that kind and name; false if there was none.
bool remove_profile(const std::filesystem::path& root, ProfileKind kind, const std::string& name);

} // namespace tisma
