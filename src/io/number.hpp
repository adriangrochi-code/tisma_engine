#pragma once

#include <charconv>
#include <locale>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>

namespace tisma {

// A whole decimal number (a leading '+' allowed), independent of the locale.
inline bool parse_number(std::string_view s, double& value) {
    if (!s.empty() && s.front() == '+')
        s.remove_prefix(1); // from_chars does not accept it
    if (s.empty())
        return false;
#if defined(__cpp_lib_to_chars) && __cpp_lib_to_chars >= 201611L
    const auto [end, ec] = std::from_chars(s.data(), s.data() + s.size(), value);
    return ec == std::errc() && end == s.data() + s.size();
#else
    // Standard libraries without floating-point from_chars: locale-independent stream.
    std::istringstream in{std::string(s)};
    in.imbue(std::locale::classic());
    in >> value;
    return !in.fail() && in.peek() == std::char_traits<char>::eof();
#endif
}

// std::from_chars for every type, also on standard libraries without its
// floating-point overloads (Apple's before macOS 26): there a number is read
// from a locale-independent stream, as from_chars would read it (no leading
// space or '+', `fixed` without an exponent). Own design.
// The stream reading, on every library (tested on all of them).
inline std::from_chars_result chars_to_stream(const char* first, const char* last, double& value,
                                              std::chars_format format) {
    if (first == last || *first == '+' || *first == ' ' || *first == '\t' || *first == '\n')
        return {first, std::errc::invalid_argument};
    std::string text(first, last);
    if (format == std::chars_format::fixed)
        text = text.substr(0, text.find_first_of("eE"));
    std::istringstream in{text};
    in.imbue(std::locale::classic());
    double v = 0.0;
    in >> v;
    if (in.fail())
        return {first, std::errc::invalid_argument};
    const std::streamoff used = in.eof() ? std::streamoff(text.size()) : std::streamoff(in.tellg());
    value = v;
    return {first + used, std::errc()};
}

inline std::from_chars_result chars_to(const char* first, const char* last, double& value,
                                       std::chars_format format) {
#if defined(__cpp_lib_to_chars) && __cpp_lib_to_chars >= 201611L
    return std::from_chars(first, last, value, format);
#else
    return chars_to_stream(first, last, value, format);
#endif
}

template <class T>
std::from_chars_result chars_to(const char* first, const char* last, T& value) {
    if constexpr (std::is_same_v<T, double>) {
        return chars_to(first, last, value, std::chars_format::general);
    } else if constexpr (std::is_same_v<T, float>) {
        double d = 0.0;
        const std::from_chars_result r = chars_to(first, last, d, std::chars_format::general);
        if (r.ec == std::errc())
            value = float(d);
        return r;
    } else {
        return std::from_chars(first, last, value);
    }
}

} // namespace tisma
