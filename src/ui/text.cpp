// The interface's texts. Own design; see PROVENANCE.md.

#include "ui/text.hpp"

#include <atomic>
#include <cctype>
#include <cstdio>
#include <cstring>

namespace tisma::ui {
namespace {

struct Entry {
    const char* es;
    const char* en;
};

constexpr Entry kText[] = {
#define TISMA_UI_TEXT_ENTRY(id, es, en) {es, en},
    TISMA_UI_TEXT(TISMA_UI_TEXT_ENTRY)
#undef TISMA_UI_TEXT_ENTRY
};
static_assert(sizeof(kText) / sizeof(kText[0]) == static_cast<std::size_t>(T::Count));

std::atomic<Language> current{Language::Spanish};

} // namespace

void set_language(Language l) { current = l; }
Language language() { return current; }

const char* tr(T id) {
    const Entry& e = kText[static_cast<int>(id)];
    return current == Language::Spanish ? e.es : e.en;
}

std::string localized(std::string number) {
    if (current == Language::Spanish)
        for (char& c : number)
            if (c == '.')
                c = ',';
    return number;
}

std::string grouped(unsigned long long n) {
    const std::string digits = std::to_string(n);
    const char gap = current == Language::Spanish ? ' ' : ',';
    std::string out;
    for (std::size_t i = 0; i < digits.size(); ++i) {
        if (i > 0 && (digits.size() - i) % 3 == 0)
            out += gap;
        out += digits[i];
    }
    return out;
}

std::string vformat(const char* fmt, std::va_list args) {
    std::string out;
    char piece[512];
    for (const char* p = fmt; *p;) {
        if (*p != '%') {
            out += *p++;
            continue;
        }
        // One conversion: flags, width, precision, length and its letter.
        const char* start = p++;
        if (*p == '%') {
            out += '%';
            ++p;
            continue;
        }
        while (*p && std::strchr("-+ #0", *p))
            ++p;
        int stars = 0;
        if (*p == '*')
            ++stars, ++p;
        while (std::isdigit(static_cast<unsigned char>(*p)))
            ++p;
        if (*p == '.') {
            ++p;
            if (*p == '*')
                ++stars, ++p;
            while (std::isdigit(static_cast<unsigned char>(*p)))
                ++p;
        }
        std::string length;
        while (*p && std::strchr("hlLjzt", *p))
            length += *p++;
        const char letter = *p;
        if (!letter)
            break; // a lone '%' at the end, as printf: nothing
        ++p;
        const std::string spec(start, p);
        int star[2] = {0, 0};
        for (int k = 0; k < stars; ++k)
            star[k] = va_arg(args, int);
        // spec with the stars' values, so one snprintf call takes one value.
        const auto print = [&](auto value) {
            if (stars == 2)
                std::snprintf(piece, sizeof piece, spec.c_str(), star[0], star[1], value);
            else if (stars == 1)
                std::snprintf(piece, sizeof piece, spec.c_str(), star[0], value);
            else
                std::snprintf(piece, sizeof piece, spec.c_str(), value);
        };
        switch (letter) {
        case 'f': case 'F': case 'e': case 'E': case 'g': case 'G': case 'a': case 'A':
            if (length == "L")
                print(va_arg(args, long double));
            else
                print(va_arg(args, double));
            out += localized(piece);
            break;
        case 'd': case 'i':
            if (length == "ll")
                print(va_arg(args, long long));
            else if (length == "l")
                print(va_arg(args, long));
            else if (length == "z" || length == "t")
                print(va_arg(args, std::ptrdiff_t));
            else if (length == "j")
                print(va_arg(args, long long));
            else
                print(va_arg(args, int));
            out += piece;
            break;
        case 'u': case 'x': case 'X': case 'o':
            if (length == "ll" || length == "j")
                print(va_arg(args, unsigned long long));
            else if (length == "l")
                print(va_arg(args, unsigned long));
            else if (length == "z" || length == "t")
                print(va_arg(args, std::size_t));
            else
                print(va_arg(args, unsigned));
            out += piece;
            break;
        case 'c':
            print(va_arg(args, int));
            out += piece;
            break;
        case 's': {
            const char* s = va_arg(args, const char*);
            if (spec == "%s")
                out += s ? s : "(null)";
            else {
                print(s);
                out += piece;
            }
            break;
        }
        case 'p':
            print(va_arg(args, void*));
            out += piece;
            break;
        default: // not a conversion this formatter knows: as written
            out += spec;
            break;
        }
    }
    return out;
}

std::string format(const char* fmt, ...) {
    std::va_list args;
    va_start(args, fmt);
    std::string out = vformat(fmt, args);
    va_end(args);
    return out;
}

} // namespace tisma::ui
