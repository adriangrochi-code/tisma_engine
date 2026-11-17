// Ed25519 verification (src/net/ed25519): RFC 8032's test vectors (section
// 7.1, tests 1 and 2), a signature made with OpenSSL 3 over a message of the
// library key's certificate form, and what must be refused.
#include "net/ed25519.hpp"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

using tisma::net::ed25519_verify;
using tisma::net::key_bytes;

namespace {
std::vector<std::uint8_t> text(const std::string& s) { return {s.begin(), s.end()}; }
} // namespace

TEST_CASE("ed25519: RFC 8032's vectors verify, and a changed byte does not", "[net][ed25519]") {
    const auto key1 = key_bytes("d75a980182b10ab7d54bfed3c964073a0ee172f3daa62325af021a68f707511a");
    const auto sig1 = key_bytes("e5564300c360ac729086e2cc806e828a84877f1eb8e5d974d873e065224901555fb8821590a33bacc61e39701cf9b46bd25bf5f0595bbe24655141438e7a100b");
    CHECK(ed25519_verify(key1, {}, sig1));
    const auto key2 = key_bytes("3d4017c3e843895a92b70aa74d1b7ebc9c982ccf2ec4968cc0cd55f12af4660c");
    const auto sig2 = key_bytes("92a009a9f0d4cab8720e820b5f642540a2b27b5416503f8fb3762223ebdb69da085ac1e43e15996e458f3613d0f11d8c387b2eaeb4302aeeb00d291612bb0c00");
    const std::vector<std::uint8_t> msg2{0x72};
    CHECK(ed25519_verify(key2, msg2, sig2));
    CHECK_FALSE(ed25519_verify(key2, std::vector<std::uint8_t>{0x73}, sig2)); // another message
    CHECK_FALSE(ed25519_verify(key1, msg2, sig2));                            // another key
    auto bad = sig2;
    bad[40] ^= 1;
    CHECK_FALSE(ed25519_verify(key2, msg2, bad)); // a changed S
    bad = sig2;
    bad[3] ^= 1;
    CHECK_FALSE(ed25519_verify(key2, msg2, bad)); // a changed R
    CHECK_FALSE(ed25519_verify(key2, msg2, std::vector<std::uint8_t>(sig2.begin(), sig2.end() - 1)));
}

TEST_CASE("ed25519: a signature made by OpenSSL 3, and keys in hex or base64", "[net][ed25519]") {
    const auto key = key_bytes("13ea8dd552898e7359fe20e657c5cdaa206927c82ab62bf09189bb4694ff83af");
    const auto sig = key_bytes("8665a27895814aaf09eaef1b0a6687b91650b14ce7ab260a6a2b27ebb18168f14c9af62818e832a357847f7bf1defa9fb71496eab62f33e8897496d90efd9c07");
    CHECK(ed25519_verify(key, text("tisma-library-key/1\nhello index"), sig));
    CHECK_FALSE(ed25519_verify(key, text("tisma-library-key/1\nhello indey"), sig));
    // The same key in base64.
    CHECK(key_bytes("E+qN1VKJjnNZ/iDmV8XNqiBpJ8gqtivwkYm7RpT/g68=") == key);
    CHECK(key_bytes("not a key!").empty());
}
