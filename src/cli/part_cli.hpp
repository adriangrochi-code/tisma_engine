#pragma once

// Parts on the command line, for the phone (mobile_app.md, the owner's
// request: the phone runs only tisma-engine): gears and fasteners from their
// JSON spec, written as STL.

#include <ostream>
#include <span>
#include <string_view>

namespace tisma {

// tisma-engine part gear|fastener <spec.json> -o <out.stl>
// tisma-engine part gear|fastener --defaults
int cmd_part(std::span<const std::string_view> args, std::ostream& out, std::ostream& err);

} // namespace tisma
