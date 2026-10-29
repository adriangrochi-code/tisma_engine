// Folder listing for the interface's file browser. Own design; see
// PROVENANCE.md.

#include "ui/files.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <system_error>

namespace tisma::ui {
namespace {

std::string lower(std::string text) {
    for (char& c : text)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return text;
}

} // namespace

std::string utf8(const std::filesystem::path& path) {
    const std::u8string text = path.u8string();
    return std::string(text.begin(), text.end());
}

std::filesystem::path from_utf8(std::string_view text) {
    return std::filesystem::path(std::u8string(text.begin(), text.end()));
}

std::string lower_extension(const std::filesystem::path& path) { return lower(utf8(path.extension())); }

bool list_folder(const std::filesystem::path& folder, const std::vector<std::string>& extensions,
                 std::vector<FolderEntry>& out) {
    out.clear();
    std::error_code ec;
    std::filesystem::directory_iterator it(folder, std::filesystem::directory_options::skip_permission_denied, ec);
    if (ec)
        return false;
    for (const std::filesystem::directory_iterator end; it != end; it.increment(ec)) {
        if (ec)
            break;
        const std::filesystem::directory_entry& entry = *it;
        FolderEntry e;
        e.path = entry.path();
        e.name = utf8(e.path.filename());
        if (e.name.empty() || e.name.front() == '.')
            continue;
        std::error_code type_ec;
        e.folder = entry.is_directory(type_ec);
        if (type_ec)
            continue;
        if (!e.folder) {
            if (!extensions.empty() &&
                std::find(extensions.begin(), extensions.end(), lower_extension(e.path)) == extensions.end())
                continue;
            e.size = entry.file_size(type_ec);
            if (type_ec)
                e.size = 0;
        }
        out.push_back(std::move(e));
    }
    std::sort(out.begin(), out.end(), [](const FolderEntry& a, const FolderEntry& b) {
        if (a.folder != b.folder)
            return a.folder;
        const std::string la = lower(a.name), lb = lower(b.name);
        return la != lb ? la < lb : a.name < b.name;
    });
    return true;
}

std::vector<std::filesystem::path> folder_roots() {
    std::vector<std::filesystem::path> roots;
#ifdef _WIN32
    if (const char* home = std::getenv("USERPROFILE"))
        roots.emplace_back(home);
    for (char drive = 'A'; drive <= 'Z'; ++drive) {
        const std::filesystem::path root = std::string(1, drive) + ":\\";
        std::error_code ec;
        if (std::filesystem::exists(root, ec))
            roots.push_back(root);
    }
#else
    if (const char* home = std::getenv("HOME"))
        roots.emplace_back(home);
    roots.emplace_back("/");
#endif
    return roots;
}

} // namespace tisma::ui
