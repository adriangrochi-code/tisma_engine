#pragma once

#include <map>
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
};

// Renders a template (gcode_templates.md, rules 1 to 8):
// - [name] is replaced by the variable's value;
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
