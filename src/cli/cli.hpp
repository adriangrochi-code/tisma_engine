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
};

// Runs the command line. args excludes the program name.
int run_cli(std::span<const std::string_view> args, std::ostream& out, std::ostream& err);

} // namespace tisma
