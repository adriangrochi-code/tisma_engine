#pragma once

#include <charconv>
#include <locale>
#include <sstream>
#include <string>
#include <string_view>

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

} // namespace tisma
