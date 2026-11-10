// The G-code template language (docs/spec/features/gcode_templates.md, rules
// 1 to 8). Own design: a recursive-descent parser that evaluates as it reads;
// see PROVENANCE.md.

#include "gcode/template.hpp"
#include "io/number.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cmath>
#include <cstdlib>
#include <functional>
#include <optional>
#include <regex>
#include <set>
#include <system_error>
#include <utility>

namespace tisma {

TemplateValue TemplateValue::of(bool v) {
    TemplateValue out;
    out.kind = Kind::Bool;
    out.boolean = v;
    return out;
}

TemplateValue TemplateValue::of(long long v) {
    TemplateValue out;
    out.kind = Kind::Int;
    out.integer = v;
    return out;
}

TemplateValue TemplateValue::of(double v) {
    TemplateValue out;
    out.kind = Kind::Real;
    out.real = v;
    return out;
}

TemplateValue TemplateValue::of(std::string v) {
    TemplateValue out;
    out.kind = Kind::Text;
    out.text = std::move(v);
    return out;
}

TemplateValue TemplateValue::of(std::vector<TemplateValue> v) {
    TemplateValue out;
    out.kind = Kind::List;
    out.list = std::move(v);
    return out;
}

TemplateValue TemplateValue::parse(std::string_view written) {
    if (written == "true" || written == "false")
        return of(written == "true");
    if (written.size() >= 2 && written.front() == '"' && written.back() == '"') { // JSON text
        std::string text;
        for (std::size_t i = 1; i + 1 < written.size(); ++i) {
            char c = written[i];
            if (c == '\\' && i + 2 < written.size()) {
                c = written[++i];
                c = c == 'n' ? '\n' : c == 't' ? '\t' : c;
            }
            text += c;
        }
        return of(std::move(text));
    }
    long long integer = 0;
    if (const auto r = chars_to(written.data(), written.data() + written.size(), integer);
        r.ec == std::errc() && r.ptr == written.data() + written.size() && !written.empty())
        return of(integer);
    double real = 0.0;
    if (const auto r = chars_to(written.data(), written.data() + written.size(), real);
        r.ec == std::errc() && r.ptr == written.data() + written.size() && !written.empty())
        return of(real);
    return of(std::string(written));
}

std::string template_text(const TemplateValue& v) {
    switch (v.kind) {
    case TemplateValue::Kind::Nil:
        return "";
    case TemplateValue::Kind::Bool:
        return v.boolean ? "true" : "false";
    case TemplateValue::Kind::Int:
        return std::to_string(v.integer);
    case TemplateValue::Kind::Real: {
        char buffer[64];
        const auto r = std::to_chars(buffer, buffer + sizeof buffer, v.real == 0.0 ? 0.0 : v.real);
        return std::string(buffer, r.ptr);
    }
    case TemplateValue::Kind::Text:
        return v.text;
    case TemplateValue::Kind::List: {
        std::string out;
        for (std::size_t i = 0; i < v.list.size(); ++i)
            out += (i ? "," : "") + template_text(v.list[i]);
        return out;
    }
    }
    return "";
}

namespace {

using Kind = TemplateValue::Kind;
using Variables = std::map<std::string, TemplateValue, std::less<>>;

// A list of one value (a setting per extruder, on one extruder) counts as its
// value in sums, comparisons and conditions.
const TemplateValue& one(const TemplateValue& v) { return v.kind == Kind::List && v.list.size() == 1 ? v.list[0] : v; }
bool numeric(const TemplateValue& v) { return one(v).kind == Kind::Int || one(v).kind == Kind::Real; }
double real_of(const TemplateValue& v) { return one(v).kind == Kind::Int ? double(one(v).integer) : one(v).real; }
bool ident_start(char c) { return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_'; }
bool ident_char(char c) { return ident_start(c) || (c >= '0' && c <= '9') || c == '.'; }

// The template being rendered, for the errors' line and mark.
// What marks a text as a regular expression from ~name (one_of() only).
constexpr char kPatternMark = '\x01';

struct Source {
    std::string_view text;
    std::string_view section;
    // Lists read past their end, which give their last value instead
    // (gcode_macros.md, rule 10 quater): named here for the log.
    std::set<std::string, std::less<>>* clamped = nullptr;

    [[noreturn]] void fail(std::size_t at, const std::string& message) const {
        at = std::min(at, text.size());
        const std::size_t begin = at == 0 ? 0 : text.rfind('\n', at - 1) == std::string_view::npos ? 0 : text.rfind('\n', at - 1) + 1;
        const std::size_t end = std::min(text.find('\n', at), text.size());
        const long line = 1 + long(std::count(text.begin(), text.begin() + long(begin), '\n'));
        throw TemplateError(std::string(section) + ", line " + std::to_string(line) + ": " + message + "\n" +
                            std::string(text.substr(begin, end - begin)) + "\n" + std::string(at - begin, ' ') + "^");
    }
};

// Reads and evaluates one expression of a {block}: text[at, end).
class Expression {
public:
    Expression(const Source& source, std::size_t at, std::size_t end,
               const std::function<const TemplateValue*(std::string_view)>& lookup)
        : src_(source), at_(at), end_(end), lookup_(lookup) {}

    TemplateValue whole() {
        TemplateValue v = ternary();
        space();
        if (at_ < end_)
            src_.fail(at_, "unexpected '" + std::string(1, src_.text[at_]) + "'");
        return v;
    }

    // name = expression (local and global).
    std::pair<std::string, TemplateValue> assignment() {
        space();
        const std::size_t start = at_;
        const std::string name = identifier();
        if (name.empty())
            src_.fail(start, "a variable name is expected");
        space();
        if (!take("="))
            src_.fail(at_, "'=' is expected");
        return {name, whole()};
    }

    bool truth(const TemplateValue& value, std::size_t at) const {
        const TemplateValue& v = one(value);
        if (v.kind == Kind::Bool)
            return v.boolean;
        if (numeric(v))
            return real_of(v) != 0.0;
        src_.fail(at, "a condition must be true or false");
    }

private:
    void space() {
        while (at_ < end_ && (src_.text[at_] == ' ' || src_.text[at_] == '\t' || src_.text[at_] == '\n' || src_.text[at_] == '\r'))
            ++at_;
    }
    bool peek(std::string_view s) {
        space();
        return src_.text.substr(at_, std::min(s.size(), end_ - at_)) == s;
    }
    bool take(std::string_view s) {
        if (!peek(s))
            return false;
        at_ += s.size();
        return true;
    }
    // A keyword: the word, not the start of a longer name.
    bool keyword(std::string_view word) {
        if (!peek(word) || (at_ + word.size() < end_ && ident_char(src_.text[at_ + word.size()])))
            return false;
        at_ += word.size();
        return true;
    }
    std::string identifier() {
        space();
        if (at_ >= end_ || !ident_start(src_.text[at_]))
            return {};
        const std::size_t start = at_;
        while (at_ < end_ && ident_char(src_.text[at_]))
            ++at_;
        return std::string(src_.text.substr(start, at_ - start));
    }

    TemplateValue ternary() {
        const std::size_t start = at_;
        TemplateValue condition = disjunction();
        if (!take("?"))
            return condition;
        const bool yes = truth(condition, start);
        TemplateValue a = ternary();
        if (!take(":"))
            src_.fail(at_, "':' is expected");
        TemplateValue b = ternary();
        return yes ? a : b;
    }

    TemplateValue disjunction() {
        const std::size_t start = at_;
        TemplateValue v = conjunction();
        while (keyword("or") || take("||")) {
            const std::size_t right = at_;
            const bool a = truth(v, start);
            const TemplateValue w = conjunction();
            v = TemplateValue::of(a || truth(w, right));
        }
        return v;
    }

    TemplateValue conjunction() {
        const std::size_t start = at_;
        TemplateValue v = negation();
        while (keyword("and") || take("&&")) {
            const std::size_t right = at_;
            const bool a = truth(v, start);
            const TemplateValue w = negation();
            v = TemplateValue::of(a && truth(w, right));
        }
        return v;
    }

    TemplateValue negation() {
        space();
        const std::size_t start = at_;
        if (keyword("not") || (peek("!") && !peek("!=") && take("!"))) {
            const TemplateValue v = negation();
            return TemplateValue::of(!truth(v, start));
        }
        return comparison();
    }

    TemplateValue comparison() {
        const TemplateValue left = sum();
        const TemplateValue& a = one(left);
        space();
        const std::size_t at = at_;
        if (take("=~") || take("!~")) {
            const bool match = src_.text.substr(at, 2) == "=~";
            space();
            if (!take("/"))
                src_.fail(at_, "a /regular expression/ is expected");
            std::string pattern;
            while (at_ < end_ && src_.text[at_] != '/') {
                if (src_.text[at_] == '\\' && at_ + 1 < end_ && src_.text[at_ + 1] == '/')
                    ++at_;
                pattern += src_.text[at_++];
            }
            if (!take("/"))
                src_.fail(at_, "the regular expression has no closing /");
            try {
                const bool found = std::regex_search(template_text(a), std::regex(pattern));
                return TemplateValue::of(found == match);
            } catch (const std::regex_error&) {
                src_.fail(at, "a regular expression that cannot be read");
            }
        }
        for (const std::string_view op : {"==", "!=", "<>", "<=", ">=", "<", ">"}) {
            if (!take(op))
                continue;
            const TemplateValue b = one(sum());
            int order = 0;
            if (numeric(a) && numeric(b)) {
                order = real_of(a) < real_of(b) ? -1 : real_of(a) > real_of(b) ? 1 : 0;
            } else if (a.kind == Kind::Text && b.kind == Kind::Text) {
                order = a.text.compare(b.text) < 0 ? -1 : a.text.compare(b.text) > 0 ? 1 : 0;
            } else if (op == "==" || op == "!=" || op == "<>") {
                order = template_text(a) == template_text(b) ? 0 : 1;
            } else {
                src_.fail(at, "only numbers or texts can be put in order");
            }
            const bool r = op == "==" ? order == 0 : op == "!=" || op == "<>" ? order != 0 : op == "<=" ? order <= 0
                         : op == ">=" ? order >= 0 : op == "<" ? order < 0 : order > 0;
            return TemplateValue::of(r);
        }
        return left;
    }

    TemplateValue sum() {
        TemplateValue a = product();
        for (;;) {
            space();
            const std::size_t at = at_;
            if (take("+")) {
                const TemplateValue b = product();
                if (a.kind == Kind::Text || b.kind == Kind::Text)
                    a = TemplateValue::of(template_text(a) + template_text(b));
                else
                    a = arithmetic('+', a, b, at);
            } else if (take("-")) {
                a = arithmetic('-', a, product(), at);
            } else {
                return a;
            }
        }
    }

    TemplateValue product() {
        TemplateValue a = unary();
        for (;;) {
            space();
            const std::size_t at = at_;
            char op = 0;
            for (const char c : {'*', '/', '%'})
                if (at_ < end_ && src_.text[at_] == c)
                    op = c;
            if (!op)
                return a;
            ++at_;
            a = arithmetic(op, a, unary(), at);
        }
    }

    TemplateValue arithmetic(char op, const TemplateValue& left, const TemplateValue& right, std::size_t at) const {
        const TemplateValue& a = one(left);
        const TemplateValue& b = one(right);
        if (!numeric(a) || !numeric(b))
            src_.fail(at, std::string("'") + op + "' needs numbers");
        if (a.kind == Kind::Int && b.kind == Kind::Int) {
            if ((op == '/' || op == '%') && b.integer == 0)
                src_.fail(at, "a division by zero");
            return TemplateValue::of(op == '+'   ? a.integer + b.integer
                                     : op == '-' ? a.integer - b.integer
                                     : op == '*' ? a.integer * b.integer
                                     : op == '/' ? a.integer / b.integer
                                                 : a.integer % b.integer);
        }
        const double x = real_of(a), y = real_of(b);
        if ((op == '/' || op == '%') && y == 0.0)
            src_.fail(at, "a division by zero");
        return TemplateValue::of(op == '+' ? x + y : op == '-' ? x - y : op == '*' ? x * y : op == '/' ? x / y : std::fmod(x, y));
    }

    TemplateValue unary() {
        space();
        const std::size_t at = at_;
        if (take("-")) {
            const TemplateValue v = one(unary());
            if (!numeric(v))
                src_.fail(at, "'-' needs a number");
            return v.kind == Kind::Int ? TemplateValue::of(-v.integer) : TemplateValue::of(-v.real);
        }
        if (take("+"))
            return unary();
        return primary();
    }

    TemplateValue primary() {
        space();
        const std::size_t at = at_;
        if (at_ >= end_)
            src_.fail(at_, "a value is expected");
        const char c = src_.text[at_];
        if (c == '(') { // a value in parentheses, or a list
            ++at_;
            std::vector<TemplateValue> items{ternary()};
            while (take(","))
                items.push_back(ternary());
            if (!take(")"))
                src_.fail(at_, "')' is expected");
            return items.size() == 1 ? items.front() : TemplateValue::of(std::move(items));
        }
        if (c == '"') {
            ++at_;
            std::string text;
            while (at_ < end_ && src_.text[at_] != '"') {
                char d = src_.text[at_++];
                if (d == '\\' && at_ < end_) {
                    d = src_.text[at_++];
                    d = d == 'n' ? '\n' : d;
                }
                text += d;
            }
            if (at_ >= end_)
                src_.fail(at, "a text without its closing \"");
            ++at_;
            return TemplateValue::of(std::move(text));
        }
        if ((c >= '0' && c <= '9') || c == '.') {
            std::size_t j = at_;
            bool real = false;
            while (j < end_ && ((src_.text[j] >= '0' && src_.text[j] <= '9') || src_.text[j] == '.' ||
                                ((src_.text[j] == 'e' || src_.text[j] == 'E') && j + 1 < end_)))
                real = real || !(src_.text[j] >= '0' && src_.text[j] <= '9'), ++j;
            const std::string_view number = src_.text.substr(at_, j - at_);
            at_ = j;
            const TemplateValue v = TemplateValue::parse(number);
            if (!numeric(v) || (real && v.kind == Kind::Int))
                src_.fail(at, "a number that cannot be read");
            return v;
        }
        // /pattern/: a regular expression written in place, for
        // regex_replace() and one_of() (orca_2_5.md, rule 20). A \/ is a /.
        if (c == '/') {
            std::string pattern;
            std::size_t j = at_ + 1;
            for (; j < end_ && src_.text[j] != '/'; ++j) {
                if (src_.text[j] == '\\' && j + 1 < end_ && src_.text[j + 1] == '/')
                    ++j;
                else if (src_.text[j] == '\\' && j + 1 < end_)
                    pattern += src_.text[j++];
                pattern += src_.text[j];
            }
            if (j >= end_)
                src_.fail(at, "a regular expression without its closing /");
            at_ = j + 1;
            return TemplateValue::of(std::string(1, kPatternMark) + pattern);
        }
        // ~name: the variable's text as a regular expression, for one_of()
        // (the OrcaSlicer family's G-code; gcode_macros.md, rule 10 quater).
        if (take("~")) {
            const std::size_t name_at = at_;
            const std::string pattern_of = identifier();
            const TemplateValue* p = pattern_of.empty() ? nullptr : lookup_(pattern_of);
            if (!p)
                src_.fail(name_at, "'~' takes a variable holding a regular expression");
            const TemplateValue& text = one(*p);
            if (text.kind != Kind::Text)
                src_.fail(name_at, "'~" + pattern_of + "' is not a text (a regular expression is expected)");
            return TemplateValue::of(std::string(1, kPatternMark) + text.text);
        }
        const std::string name = identifier();
        if (name.empty())
            src_.fail(at, "a value is expected");
        if (name == "true" || name == "false")
            return TemplateValue::of(name == "true");
        space();
        if (at_ < end_ && src_.text[at_] == '(')
            return call(name, at);
        const TemplateValue* v = lookup_(name);
        if (!v)
            src_.fail(at, "unknown variable '" + name + "'");
        if (take("[")) {
            const std::size_t index_at = at_;
            const TemplateValue index = ternary();
            if (!take("]"))
                src_.fail(at_, "']' is expected");
            if (index.kind != Kind::Int)
                src_.fail(index_at, "an index must be a whole number");
            if (v->kind != Kind::List) { // a single value is a list of one
                if (index.integer != 0)
                    src_.fail(index_at, "'" + name + "' has one value: index 0 only");
                return *v;
            }
            if (index.integer < 0 || v->list.empty())
                src_.fail(index_at, "index " + std::to_string(index.integer) + " is out of '" + name + "' (" +
                                         std::to_string(v->list.size()) + " values)");
            if (std::size_t(index.integer) >= v->list.size()) { // past the end: the last value
                if (src_.clamped)
                    src_.clamped->insert(name);
                return v->list.back();
            }
            return v->list[std::size_t(index.integer)];
        }
        return *v;
    }

    TemplateValue call(const std::string& name, std::size_t at) {
        ++at_; // (
        std::vector<TemplateValue> args;
        std::vector<std::size_t> where;
        if (!take(")")) {
            do {
                space();
                where.push_back(at_);
                args.push_back(ternary());
            } while (take(","));
            if (!take(")"))
                src_.fail(at_, "')' is expected");
        }
        const auto count = [&](std::size_t lo, std::size_t hi) {
            if (args.size() < lo || args.size() > hi)
                src_.fail(at, name + "() takes " + (lo == hi ? std::to_string(lo) : std::to_string(lo) + " to " + std::to_string(hi)) +
                                   " values");
        };
        const auto number = [&](std::size_t i) {
            if (!numeric(args[i]))
                src_.fail(where[i], "a parameter of " + name + "() is not a number");
            return real_of(args[i]);
        };
        if (name == "min" || name == "max") {
            count(2, 2);
            const bool first = name == "min" ? number(0) <= number(1) : number(0) >= number(1);
            return one(first ? args[0] : args[1]);
        }
        if (name == "random") {
            // random(a, b): the same G-code every time (orca_2_5.md, rule 19),
            // so the middle of the range, whole when both ends are.
            count(2, 2);
            const double mid = (number(0) + number(1)) / 2.0;
            if (args[0].kind == Kind::Int && args[1].kind == Kind::Int)
                return TemplateValue::of(static_cast<long long>(std::floor(mid)));
            return TemplateValue::of(mid);
        }
        if (name == "int") {
            count(1, 1);
            return TemplateValue::of(static_cast<long long>(std::trunc(number(0))));
        }
        if (name == "round") { // halves up
            count(1, 1);
            return TemplateValue::of(static_cast<long long>(std::floor(number(0) + 0.5)));
        }
        if (name == "digits" || name == "zdigits") {
            count(2, 3);
            const long long width = static_cast<long long>(number(1));
            char buffer[128];
            if (args.size() == 3)
                std::snprintf(buffer, sizeof buffer, "%.*f", int(std::clamp(number(2), 0.0, 20.0)), number(0));
            else if (one(args[0]).kind == Kind::Int)
                std::snprintf(buffer, sizeof buffer, "%lld", one(args[0]).integer);
            else
                std::snprintf(buffer, sizeof buffer, "%.0f", number(0));
            std::string text = buffer;
            const std::size_t pad = width > 0 && std::size_t(width) > text.size() ? std::size_t(width) - text.size() : 0;
            if (name == "digits")
                return TemplateValue::of(std::string(pad, ' ') + text);
            const std::size_t sign = !text.empty() && text[0] == '-' ? 1 : 0;
            return TemplateValue::of(text.insert(sign, pad, '0'));
        }
        if (name == "is_nil") {
            count(1, 1);
            return TemplateValue::of(args[0].kind == Kind::Nil);
        }
        if (name == "one_of") {
            if (args.size() < 2)
                src_.fail(at, "one_of() takes a value and what it may be");
            for (std::size_t i = 1; i < args.size(); ++i) {
                const TemplateValue& option = one(args[i]);
                if (option.kind == Kind::Text && option.text.starts_with(kPatternMark)) { // ~name: the whole value matches
                    try {
                        if (std::regex_match(template_text(one(args[0])), std::regex(option.text.substr(1))))
                            return TemplateValue::of(true);
                    } catch (const std::regex_error&) {
                        src_.fail(where[i], "the regular expression in one_of() cannot be read");
                    }
                    continue;
                }
                const bool same = numeric(args[0]) && numeric(args[i]) ? real_of(args[0]) == real_of(args[i])
                                                                       : template_text(one(args[0])) == template_text(option);
                if (same)
                    return TemplateValue::of(true);
            }
            return TemplateValue::of(false);
        }
        if (name == "regex_replace") { // (text, /pattern/, replacement with $1 for the groups), rule 20
            count(3, 3);
            const TemplateValue& pattern = one(args[1]);
            std::string expression = template_text(pattern);
            if (pattern.kind == Kind::Text && pattern.text.starts_with(kPatternMark))
                expression.erase(0, 1);
            try {
                return TemplateValue::of(std::regex_replace(template_text(one(args[0])), std::regex(expression),
                                                            template_text(one(args[2]))));
            } catch (const std::regex_error&) {
                src_.fail(where[1], "the regular expression in regex_replace() cannot be read");
            }
        }
        if (name == "size" || name == "empty") {
            count(1, 1);
            if (args[0].kind != Kind::List)
                src_.fail(where[0], "the parameter of " + name + "() is not a list");
            return name == "size" ? TemplateValue::of(static_cast<long long>(args[0].list.size()))
                                  : TemplateValue::of(args[0].list.empty());
        }
        if (name == "interpolate_table") {
            if (args.size() < 2)
                src_.fail(at, "interpolate_table() takes a value and (x, y) pairs");
            const double x = number(0);
            std::vector<std::pair<double, double>> table;
            for (std::size_t i = 1; i < args.size(); ++i) {
                const TemplateValue& p = args[i];
                if (p.kind != Kind::List || p.list.size() != 2 || !numeric(p.list[0]) || !numeric(p.list[1]))
                    src_.fail(where[i], "interpolate_table() takes (x, y) pairs of numbers");
                table.push_back({real_of(p.list[0]), real_of(p.list[1])});
            }
            std::sort(table.begin(), table.end());
            if (x <= table.front().first)
                return TemplateValue::of(table.front().second);
            for (std::size_t i = 1; i < table.size(); ++i)
                if (x <= table[i].first) {
                    const auto [x0, y0] = table[i - 1];
                    const auto [x1, y1] = table[i];
                    return TemplateValue::of(x1 > x0 ? y0 + (y1 - y0) * (x - x0) / (x1 - x0) : y1);
                }
            return TemplateValue::of(table.back().second);
        }
        src_.fail(at, "unknown function '" + name + "'");
    }

    const Source& src_;
    std::size_t at_, end_;
    const std::function<const TemplateValue*(std::string_view)>& lookup_;
};

// Where the } that closes the { at `open` is, past texts in quotes.
std::size_t closing(std::string_view text, std::size_t open) {
    bool quoted = false;
    for (std::size_t i = open + 1; i < text.size(); ++i) {
        if (quoted && text[i] == '\\')
            ++i;
        else if (text[i] == '"')
            quoted = !quoted;
        else if (!quoted && text[i] == '}')
            return i;
    }
    return std::string_view::npos;
}

} // namespace

namespace {

std::string render_raw(std::string_view text, TemplateContext& context, std::string_view section);

// Each F word of value 0 outside comments (F0, F0.0) becomes F<feed>
// (gcode_macros.md, rule 10 quater): a machine's own G-code with a speed
// variable the engine left at 0 must not stop the print.
std::string without_zero_feeds(std::string text, TemplateContext& context) {
    if (context.zero_feed <= 0.0)
        return text;
    std::string out;
    out.reserve(text.size());
    std::size_t start = 0;
    while (start <= text.size()) {
        std::size_t end = text.find('\n', start);
        if (end == std::string::npos)
            end = text.size();
        std::string line = text.substr(start, end - start);
        const std::size_t comment = std::min(line.find(';'), line.size());
        // Moves only (G0 to G3): elsewhere an F is no speed (Prusa's M862.1 F
        // is a check of the nozzle's kind).
        std::size_t w = line.find_first_not_of(" \t");
        bool move = false;
        if (w != std::string::npos && (line[w] == 'G' || line[w] == 'g')) {
            std::size_t d = w + 1;
            while (d < comment && line[d] == '0')
                ++d;
            const bool digit = d < comment && line[d] >= '1' && line[d] <= '3';
            const std::size_t after = digit ? d + 1 : d;
            move = (digit || (d > w + 1)) && (after >= comment || line[after] == ' ' || line[after] == '\t');
        }
        for (std::size_t i = 0; move && i < comment; ++i) {
            if ((line[i] != 'F' && line[i] != 'f') || (i > 0 && line[i - 1] != ' ' && line[i - 1] != '\t'))
                continue;
            std::size_t j = i + 1;
            while (j < comment && (std::isdigit(static_cast<unsigned char>(line[j])) || line[j] == '.' || line[j] == '-' ||
                                   line[j] == '+'))
                ++j;
            if (j == i + 1 || (j < comment && line[j] != ' ' && line[j] != '\t' && line[j] != '\r'))
                continue;
            const std::string number = line.substr(i + 1, j - i - 1);
            char* end_of = nullptr;
            const double v = std::strtod(number.c_str(), &end_of);
            if (end_of != number.c_str() + number.size() || v != 0.0)
                continue;
            const std::string feed = std::to_string(std::llround(context.zero_feed));
            line.replace(i + 1, j - i - 1, feed);
            ++context.zero_feeds;
            i += feed.size();
        }
        out += line;
        if (end == text.size())
            break;
        out += '\n';
        start = end + 1;
    }
    return out;
}

} // namespace

std::string render_template(std::string_view text, TemplateContext& context, std::string_view section) {
    return without_zero_feeds(render_raw(text, context, section), context);
}

namespace {

std::string render_raw(std::string_view text, TemplateContext& context, std::string_view section) {
    const Source src{text, section, &context.index_clamped};
    Variables locals;
    Variables converted; // aliases' values in their own type
    std::function<const TemplateValue*(std::string_view)> lookup;
    lookup = [&](std::string_view name) -> const TemplateValue* {
        for (const Variables* scope : {&locals, &context.globals, &context.variables})
            if (const auto it = scope->find(name); it != scope->end()) {
                if (scope == &context.variables && context.unfilled.contains(name))
                    context.unfilled_used.emplace(name);
                if (scope == &context.variables && context.estimated.contains(name))
                    context.estimated_used.emplace(name);
                return &it->second;
            }
        const auto alias = context.aliases.find(name);
        if (alias == context.aliases.end())
            return nullptr;
        const TemplateValue* v = lookup(alias->second.first);
        if (!v)
            return nullptr;
        const char type = alias->second.second;
        TemplateValue as = *v;
        if (std::string_view("nibt").find(type) != std::string_view::npos && v->kind == TemplateValue::Kind::List)
            as = v->list.empty() ? TemplateValue{} : v->list.front();
        else if (std::string_view("NIBT").find(type) != std::string_view::npos && v->kind != TemplateValue::Kind::List)
            as = TemplateValue::of(std::vector<TemplateValue>{*v});
        return &(converted[std::string(name)] = std::move(as));
    };
    // The cura dialect (gcode_macros.md, rule 10 bis): {name} is replaced by
    // its value (a list's first: one value per extruder there), and nothing
    // else is read; a name the engine does not know stays as written.
    if (context.braces_only) {
        std::string out;
        std::size_t i = 0;
        while (i < text.size()) {
            if (text[i] == '{' && i + 1 < text.size() && ident_start(text[i + 1])) {
                std::size_t j = i + 1;
                while (j < text.size() && ident_char(text[j]))
                    ++j;
                if (j < text.size() && text[j] == '}') {
                    const std::string_view name = text.substr(i + 1, j - i - 1);
                    if (const TemplateValue* v = lookup(name)) {
                        out += template_text(v->kind == TemplateValue::Kind::List
                                                 ? (v->list.empty() ? TemplateValue{} : v->list.front())
                                                 : *v);
                    } else {
                        context.unfilled_used.emplace(name);
                        out.append(text.substr(i, j + 1 - i));
                    }
                    i = j + 1;
                    continue;
                }
            }
            out += text[i++];
        }
        return out;
    }
    // The {if} blocks open: whether the text around them is kept, whether a
    // branch of theirs was taken, and whether the current one is kept.
    struct Branch {
        bool outer, taken, kept;
        std::size_t at;
        bool after_else = false;
    };
    std::vector<Branch> open;
    const auto kept = [&] { return open.empty() || open.back().kept; };
    std::string out;
    std::size_t i = 0;
    while (i < text.size()) {
        const char c = text[i];
        if (c == '[' && i + 1 < text.size() && ident_start(text[i + 1])) { // [name], [name[index]], [name_index]
            std::size_t j = i + 1;
            while (j < text.size() && ident_char(text[j]))
                ++j;
            const std::string_view name = text.substr(i + 1, j - i - 1);
            // [name[index]]: the index a number or a variable, no sums.
            std::string_view index;
            std::size_t index_at = 0;
            if (j < text.size() && text[j] == '[') {
                std::size_t k = j + 1;
                while (k < text.size() && ident_char(text[k]))
                    ++k;
                if (k > j + 1 && k + 1 < text.size() && text[k] == ']' && text[k + 1] == ']') {
                    index = text.substr(j + 1, k - j - 1);
                    index_at = j + 1;
                    j = k + 1;
                }
            }
            if (j < text.size() && text[j] == ']') {
                if (kept()) {
                    const TemplateValue* v = lookup(name);
                    // [temperature_[current_extruder]]: the old form with a
                    // variable index, the list «temperature».
                    if (!v && !index.empty() && name.size() > 1 && name.back() == '_')
                        v = lookup(name.substr(0, name.size() - 1));
                    // [temperature_0]: the list «temperature», item 0.
                    if (!v && index.empty()) {
                        const std::size_t under = name.find_last_of('_');
                        if (under != std::string_view::npos && under + 1 < name.size() &&
                            name.find_first_not_of("0123456789", under + 1) == std::string_view::npos) {
                            if (const TemplateValue* list = lookup(name.substr(0, under))) {
                                v = list;
                                index = name.substr(under + 1);
                                index_at = i + 1 + under + 1;
                            }
                        }
                    }
                    if (!v && index.empty() && context.verbatim.contains(name)) {
                        out += text.substr(i, j - i + 1);
                        i = j + 1;
                        continue;
                    }
                    if (!v)
                        src.fail(i + 1, "unknown variable '" + std::string(name) + "'");
                    if (!index.empty()) {
                        long long at = 0;
                        if (index.find_first_not_of("0123456789") == std::string_view::npos) {
                            if (chars_to(index.data(), index.data() + index.size(), at).ec != std::errc())
                                src.fail(index_at, "the index " + std::string(index) + " is too large");
                        } else {
                            const TemplateValue* n = lookup(index);
                            if (!n)
                                src.fail(index_at, "unknown variable '" + std::string(index) + "'");
                            if (n->kind == TemplateValue::Kind::Int)
                                at = n->integer;
                            else if (n->kind == TemplateValue::Kind::Real && n->real == std::floor(n->real))
                                at = static_cast<long long>(n->real);
                            else
                                src.fail(index_at, "the index '" + std::string(index) + "' is not a whole number");
                        }
                        // A single value is a list of one (rule 2).
                        const std::size_t size = v->kind == TemplateValue::Kind::List ? v->list.size() : 1;
                        if (at < 0 || size == 0)
                            src.fail(index_at, "index " + std::to_string(at) + " out of '" + std::string(name) + "' (" +
                                                   std::to_string(size) + " values)");
                        if (static_cast<std::size_t>(at) >= size) { // past the end: the last value
                            context.index_clamped.emplace(name);
                            at = static_cast<decltype(at)>(size - 1);
                        }
                        if (v->kind == TemplateValue::Kind::List)
                            v = &v->list[static_cast<std::size_t>(at)];
                    }
                    out += template_text(*v);
                }
                i = j + 1;
                continue;
            }
        }
        if (c != '{') {
            if (kept())
                out += c;
            ++i;
            continue;
        }
        const std::size_t close = closing(text, i);
        if (close == std::string_view::npos)
            src.fail(i, "a { without its }");
        std::size_t begin = i + 1, end = close;
        while (begin < end && (text[begin] == ' ' || text[begin] == '\t'))
            ++begin;
        while (end > begin && (text[end - 1] == ' ' || text[end - 1] == '\t'))
            --end;
        const std::string_view block = text.substr(begin, end - begin);
        const auto word = [&](std::string_view w) {
            return block.substr(0, w.size()) == w && (block.size() == w.size() || !ident_char(block[w.size()]));
        };
        if (word("if")) {
            Expression cond(src, begin + 2, end, lookup);
            bool yes = false;
            if (kept()) {
                const TemplateValue v = cond.whole();
                yes = cond.truth(v, begin + 2);
            }
            open.push_back({kept(), yes, kept() && yes, i});
        } else if (word("elsif")) {
            if (open.empty() || open.back().after_else)
                src.fail(i, "an {elsif} without its {if}");
            Branch& b = open.back();
            bool yes = false;
            if (b.outer && !b.taken) {
                Expression cond(src, begin + 5, end, lookup);
                const TemplateValue v = cond.whole();
                yes = cond.truth(v, begin + 5);
            }
            b.kept = b.outer && !b.taken && yes;
            b.taken = b.taken || yes;
        } else if (word("else")) {
            if (open.empty() || open.back().after_else)
                src.fail(i, "an {else} without its {if}");
            Branch& b = open.back();
            b.kept = b.outer && !b.taken;
            b.taken = true;
            b.after_else = true;
        } else if (word("endif")) {
            if (open.empty())
                src.fail(i, "an {endif} without its {if}");
            open.pop_back();
        } else if (kept()) {
            if (block.empty())
                src.fail(i, "an empty {}");
            // Statements apart by ';' (outside texts and brackets): local and
            // global ones, assignments to a variable or a list's item
            // (name = …, name[i] = …), and expressions, whose values are
            // written (PrusaSlicer's documented macro language).
            std::vector<std::pair<std::size_t, std::size_t>> statements;
            {
                std::size_t from = begin;
                int depth = 0;
                char quote = 0;
                for (std::size_t k = begin; k < end; ++k) {
                    const char ch = text[k];
                    if (quote) {
                        if (ch == '\\')
                            ++k;
                        else if (ch == quote)
                            quote = 0;
                    } else if (ch == '"') {
                        quote = ch;
                    } else if (ch == '(' || ch == '[') {
                        ++depth;
                    } else if ((ch == ')' || ch == ']') && depth > 0) {
                        --depth;
                    } else if (ch == ';' && depth == 0) {
                        statements.emplace_back(from, k);
                        from = k + 1;
                    }
                }
                statements.emplace_back(from, end);
            }
            for (auto [a, b] : statements) {
                while (a < b && (text[a] == ' ' || text[a] == '\t' || text[a] == '\n' || text[a] == '\r'))
                    ++a;
                while (b > a && (text[b - 1] == ' ' || text[b - 1] == '\t' || text[b - 1] == '\n' || text[b - 1] == '\r'))
                    --b;
                if (a == b)
                    continue;
                const std::string_view st = text.substr(a, b - a);
                const auto starts = [&](std::string_view w) {
                    return st.substr(0, w.size()) == w && (st.size() == w.size() || !ident_char(st[w.size()]));
                };
                if (starts("local") || starts("global")) {
                    const bool global = starts("global");
                    Expression assignment(src, a + (global ? 6 : 5), b, lookup);
                    auto [name, value] = assignment.assignment();
                    (global ? context.globals : locals)[name] = std::move(value);
                    continue;
                }
                // name = … or name[index] = … (not ==)
                std::size_t k = 0;
                while (k < st.size() && ident_char(st[k]))
                    ++k;
                std::size_t index_from = 0, index_to = 0;
                std::size_t eq = k;
                if (k > 0 && ident_start(st[0])) {
                    while (eq < st.size() && st[eq] == ' ')
                        ++eq;
                    if (eq < st.size() && st[eq] == '[') {
                        int d = 0;
                        std::size_t m = eq;
                        for (; m < st.size(); ++m) {
                            if (st[m] == '[')
                                ++d;
                            else if (st[m] == ']' && --d == 0)
                                break;
                        }
                        index_from = eq + 1;
                        index_to = m;
                        eq = m + 1;
                        while (eq < st.size() && st[eq] == ' ')
                            ++eq;
                    }
                }
                if (k > 0 && ident_start(st[0]) && eq < st.size() && st[eq] == '=' &&
                    (eq + 1 >= st.size() || st[eq + 1] != '=')) {
                    const std::string name(st.substr(0, k));
                    TemplateValue value = Expression(src, a + eq + 1, b, lookup).whole();
                    TemplateValue* target = nullptr;
                    if (const auto it = locals.find(name); it != locals.end())
                        target = &it->second;
                    else if (const auto g = context.globals.find(name); g != context.globals.end())
                        target = &g->second;
                    else if (const TemplateValue* known = lookup(name))
                        target = &(locals[name] = *known);
                    else
                        src.fail(a, "unknown variable '" + name + "'");
                    if (index_to > index_from) {
                        const TemplateValue at = Expression(src, a + index_from, a + index_to, lookup).whole();
                        if (at.kind != TemplateValue::Kind::Int || at.integer < 0 || at.integer > 1000)
                            src.fail(a + index_from, "an index must be a whole number from 0");
                        if (target->kind != TemplateValue::Kind::List)
                            *target = TemplateValue::of(std::vector<TemplateValue>{*target});
                        if (target->list.size() <= std::size_t(at.integer))
                            target->list.resize(std::size_t(at.integer) + 1, target->list.empty() ? value : target->list.back());
                        target->list[std::size_t(at.integer)] = std::move(value);
                    } else {
                        *target = std::move(value);
                    }
                    continue;
                }
                out += template_text(Expression(src, a, b, lookup).whole());
            }
        }
        i = close + 1;
    }
    if (!open.empty())
        src.fail(open.back().at, "an {if} without its {endif}");
    return out;
}

} // namespace

} // namespace tisma
