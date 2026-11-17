#pragma once

#include "gcode/template.hpp"

#include <span>
#include <string>
#include <string_view>

namespace tisma {

struct Settings;

// A variable of the printers' own G-code (docs/spec/features/gcode_variables.md,
// gcode_macros.md rule 9): its name, the variable whose value it takes (empty:
// none) and its type letter (TemplateContext::aliases; v: a word in a comment).
struct MacroVariable {
    std::string_view name, equivalent;
    char type;
};

// Every variable of that list.
std::span<const MacroVariable> macro_variables();

// Gives a template context the list's variables (gcode_macros.md, rules 7 to
// 10 bis): the ones the engine knows from its settings (the bed, the
// machine's limits, retraction, fans...), the others' names as their
// equivalent's, and those it cannot fill yet a neutral value (0, no, "" or a
// list of one), named in TemplateContext::unfilled. Names already set are
// kept. Own design.
void add_macro_variables(TemplateContext& context, const Settings& settings);

// Names the printer, filament and print profiles chosen for the machine's
// G-code (gcode_macros.md, rule 9 bis): printer_settings_id,
// filament_settings_id and print_settings_id in settings.variables, unless
// already there; an empty name is left out.
void name_profiles(Settings& settings, const std::string& printer, const std::string& filament,
                   const std::string& print);

} // namespace tisma
