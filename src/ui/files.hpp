#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace tisma::ui {

// Paths and UTF-8 text: the interface shows and types UTF-8, the file system
// takes native paths (wide on Windows).
std::string utf8(const std::filesystem::path& path);
std::filesystem::path from_utf8(std::string_view text);

// The extension in lower case with its dot (".stl"), or "".
std::string lower_extension(const std::filesystem::path& path);

struct FolderEntry {
    std::string name; // UTF-8
    std::filesystem::path path;
    bool folder = false;
    std::uintmax_t size = 0; // bytes, files only
};

// The folders, then the files whose extension is in `extensions` (lower case,
// with the dot; empty: every file), each group sorted by name ignoring case.
// Hidden entries (a name starting with '.') are left out. Entries that cannot
// be read are skipped; false if the folder itself cannot be read.
bool list_folder(const std::filesystem::path& folder, const std::vector<std::string>& extensions,
                 std::vector<FolderEntry>& out);

// Places to start browsing from: the home folder and, on Windows, the drives
// that exist.
std::vector<std::filesystem::path> folder_roots();

} // namespace tisma::ui
