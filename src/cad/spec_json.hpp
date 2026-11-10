#pragma once

// Reading a part's JSON spec (a gear, a fastener) with plain messages: what
// is wrong and which key, never the JSON library's own text.

#include <nlohmann/json.hpp>

#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>

namespace tisma::cad {

// The spec's object; throws std::runtime_error "not valid JSON" or "not a
// <what>" when it is not one with "format": `format`.
inline nlohmann::json spec_object(std::string_view text, const char* format, const char* what) {
    nlohmann::json j = nlohmann::json::parse(text, nullptr, false);
    if (j.is_discarded())
        throw std::runtime_error("not valid JSON");
    if (!j.is_object() || !j.contains("format") || !j.at("format").is_string() || j.at("format").get<std::string>() != format)
        throw std::runtime_error(std::string("not a ") + what);
    return j;
}

// `key` of the spec, `fallback` when it is left out; throws
// std::runtime_error naming the key when its type is wrong.
template <class T>
T spec_field(const nlohmann::json& j, const char* key, const T& fallback) {
    if (!j.contains(key) || j.at(key).is_null())
        return fallback;
    const nlohmann::json& v = j.at(key);
    if constexpr (std::is_same_v<T, bool>) {
        if (v.is_boolean())
            return v.get<bool>();
        throw std::runtime_error(std::string("\"") + key + "\" must be true or false");
    } else if constexpr (std::is_integral_v<T>) {
        if (v.is_number_integer())
            return v.get<T>();
        throw std::runtime_error(std::string("\"") + key + "\" must be a whole number");
    } else if constexpr (std::is_floating_point_v<T>) {
        if (v.is_number())
            return v.get<T>();
        throw std::runtime_error(std::string("\"") + key + "\" must be a number");
    } else {
        if (v.is_string())
            return v.get<std::string>();
        throw std::runtime_error(std::string("\"") + key + "\" must be text");
    }
}

} // namespace tisma::cad
