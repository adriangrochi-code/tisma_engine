#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace tisma {

class ZipError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

// DEFLATE data (RFC 1951) inflated: stored, fixed and dynamic Huffman
// blocks. `limit` bounds the output (an archive gives the size), so a damaged
// or hostile stream cannot grow without end. Throws ZipError.
std::string inflate(std::string_view deflated, std::size_t limit);

// Whether the data starts like a ZIP archive (a local file header).
bool looks_like_zip(std::string_view data);

// A ZIP archive held in memory (PKWARE's APPNOTE.TXT): the entries of its
// central directory, and each entry's data on demand, stored or deflated,
// with its CRC-32 checked. ZIP64 sizes and offsets are read; encrypted
// entries and other compression methods are refused. Throws ZipError. The
// archive's bytes must outlive it.
class ZipArchive {
public:
    struct Entry {
        std::string name;
        std::uint64_t header = 0;     // offset of the local file header
        std::uint64_t compressed = 0; // bytes stored
        std::uint64_t size = 0;       // bytes once inflated
        std::uint32_t crc = 0;
        std::uint16_t method = 0;     // 0 stored, 8 deflated
        std::uint16_t flags = 0;
    };

    explicit ZipArchive(std::string_view data);

    const std::vector<Entry>& entries() const { return entries_; }
    // The entry with this name: ASCII case does not matter, nor a leading '/'
    // (3MF parts are named as absolute paths in the package). Null if none.
    const Entry* find(std::string_view name) const;
    std::string read(const Entry& entry) const;

private:
    std::string_view data_;
    std::vector<Entry> entries_;
};

} // namespace tisma
