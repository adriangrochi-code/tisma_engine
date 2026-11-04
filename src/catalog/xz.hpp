#pragma once

// Reading .xz files and the tar archives inside them, for the profile pack
// (docs/spec profile_pack_format.md, «Descarga»). Own code from public
// format descriptions: the .xz file format (Tukaani, xz-file-format.txt),
// the LZMA specification (Igor Pavlov, lzma-specification.txt), the LZMA2
// chunk layout, and the POSIX ustar and pax formats. See PROVENANCE.md.

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace tisma {

// CRC-64 of ECMA-182 as xz uses it (polynomial 0xC96C5795D7870F42 reflected).
std::uint64_t crc64(std::string_view data, std::uint64_t crc = 0);

// Decompresses a whole .xz file: one or more streams, with stream padding,
// LZMA2 as the only filter, and the None, CRC32, CRC64 or SHA-256 check.
// Every header, index and check is verified. Stops with an error instead of
// writing more than `max_output` bytes. False with `error` set on failure.
bool xz_decompress(std::string_view data, std::string& out, std::string& error,
                   std::size_t max_output = std::size_t(1) << 31);

// One regular file of a tar archive.
struct TarEntry {
    std::string path; // as stored (pax `path` or GNU long name when present)
    std::string data;
};

// The regular files of a tar archive (ustar, pax and GNU long names).
// Directories are skipped; links and devices go to `skipped` with their path.
// False with `error` set when the archive is broken (bad header checksum,
// a size past the end).
bool tar_read(std::string_view data, std::vector<TarEntry>& files, std::vector<std::string>& skipped,
              std::string& error);

} // namespace tisma
