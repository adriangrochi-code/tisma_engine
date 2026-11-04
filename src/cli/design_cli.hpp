#pragma once

// The Design tab's logic on the command line, for the phone (the owner's
// request: the phone runs tisma-engine and the bundled solidcraft-cli, and
// only edits the design's JSON).

#include <ostream>
#include <span>
#include <string_view>

namespace tisma {

// tisma-engine design new | check <design.json> | build <design.json> --solidcraft <exe> -o <out.stl|out.3mf> ...
int cmd_design(std::span<const std::string_view> args, std::ostream& out, std::ostream& err);

} // namespace tisma
