#pragma once
// A file of the shared library brought from Tisma Cloud's seeder over HTTPS
// (p2p_library.md, rule 8 octies, provisional until the peer network): a
// ticket for the account, the list of the file's pieces checked against the
// signed index's piece root, each piece checked against the list, and the
// whole file against its SHA-256. Own design; see PROVENANCE.md.
#include "library/shared_index.hpp"

#include <atomic>
#include <filesystem>
#include <functional>
#include <string>

namespace tisma::library {

struct FetchResult {
    bool ok = false;
    // login (no session or it expired), not_available (404), blocked (403,
    // 451), busy (429), bad (a piece or the list does not match), failed.
    std::string code;
    std::string error; // in English, for the log
};

// Brings `file` of `model` into `out`. `server` is the cloud's address,
// `session` the account's token. `progress` gets 0 to 1; `cancel` stops it.
// Nothing is left at `out` unless the whole file matched.
FetchResult fetch_shared_file(const std::string& server, const std::string& session, const SharedModel& model,
                              const SharedFile& file, const std::filesystem::path& out,
                              const std::function<void(double)>& progress = {}, const std::atomic<bool>* cancel = nullptr);

} // namespace tisma::library
