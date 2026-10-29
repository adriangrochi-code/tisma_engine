#pragma once

#include <filesystem>
#include <string>
#include <system_error>

#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#endif

// The temporary folder of this test process, removed when it ends: two runs
// of the tests on one machine at once (two build trees) do not write each
// other's files.
inline std::filesystem::path test_temp_dir() {
    struct Folder {
        std::filesystem::path path;
        Folder() {
#ifdef _WIN32
            const long pid = long(_getpid());
#else
            const long pid = long(getpid());
#endif
            path = std::filesystem::temp_directory_path() / ("tisma_tests_" + std::to_string(pid));
            std::filesystem::create_directories(path);
        }
        ~Folder() {
            std::error_code ignored;
            std::filesystem::remove_all(path, ignored);
        }
    };
    static const Folder folder;
    return folder.path;
}
