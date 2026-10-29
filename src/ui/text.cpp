// The interface's texts. Own design; see PROVENANCE.md.

#include "ui/text.hpp"

#include <atomic>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <optional>
#include <string_view>
#include <vector>

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

namespace {

// A message pattern, "{}" where a part of it goes, and its Spanish.
struct ErrorPattern {
    std::string_view english, spanish;
};

constexpr ErrorPattern kErrors[] = {
    {"binary STL is truncated: header announces {} facets ({} bytes) but the file has {} bytes",
     "el STL binario está cortado: la cabecera anuncia {} caras ({} bytes) y el archivo tiene {} bytes"},
    {"file too short to be an STL ({} bytes)", "el archivo es demasiado corto para ser un STL ({} bytes)"},
    {"facet {} has a non-finite coordinate", "la cara {} tiene una coordenada no finita"},
    {"vertex {} is not defined yet ({} so far)", "el vértice {} todavía no está definido (hay {})"},
    {"a face needs three vertices", "una cara necesita tres vértices"},
    {"no faces", "no tiene caras"},
    {"not a ZIP package", "no es un paquete ZIP"},
    {"nothing on the build plate to print", "no hay nada en la placa para imprimir"},
    {"cut short in a chunk header", "cortado en la cabecera de un bloque"},
    {"no extrusions found", "no tiene extrusiones"},
    {"not valid JSON at line {}, column {}", "JSON inválido en la línea {}, columna {}"},
    {"the file is empty", "el archivo está vacío"},
    {"empty file", "archivo vacío"},
    {"'{}' is a folder, not a model", "«{}» es una carpeta, no un modelo"},
    {"'{}' is a folder, not a settings file", "«{}» es una carpeta, no un archivo de ajustes"},
    {"{} files are not supported yet: save the model as STL, OBJ or 3MF",
     "los archivos {} todavía no se leen: guardá el modelo como STL, OBJ o 3MF"},
    {"cannot open '{}'", "no se puede abrir «{}»"},
    {"cannot read '{}'", "no se puede leer «{}»"},
    {"the top level must be an object", "el nivel de arriba tiene que ser un objeto"},
};

// What goes before a message, kept as a place.
constexpr ErrorPattern kPlaces[] = {
    {"OBJ, line {}: ", "OBJ, línea {}: "},       {"ASCII STL, line {}: ", "STL ASCII, línea {}: "},
    {"3MF, line {}: ", "3MF, línea {}: "},       {"3MF: ", "3MF: "},
    {"OBJ: ", "OBJ: "},                          {"binary STL: ", "STL binario: "},
    {"project: ", "proyecto: "},                 {"settings: ", "ajustes: "},
    {"case: ", "caso de carga: "},
};

// The parts "{}" stands for, or nothing when the message is not the
// pattern; with `prefix`, the pattern may end before the message does, and
// `used` says where.
std::optional<std::vector<std::string_view>> match(std::string_view pattern, std::string_view text, bool prefix,
                                                   std::size_t* used = nullptr) {
    std::vector<std::string_view> literals;
    for (std::size_t p = 0;;) {
        const std::size_t hole = pattern.find("{}", p);
        literals.push_back(pattern.substr(p, hole == std::string_view::npos ? std::string_view::npos : hole - p));
        if (hole == std::string_view::npos)
            break;
        p = hole + 2;
    }
    if (!text.starts_with(literals.front()))
        return std::nullopt;
    std::size_t t = literals.front().size();
    std::vector<std::string_view> parts;
    for (std::size_t k = 1; k < literals.size(); ++k) {
        const std::string_view next = literals[k];
        std::size_t end;
        if (k + 1 == literals.size() && !prefix) {
            // The last part runs to the closing words at the very end.
            if (text.size() < t + next.size() || !text.ends_with(next))
                return std::nullopt;
            end = text.size() - next.size();
        } else {
            end = next.empty() ? text.size() : text.find(next, t);
            if (end == std::string_view::npos)
                return std::nullopt;
        }
        parts.push_back(text.substr(t, end - t));
        t = end + next.size();
    }
    if (!prefix && t != text.size())
        return std::nullopt;
    if (used)
        *used = t;
    return parts;
}

std::string filled(std::string_view pattern, const std::vector<std::string_view>& parts) {
    std::string out;
    std::size_t p = 0, k = 0;
    for (std::size_t hole = pattern.find("{}"); hole != std::string_view::npos; hole = pattern.find("{}", p)) {
        out += pattern.substr(p, hole - p);
        if (k < parts.size())
            out += parts[k++];
        p = hole + 2;
    }
    out += pattern.substr(p);
    return out;
}

std::string spanish_error(std::string_view text) {
    for (const ErrorPattern& e : kErrors)
        if (const auto parts = match(e.english, text, false))
            return filled(e.spanish, *parts);
    for (const ErrorPattern& e : kPlaces) {
        std::size_t used = 0;
        if (const auto parts = match(e.english, text, true, &used))
            return filled(e.spanish, *parts) + spanish_error(text.substr(used));
    }
    // A path or a name before ": ", kept; the rest translated.
    if (const std::size_t colon = text.find(": "); colon != std::string_view::npos)
        return std::string(text.substr(0, colon + 2)) + spanish_error(text.substr(colon + 2));
    return std::string(text);
}

} // namespace

std::string translated_error(std::string_view message) {
    return language() == Language::Spanish ? spanish_error(message) : std::string(message);
}

std::optional<double> typed_number(std::string_view text) {
    std::string plain;
    for (const char c : text)
        if (c != ' ')
            plain += c == ',' ? '.' : c;
    if (plain.empty() || plain.find_first_not_of("0123456789.+-eE") != std::string::npos)
        return std::nullopt;
    char* end = nullptr;
    const double v = std::strtod(plain.c_str(), &end);
    if (end != plain.c_str() + plain.size() || !std::isfinite(v))
        return std::nullopt;
    return v;
}

} // namespace tisma::ui
