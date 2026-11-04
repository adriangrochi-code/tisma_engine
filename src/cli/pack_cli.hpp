#pragma once

// The profile pack on the command line (profile_pack.md): what to download,
// installing what was downloaded, what is installed, and the settings of a
// pack printer. The engine never downloads: the caller (the interface, the
// phone app) fetches the plan's files into a folder and installs from it.

#include <ostream>
#include <span>
#include <string_view>

namespace tisma {

// tisma-engine pack plan|install|list|community ...
int cmd_pack(std::span<const std::string_view> args, std::ostream& out, std::ostream& err);

// tisma-engine profile --pack <data> --printer "<model file>#<variant>" ...
int cmd_pack_profile(std::span<const std::string_view> args, std::ostream& out, std::ostream& err);

} // namespace tisma
