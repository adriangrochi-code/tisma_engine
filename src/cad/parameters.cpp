// Own design; see PROVENANCE.md.

#include "cad/parameters.hpp"

#include <cctype>
#include <cmath>
#include <cstdlib>
#include <set>
#include <stdexcept>

namespace tisma::cad {

namespace {

struct Failure : std::runtime_error {
    using std::runtime_error::runtime_error;
};

bool name_start(unsigned char c) { return std::isalpha(c) || c == '_' || c >= 0x80; }
bool name_char(unsigned char c) { return std::isalnum(c) || c == '_' || c >= 0x80; }

// The units: their measure and how much one is in mm or degrees.
std::optional<Quantity> unit(const std::string& word) {
    if (word == "mm")
        return Quantity{1.0, Measure::Length};
    if (word == "cm")
        return Quantity{10.0, Measure::Length};
    if (word == "m")
        return Quantity{1000.0, Measure::Length};
    if (word == "in")
        return Quantity{25.4, Measure::Length};
    if (word == "deg" || word == "\xc2\xb0")
        return Quantity{1.0, Measure::Angle};
    if (word == "rad")
        return Quantity{180.0 / 3.14159265358979323846, Measure::Angle};
    return std::nullopt;
}

// expression := term (('+' | '-') term)*
// term       := factor (('*' | '/') factor)*
// factor     := '-' factor | primary unit?
// primary    := number | name | '(' expression ')'
class Reader {
public:
    Reader(const std::string& text, const std::map<std::string, Quantity>& values) : s_(text), values_(values) {}

    Quantity read() {
        Quantity q = expression();
        space();
        if (i_ < s_.size())
            throw Failure("unexpected \"" + s_.substr(i_) + "\"");
        return q;
    }

private:
    void space() {
        while (i_ < s_.size() && std::isspace(static_cast<unsigned char>(s_[i_])))
            ++i_;
    }
    bool take(char c) {
        space();
        if (i_ < s_.size() && s_[i_] == c) {
            ++i_;
            return true;
        }
        return false;
    }
    static Quantity add(Quantity a, Quantity b, bool minus) {
        // A plain number takes the other side's measure (millimetres, degrees).
        if (a.measure == Measure::None)
            a.measure = b.measure;
        if (b.measure == Measure::None)
            b.measure = a.measure;
        if (a.measure != b.measure)
            throw Failure("a length and an angle cannot be added");
        return {minus ? a.value - b.value : a.value + b.value, a.measure};
    }
    Quantity expression() {
        Quantity q = term();
        for (;;) {
            if (take('+'))
                q = add(q, term(), false);
            else if (take('-'))
                q = add(q, term(), true);
            else
                return q;
        }
    }
    Quantity term() {
        Quantity q = factor();
        for (;;) {
            if (take('*')) {
                const Quantity b = factor();
                if (q.measure != Measure::None && b.measure != Measure::None)
                    throw Failure("a measure times a measure is neither a length nor an angle");
                q = {q.value * b.value, q.measure != Measure::None ? q.measure : b.measure};
            } else if (take('/')) {
                const Quantity b = factor();
                if (b.value == 0.0)
                    throw Failure("division by zero");
                if (b.measure != Measure::None && b.measure != q.measure)
                    throw Failure("only a measure divided by the same measure is a plain number");
                q = {q.value / b.value, b.measure == Measure::None ? q.measure : Measure::None};
            } else {
                return q;
            }
        }
    }
    Quantity factor() {
        if (take('-')) {
            Quantity q = factor();
            q.value = -q.value;
            return q;
        }
        if (take('+'))
            return factor();
        Quantity q = primary();
        // A unit right after: «5 mm», «30 deg», «2,5cm».
        space();
        const std::size_t at = i_;
        std::string word;
        if (s_.compare(i_, 2, "\xc2\xb0") == 0) {
            word = "\xc2\xb0";
            i_ += 2;
        } else {
            while (i_ < s_.size() && std::isalpha(static_cast<unsigned char>(s_[i_])))
                word += s_[i_++];
        }
        if (!word.empty()) {
            const std::optional<Quantity> u = unit(word);
            if (!u) { // not a unit: leave it for whoever reads next (it fails there)
                i_ = at;
                return q;
            }
            if (q.measure != Measure::None)
                throw Failure("\"" + word + "\" after a value that already has a unit");
            q = {q.value * u->value, u->measure};
        }
        return q;
    }
    Quantity primary() {
        space();
        if (i_ >= s_.size())
            throw Failure("a value is missing at the end");
        if (take('(')) {
            const Quantity q = expression();
            if (!take(')'))
                throw Failure("a ')' is missing");
            return q;
        }
        const unsigned char c = static_cast<unsigned char>(s_[i_]);
        if (std::isdigit(c) || c == '.' || c == ',') {
            std::string number;
            while (i_ < s_.size() && (std::isdigit(static_cast<unsigned char>(s_[i_])) || s_[i_] == '.' || s_[i_] == ','))
                number += s_[i_] == ',' ? '.' : s_[i_], ++i_;
            char* end = nullptr;
            const double v = std::strtod(number.c_str(), &end);
            if (end == number.c_str() || *end != '\0')
                throw Failure("\"" + number + "\" is not a number");
            return {v, Measure::None};
        }
        if (name_start(c)) {
            std::string name;
            while (i_ < s_.size() && name_char(static_cast<unsigned char>(s_[i_])))
                name += s_[i_++];
            const auto it = values_.find(name);
            if (it == values_.end())
                throw Failure("unknown name \"" + name + "\"");
            return it->second;
        }
        throw Failure(std::string("unexpected \"") + s_[i_] + "\"");
    }

    const std::string& s_;
    const std::map<std::string, Quantity>& values_;
    std::size_t i_ = 0;
};

// The names an expression uses (to order the parameters).
std::set<std::string> names_in(const std::string& s) {
    std::set<std::string> out;
    for (std::size_t i = 0; i < s.size();) {
        const unsigned char c = static_cast<unsigned char>(s[i]);
        if (name_start(c) && (i == 0 || !(std::isdigit(static_cast<unsigned char>(s[i - 1])) || s[i - 1] == '.' ||
                                          s[i - 1] == ','))) {
            std::string name;
            while (i < s.size() && name_char(static_cast<unsigned char>(s[i])))
                name += s[i++];
            if (!unit(name))
                out.insert(name);
        } else {
            ++i;
        }
    }
    return out;
}

} // namespace

Evaluation evaluate(const std::string& expression, const std::map<std::string, Quantity>& values) {
    Evaluation out;
    try {
        Reader reader(expression, values);
        const Quantity q = reader.read();
        if (!std::isfinite(q.value))
            throw Failure("the value is not finite");
        out.value = q;
    } catch (const Failure& e) {
        out.error = e.what();
    }
    return out;
}

ParameterValues evaluate_parameters(const std::vector<Parameter>& parameters) {
    ParameterValues out;
    std::map<std::string, const Parameter*> by_name;
    for (const Parameter& p : parameters) {
        if (!valid_parameter_name(p.name))
            out.errors[p.name] = "not a valid name";
        else if (by_name.count(p.name))
            out.errors[p.name] = "the name is used twice";
        else
            by_name[p.name] = &p;
    }
    // Depth-first, each parameter after the ones it uses; a cycle fails.
    std::map<std::string, int> state; // 1 visiting, 2 done
    const auto visit = [&](const auto& self, const std::string& name) -> void {
        if (state[name] == 2)
            return;
        if (state[name] == 1) {
            out.errors[name] = "it uses itself, directly or through others";
            return;
        }
        state[name] = 1;
        const Parameter& p = *by_name.at(name);
        for (const std::string& used : names_in(p.expression))
            if (by_name.count(used) && used != name)
                self(self, used);
            else if (used == name)
                out.errors[name] = "it uses itself, directly or through others";
        state[name] = 2;
        if (out.errors.count(name))
            return;
        const Evaluation e = evaluate(p.expression, out.values);
        if (e.value)
            out.values[name] = *e.value;
        else
            out.errors[name] = e.error;
    };
    for (const auto& [name, p] : by_name)
        visit(visit, name);
    return out;
}

bool valid_parameter_name(const std::string& name) {
    if (name.empty() || !name_start(static_cast<unsigned char>(name[0])))
        return false;
    for (const char c : name)
        if (!name_char(static_cast<unsigned char>(c)))
            return false;
    return !unit(name);
}

} // namespace tisma::cad
