// Running another program with a time limit. Own implementation over
// posix_spawn and pipes (POSIX) and CreateProcess (Win32); see PROVENANCE.md.

#include "cad/process.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <deque>
#include <mutex>
#include <thread>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <thread>
#else
#include <poll.h>
#include <pthread.h>
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

ProcessResult run_process(const std::filesystem::path& exe, const std::vector<std::string>& args, double timeout_seconds,
                          const std::string& input) {
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
    HANDLE in_read = INVALID_HANDLE_VALUE, in_write = nullptr;
    if (!input.empty()) {
        if (!CreatePipe(&in_read, &in_write, &inherit, 0)) {
            CloseHandle(out_read), CloseHandle(out_write), CloseHandle(err_read), CloseHandle(err_write);
            r.error = "cannot make the pipes";
            return r;
        }
        SetHandleInformation(in_write, HANDLE_FLAG_INHERIT, 0);
    }
    std::wstring line = quoted(exe.wstring());
    for (const std::string& a : args)
        line += L" " + quoted(wide(a));
    STARTUPINFOW si{};
    si.cb = sizeof si;
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = in_read;
    si.hStdOutput = out_write;
    si.hStdError = err_write;
    PROCESS_INFORMATION pi{};
    const BOOL made = CreateProcessW(exe.c_str(), line.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr,
                                     nullptr, &si, &pi);
    CloseHandle(out_write);
    CloseHandle(err_write);
    if (!input.empty())
        CloseHandle(in_read);
    if (!made) {
        r.error = "cannot start " + exe.filename().string() + " (error " + std::to_string(GetLastError()) + ")";
        CloseHandle(out_read);
        CloseHandle(err_read);
        if (in_write)
            CloseHandle(in_write);
        return r;
    }
    r.started = true;
    // The input (small: a secret or a short text) and then the end of it.
    if (in_write) {
        DWORD written = 0;
        WriteFile(in_write, input.data(), DWORD(input.size()), &written, nullptr);
        CloseHandle(in_write);
    }
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

ProcessResult run_process(const std::filesystem::path& exe, const std::vector<std::string>& args, double timeout_seconds,
                          const std::string& input) {
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
    int in_pipe[2] = {-1, -1};
    if (!input.empty() && pipe(in_pipe) != 0) {
        close(out_pipe[0]), close(out_pipe[1]), close(err_pipe[0]), close(err_pipe[1]);
        r.error = "cannot make the pipes";
        return r;
    }
    for (const int fd : {out_pipe[0], out_pipe[1], err_pipe[0], err_pipe[1], in_pipe[0], in_pipe[1]})
        if (fd >= 0)
            fcntl(fd, F_SETFD, FD_CLOEXEC);
    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    if (in_pipe[0] >= 0)
        posix_spawn_file_actions_adddup2(&actions, in_pipe[0], 0);
    else
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
    if (in_pipe[0] >= 0)
        close(in_pipe[0]);
    if (spawned != 0) {
        r.error = "cannot start " + exe.filename().string() + ": " + std::strerror(spawned);
        close(out_pipe[0]);
        close(err_pipe[0]);
        if (in_pipe[1] >= 0)
            close(in_pipe[1]);
        return r;
    }
    r.started = true;
    // The input (small: it fits the pipe) and then the end of it. A program
    // that left early must not end this one with SIGPIPE.
    if (in_pipe[1] >= 0) {
        // SIGPIPE blocked in this thread only, and taken back if it came.
        sigset_t pipe_signal, before;
        sigemptyset(&pipe_signal);
        sigaddset(&pipe_signal, SIGPIPE);
        pthread_sigmask(SIG_BLOCK, &pipe_signal, &before);
        bool broken = false;
        for (std::size_t sent = 0; sent < input.size();) {
            const ssize_t n = write(in_pipe[1], input.data() + sent, input.size() - sent);
            if (n < 0 && errno == EINTR)
                continue;
            if (n <= 0) {
                broken = n < 0 && errno == EPIPE;
                break;
            }
            sent += std::size_t(n);
        }
        close(in_pipe[1]);
        sigset_t pending;
        sigemptyset(&pending);
        if (broken && sigpending(&pending) == 0 && sigismember(&pending, SIGPIPE) == 1) {
            int taken = 0;
            sigwait(&pipe_signal, &taken);
        }
        pthread_sigmask(SIG_SETMASK, &before, nullptr);
    }
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

// ---- ChildProcess: a reader thread per output; lines handed over under a lock.

struct ChildProcess::Impl {
    mutable std::mutex lock;
    std::condition_variable arrived;
    std::deque<std::string> lines;
    std::string partial, err;
    bool ended = false;
    std::thread out_reader, err_reader;
    // Set by stop(): the readers end even when a process the child started
    // still holds the pipes open (no end of file ever comes then).
    std::atomic<bool> stopping{false}, out_done{false}, err_done{false};
    void join_readers();
#ifdef _WIN32
    HANDLE process = nullptr, in_write = nullptr, out_read = nullptr, err_read = nullptr;
#else
    pid_t pid = -1;
    int in_fd = -1, out_fd = -1, err_fd = -1;
#endif
    void got_out(const char* data, std::size_t n) {
        std::lock_guard<std::mutex> g(lock);
        for (std::size_t i = 0; i < n; ++i) {
            if (data[i] == '\n') {
                if (!partial.empty() && partial.back() == '\r')
                    partial.pop_back();
                lines.push_back(std::move(partial));
                partial.clear();
            } else {
                partial += data[i];
            }
        }
        arrived.notify_all();
    }
    void got_err(const char* data, std::size_t n) {
        std::lock_guard<std::mutex> g(lock);
        err.append(data, n);
        if (err.size() > 65536)
            err.erase(0, err.size() - 65536);
    }
    void out_closed() {
        std::lock_guard<std::mutex> g(lock);
        ended = true;
        arrived.notify_all();
    }
};

ChildProcess::ChildProcess() : impl_(std::make_unique<Impl>()) {}
ChildProcess::~ChildProcess() { stop(); }

std::optional<std::string> ChildProcess::read_line(double timeout_seconds) {
    std::unique_lock<std::mutex> g(impl_->lock);
    const auto until = std::chrono::steady_clock::now() + std::chrono::milliseconds(long(std::max(0.0, timeout_seconds) * 1000.0));
    impl_->arrived.wait_until(g, until, [&] { return !impl_->lines.empty() || impl_->ended; });
    if (impl_->lines.empty())
        return std::nullopt;
    std::string line = std::move(impl_->lines.front());
    impl_->lines.pop_front();
    return line;
}

std::string ChildProcess::stderr_text() const {
    std::lock_guard<std::mutex> g(impl_->lock);
    return impl_->err;
}

#ifdef _WIN32

// A blocked ReadFile is cancelled until the reader sees `stopping`.
void ChildProcess::Impl::join_readers() {
    stopping = true;
    for (std::thread* t : {&out_reader, &err_reader}) {
        if (!t->joinable())
            continue;
        std::atomic<bool>& done = t == &out_reader ? out_done : err_done;
        while (!done) {
            CancelSynchronousIo(t->native_handle());
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        t->join();
    }
}

bool ChildProcess::start(const std::filesystem::path& exe, const std::vector<std::string>& args, std::string& error) {
    stop();
    impl_ = std::make_unique<Impl>();
    SECURITY_ATTRIBUTES inherit{};
    inherit.nLength = sizeof inherit;
    inherit.bInheritHandle = TRUE;
    HANDLE in_read = nullptr, out_write = nullptr, err_write = nullptr;
    if (!CreatePipe(&in_read, &impl_->in_write, &inherit, 0) || !CreatePipe(&impl_->out_read, &out_write, &inherit, 0) ||
        !CreatePipe(&impl_->err_read, &err_write, &inherit, 0)) {
        error = "cannot make the pipes";
        return false;
    }
    SetHandleInformation(impl_->in_write, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(impl_->out_read, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(impl_->err_read, HANDLE_FLAG_INHERIT, 0);
    std::wstring line = quoted(exe.wstring());
    for (const std::string& a : args)
        line += L" " + quoted(wide(a));
    STARTUPINFOW si{};
    si.cb = sizeof si;
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = in_read;
    si.hStdOutput = out_write;
    si.hStdError = err_write;
    PROCESS_INFORMATION pi{};
    const BOOL made = CreateProcessW(exe.c_str(), line.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
    CloseHandle(in_read);
    CloseHandle(out_write);
    CloseHandle(err_write);
    if (!made) {
        error = "cannot start " + exe.filename().string() + " (error " + std::to_string(GetLastError()) + ")";
        CloseHandle(impl_->in_write), CloseHandle(impl_->out_read), CloseHandle(impl_->err_read);
        impl_->in_write = impl_->out_read = impl_->err_read = nullptr;
        return false;
    }
    CloseHandle(pi.hThread);
    impl_->process = pi.hProcess;
    Impl* self = impl_.get();
    self->out_reader = std::thread([self] {
        char buffer[4096];
        DWORD got = 0;
        while (!self->stopping && ReadFile(self->out_read, buffer, sizeof buffer, &got, nullptr) && got > 0)
            self->got_out(buffer, got);
        self->out_closed();
        self->out_done = true;
    });
    self->err_reader = std::thread([self] {
        char buffer[4096];
        DWORD got = 0;
        while (!self->stopping && ReadFile(self->err_read, buffer, sizeof buffer, &got, nullptr) && got > 0)
            self->got_err(buffer, got);
        self->err_done = true;
    });
    return true;
}

bool ChildProcess::write_line(const std::string& line) {
    if (!impl_->in_write || !running())
        return false;
    const std::string data = line + "\n";
    DWORD written = 0;
    return WriteFile(impl_->in_write, data.data(), DWORD(data.size()), &written, nullptr) && written == data.size();
}

bool ChildProcess::running() const {
    return impl_->process && WaitForSingleObject(impl_->process, 0) == WAIT_TIMEOUT;
}

void ChildProcess::stop() {
    if (!impl_)
        return;
    if (impl_->in_write)
        CloseHandle(impl_->in_write), impl_->in_write = nullptr;
    if (impl_->process) {
        if (WaitForSingleObject(impl_->process, 0) == WAIT_TIMEOUT)
            TerminateProcess(impl_->process, 1);
        WaitForSingleObject(impl_->process, INFINITE);
        CloseHandle(impl_->process);
        impl_->process = nullptr;
    }
    impl_->join_readers();
    if (impl_->out_read)
        CloseHandle(impl_->out_read), impl_->out_read = nullptr;
    if (impl_->err_read)
        CloseHandle(impl_->err_read), impl_->err_read = nullptr;
}

#else

// The readers wake every 100 ms to see `stopping`.
void ChildProcess::Impl::join_readers() {
    stopping = true;
    if (out_reader.joinable())
        out_reader.join();
    if (err_reader.joinable())
        err_reader.join();
}

bool ChildProcess::start(const std::filesystem::path& exe, const std::vector<std::string>& args, std::string& error) {
    stop();
    impl_ = std::make_unique<Impl>();
    int in_pipe[2], out_pipe[2], err_pipe[2];
    if (pipe(in_pipe) != 0) {
        error = "cannot make the pipes";
        return false;
    }
    if (pipe(out_pipe) != 0) {
        close(in_pipe[0]), close(in_pipe[1]);
        error = "cannot make the pipes";
        return false;
    }
    if (pipe(err_pipe) != 0) {
        close(in_pipe[0]), close(in_pipe[1]), close(out_pipe[0]), close(out_pipe[1]);
        error = "cannot make the pipes";
        return false;
    }
    for (const int fd : {in_pipe[1], out_pipe[0], err_pipe[0]})
        fcntl(fd, F_SETFD, FD_CLOEXEC);
    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_adddup2(&actions, in_pipe[0], 0);
    posix_spawn_file_actions_adddup2(&actions, out_pipe[1], 1);
    posix_spawn_file_actions_adddup2(&actions, err_pipe[1], 2);
    std::vector<std::string> words{exe.string()};
    words.insert(words.end(), args.begin(), args.end());
    std::vector<char*> argv;
    for (std::string& w : words)
        argv.push_back(w.data());
    argv.push_back(nullptr);
    pid_t pid = -1;
    const int spawned = posix_spawn(&pid, words.front().c_str(), &actions, nullptr, argv.data(), environ);
    posix_spawn_file_actions_destroy(&actions);
    close(in_pipe[0]);
    close(out_pipe[1]);
    close(err_pipe[1]);
    if (spawned != 0) {
        close(in_pipe[1]), close(out_pipe[0]), close(err_pipe[0]);
        error = "cannot start " + exe.filename().string() + ": " + std::strerror(spawned);
        return false;
    }
    // A write to a program that has gone must not end ours.
    signal(SIGPIPE, SIG_IGN);
    impl_->pid = pid;
    impl_->in_fd = in_pipe[1];
    impl_->out_fd = out_pipe[0];
    impl_->err_fd = err_pipe[0];
    Impl* self = impl_.get();
    self->out_reader = std::thread([self] {
        char buffer[4096];
        while (!self->stopping) {
            pollfd wait{self->out_fd, POLLIN, 0};
            const int ready = poll(&wait, 1, 100);
            if (ready == 0 || (ready < 0 && errno == EINTR))
                continue;
            if (ready < 0)
                break;
            const ssize_t got = read(self->out_fd, buffer, sizeof buffer);
            if (got > 0)
                self->got_out(buffer, std::size_t(got));
            else if (got < 0 && errno == EINTR)
                continue;
            else
                break;
        }
        self->out_closed();
    });
    self->err_reader = std::thread([self] {
        char buffer[4096];
        while (!self->stopping) {
            pollfd wait{self->err_fd, POLLIN, 0};
            const int ready = poll(&wait, 1, 100);
            if (ready == 0 || (ready < 0 && errno == EINTR))
                continue;
            if (ready < 0)
                break;
            const ssize_t got = read(self->err_fd, buffer, sizeof buffer);
            if (got > 0)
                self->got_err(buffer, std::size_t(got));
            else if (got < 0 && errno == EINTR)
                continue;
            else
                break;
        }
    });
    return true;
}

bool ChildProcess::write_line(const std::string& line) {
    if (impl_->in_fd < 0 || !running())
        return false;
    const std::string data = line + "\n";
    std::size_t done = 0;
    while (done < data.size()) {
        const ssize_t n = write(impl_->in_fd, data.data() + done, data.size() - done);
        if (n < 0 && errno == EINTR)
            continue;
        if (n <= 0)
            return false;
        done += std::size_t(n);
    }
    return true;
}

bool ChildProcess::running() const {
    if (impl_->pid <= 0)
        return false;
    int status = 0;
    const pid_t r = waitpid(impl_->pid, &status, WNOHANG);
    if (r == impl_->pid) {
        impl_->pid = -1; // reaped
        return false;
    }
    return r == 0;
}

void ChildProcess::stop() {
    if (!impl_)
        return;
    if (impl_->in_fd >= 0)
        close(impl_->in_fd), impl_->in_fd = -1;
    if (impl_->pid > 0) {
        kill(impl_->pid, SIGKILL);
        int status = 0;
        waitpid(impl_->pid, &status, 0);
        impl_->pid = -1;
    }
    impl_->join_readers();
    if (impl_->out_fd >= 0)
        close(impl_->out_fd), impl_->out_fd = -1;
    if (impl_->err_fd >= 0)
        close(impl_->err_fd), impl_->err_fd = -1;
}

#endif

} // namespace tisma::cad

