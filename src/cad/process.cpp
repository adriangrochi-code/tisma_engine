// Running another program with a time limit. Own implementation over
// posix_spawn and pipes (POSIX) and CreateProcess (Win32); see PROVENANCE.md.

#include "cad/process.hpp"

#include <chrono>
#include <cstring>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <thread>
#else
#include <poll.h>
#include <signal.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cerrno>
#include <fcntl.h>

extern char** environ;
#endif

namespace tisma::cad {

std::filesystem::path executable_dir() {
    std::error_code ec;
#ifdef _WIN32
    wchar_t buffer[MAX_PATH];
    const DWORD n = GetModuleFileNameW(nullptr, buffer, MAX_PATH);
    if (n > 0 && n < MAX_PATH)
        return std::filesystem::path(buffer).parent_path();
#else
    const std::filesystem::path self = std::filesystem::read_symlink("/proc/self/exe", ec);
    if (!ec)
        return self.parent_path();
#endif
    const std::filesystem::path here = std::filesystem::current_path(ec);
    return ec ? std::filesystem::path() : here;
}

#ifdef _WIN32

namespace {

// An argument quoted as CommandLineToArgvW reads it back: quotes around one
// with spaces or quotes, backslashes before a quote doubled.
std::wstring quoted(const std::wstring& arg) {
    if (!arg.empty() && arg.find_first_of(L" \t\"") == std::wstring::npos)
        return arg;
    std::wstring out = L"\"";
    std::size_t backslashes = 0;
    for (const wchar_t c : arg) {
        if (c == L'\\') {
            ++backslashes;
            continue;
        }
        if (c == L'"') {
            out.append(2 * backslashes + 1, L'\\');
            out += L'"';
        } else {
            out.append(backslashes, L'\\');
            out += c;
        }
        backslashes = 0;
    }
    out.append(2 * backslashes, L'\\');
    out += L'"';
    return out;
}

std::wstring wide(const std::string& utf8) {
    if (utf8.empty())
        return {};
    const int n = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), int(utf8.size()), nullptr, 0);
    std::wstring out(std::size_t(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.data(), int(utf8.size()), out.data(), n);
    return out;
}

void read_all(HANDLE h, std::string& into) {
    char buffer[4096];
    DWORD got = 0;
    while (ReadFile(h, buffer, sizeof buffer, &got, nullptr) && got > 0)
        into.append(buffer, got);
}

} // namespace

ProcessResult run_process(const std::filesystem::path& exe, const std::vector<std::string>& args, double timeout_seconds) {
    ProcessResult r;
    SECURITY_ATTRIBUTES inherit{};
    inherit.nLength = sizeof inherit;
    inherit.bInheritHandle = TRUE;
    HANDLE out_read = nullptr, out_write = nullptr, err_read = nullptr, err_write = nullptr;
    if (!CreatePipe(&out_read, &out_write, &inherit, 0) || !CreatePipe(&err_read, &err_write, &inherit, 0)) {
        r.error = "cannot make the pipes";
        return r;
    }
    SetHandleInformation(out_read, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(err_read, HANDLE_FLAG_INHERIT, 0);
    std::wstring line = quoted(exe.wstring());
    for (const std::string& a : args)
        line += L" " + quoted(wide(a));
    STARTUPINFOW si{};
    si.cb = sizeof si;
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = INVALID_HANDLE_VALUE;
    si.hStdOutput = out_write;
    si.hStdError = err_write;
    PROCESS_INFORMATION pi{};
    const BOOL made = CreateProcessW(exe.c_str(), line.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr,
                                     nullptr, &si, &pi);
    CloseHandle(out_write);
    CloseHandle(err_write);
    if (!made) {
        r.error = "cannot start " + exe.filename().string() + " (error " + std::to_string(GetLastError()) + ")";
        CloseHandle(out_read);
        CloseHandle(err_read);
        return r;
    }
    r.started = true;
    std::thread out_reader([&] { read_all(out_read, r.out); });
    std::thread err_reader([&] { read_all(err_read, r.err); });
    const DWORD wait = timeout_seconds > 0.0 ? DWORD(timeout_seconds * 1000.0) : INFINITE;
    if (WaitForSingleObject(pi.hProcess, wait) == WAIT_TIMEOUT) {
        TerminateProcess(pi.hProcess, 1);
        WaitForSingleObject(pi.hProcess, INFINITE);
        r.timed_out = true;
        r.error = exe.filename().string() + " did not answer in time";
    }
    out_reader.join();
    err_reader.join();
    DWORD code = 1;
    GetExitCodeProcess(pi.hProcess, &code);
    r.status = int(code);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CloseHandle(out_read);
    CloseHandle(err_read);
    return r;
}

#else

ProcessResult run_process(const std::filesystem::path& exe, const std::vector<std::string>& args, double timeout_seconds) {
    ProcessResult r;
    int out_pipe[2], err_pipe[2];
    if (pipe(out_pipe) != 0) {
        r.error = "cannot make the pipes";
        return r;
    }
    if (pipe(err_pipe) != 0) {
        close(out_pipe[0]), close(out_pipe[1]);
        r.error = "cannot make the pipes";
        return r;
    }
    for (const int fd : {out_pipe[0], out_pipe[1], err_pipe[0], err_pipe[1]})
        fcntl(fd, F_SETFD, FD_CLOEXEC);
    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_addopen(&actions, 0, "/dev/null", O_RDONLY, 0);
    posix_spawn_file_actions_adddup2(&actions, out_pipe[1], 1);
    posix_spawn_file_actions_adddup2(&actions, err_pipe[1], 2);
    std::vector<std::string> words;
    words.push_back(exe.string());
    words.insert(words.end(), args.begin(), args.end());
    std::vector<char*> argv;
    for (std::string& w : words)
        argv.push_back(w.data());
    argv.push_back(nullptr);
    pid_t pid = 0;
    const int spawned = posix_spawn(&pid, words.front().c_str(), &actions, nullptr, argv.data(), environ);
    posix_spawn_file_actions_destroy(&actions);
    close(out_pipe[1]);
    close(err_pipe[1]);
    if (spawned != 0) {
        r.error = "cannot start " + exe.filename().string() + ": " + std::strerror(spawned);
        close(out_pipe[0]);
        close(err_pipe[0]);
        return r;
    }
    r.started = true;
    // Both pipes read until they close, within the time limit.
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::duration<double>(timeout_seconds);
    pollfd fds[2] = {{out_pipe[0], POLLIN, 0}, {err_pipe[0], POLLIN, 0}};
    int open_pipes = 2;
    char buffer[4096];
    while (open_pipes > 0) {
        int wait = -1;
        if (timeout_seconds > 0.0) {
            const auto left = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now()).count();
            if (left <= 0) {
                r.timed_out = true;
                break;
            }
            wait = int(left);
        }
        const int ready = poll(fds, 2, wait);
        if (ready < 0) {
            if (errno == EINTR)
                continue;
            break;
        }
        if (ready == 0) {
            r.timed_out = true;
            break;
        }
        for (int i = 0; i < 2; ++i) {
            if (fds[i].fd < 0 || fds[i].revents == 0)
                continue;
            const ssize_t got = read(fds[i].fd, buffer, sizeof buffer);
            if (got > 0) {
                (i == 0 ? r.out : r.err).append(buffer, std::size_t(got));
            } else if (got == 0 || (got < 0 && errno != EINTR && errno != EAGAIN)) {
                close(fds[i].fd);
                fds[i].fd = -1;
                --open_pipes;
            }
        }
    }
    for (pollfd& f : fds)
        if (f.fd >= 0)
            close(f.fd);
    if (r.timed_out) {
        kill(pid, SIGKILL);
        r.error = exe.filename().string() + " did not answer in time";
    }
    int status = 0;
    while (waitpid(pid, &status, 0) < 0 && errno == EINTR) {
    }
    if (WIFEXITED(status))
        r.status = WEXITSTATUS(status);
    else if (WIFSIGNALED(status)) {
        r.status = 128 + WTERMSIG(status);
        if (!r.timed_out)
            r.error = exe.filename().string() + " ended by signal " + std::to_string(WTERMSIG(status));
    }
    return r;
}

#endif

} // namespace tisma::cad
