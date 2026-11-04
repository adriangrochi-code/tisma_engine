#pragma once

// The free profile pack (profile_pack.md, profile_pack_format.md): its index,
// what to download and how to check it, and the printer, filament and print
// profiles it holds. The engine reads the pack as data; the fields with the
// source program's names (original, original_common) are never read. Own
// design; SHA-256 from FIPS 180-4. See PROVENANCE.md.

#include "config/profiles.hpp"

#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace tisma {

class PackError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

inline constexpr int kPackVersion = 2;
// The pack's repository, linked beside a pack profile's license (rule 15).
inline constexpr const char* kPackRepository = "https://github.com/adriangrochi-code/Tisma-profiles-";
inline constexpr const char* kPackUrl = "https://github.com/adriangrochi-code/Tisma-profiles-/releases/latest/download/";

struct PackFile {
    std::string name;       // as published next to index.json
    std::uint64_t size = 0; // bytes; 0 unknown
    std::string sha256;     // lowercase hex; empty unknown
};

struct PackVariant {
    std::string id;
    double nozzle = 0.0;
};

struct PackModel {
    std::string id, name, technology, family, file, default_variant;
    std::vector<PackVariant> variants;
    std::string thumbnail, bed_model, bed_texture;
};

struct PackVendor {
    std::string id, name, license, origin;
    std::vector<PackModel> models;
    std::vector<std::string> printers;
    std::vector<std::string> filaments, prints, resins, resin_prints; // paths in the pack's folder
    PackFile archive;
};

struct PackIndex {
    int version = 0;
    std::string pack_version;
    std::vector<PackVendor> vendors;
    std::vector<std::string> printers; // "<model file>#<variant>": a printer's number is its place here
    PackFile compatible, community;
};

// Throws PackError: not a pack index, or a version newer than this program
// reads (PF1: «El paquete de perfiles es de una versión más nueva: actualizá Tisma»).
PackIndex parse_pack_index(std::string_view json);

// What to download (profile_pack.md, rules 1, 2, 20): without an index, the
// index; with it, compatible.json, the community and knowledge archives, and
// each chosen vendor's archive (all of them when `vendors` is empty), with
// the size and SHA-256 the index gives. `installed`: the index already
// installed, to leave out the vendors whose archive did not change.
struct PackDownload {
    std::string url, name;
    std::uint64_t size = 0;
    std::string sha256;
};
std::vector<PackDownload> pack_plan(const PackIndex* index, const std::string& base_url, const std::vector<std::string>& vendors,
                                    const PackIndex* installed = nullptr);

// SHA-256 (FIPS 180-4) as 64 lowercase hex digits.
std::string sha256_hex(std::string_view data);

// Whether an archive's entry may be written (rule 5): relative, under
// vendors/<id>/, community/ or knowledge/, without "..", "." parts or
// backslashes.
bool safe_pack_path(std::string_view path);

// A variant's printer profile (rule 10, PF4): common.settings with the
// variant's settings over them, group by group and key by key, named after
// the variant. Unknown keys are left out, each with a warning (rule 10).
Profile pack_printer_profile(std::string_view model_json, const std::string& variant, std::vector<std::string>& warnings);

// A filament, print or resin entry's profile (rule 10, PF6): its "profile",
// unknown keys left out with a warning; "original" is never read.
struct PackEntry {
    Profile profile;
    std::optional<int> compatible; // a set of compatible.json; none: every printer
    bool generic = false;
    std::string license, origin;
    std::string vendor; // the entry's maker, as the community data names it
    // A resin ("resin") or resin print ("resin_print") profile: the engine
    // keeps it as a filament or print profile (the resin's keys are those
    // kinds', docs/PROFILES.md), for a resin printer only.
    bool resin = false;
    // The printer's settings the entry carries as a JSON object of groups
    // ("{}" when none): a pack print profile keeps the speeds, and a filament
    // cooling.min_speed, which the engine keeps in the printer profile.
    // pack_profiles lays them over the printer's.
    std::string printer_settings = "{}";
};
PackEntry parse_pack_entry(std::string_view json, std::vector<std::string>& warnings);

// compatible.json's sets (lists of printer numbers).
std::vector<std::vector<int>> parse_pack_compatible(std::string_view json);
// Whether an entry is offered for printer number `printer` (rule 12, PF5).
bool pack_compatible(const std::optional<int>& set, const std::vector<std::vector<int>>& sets, int printer);

// Installs the pack from `staging`, a folder with the downloaded files under
// their published names (index.json first), into `pack_dir` (rules 4, 5, 7,
// 8, 20, 21). Each file is checked against the index's size and SHA-256 and
// each archive entry against rule 5; a file that fails is discarded with a
// warning and the earlier install of it is kept (PF2, PF3, PK3, PK4). What
// was not downloaded is kept from the earlier install. The new pack is built
// beside the old one and switched in one rename (rule 7, PK14). Throws
// PackError when the index cannot be read (PF1) or the folder not written;
// then nothing changed. `progress` gets one line per step.
struct PackInstallReport {
    std::string pack_version;
    std::vector<std::string> installed, kept, discarded; // file names
    std::vector<std::string> warnings;
};
PackInstallReport pack_install(const std::filesystem::path& pack_dir, const std::filesystem::path& staging,
                               const std::function<void(const std::string&)>& progress = {});

// The installed pack's index, each vendor's archive SHA-256 being the one
// really installed (empty when not installed), for pack_plan's `installed`.
// Throws PackError when there is none.
PackIndex pack_installed(const std::filesystem::path& pack_dir);

// A pack printer with its filament and print profile (rules 10 to 13, PK6,
// PK7): `printer` is "<model file>#<variant>" as the index names it (the file
// with or without "vendors/" in front); `filament` and `print` are paths in the
// pack, and when left empty the variant's defaults (its first filament, its
// print profile; for a resin printer, its first resin and its resin print
// profile), if it has them. Throws PackError: the printer not in the
// index or not installed, a path not in the pack, a profile of another kind,
// or a filament or print profile not for this printer (compatible.json).
struct PackProfiles {
    int number = -1; // the printer's number in the index
    Profile printer;
    std::optional<Profile> filament, print;
    std::string filament_path, print_path;
    std::string filament_vendor; // the filament entry's maker (community data)
    // The printer profile has the printer settings its filament and print
    // profile carry (their speeds), over the variant's (BH27).
    bool resin = false;          // a resin printer: the filament is its resin
    // The printer settings the filament and the print entry carry
    // (PackEntry::printer_settings), already laid over `printer`.
    std::string filament_printer_settings = "{}", print_printer_settings = "{}";
    // The filament's and the print entry's license and origin (rule 15).
    std::string filament_license, filament_origin, print_license, print_origin;
    std::vector<std::string> warnings; // keys left out
};
PackProfiles pack_profiles(const std::filesystem::path& pack_dir, const PackIndex& index, const std::string& printer,
                           const std::string& filament = {}, const std::string& print = {});

// The pack's models whose vendor or name has every word of `query` (case
// apart), vendor by vendor as the index lists them; all for an empty query.
struct PackModelRef {
    const PackVendor* vendor = nullptr;
    const PackModel* model = nullptr;
};
std::vector<PackModelRef> find_pack_models(const PackIndex& index, const std::string& query);
// Whether printer number `printer` of the index is a resin printer (its
// model's technology is "sla").
bool pack_printer_is_resin(const PackIndex& index, int printer);

// The filaments (for a resin printer, the resins) of the installed vendors
// offered for printer number `printer` (rule 12, PK7): each with its path in the pack and its profile's
// name, vendor by vendor as the index lists them. Entries that cannot be read
// are left out.
struct PackFilamentRef {
    std::string path, name, vendor;
};
std::vector<PackFilamentRef> pack_filaments_for(const std::filesystem::path& pack_dir, const PackIndex& index, int printer);
// The same for the print profiles (for a resin printer, the resin print
// profiles), rule 12.
std::vector<PackFilamentRef> pack_prints_for(const std::filesystem::path& pack_dir, const PackIndex& index, int printer);
// One filament or print profile of the pack by its path (rule 10).
Profile pack_entry_profile(const std::filesystem::path& pack_dir, const std::string& path, std::vector<std::string>& warnings);
PackEntry read_pack_entry(const std::filesystem::path& pack_dir, const std::string& path, std::vector<std::string>& warnings);

// The community data (profile_pack.md, rules 22 to 24; profile_pack_format.md,
// «Datos de la comunidad»): values for a printer, filament or resin, each
// with its kind and source.
enum class CommunityKind { Official, Measured, Community };
struct CommunitySource {
    std::string type, url, title, author, published, at, accessed;
};
struct CommunityValue {
    std::string key;
    std::string value; // as JSON text: 0.4, "PEI", [1, 2]
    std::string unit, conditions;
    CommunityKind kind = CommunityKind::Community;
    CommunitySource source;
};
// The keys a value may have besides the engine's settings (an unknown key is
// left out).
bool community_key_known(const std::string& key);
// One community file; values with an unknown key or kind are left out.
std::vector<CommunityValue> parse_community(std::string_view json);
// The file for a printer ("printers"), filament ("filaments") or resin
// ("resins") by its vendor and model or name; empty when there is none.
std::vector<CommunityValue> pack_community(const std::filesystem::path& pack_dir, const std::string& group,
                                           const std::string& vendor, const std::string& name);
// Rule 24: whether a value may fill the setting by itself (an official one,
// for an engine setting the profile leaves empty); measured and community
// values only with the person's confirmation.
bool community_fills_alone(const CommunityValue& v, bool setting_empty);

} // namespace tisma
