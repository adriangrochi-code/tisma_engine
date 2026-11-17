#pragma once
// Ed25519 signature checking (RFC 8032, section 5.1.7): only verification, the
// program never signs. For the shared library's signed index and block list
// and the library key's certificate (p2p_library.md, rules 3 bis to 3
// quinquies). Own design from the RFC; the big numbers and SHA-512 come from
// Mbed TLS (THIRD_PARTY.md). See PROVENANCE.md.
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace tisma::net {

// Whether `signature` (64 bytes) is `public_key`'s (32 bytes) over `message`.
// Any wrong size, a key or R that is not a point, or S not below the group's
// order: false.
bool ed25519_verify(std::span<const std::uint8_t> public_key, std::span<const std::uint8_t> message,
                    std::span<const std::uint8_t> signature);

// Hex (either case) or base64 to bytes; empty when it is neither.
std::vector<std::uint8_t> key_bytes(const std::string& text);

} // namespace tisma::net
