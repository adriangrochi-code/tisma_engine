// The G-code template language (docs/spec/features/gcode_templates.md, rules
// 1 to 8). Own design: a recursive-descent parser that evaluates as it reads;
// see PROVENANCE.md.

#include "gcode/template.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdlib>
#include <functional>
#include <optional>
#include <regex>
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
    if (const auto r = std::from_chars(written.data(), written.data() + written.size(), integer);
        r.ec == std::errc() && r.ptr == written.data() + written.size() && !written.empty())
        return of(integer);
    double real = 0.0;
    if (const auto r = std::from_chars(written.data(), written.data() + written.size(), real);
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
struct Source {
    std::string_view text;
    std::string_view section;

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
            if (index.integer < 0 || std::size_t(index.integer) >= v->list.size())
                src_.fail(index_at, "index " + std::to_string(index.integer) + " is out of '" + name + "' (" +
                                         std::to_string(v->list.size()) + " values)");
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
                const bool same = numeric(args[0]) && numeric(args[i]) ? real_of(args[0]) == real_of(args[i])
                                                                       : template_text(one(args[0])) == template_text(one(args[i]));
                if (same)
                    return TemplateValue::of(true);
            }
            return TemplateValue::of(false);
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

std::string render_template(std::string_view text, TemplateContext& context, std::string_view section) {
    const Source src{text, section};
    Variables locals;
    const std::function<const TemplateValue*(std::string_view)> lookup = [&](std::string_view name) -> const TemplateValue* {
        for (const Variables* scope : {&locals, &context.globals, &context.variables})
            if (const auto it = scope->find(name); it != scope->end())
                return &it->second;
        return nullptr;
    };
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
        if (c == '[' && i + 1 < text.size() && ident_start(text[i + 1])) { // [name]
            std::size_t j = i + 1;
            while (j < text.size() && ident_char(text[j]))
                ++j;
            if (j < text.size() && text[j] == ']') {
                if (kept()) {
                    const std::string_view name = text.substr(i + 1, j - i - 1);
                    const TemplateValue* v = lookup(name);
                    if (!v)
                        src.fail(i + 1, "unknown variable '" + std::string(name) + "'");
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
        Expression e(src, begin, end, lookup);
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
        } else if (word("local") || word("global")) {
            const bool global = word("global");
            Expression assignment(src, begin + (global ? 6 : 5), end, lookup);
            if (kept()) {
                auto [name, value] = assignment.assignment();
                (global ? context.globals : locals)[name] = std::move(value);
            }
        } else if (kept()) {
            if (block.empty())
                src.fail(i, "an empty {}");
            out += template_text(e.whole());
        }
        i = close + 1;
    }
    if (!open.empty())
        src.fail(open.back().at, "an {if} without its {endif}");
    return out;
}

} // namespace tisma
