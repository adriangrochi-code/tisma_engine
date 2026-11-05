// Own design; see PROVENANCE.md.

#include "ui/session_marks.hpp"

#include <cstdint>
#include <fstream>
#include <iterator>
#include <system_error>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <cerrno>
#include <csignal>
#include <unistd.h>
#endif

namespace tisma::ui {

long current_process_id() {
#ifdef _WIN32
    return long(GetCurrentProcessId());
#else
    return long(getpid());
#endif
}

bool process_running(long id) {
    if (id <= 0)
        return false;
#ifdef _WIN32
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, DWORD(id));
    if (!h)
        return GetLastError() == ERROR_ACCESS_DENIED; // there, but not ours to look at
    DWORD code = 0;
    const bool running = GetExitCodeProcess(h, &code) && code == STILL_ACTIVE;
    CloseHandle(h);
    return running;
#else
    if (id > long(INT32_MAX))
        return false;
    return kill(pid_t(id), 0) == 0 || errno == EPERM;
#endif
}

namespace {

// The process id of "session-<id>.lock", or 0.
long mark_id(const std::filesystem::path& file) {
    const std::string name = file.filename().string();
    constexpr std::string_view head = "session-", tail = ".lock";
    if (name.size() <= head.size() + tail.size() || !name.starts_with(head) || !name.ends_with(tail))
        return 0;
    const std::string digits = name.substr(head.size(), name.size() - head.size() - tail.size());
    if (digits.empty() || digits.size() > 18 || digits.find_first_not_of("0123456789") != std::string::npos)
        return 0;
    return std::stol(digits);
}

std::filesystem::path log_of(const std::filesystem::path& lock) {
    std::filesystem::path log = lock;
    log.replace_extension(".log");
    return log;
}

} // namespace

SessionMark mark_session(const std::filesystem::path& folder) {
    std::error_code ec;
    std::filesystem::create_directories(folder, ec);
    const std::string name = "session-" + std::to_string(current_process_id());
    SessionMark mark{folder / (name + ".lock"), folder / (name + ".log")};
    std::ofstream lock(mark.lock, std::ios::binary | std::ios::trunc);
    std::ofstream log(mark.log, std::ios::binary | std::ios::trunc);
    if (!lock || !log) {
        std::filesystem::remove(mark.lock, ec);
        std::filesystem::remove(mark.log, ec);
        return {};
    }
    return mark;
}

void end_session(const SessionMark& mark) {
    std::error_code ec;
    if (!mark.log.empty())
        std::filesystem::remove(mark.log, ec);
    if (!mark.lock.empty())
        std::filesystem::remove(mark.lock, ec);
}

std::optional<std::string> closed_session_log(const std::filesystem::path& folder, std::size_t max_bytes) {
    std::error_code ec;
    std::vector<std::filesystem::path> gone;
    for (std::filesystem::directory_iterator it(folder, ec), end; !ec && it != end; it.increment(ec)) {
        const long id = mark_id(it->path());
        if (id > 0 && id != current_process_id() && !process_running(id))
            gone.push_back(it->path());
    }
    std::optional<std::string> newest;
    std::filesystem::file_time_type newest_time{};
    for (const std::filesystem::path& lock : gone) {
        const std::filesystem::path log = log_of(lock);
        std::error_code tc;
        const auto time = std::filesystem::last_write_time(lock, tc);
        if (!newest || (!tc && time > newest_time)) {
            std::ifstream in(log, std::ios::binary);
            std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
            if (text.size() > max_bytes) {
                text.erase(0, text.size() - max_bytes);
                text.erase(0, text.find('\n') + 1); // from a line's start
            }
            newest = std::move(text);
            newest_time = tc ? newest_time : time;
        }
        std::filesystem::remove(log, ec);
        std::filesystem::remove(lock, ec);
    }
    return newest;
}

} // namespace tisma::ui
