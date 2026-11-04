#pragma once

// Downloading the profile pack (profile_pack.md, rules 1, 7, 19, 20, 21) for
// the interface: the plan's files into a folder, then pack_install. In
// tisma_net; the command-line engine never downloads. Own design.

#include "catalog/pack.hpp"

#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace tisma {

// Downloads each file of the plan into `staging` under its name. A redirect
// is followed up to 5 times (the latest-release links answer with one), never
// from https down to http. A file of known size must have that size. Returns
// the errors, one per file that could not be downloaded ("<name>: <why>").
std::vector<std::string> pack_fetch(const std::vector<PackDownload>& plan, const std::filesystem::path& staging,
                                    const std::function<void(const std::string&)>& progress = {});

// Which vendors an update brings: `all`, or the `chosen` ones plus the ones
// already installed (rule 20). With none, only the index, compatible.json and
// the community and knowledge archives (rules 1 and 21).
struct PackChoice {
    bool all = false;
    std::vector<std::string> chosen;
};

// The whole update: the index into `<pack_dir>.download`, the plan against
// what is installed, the rest, then pack_install, and the download folder
// removed. Throws PackError when the index cannot be downloaded (offline,
// PK2) or read (PF1); nothing changes then.
PackInstallReport pack_update(const std::filesystem::path& pack_dir, const std::string& base_url, const PackChoice& choice,
                              const std::function<void(const std::string&)>& progress = {});

// Whether a newer pack is published (rule 19): only the index is downloaded.
// Empty when it is the installed one or the index cannot be had; else the
// published pack_version.
std::string pack_newer(const std::filesystem::path& pack_dir, const std::string& base_url);

} // namespace tisma
