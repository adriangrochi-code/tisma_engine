#pragma once
// Parameters of the Design tab (docs/spec/features/cad_design.md, rule 9):
// named variables and expressions with units («2 * ancho + 5 mm»,
// «angulo / 2») usable in any size or operation; changing one rebuilds the
// part. Own design: a recursive-descent reader of the usual arithmetic
// grammar; see PROVENANCE.md.
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace tisma::cad {

// What a value measures: nothing (a plain number), a length (mm) or an
// angle (degrees).
enum class Measure { None, Length, Angle };

struct Quantity {
    double value = 0.0; // mm for a length, degrees for an angle
    Measure measure = Measure::None;
};

struct Parameter {
    std::string name;       // a letter or '_' first, then letters, digits, '_'
    std::string expression; // «40 mm», «2 * ancho + 5 mm»
    friend bool operator==(const Parameter&, const Parameter&) = default;
};

// An expression's value, or why it has none: numbers (with '.' or ',' as the
// decimal point), + - * / and parentheses, a unary minus, the units mm, cm,
// m, in, deg (also °) and rad, and the names in `values`. Adding a length to
// an angle, or multiplying two lengths, is an error; a plain number added to
// a length counts as millimetres.
struct Evaluation {
    std::optional<Quantity> value;
    std::string error; // in English, for the log; the interface says its own
};
Evaluation evaluate(const std::string& expression, const std::map<std::string, Quantity>& values);

// Every parameter's value, each able to use those before or after it: a
// name used before it has a value, a cycle, an unknown name or a bad
// expression leaves that parameter (and those that use it) without a value,
// its reason in `errors` by name.
struct ParameterValues {
    std::map<std::string, Quantity> values;
    std::map<std::string, std::string> errors;
};
ParameterValues evaluate_parameters(const std::vector<Parameter>& parameters);

// Whether a name can be a parameter's: a letter or '_' first, then letters,
// digits or '_', and not a unit's name.
bool valid_parameter_name(const std::string& name);

} // namespace tisma::cad
