#pragma once

#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace tisma {

// A value in a G-code template (docs/spec/features/gcode_templates.md).
struct TemplateValue {
    enum class Kind { Nil, Bool, Int, Real, Text, List };
    Kind kind = Kind::Nil;
    bool boolean = false;
    long long integer = 0;
    double real = 0.0;
    std::string text;
    std::vector<TemplateValue> list;

    static TemplateValue of(bool v);
    static TemplateValue of(long long v);
    static TemplateValue of(double v);
    static TemplateValue of(std::string v);
    static TemplateValue of(std::vector<TemplateValue> v);
    // A value as written in the settings: a number, true or false, or text.
    static TemplateValue parse(std::string_view written);
};

// A template that cannot be rendered: the section, the line, the message,
// then the line and a mark under the column (rule 8).
class TemplateError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

// What templates see: the print's variables, and the globals that templates
// set for the ones after them (rule 6).
struct TemplateContext {
    std::map<std::string, TemplateValue, std::less<>> variables;
    std::map<std::string, TemplateValue, std::less<>> globals;
    // Other programs' names for a variable (gcode_macros.md, rule 10 bis):
    // name → (the variable it is, its type). Looked up when the name itself is
    // not set; a single-value type takes a list's first value, a list type
    // makes a single value a list of one. Types as in gcode_variables.md: n
    // number, i integer, b yes/no, t text, N I B T their lists, ? unknown.
    std::map<std::string, std::pair<std::string, char>, std::less<>> aliases;
    // Variables the engine cannot fill yet, given a neutral value (rule 9),
    // and those a template used, for the log.
    std::set<std::string, std::less<>> unfilled, unfilled_used;
    // [word]s in other programs' G-code that are not variables (a comment's
    // «[Checks]»): kept as written.
    std::set<std::string, std::less<>> verbatim;
};

// Renders a template (gcode_templates.md, rules 1 to 8):
// - [name] is replaced by the variable's value; [name[index]] and
//   [name_index] by a list's item, the index a number or a variable;
// - {expression} by the expression's: numbers, "text", true, false; + - * / %
//   (an integer divided by an integer is an integer; + joins texts);
//   comparisons, and, or, not, (condition ? a : b), =~ /regular expression/;
//   name[index] of a list (a single value is a list of one); (a, b, c) lists;
//   min, max, int, round, digits, zdigits, is_nil, one_of, size, empty and
//   interpolate_table;
// - {if ...}, {elsif ...}, {else}, {endif} keep or drop the text between them;
// - {local name = expression} and {global name = expression} set variables,
//   for this template or for the ones after it too.
// An unknown variable or function, a misused one or a syntax error throws
// TemplateError, named after `section`. Own design.
std::string render_template(std::string_view text, TemplateContext& context, std::string_view section);

// A value as a template writes it: integers as they are, reals in their
// shortest exact form, true or false, lists joined by commas.
std::string template_text(const TemplateValue& value);

} // namespace tisma
