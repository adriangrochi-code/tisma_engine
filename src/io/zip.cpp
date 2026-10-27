// ZIP archives and DEFLATE. Own implementation from PKWARE's APPNOTE.TXT and
// RFC 1951; see PROVENANCE.md.

#include "io/zip.hpp"

#include "io/checksum.hpp"

#include <algorithm>
#include <array>
#include <cctype>

namespace tisma {
namespace {

// Reads the deflate stream's bits, the first one the lowest of its byte
// (RFC 1951, 3.1.1). Past the end of the data it reads zeros, so a short code
// at the very end can still be looked up a few bits at a time; using any of
// those zeros is an error.
class BitReader {
public:
    explicit BitReader(std::string_view data) : data_(data) {}

    std::uint32_t peek(int n) {
        if (count_ < n)
            fill();
        return std::uint32_t(bits_ & ((std::uint64_t(1) << n) - 1));
    }
    void drop(int n) {
        bits_ >>= n;
        count_ -= n;
        if (count_ < padding_)
            throw ZipError("the deflate data ends early");
    }
    std::uint32_t take(int n) {
        const std::uint32_t v = peek(n);
        drop(n);
        return v;
    }
    // To the next byte boundary, for a stored block.
    void align() { drop(count_ % 8); }
    // Bytes, once aligned: those held first, then straight from the data.
    void copy(std::size_t n, std::string& out) {
        while (n > 0 && count_ - padding_ >= 8) {
            out.push_back(char(take(8)));
            --n;
        }
        if (n == 0)
            return;
        if (n > data_.size() - pos_)
            throw ZipError("the deflate data ends early");
        out.append(data_.substr(pos_, n));
        pos_ += n;
    }

private:
    void fill() {
        while (count_ <= 56) {
            if (pos_ < data_.size())
                bits_ |= std::uint64_t(std::uint8_t(data_[pos_++])) << count_;
            else
                padding_ += 8;
            count_ += 8;
        }
    }

    std::string_view data_;
    std::size_t pos_ = 0;
    std::uint64_t bits_ = 0;
    int count_ = 0;   // bits held, the zeros past the end included
    int padding_ = 0; // of which zeros past the end
};

constexpr int kMaxBits = 15;

// A canonical Huffman code (RFC 1951, 3.2.2) from its code lengths. Codes of
// up to kFast bits are found with one look-up of the next kFast input bits;
// longer ones bit by bit, counting codes of each length.
class Huffman {
public:
    static constexpr int kFast = 10;

    void build(const std::uint8_t* lengths, int n) {
        count_.fill(0);
        for (int s = 0; s < n; ++s)
            ++count_[lengths[s]];
        count_[0] = 0;
        int left = 1;
        for (int len = 1; len <= kMaxBits; ++len) {
            left = 2 * left - count_[std::size_t(len)];
            if (left < 0)
                throw ZipError("a Huffman code in the deflate data has too many codes");
        }
        // Symbols by length, then by value: the canonical order.
        std::array<int, kMaxBits + 2> start{};
        for (int len = 1; len <= kMaxBits; ++len)
            start[std::size_t(len + 1)] = start[std::size_t(len)] + count_[std::size_t(len)];
        symbols_.assign(std::size_t(start[kMaxBits + 1]), 0);
        std::array<int, kMaxBits + 2> next = start;
        for (int s = 0; s < n; ++s)
            if (lengths[s] > 0)
                symbols_[std::size_t(next[lengths[s]]++)] = std::uint16_t(s);
        // The look-up table: each code, read lowest bit first, with every
        // value of the bits after it.
        fast_.assign(std::size_t(1) << kFast, 0);
        std::uint32_t code = 0;
        std::size_t index = 0;
        for (int len = 1; len <= kMaxBits; ++len) {
            for (int k = 0; k < count_[std::size_t(len)]; ++k, ++code, ++index) {
                if (len > kFast)
                    continue;
                std::uint32_t reversed = 0;
                for (int b = 0; b < len; ++b)
                    reversed |= ((code >> b) & 1u) << (len - 1 - b);
                for (std::uint32_t rest = 0; rest < (1u << (kFast - len)); ++rest)
                    fast_[reversed | (rest << len)] = std::uint16_t((symbols_[index] << 4) | len);
            }
            code <<= 1;
        }
    }

    int decode(BitReader& in) const {
        if (const std::uint16_t e = fast_[in.peek(kFast)]; e & 15) {
            in.drop(e & 15);
            return e >> 4;
        }
        // Bit by bit: the codes of each length are consecutive numbers,
        // starting where the shorter ones left off, doubled.
        int code = 0, first = 0, index = 0;
        for (int len = 1; len <= kMaxBits; ++len) {
            code |= int(in.take(1));
            const int n = count_[std::size_t(len)];
            if (code - first < n)
                return symbols_[std::size_t(index + code - first)];
            index += n;
            first = (first + n) << 1;
            code <<= 1;
        }
        throw ZipError("an invalid Huffman code in the deflate data");
    }

private:
    std::array<int, kMaxBits + 1> count_{};
    std::vector<std::uint16_t> symbols_;
    std::vector<std::uint16_t> fast_;
};

// RFC 1951, 3.2.5: the lengths of codes 257 to 285 and the distances of
// codes 0 to 29, with their extra bits.
constexpr std::array<std::uint16_t, 29> kLengthBase = {3,  4,  5,  6,  7,  8,  9,  10, 11,  13,  15,  17,  19,  23, 27,
                                                       31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
constexpr std::array<std::uint8_t, 29> kLengthExtra = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2,
                                                       2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
constexpr std::array<std::uint16_t, 30> kDistanceBase = {1,    2,    3,    4,    5,    7,     9,     13,    17,  25,
                                                         33,   49,   65,   97,   129,  193,   257,   385,   513, 769,
                                                         1025, 1537, 2049, 3073, 4097, 6145,  8193,  12289, 16385, 24577};
constexpr std::array<std::uint8_t, 30> kDistanceExtra = {0, 0, 0, 0, 1, 1, 2, 2,  3,  3,  4,  4,  5,  5,  6,
                                                         6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};

// The order the code length code lengths come in (RFC 1951, 3.2.7).
constexpr std::array<std::uint8_t, 19> kLengthOrder = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};

void fixed_codes(Huffman& literals, Huffman& distances) {
    std::array<std::uint8_t, 288> l{};
    std::fill(l.begin(), l.begin() + 144, std::uint8_t(8));
    std::fill(l.begin() + 144, l.begin() + 256, std::uint8_t(9));
    std::fill(l.begin() + 256, l.begin() + 280, std::uint8_t(7));
    std::fill(l.begin() + 280, l.end(), std::uint8_t(8));
    literals.build(l.data(), 288);
    std::array<std::uint8_t, 30> d{};
    d.fill(5);
    distances.build(d.data(), 30);
}

void dynamic_codes(BitReader& in, Huffman& literals, Huffman& distances) {
    const int nlit = int(in.take(5)) + 257, ndist = int(in.take(5)) + 1, nlen = int(in.take(4)) + 4;
    if (nlit > 286 || ndist > 30)
        throw ZipError("a dynamic block in the deflate data has too many codes");
    std::array<std::uint8_t, 19> cl{};
    for (int i = 0; i < nlen; ++i)
        cl[kLengthOrder[std::size_t(i)]] = std::uint8_t(in.take(3));
    Huffman lengths;
    lengths.build(cl.data(), 19);
    std::array<std::uint8_t, 286 + 30> l{};
    for (int i = 0; i < nlit + ndist;) {
        const int sym = lengths.decode(in);
        if (sym < 16) {
            l[std::size_t(i++)] = std::uint8_t(sym);
            continue;
        }
        int repeat = 0;
        std::uint8_t value = 0;
        if (sym == 16) {
            if (i == 0)
                throw ZipError("a dynamic block in the deflate data repeats a length before the first");
            value = l[std::size_t(i - 1)];
            repeat = 3 + int(in.take(2));
        } else if (sym == 17) {
            repeat = 3 + int(in.take(3));
        } else {
            repeat = 11 + int(in.take(7));
        }
        if (i + repeat > nlit + ndist)
            throw ZipError("a dynamic block in the deflate data has too many code lengths");
        std::fill(l.begin() + i, l.begin() + i + repeat, value);
        i += repeat;
    }
    if (l[256] == 0)
        throw ZipError("a dynamic block in the deflate data has no end-of-block code");
    literals.build(l.data(), nlit);
    distances.build(l.data() + nlit, ndist);
}

std::uint16_t u16(std::string_view d, std::uint64_t at) {
    if (at + 2 > d.size())
        throw ZipError("the ZIP archive is truncated");
    return std::uint16_t(std::uint8_t(d[at]) | (std::uint8_t(d[at + 1]) << 8));
}
std::uint32_t u32(std::string_view d, std::uint64_t at) {
    return std::uint32_t(u16(d, at)) | (std::uint32_t(u16(d, at + 2)) << 16);
}
std::uint64_t u64(std::string_view d, std::uint64_t at) {
    return std::uint64_t(u32(d, at)) | (std::uint64_t(u32(d, at + 4)) << 32);
}

// DEFLATE gives at most 258 bytes for a code of 1 bit and a distance of 1
// (RFC 1951, 3.2.5), so a stream never inflates to more than about 1032
// times its size; an archive that says more is damaged.
constexpr std::uint64_t kMaxRatio = 1032;

constexpr std::uint32_t kLocalHeader = 0x04034b50, kCentralHeader = 0x02014b50, kEnd = 0x06054b50,
                        kEnd64Locator = 0x07064b50, kEnd64 = 0x06064b50;

std::string normalized(std::string_view name) {
    while (!name.empty() && (name.front() == '/' || name.front() == '\\'))
        name.remove_prefix(1);
    std::string s(name);
    for (char& c : s)
        c = c == '\\' ? '/' : char(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

} // namespace

std::string inflate(std::string_view deflated, std::size_t limit) {
    std::string out;
    out.reserve(std::min<std::uint64_t>(limit, kMaxRatio * std::uint64_t(deflated.size()) + 64));
    BitReader in(deflated);
    Huffman literals, distances;
    for (bool last = false; !last;) {
        last = in.take(1) != 0;
        const std::uint32_t type = in.take(2);
        if (type == 0) {
            in.align();
            const std::uint32_t len = in.take(16), nlen = in.take(16);
            if (len != (~nlen & 0xFFFFu))
                throw ZipError("a stored block in the deflate data has a bad length");
            if (len > limit - out.size())
                throw ZipError("the deflate data holds more than the archive says");
            in.copy(len, out);
            continue;
        }
        if (type == 1)
            fixed_codes(literals, distances);
        else if (type == 2)
            dynamic_codes(in, literals, distances);
        else
            throw ZipError("a block of an unknown type in the deflate data");
        for (;;) {
            const int sym = literals.decode(in);
            if (sym < 256) {
                if (out.size() >= limit)
                    throw ZipError("the deflate data holds more than the archive says");
                out.push_back(char(sym));
                continue;
            }
            if (sym == 256)
                break;
            if (sym > 285)
                throw ZipError("an invalid length code in the deflate data");
            const std::size_t li = std::size_t(sym - 257);
            const std::size_t length = kLengthBase[li] + in.take(kLengthExtra[li]);
            const int dsym = distances.decode(in);
            if (dsym >= 30)
                throw ZipError("an invalid distance code in the deflate data");
            const std::size_t distance = kDistanceBase[std::size_t(dsym)] + in.take(kDistanceExtra[std::size_t(dsym)]);
            if (distance > out.size())
                throw ZipError("the deflate data refers back before its start");
            if (length > limit - out.size())
                throw ZipError("the deflate data holds more than the archive says");
            // Byte by byte: the copy may overlap what it writes.
            std::size_t from = out.size() - distance;
            for (std::size_t k = 0; k < length; ++k)
                out.push_back(out[from + k]);
        }
    }
    return out;
}

bool looks_like_zip(std::string_view data) { return data.size() >= 4 && u32(data, 0) == kLocalHeader; }

ZipArchive::ZipArchive(std::string_view data) : data_(data) {
    // The end of central directory record: the last one in the final 64 KiB
    // and 22 bytes (its comment is at most 65535 bytes).
    if (data.size() < 22)
        throw ZipError("not a ZIP archive: too short");
    std::uint64_t end = data.size() - 22;
    const std::uint64_t lowest = data.size() > 22 + 65535 ? data.size() - 22 - 65535 : 0;
    while (u32(data, end) != kEnd) {
        if (end == lowest)
            throw ZipError("not a ZIP archive: no end of central directory");
        --end;
    }
    std::uint64_t entries = u16(data, end + 10), size = u32(data, end + 12), offset = u32(data, end + 16);
    // ZIP64: the locator right before the record points to the larger one.
    if ((entries == 0xFFFF || size == 0xFFFFFFFFu || offset == 0xFFFFFFFFu) && end >= 20 &&
        u32(data, end - 20) == kEnd64Locator) {
        const std::uint64_t at = u64(data, end - 20 + 8);
        if (u32(data, at) != kEnd64)
            throw ZipError("the ZIP64 end of central directory is missing");
        entries = u64(data, at + 32), size = u64(data, at + 40), offset = u64(data, at + 48);
    }
    if (offset > data.size() || size > data.size() - offset)
        throw ZipError("the ZIP central directory lies outside the archive");
    if (entries > size / 46)
        throw ZipError("the ZIP central directory is too small for its entries");
    entries_.reserve(std::size_t(entries));
    std::uint64_t at = offset;
    for (std::uint64_t i = 0; i < entries; ++i) {
        if (u32(data, at) != kCentralHeader)
            throw ZipError("a damaged ZIP central directory");
        Entry e;
        e.flags = u16(data, at + 8);
        e.method = u16(data, at + 10);
        e.crc = u32(data, at + 16);
        e.compressed = u32(data, at + 20);
        e.size = u32(data, at + 24);
        const std::uint16_t name = u16(data, at + 28), extra = u16(data, at + 30), comment = u16(data, at + 32);
        e.header = u32(data, at + 42);
        if (at + 46 + name + extra + comment > data.size())
            throw ZipError("the ZIP archive is truncated");
        e.name = std::string(data.substr(std::size_t(at + 46), name));
        // The ZIP64 field gives, in this order, those of the size, the
        // compressed size and the offset that do not fit in 32 bits.
        for (std::uint64_t x = at + 46 + name; x + 4 <= at + 46 + name + extra;) {
            const std::uint16_t id = u16(data, x), len = u16(data, x + 2);
            if (id == 0x0001) {
                std::uint64_t f = x + 4;
                const std::uint64_t f_end = f + len;
                const auto field = [&](std::uint64_t& v) {
                    if (v == 0xFFFFFFFFu && f + 8 <= f_end)
                        v = u64(data, f), f += 8;
                };
                field(e.size);
                field(e.compressed);
                field(e.header);
            }
            x += 4 + std::uint64_t(len);
        }
        at += 46 + std::uint64_t(name) + extra + comment;
        entries_.push_back(std::move(e));
    }
}

const ZipArchive::Entry* ZipArchive::find(std::string_view name) const {
    const std::string wanted = normalized(name);
    for (const Entry& e : entries_)
        if (normalized(e.name) == wanted)
            return &e;
    return nullptr;
}

std::string ZipArchive::read(const Entry& e) const {
    if (e.flags & 1)
        throw ZipError(e.name + ": encrypted ZIP entries are not supported");
    if (u32(data_, e.header) != kLocalHeader)
        throw ZipError(e.name + ": a damaged ZIP local header");
    // The local header's own name and extra field lengths; its sizes may be
    // zero when a data descriptor follows (flag 3), so the central
    // directory's are used.
    const std::uint64_t start = e.header + 30 + u16(data_, e.header + 26) + u16(data_, e.header + 28);
    if (start > data_.size() || e.compressed > data_.size() - start)
        throw ZipError(e.name + ": the ZIP entry lies outside the archive");
    const std::string_view stored = data_.substr(std::size_t(start), std::size_t(e.compressed));
    std::string out;
    if (e.method == 0) {
        if (e.compressed != e.size)
            throw ZipError(e.name + ": a stored ZIP entry whose sizes differ");
        out = std::string(stored);
    } else if (e.method == 8) {
        if (e.size > kMaxRatio * e.compressed + 64)
            throw ZipError(e.name + ": the ZIP entry says it inflates to more than deflate can hold");
        out = inflate(stored, std::size_t(e.size));
        if (out.size() != e.size)
            throw ZipError(e.name + ": the ZIP entry inflates to another size than it says");
    } else {
        throw ZipError(e.name + ": ZIP compression method " + std::to_string(e.method) + " is not supported");
    }
    if (crc32(reinterpret_cast<const std::uint8_t*>(out.data()), out.size()) != e.crc)
        throw ZipError(e.name + ": the ZIP entry fails its CRC check");
    return out;
}

} // namespace tisma
