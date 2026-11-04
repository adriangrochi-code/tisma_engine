#pragma once

// Running another program and reading what it prints, with a time limit:
// the bundled solidcraft-cli (cad_design.md, rule 1). Own implementation
// over the POSIX and Win32 process calls; see PROVENANCE.md.

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace tisma::cad {

struct ProcessResult {
    bool started = false;   // the program ran (else `error` says why not)
    bool timed_out = false; // killed at the time limit
    int status = -1;        // its exit status when it ended on its own
    std::string out, err;   // what it printed
    std::string error;      // why it could not be started, or how it ended badly
    bool ok() const { return started && !timed_out && status == 0; }
};

// Runs `exe` with `args`, no window, and waits up to `timeout_seconds` (0:
// without limit) for it to end. Its stdin gets `input` and then closes (empty:
// closed from the start); a secret goes there, never among the arguments,
// which other programs can see.
ProcessResult run_process(const std::filesystem::path& exe, const std::vector<std::string>& args,
                          double timeout_seconds = 0.0, const std::string& input = {});

// A program kept running to talk to by lines (the design component's session,
// cad_design.md, rule 20 bis): its stdin and stdout are ours; what it writes
// to stderr is drained and kept (the last 64 kB). Stopped (killed) when
// destroyed.
class ChildProcess {
public:
    ChildProcess();
    ~ChildProcess();
    ChildProcess(const ChildProcess&) = delete;
    ChildProcess& operator=(const ChildProcess&) = delete;
    // False, with `error`, when it cannot be started.
    bool start(const std::filesystem::path& exe, const std::vector<std::string>& args, std::string& error);
    // One line to its stdin (the newline added). False when it is gone.
    bool write_line(const std::string& line);
    // The next line it wrote to stdout, waiting up to `timeout_seconds`;
    // none when the time runs out or it ended.
    std::optional<std::string> read_line(double timeout_seconds);
    bool running() const;
    void stop();
    std::string stderr_text() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// The folder the running program is in (where the bundled tool is looked
// for); the current folder when it cannot be known.
std::filesystem::path executable_dir();

} // namespace tisma::cad
