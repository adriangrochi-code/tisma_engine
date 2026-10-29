#pragma once

#include <filesystem>
#include <string>

namespace tisma {

// What an output path writes to and whether it may (the spec team's BH15
// and BH16: -o could replace the model, the settings or a device node, and
// replaced a symbolic link instead of the file it points to).

// The file a path writes: a symbolic link's target, followed to the end;
// else the path itself.
std::filesystem::path write_target(const std::filesystem::path& path);

// Why an output cannot be written there, or "" when it can: something that
// exists and is not a regular file (a folder, a device, a FIFO, a socket).
std::string output_problem(const std::filesystem::path& path);

// Whether two paths name the same file: both exist and are the same one
// (links and "./" included), or neither exists and they are the same path
// once made absolute.
bool same_file(const std::filesystem::path& a, const std::filesystem::path& b);

} // namespace tisma
