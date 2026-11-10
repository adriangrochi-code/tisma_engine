#include "io/number.hpp"

#include <catch2/catch_test_macros.hpp>

#include <string_view>

using namespace tisma;

namespace {

// Both ways: the standard library's where it has floating-point from_chars,
// and the stream (Apple's libraries before it) everywhere.
void same(std::string_view text, std::chars_format format = std::chars_format::general) {
    INFO(text);
    double a = -1.0, b = -1.0;
    const auto ra = chars_to(text.data(), text.data() + text.size(), a, format);
    const auto rb = chars_to_stream(text.data(), text.data() + text.size(), b, format);
    CHECK((ra.ec == std::errc()) == (rb.ec == std::errc()));
    if (ra.ec == std::errc() && rb.ec == std::errc()) {
        CHECK(a == b);
        CHECK(ra.ptr == rb.ptr);
    }
}

} // namespace

TEST_CASE("numbers read the same with or without the library's floating-point from_chars", "[number]") {
    for (const char* t : {"0", "1", "-2.5", "0.0125", "215", "1e3", "2.5E-2", "12.5mm", "3.", ".5", "-0", "7 8"})
        same(t);
    for (const char* t : {"", "+1", " 1", "abc", "-"})
        same(t);
    same("1.5e3", std::chars_format::fixed); // 1.5, the exponent left
    double v = 0.0;
    const std::string_view s = "4.200 ;";
    const auto r = chars_to_stream(s.data(), s.data() + s.size(), v, std::chars_format::general);
    CHECK(r.ec == std::errc());
    CHECK(v == 4.2);
    CHECK(r.ptr == s.data() + 5);
    float f = 0.0f;
    const std::string_view g = "0.4";
    CHECK(chars_to(g.data(), g.data() + g.size(), f).ec == std::errc());
    CHECK(f == 0.4f);
    int i = 0;
    const std::string_view n = "42";
    CHECK(chars_to(n.data(), n.data() + n.size(), i).ec == std::errc());
    CHECK(i == 42);
}
