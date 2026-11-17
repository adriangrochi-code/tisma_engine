// Ed25519 verification from RFC 8032 (5.1.2 to 5.1.4 and 5.1.7), with the
// curve's numbers in Mbed TLS's mbedtls_mpi (a slow but plain path: one check
// per download of a signed file). Own design; see PROVENANCE.md.
#include "net/ed25519.hpp"

#include <mbedtls/bignum.h>
#include <mbedtls/sha512.h>

#include <algorithm>
#include <array>
#include <cctype>

namespace tisma::net {
namespace {

// An mbedtls_mpi that frees itself.
struct Num {
    mbedtls_mpi v;
    Num() { mbedtls_mpi_init(&v); }
    explicit Num(long n) {
        mbedtls_mpi_init(&v);
        mbedtls_mpi_lset(&v, n);
    }
    Num(const Num& o) {
        mbedtls_mpi_init(&v);
        mbedtls_mpi_copy(&v, &o.v);
    }
    Num& operator=(const Num& o) {
        if (this != &o)
            mbedtls_mpi_copy(&v, &o.v);
        return *this;
    }
    ~Num() { mbedtls_mpi_free(&v); }
};

struct Field {
    Num p, d, sqrt_m1, order;
    Field() {
        // p = 2^255 - 19
        mbedtls_mpi_lset(&p.v, 1);
        mbedtls_mpi_shift_l(&p.v, 255);
        mbedtls_mpi_sub_int(&p.v, &p.v, 19);
        // d = -121665 / 121666 mod p
        Num a(121665), b(121666), inv;
        mbedtls_mpi_inv_mod(&inv.v, &b.v, &p.v);
        mbedtls_mpi_mul_mpi(&d.v, &a.v, &inv.v);
        mbedtls_mpi_mod_mpi(&d.v, &d.v, &p.v);
        mbedtls_mpi_sub_mpi(&d.v, &p.v, &d.v);
        // sqrt(-1) = 2^((p - 1) / 4) mod p
        Num two(2), e;
        mbedtls_mpi_sub_int(&e.v, &p.v, 1);
        mbedtls_mpi_shift_r(&e.v, 2);
        mbedtls_mpi_exp_mod(&sqrt_m1.v, &two.v, &e.v, &p.v, nullptr);
        // L = 2^252 + 27742317777372353535851937790883648493
        mbedtls_mpi_read_string(&order.v, 16, "1000000000000000000000000000000014DEF9DEA2F79CD65812631A5CF5D3ED");
    }
};

const Field& field() {
    static const Field f;
    return f;
}

Num mod(const Num& a) {
    Num r;
    mbedtls_mpi_mod_mpi(&r.v, &a.v, &field().p.v);
    return r;
}
Num add(const Num& a, const Num& b) {
    Num r;
    mbedtls_mpi_add_mpi(&r.v, &a.v, &b.v);
    return mod(r);
}
Num sub(const Num& a, const Num& b) {
    Num r;
    mbedtls_mpi_sub_mpi(&r.v, &a.v, &b.v);
    return mod(r); // mbedtls_mpi_mod_mpi gives 0 <= r < p for a negative a too
}
Num mul(const Num& a, const Num& b) {
    Num r;
    mbedtls_mpi_mul_mpi(&r.v, &a.v, &b.v);
    return mod(r);
}
Num power(const Num& a, const Num& e) {
    Num r;
    mbedtls_mpi_exp_mod(&r.v, &a.v, &e.v, &field().p.v, nullptr);
    return r;
}
Num inverse(const Num& a) {
    Num r;
    mbedtls_mpi_inv_mod(&r.v, &a.v, &field().p.v);
    return r;
}
bool equal(const Num& a, const Num& b) { return mbedtls_mpi_cmp_mpi(&a.v, &b.v) == 0; }

// A point in extended coordinates (RFC 8032, 5.1.4): x = X/Z, y = Y/Z, x y = T/Z.
struct Point {
    Num x, y, z, t;
};

Point add(const Point& p, const Point& q) {
    const Field& f = field();
    const Num a = mul(sub(p.y, p.x), sub(q.y, q.x));
    const Num b = mul(add(p.y, p.x), add(q.y, q.x));
    const Num c = mul(mul(mul(p.t, Num(2)), f.d), q.t);
    const Num d = mul(mul(p.z, Num(2)), q.z);
    const Num e = sub(b, a), g = add(d, c), ff = sub(d, c), h = add(b, a);
    return {mul(e, ff), mul(g, h), mul(ff, g), mul(e, h)};
}

Point scale(const Point& p, const Num& s) {
    Point r{Num(0), Num(1), Num(1), Num(0)}; // the neutral point
    Point q = p;
    const std::size_t bits = mbedtls_mpi_bitlen(&s.v);
    for (std::size_t i = 0; i < bits; ++i) {
        if (mbedtls_mpi_get_bit(&s.v, i))
            r = add(r, q);
        q = add(q, q);
    }
    return r;
}

// RFC 8032, 5.1.3: 32 bytes to a point, or false.
bool decode(std::span<const std::uint8_t> b, Point& out) {
    const Field& f = field();
    std::array<std::uint8_t, 32> c{};
    std::copy(b.begin(), b.end(), c.begin());
    const int sign = c[31] >> 7;
    c[31] &= 0x7F;
    Num y;
    mbedtls_mpi_read_binary_le(&y.v, c.data(), c.size());
    if (mbedtls_mpi_cmp_mpi(&y.v, &f.p.v) >= 0)
        return false;
    const Num yy = mul(y, y);
    const Num u = sub(yy, Num(1));
    const Num v = add(mul(f.d, yy), Num(1));
    // x = (u / v)^((p + 3) / 8) = u v^3 (u v^7)^((p - 5) / 8)
    Num e;
    mbedtls_mpi_sub_int(&e.v, &f.p.v, 5);
    mbedtls_mpi_shift_r(&e.v, 3);
    const Num v3 = mul(mul(v, v), v);
    const Num v7 = mul(mul(v3, v3), v);
    Num x = mul(mul(u, v3), power(mul(u, v7), e));
    const Num vxx = mul(v, mul(x, x));
    if (equal(vxx, u)) {
    } else if (equal(vxx, sub(Num(0), u))) {
        x = mul(x, f.sqrt_m1);
    } else {
        return false;
    }
    const bool zero = mbedtls_mpi_cmp_int(&x.v, 0) == 0;
    if (zero && sign == 1)
        return false;
    if (int(mbedtls_mpi_get_bit(&x.v, 0)) != sign)
        x = sub(Num(0), x);
    out = {x, y, Num(1), mul(x, y)};
    return true;
}

// RFC 8032, 5.1.2.
std::array<std::uint8_t, 32> encode(const Point& p) {
    const Num zi = inverse(p.z);
    const Num x = mul(p.x, zi), y = mul(p.y, zi);
    std::array<std::uint8_t, 32> out{};
    mbedtls_mpi_write_binary_le(&y.v, out.data(), out.size());
    if (mbedtls_mpi_get_bit(&x.v, 0))
        out[31] |= 0x80;
    return out;
}

Point base() {
    // The base point's y = 4/5; x the even root.
    std::array<std::uint8_t, 32> b{};
    const Num y = mul(Num(4), inverse(Num(5)));
    mbedtls_mpi_write_binary_le(&y.v, b.data(), b.size());
    Point p;
    decode(b, p);
    return p;
}

} // namespace

bool ed25519_verify(std::span<const std::uint8_t> public_key, std::span<const std::uint8_t> message,
                    std::span<const std::uint8_t> signature) {
    if (public_key.size() != 32 || signature.size() != 64)
        return false;
    Point a, r;
    if (!decode(public_key, a) || !decode(signature.subspan(0, 32), r))
        return false;
    Num s;
    mbedtls_mpi_read_binary_le(&s.v, signature.data() + 32, 32);
    if (mbedtls_mpi_cmp_mpi(&s.v, &field().order.v) >= 0)
        return false;
    // k = SHA-512(R || A || M) mod L
    std::vector<std::uint8_t> data;
    data.reserve(64 + message.size());
    data.insert(data.end(), signature.begin(), signature.begin() + 32);
    data.insert(data.end(), public_key.begin(), public_key.end());
    data.insert(data.end(), message.begin(), message.end());
    std::array<std::uint8_t, 64> h{};
    if (mbedtls_sha512(data.data(), data.size(), h.data(), 0) != 0)
        return false;
    Num k;
    mbedtls_mpi_read_binary_le(&k.v, h.data(), h.size());
    mbedtls_mpi_mod_mpi(&k.v, &k.v, &field().order.v);
    // [S]B = R + [k]A, compared encoded: [S]B - [k]A == R
    static const Point b = base();
    const Point sb = scale(b, s);
    Point ka = scale(a, k);
    ka.x = sub(Num(0), ka.x), ka.t = sub(Num(0), ka.t); // the negative point
    const std::array<std::uint8_t, 32> got = encode(add(sb, ka));
    return std::equal(got.begin(), got.end(), signature.begin());
}

std::vector<std::uint8_t> key_bytes(const std::string& text) {
    std::string s;
    for (const char c : text)
        if (!std::isspace(static_cast<unsigned char>(c)))
            s += c;
    if (s.empty())
        return {};
    const auto hex = [](char c) { return std::isxdigit(static_cast<unsigned char>(c)) != 0; };
    if (s.size() % 2 == 0 && std::all_of(s.begin(), s.end(), hex)) {
        std::vector<std::uint8_t> out(s.size() / 2);
        for (std::size_t i = 0; i < out.size(); ++i)
            out[i] = std::uint8_t(std::stoi(s.substr(2 * i, 2), nullptr, 16));
        return out;
    }
    // Base64 (RFC 4648), with or without padding.
    static const std::string letters = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::vector<std::uint8_t> out;
    std::uint32_t acc = 0;
    int bits = 0;
    for (const char c : s) {
        if (c == '=')
            break;
        const std::size_t at = letters.find(c);
        if (at == std::string::npos)
            return {};
        acc = (acc << 6) | std::uint32_t(at);
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out.push_back(std::uint8_t((acc >> bits) & 0xFF));
        }
    }
    return out;
}

} // namespace tisma::net
