#pragma once

#include <cstddef>
#include <cstdint>

namespace tisma {

// CRC-32 as ISO 3309 and the PNG specification define it (the one ZIP uses),
// and Adler-32 of RFC 1950. Both continue from a previous value.
std::uint32_t crc32(const std::uint8_t* data, std::size_t size, std::uint32_t crc = 0);
std::uint32_t adler32(const std::uint8_t* data, std::size_t size, std::uint32_t adler = 1);

} // namespace tisma
