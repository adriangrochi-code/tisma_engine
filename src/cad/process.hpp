#pragma once

// Running another program and reading what it prints, with a time limit:
// the bundled solidcraft-cli (cad_design.md, rule 1). Own implementation
// over the POSIX and Win32 process calls; see PROVENANCE.md.

#include <filesystem>
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

// The folder the running program is in (where the bundled tool is looked
// for); the current folder when it cannot be known.
std::filesystem::path executable_dir();

} // namespace tisma::cad
