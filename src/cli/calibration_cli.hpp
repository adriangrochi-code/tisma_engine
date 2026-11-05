#pragma once

// Calibrations on the command line, for the phone (the owner's request: the
// phone runs only tisma-engine): the same parts and height changes as the
// Calibrate tab (docs/CALIBRATION.md), then the value that printed best
// written into the settings.

#include <ostream>
#include <span>
#include <string_view>

namespace tisma {

// tisma-engine calibration <kind> --config <settings.json> [--start --end --step --segment]
//                          --model <out.stl> --settings <out.json>
// tisma-engine calibration apply <kind> --value <v> --config <settings.json> -o <out.json>
int cmd_calibration(std::span<const std::string_view> args, std::ostream& out, std::ostream& err);

} // namespace tisma
