#pragma once

#include "config/settings.hpp"
#include "slice/pipeline.hpp"

#include <ostream>
#include <string>
#include <string_view>

namespace tisma {

// Replaces [name], {name} and {name[i]} in G-code text with the variables'
// values (docs/spec/GCODE_OUTPUT.md, first version). Unknown names and indices
// are left as they are.
std::string substitute_variables(std::string_view text, const std::map<std::string, std::vector<std::string>>& variables);

// Writes the G-code of a sliced model, following docs/spec/GCODE_OUTPUT.md:
// the header line, the start G-code, every layer with the comments the Tisma
// preview reads (;LAYER_CHANGE, ;Z:, ;HEIGHT:, ;TYPE:, ;WIDTH:), the end
// G-code and a "; key = value" line per setting. Within a layer: walls from the
// inside out with the outer wall last, then bottom and top surfaces, bridges,
// gap fill and sparse infill. E follows the volume laid down (width x height x
// length over the filament's cross-section; a variable-width piece uses its
// mean width). Travels are straight G1 moves without E (no travel planning yet).
void write_gcode(std::ostream& out, const SlicedModel& model, const Settings& settings);

} // namespace tisma
