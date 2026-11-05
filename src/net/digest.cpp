#include "net/digest.hpp"

#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>

namespace tisma::net {

namespace {

std::uint32_t rotate_left(std::uint32_t x, int c) { return (x << c) | (x >> (32 - c)); }

// RFC 1321, 3.4: T[i] is the integer part of 4294967296 times abs(sin(i)), i in radians.
const std::array<std::uint32_t, 64>& sine_table() {
    static const std::array<std::uint32_t, 64> t = [] {
        std::array<std::uint32_t, 64> v{};
        for (int i = 0; i < 64; ++i)
            v[std::size_t(i)] = std::uint32_t(std::floor(std::fabs(std::sin(double(i + 1))) * 4294967296.0));
        return v;
    }();
    return t;
}

} // namespace

std::string md5_hex(std::string_view data) {
    // RFC 1321, 3.1 to 3.5: pad to 448 mod 512 bits, append the length, four rounds per 16-word block.
    std::vector<std::uint8_t> m(data.begin(), data.end());
    const std::uint64_t bits = std::uint64_t(data.size()) * 8u;
    m.push_back(0x80);
    while (m.size() % 64 != 56)
        m.push_back(0);
    for (int i = 0; i < 8; ++i)
        m.push_back(std::uint8_t(bits >> (8 * i)));
    std::uint32_t a0 = 0x67452301, b0 = 0xefcdab89, c0 = 0x98badcfe, d0 = 0x10325476;
    static constexpr int shifts[4][4] = {{7, 12, 17, 22}, {5, 9, 14, 20}, {4, 11, 16, 23}, {6, 10, 15, 21}};
    const auto& t = sine_table();
    for (std::size_t block = 0; block < m.size(); block += 64) {
        std::uint32_t x[16];
        for (int i = 0; i < 16; ++i)
            x[i] = std::uint32_t(m[block + 4 * i]) | std::uint32_t(m[block + 4 * i + 1]) << 8 |
                   std::uint32_t(m[block + 4 * i + 2]) << 16 | std::uint32_t(m[block + 4 * i + 3]) << 24;
        std::uint32_t a = a0, b = b0, c = c0, d = d0;
        for (int i = 0; i < 64; ++i) {
            const int round = i / 16;
            std::uint32_t f;
            int k;
            switch (round) {
            case 0: f = (b & c) | (~b & d), k = i; break;
            case 1: f = (b & d) | (c & ~d), k = (5 * i + 1) % 16; break;
            case 2: f = b ^ c ^ d, k = (3 * i + 5) % 16; break;
            default: f = c ^ (b | ~d), k = (7 * i) % 16; break;
            }
            const std::uint32_t next = b + rotate_left(a + f + x[k] + t[std::size_t(i)], shifts[round][i % 4]);
            a = d, d = c, c = b, b = next;
        }
        a0 += a, b0 += b, c0 += c, d0 += d;
    }
    std::string out;
    char hex[3];
    for (std::uint32_t w : {a0, b0, c0, d0})
        for (int i = 0; i < 4; ++i) {
            std::snprintf(hex, sizeof hex, "%02x", unsigned((w >> (8 * i)) & 0xff));
            out += hex;
        }
    return out;
}

bool parse_digest_challenge(std::string_view header, DigestChallenge& out) {
    std::size_t i = 0;
    while (i < header.size() && header[i] == ' ')
        ++i;
    if (header.substr(i, 6) != "Digest" && header.substr(i, 6) != "digest")
        return false;
    i += 6;
    out = {};
    // key=value or key="value", separated by commas.
    while (i < header.size()) {
        while (i < header.size() && (header[i] == ' ' || header[i] == ','))
            ++i;
        const std::size_t eq = header.find('=', i);
        if (eq == std::string_view::npos)
            break;
        std::string key(header.substr(i, eq - i));
        for (char& c : key)
            c = char(std::tolower(static_cast<unsigned char>(c)));
        i = eq + 1;
        std::string value;
        if (i < header.size() && header[i] == '"') {
            ++i;
            while (i < header.size() && header[i] != '"') {
                if (header[i] == '\\' && i + 1 < header.size())
                    ++i;
                value += header[i++];
            }
            ++i;
        } else {
            while (i < header.size() && header[i] != ',')
                value += header[i++];
        }
        if (key == "realm")
            out.realm = value;
        else if (key == "nonce")
            out.nonce = value;
        else if (key == "opaque")
            out.opaque = value;
        else if (key == "qop")
            out.qop = value;
        else if (key == "algorithm")
            out.algorithm = value;
    }
    return !out.nonce.empty();
}

std::string digest_authorization(const DigestChallenge& ch, const std::string& user, const std::string& password,
                                 const std::string& method, const std::string& uri, const std::string& cnonce, unsigned count) {
    // RFC 7616, 3.4.1 to 3.4.3 (MD5, qop auth; RFC 2069's form when the server asks no qop).
    const std::string ha1 = md5_hex(user + ":" + ch.realm + ":" + password);
    const std::string ha2 = md5_hex(method + ":" + uri);
    char nc[9];
    std::snprintf(nc, sizeof nc, "%08x", count);
    const bool auth = ch.qop.find("auth") != std::string::npos;
    const std::string response =
        auth ? md5_hex(ha1 + ":" + ch.nonce + ":" + nc + ":" + cnonce + ":auth:" + ha2) : md5_hex(ha1 + ":" + ch.nonce + ":" + ha2);
    std::string h = "Digest username=\"" + user + "\", realm=\"" + ch.realm + "\", nonce=\"" + ch.nonce + "\", uri=\"" + uri +
                    "\", response=\"" + response + "\"";
    if (!ch.algorithm.empty())
        h += ", algorithm=" + ch.algorithm;
    if (!ch.opaque.empty())
        h += ", opaque=\"" + ch.opaque + "\"";
    if (auth)
        h += ", qop=auth, nc=" + std::string(nc) + ", cnonce=\"" + cnonce + "\"";
    return h;
}

} // namespace tisma::net
