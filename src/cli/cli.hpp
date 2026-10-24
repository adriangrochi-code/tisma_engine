#pragma once

#include <iosfwd>
#include <span>
#include <string_view>

namespace tisma {

// Exit codes of the tisma-engine program.
enum ExitCode : int {
    kExitOk = 0,
    kExitUsage = 1,          // bad command line
    kExitInput = 2,          // an input file is missing or malformed
    kExitNotImplemented = 3, // the command exists but does nothing yet
    kExitModel = 4,          // the model cannot be printed (empty, outside the bed, too tall)
    kExitAnalysis = 5,       // the analysis gave no valid result (nothing held, no load applied, not converged)
    kExitGcode = 6,          // the G-code failed its own check: kept as <file>.rejected, nothing under <file>
};

// Runs the command line. args excludes the program name.
int run_cli(std::span<const std::string_view> args, std::ostream& out, std::ostream& err);

} // namespace tisma
