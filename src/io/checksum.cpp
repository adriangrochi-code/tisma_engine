// CRC-32 and Adler-32. Own implementation of the definitions in the PNG
// specification (ISO 3309 CRC) and RFC 1950; see PROVENANCE.md.

#include "io/checksum.hpp"

#include <array>

namespace tisma {
namespace {

std::array<std::uint32_t, 256> crc_table() {
    std::array<std::uint32_t, 256> table{};
    for (std::uint32_t n = 0; n < 256; ++n) {
        std::uint32_t c = n;
        for (int k = 0; k < 8; ++k)
            c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
        table[n] = c;
    }
    return table;
}

} // namespace

std::uint32_t crc32(const std::uint8_t* data, std::size_t size, std::uint32_t crc) {
    static const std::array<std::uint32_t, 256> table = crc_table();
    crc = ~crc;
    for (std::size_t i = 0; i < size; ++i)
        crc = table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    return ~crc;
}

std::uint32_t adler32(const std::uint8_t* data, std::size_t size, std::uint32_t adler) {
    constexpr std::uint32_t kMod = 65521;
    std::uint32_t a = adler & 0xFFFF, b = adler >> 16;
    for (std::size_t i = 0; i < size; ++i) {
        a = (a + data[i]) % kMod;
        b = (b + a) % kMod;
    }
    return (b << 16) | a;
}

} // namespace tisma
