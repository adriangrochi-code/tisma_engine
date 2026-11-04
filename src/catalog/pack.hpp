#pragma once

// The free profile pack (profile_pack.md, profile_pack_format.md): its index,
// what to download and how to check it, and the printer, filament and print
// profiles it holds. The engine reads the pack as data; the fields with the
// source program's names (original, original_common) are never read. Own
// design; SHA-256 from FIPS 180-4. See PROVENANCE.md.

#include "config/profiles.hpp"

#include <cstdint>
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
};
PackEntry parse_pack_entry(std::string_view json, std::vector<std::string>& warnings);

// compatible.json's sets (lists of printer numbers).
std::vector<std::vector<int>> parse_pack_compatible(std::string_view json);
// Whether an entry is offered for printer number `printer` (rule 12, PF5).
bool pack_compatible(const std::optional<int>& set, const std::vector<std::vector<int>>& sets, int printer);

} // namespace tisma
