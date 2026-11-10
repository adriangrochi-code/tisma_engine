#pragma once

// My library on the command line (local_library.md): what the phone app and
// the interface call. Answers are JSON on standard output; a refused request
// is {"code", "error", "error_es"} with exit code 2, and an entry that is
// already there on import is {"exists": id} with exit code 3.

#include <ostream>
#include <span>
#include <string_view>

namespace tisma {

// tisma-engine library list|show|save|check|printed|delete|export|import ...
int cmd_library(std::span<const std::string_view> args, std::ostream& out, std::ostream& err);

} // namespace tisma
