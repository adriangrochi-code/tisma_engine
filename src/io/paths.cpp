// Output paths: links followed, special files refused, inputs kept. Own
// design; see PROVENANCE.md.

#include "io/paths.hpp"

#include <system_error>

namespace tisma {

std::filesystem::path write_target(const std::filesystem::path& path) {
    std::error_code ec;
    std::filesystem::path at = path;
    // A chain of links, each relative to the folder of the one before; a
    // loop stops at the limit and is left to the write to fail on.
    for (int hops = 0; hops < 40 && std::filesystem::is_symlink(at, ec); ++hops) {
        const std::filesystem::path next = std::filesystem::read_symlink(at, ec);
        if (ec)
            break;
        at = next.is_absolute() ? next : at.parent_path() / next;
    }
    return at;
}

std::string output_problem(const std::filesystem::path& path) {
    std::error_code ec;
    const std::filesystem::file_status st = std::filesystem::status(path, ec);
    if (ec || !std::filesystem::exists(st) || std::filesystem::is_regular_file(st))
        return {};
    if (std::filesystem::is_directory(st))
        return "is a folder";
    return "is not a regular file (a device, a pipe or a socket)";
}

bool same_file(const std::filesystem::path& a, const std::filesystem::path& b) {
    std::error_code ec;
    if (std::filesystem::exists(a, ec) && std::filesystem::exists(b, ec))
        return std::filesystem::equivalent(a, b, ec) && !ec;
    const std::filesystem::path x = std::filesystem::weakly_canonical(write_target(a), ec);
    const std::filesystem::path y = std::filesystem::weakly_canonical(write_target(b), ec);
    return !ec && x == y;
}

} // namespace tisma
