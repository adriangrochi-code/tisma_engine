#include "catalog/xz.hpp"

#include "catalog/pack.hpp"
#include "gcode/binary.hpp" // crc32

#include <algorithm>
#include <array>
#include <cstring>

namespace tisma {

namespace {

std::array<std::uint64_t, 256> make_crc64_table() {
    std::array<std::uint64_t, 256> t{};
    for (std::uint64_t i = 0; i < 256; ++i) {
        std::uint64_t c = i;
        for (int k = 0; k < 8; ++k) c = (c & 1u) ? (c >> 1) ^ 0xC96C5795D7870F42ull : c >> 1;
        t[i] = c;
    }
    return t;
}

std::uint32_t le32(const unsigned char* p) {
    return std::uint32_t(p[0]) | std::uint32_t(p[1]) << 8 | std::uint32_t(p[2]) << 16 | std::uint32_t(p[3]) << 24;
}

// The variable-length integers of xz (section 1.2): 7 bits per byte, low
// first, the high bit set on every byte but the last; at most 9 bytes.
bool varint(const unsigned char* data, std::size_t size, std::size_t& pos, std::uint64_t& value) {
    value = 0;
    for (int i = 0; i < 9; ++i) {
        if (pos >= size) return false;
        const unsigned char b = data[pos++];
        if (i > 0 && b == 0) return false; // not the shortest form
        value |= std::uint64_t(b & 0x7F) << (7 * i);
        if (!(b & 0x80)) return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// LZMA (lzma-specification.txt): the range decoder and the models over it.

constexpr unsigned kModelBits = 11;
constexpr std::uint16_t kHalf = 1u << (kModelBits - 1);
constexpr unsigned kShift = 5;

struct RangeIn {
    const unsigned char* p = nullptr;
    const unsigned char* end = nullptr;
    std::uint32_t range = 0, code = 0;
    bool overrun = false;

    unsigned char next() {
        if (p == end) {
            overrun = true;
            return 0;
        }
        return *p++;
    }
    bool start(const unsigned char* begin, const unsigned char* stop) {
        p = begin;
        end = stop;
        overrun = false;
        range = 0xFFFFFFFFu;
        code = 0;
        const unsigned char first = next();
        for (int i = 0; i < 4; ++i) code = code << 8 | next();
        return first == 0 && !overrun;
    }
    void refill() {
        if (range < (1u << 24)) {
            range <<= 8;
            code = code << 8 | next();
        }
    }
    unsigned bit(std::uint16_t& model) {
        const std::uint32_t split = (range >> kModelBits) * model;
        unsigned b;
        if (code < split) {
            range = split;
            model = std::uint16_t(model + (((1u << kModelBits) - model) >> kShift));
            b = 0;
        } else {
            range -= split;
            code -= split;
            model = std::uint16_t(model - (model >> kShift));
            b = 1;
        }
        refill();
        return b;
    }
    // Bits of equal chance, high first.
    std::uint32_t plain(unsigned count) {
        std::uint32_t v = 0;
        while (count--) {
            range >>= 1;
            const std::uint32_t b = code >= range ? 1u : 0u;
            if (b) code -= range;
            v = v << 1 | b;
            refill();
        }
        return v;
    }
    // A tree of `count` bits, high bit first; `models` has 1 << count items.
    unsigned tree(std::uint16_t* models, unsigned count) {
        unsigned node = 1;
        for (unsigned i = 0; i < count; ++i) node = node << 1 | bit(models[node]);
        return node - (1u << count);
    }
    // The same tree read low bit first.
    unsigned tree_low_first(std::uint16_t* models, unsigned count) {
        unsigned node = 1, v = 0;
        for (unsigned i = 0; i < count; ++i) {
            const unsigned b = bit(models[node]);
            node = node << 1 | b;
            v |= b << i;
        }
        return v;
    }
};

template <std::size_t N> void fill_half(std::array<std::uint16_t, N>& a) { a.fill(kHalf); }

struct Lengths {
    std::uint16_t first = kHalf, second = kHalf;
    std::array<std::uint16_t, 16 * 8> low{}, mid{};
    std::array<std::uint16_t, 256> high{};
    void reset() {
        first = second = kHalf;
        fill_half(low);
        fill_half(mid);
        fill_half(high);
    }
    // Zero-based length, 0 to 271.
    unsigned read(RangeIn& rc, unsigned pos_state) {
        if (!rc.bit(first)) return rc.tree(&low[pos_state * 8], 3);
        if (!rc.bit(second)) return 8 + rc.tree(&mid[pos_state * 8], 3);
        return 16 + rc.tree(high.data(), 8);
    }
};

class Lzma2Decoder {
public:
    Lzma2Decoder(std::string& out, std::size_t max_output) : out_(out), max_(max_output) {}

    // Decodes the LZMA2 data of one block from `data`; `used` is how many
    // bytes it took (up to and including the end byte 0x00).
    bool block(const unsigned char* data, std::size_t size, std::uint32_t dict_size, std::size_t& used,
               std::string& error) {
        dict_size_ = dict_size;
        bool dict_ready = false, props_ready = false;
        std::size_t pos = 0;
        for (;;) {
            if (pos >= size) return fail(error, "LZMA2 data ends without its end mark");
            const unsigned control = data[pos++];
            if (control == 0x00) break;
            if (control == 0x01 || control == 0x02) {
                if (control == 0x01) {
                    dict_start_ = out_.size();
                    dict_ready = true;
                } else if (!dict_ready) {
                    return fail(error, "LZMA2 data does not start with a dictionary reset");
                }
                if (pos + 2 > size) return fail(error, "LZMA2 chunk header cut short");
                const std::size_t n = (std::size_t(data[pos]) << 8 | data[pos + 1]) + 1;
                pos += 2;
                if (pos + n > size) return fail(error, "LZMA2 stored chunk cut short");
                if (out_.size() + n > max_) return fail(error, "uncompressed data larger than allowed");
                out_.append(reinterpret_cast<const char*>(data + pos), n);
                pos += n;
                continue;
            }
            if (control < 0x80) return fail(error, "LZMA2 control byte not valid");
            if (pos + 4 > size) return fail(error, "LZMA2 chunk header cut short");
            const std::size_t unpacked =
                ((std::size_t(control & 0x1F) << 16) | std::size_t(data[pos]) << 8 | data[pos + 1]) + 1;
            const std::size_t packed = (std::size_t(data[pos + 2]) << 8 | data[pos + 3]) + 1;
            pos += 4;
            const unsigned reset = (control >> 5) & 3;
            if (reset == 3) {
                dict_start_ = out_.size();
                dict_ready = true;
            } else if (!dict_ready) {
                return fail(error, "LZMA2 data does not start with a dictionary reset");
            }
            if (reset >= 2) {
                if (pos >= size) return fail(error, "LZMA2 chunk header cut short");
                if (!set_props(data[pos++])) return fail(error, "LZMA2 properties not valid");
                props_ready = true;
            } else if (!props_ready) {
                return fail(error, "LZMA2 chunk without properties");
            }
            if (reset >= 1) reset_state();
            if (pos + packed > size) return fail(error, "LZMA2 chunk cut short");
            if (out_.size() + unpacked > max_) return fail(error, "uncompressed data larger than allowed");
            if (!chunk(data + pos, packed, unpacked)) return fail(error, "LZMA data broken");
            pos += packed;
        }
        used = pos;
        return true;
    }

private:
    static bool fail(std::string& error, const char* what) {
        error = what;
        return false;
    }

    bool set_props(unsigned v) {
        if (v >= 9 * 5 * 5) return false;
        lc_ = v % 9;
        v /= 9;
        lp_ = v % 5;
        pb_ = v / 5;
        if (lc_ + lp_ > 4) return false;
        literals_.assign(std::size_t(0x300) << (lc_ + lp_), kHalf);
        return true;
    }

    void reset_state() {
        state_ = 0;
        rep_ = {0, 0, 0, 0};
        std::fill(literals_.begin(), literals_.end(), kHalf);
        fill_half(is_match_);
        fill_half(is_rep_);
        fill_half(is_rep0_);
        fill_half(is_rep1_);
        fill_half(is_rep2_);
        fill_half(rep0_long_);
        for (auto& s : slots_) fill_half(s);
        fill_half(middle_);
        fill_half(align_);
        lengths_.reset();
        rep_lengths_.reset();
    }

    std::size_t window() const { return out_.size() - dict_start_; }

    bool distance_ok(std::uint32_t rep) const {
        return rep < dict_size_ && std::size_t(rep) + 1 <= window();
    }

    void copy(std::uint32_t rep, std::size_t len) {
        const std::size_t back = std::size_t(rep) + 1;
        for (std::size_t i = 0; i < len; ++i) out_.push_back(out_[out_.size() - back]);
    }

    std::uint32_t read_distance(RangeIn& rc, unsigned len) {
        const unsigned row = len < 3 ? len : 3;
        const unsigned slot = rc.tree(slots_[row].data(), 6);
        if (slot < 4) return slot;
        const unsigned extra = (slot >> 1) - 1;
        std::uint32_t d = (2u | (slot & 1u)) << extra;
        if (slot < 14) {
            d += rc.tree_low_first(&middle_[d - slot], extra);
        } else {
            d += rc.plain(extra - 4) << 4;
            d += rc.tree_low_first(align_.data(), 4);
        }
        return d;
    }

    bool chunk(const unsigned char* data, std::size_t packed, std::size_t unpacked) {
        RangeIn rc;
        if (!rc.start(data, data + packed)) return false;
        const std::size_t target = out_.size() + unpacked;
        const unsigned pb_mask = (1u << pb_) - 1, lp_mask = (1u << lp_) - 1;
        while (out_.size() < target) {
            const unsigned pos_state = unsigned(window()) & pb_mask;
            const unsigned row = state_ * 16 + pos_state;
            if (!rc.bit(is_match_[row])) {
                // A literal, chosen by the previous byte and the position.
                const unsigned prev = window() > 0 ? static_cast<unsigned char>(out_.back()) : 0u;
                const unsigned ctx = ((unsigned(window()) & lp_mask) << lc_) + (prev >> (8 - lc_));
                std::uint16_t* m = &literals_[std::size_t(0x300) * ctx];
                unsigned sym = 1;
                if (state_ >= 7) {
                    if (!distance_ok(rep_[0])) return false;
                    unsigned match = static_cast<unsigned char>(out_[out_.size() - rep_[0] - 1]);
                    while (sym < 0x100) {
                        const unsigned mb = (match >> 7) & 1;
                        match <<= 1;
                        const unsigned b = rc.bit(m[((1 + mb) << 8) + sym]);
                        sym = sym << 1 | b;
                        if (b != mb) break;
                    }
                }
                while (sym < 0x100) sym = sym << 1 | rc.bit(m[sym]);
                out_.push_back(char(sym - 0x100));
                state_ = state_ < 4 ? 0 : state_ < 10 ? state_ - 3 : state_ - 6;
                continue;
            }
            unsigned len;
            if (!rc.bit(is_rep_[state_])) {
                rep_[3] = rep_[2];
                rep_[2] = rep_[1];
                rep_[1] = rep_[0];
                len = lengths_.read(rc, pos_state);
                state_ = state_ < 7 ? 7 : 10;
                rep_[0] = read_distance(rc, len);
                if (rep_[0] == 0xFFFFFFFFu) return false; // no end marks inside LZMA2
            } else {
                if (window() == 0) return false;
                if (!rc.bit(is_rep0_[state_])) {
                    if (!rc.bit(rep0_long_[row])) {
                        // One byte from the last distance.
                        state_ = state_ < 7 ? 9 : 11;
                        if (!distance_ok(rep_[0])) return false;
                        copy(rep_[0], 1);
                        continue;
                    }
                } else {
                    std::uint32_t d;
                    if (!rc.bit(is_rep1_[state_])) {
                        d = rep_[1];
                    } else {
                        if (!rc.bit(is_rep2_[state_])) {
                            d = rep_[2];
                        } else {
                            d = rep_[3];
                            rep_[3] = rep_[2];
                        }
                        rep_[2] = rep_[1];
                    }
                    rep_[1] = rep_[0];
                    rep_[0] = d;
                }
                len = rep_lengths_.read(rc, pos_state);
                state_ = state_ < 7 ? 8 : 11;
            }
            const std::size_t n = std::size_t(len) + 2;
            if (!distance_ok(rep_[0]) || out_.size() + n > target) return false;
            copy(rep_[0], n);
        }
        return !rc.overrun && rc.code == 0 && rc.p == rc.end;
    }

    std::string& out_;
    std::size_t max_;
    std::size_t dict_start_ = 0;
    std::uint32_t dict_size_ = 0;
    unsigned lc_ = 0, lp_ = 0, pb_ = 0;
    unsigned state_ = 0;
    std::array<std::uint32_t, 4> rep_{};
    std::vector<std::uint16_t> literals_;
    std::array<std::uint16_t, 12 * 16> is_match_{}, rep0_long_{};
    std::array<std::uint16_t, 12> is_rep_{}, is_rep0_{}, is_rep1_{}, is_rep2_{};
    std::array<std::array<std::uint16_t, 64>, 4> slots_{};
    std::array<std::uint16_t, 115> middle_{};
    std::array<std::uint16_t, 16> align_{};
    Lengths lengths_, rep_lengths_;
};

std::size_t check_size(unsigned id) {
    switch (id) {
    case 0x00: return 0;
    case 0x01: return 4;
    case 0x04: return 8;
    case 0x0A: return 32;
    default: return std::size_t(-1);
    }
}

std::string hex(const unsigned char* p, std::size_t n) {
    static const char* digits = "0123456789abcdef";
    std::string s;
    for (std::size_t i = 0; i < n; ++i) {
        s += digits[p[i] >> 4];
        s += digits[p[i] & 15];
    }
    return s;
}

} // namespace

std::uint64_t crc64(std::string_view data, std::uint64_t crc) {
    static const auto table = make_crc64_table();
    crc = ~crc;
    for (const char ch : data) crc = table[(crc ^ static_cast<unsigned char>(ch)) & 0xFF] ^ (crc >> 8);
    return ~crc;
}

bool xz_decompress(std::string_view text, std::string& out, std::string& error, std::size_t max_output) {
    const auto* d = reinterpret_cast<const unsigned char*>(text.data());
    const std::size_t size = text.size();
    auto view = [&](std::size_t at, std::size_t n) { return std::string_view(text.data() + at, n); };
    static const unsigned char magic[6] = {0xFD, '7', 'z', 'X', 'Z', 0x00};
    out.clear();
    std::size_t pos = 0;
    int streams = 0;
    while (pos < size) {
        if (streams > 0) {
            const std::size_t start = pos;
            while (pos < size && d[pos] == 0) ++pos;
            if ((pos - start) % 4 != 0) return error = "xz stream padding not a multiple of four", false;
            if (pos == size) break;
        }
        if (size - pos < 12 || std::memcmp(d + pos, magic, 6) != 0) return error = "not an xz file", false;
        if (crc32(view(pos + 6, 2)) != le32(d + pos + 8)) return error = "xz stream header damaged", false;
        if (d[pos + 6] != 0 || (d[pos + 7] & 0xF0)) return error = "xz stream flags not supported", false;
        const unsigned flags = d[pos + 7];
        const unsigned check = flags & 0x0F;
        const std::size_t csize = check_size(check);
        if (csize == std::size_t(-1)) return error = "xz check type not supported", false;
        pos += 12;
        struct Record {
            std::uint64_t unpadded, uncompressed;
        };
        std::vector<Record> records;
        // Blocks until the index indicator.
        for (;;) {
            if (pos >= size) return error = "xz file cut short", false;
            if (d[pos] == 0) break;
            const std::size_t header = (std::size_t(d[pos]) + 1) * 4;
            if (pos + header > size) return error = "xz file cut short", false;
            if (crc32(view(pos, header - 4)) != le32(d + pos + header - 4))
                return error = "xz block header damaged", false;
            const unsigned bflags = d[pos + 1];
            if (bflags & 0x3C) return error = "xz block flags not supported", false;
            std::size_t p = pos + 2;
            const std::size_t hend = pos + header - 4;
            std::uint64_t packed = 0, unpacked = 0;
            const bool has_packed = bflags & 0x40, has_unpacked = bflags & 0x80;
            if (has_packed && (!varint(d, hend, p, packed) || packed == 0))
                return error = "xz block header damaged", false;
            if (has_unpacked && !varint(d, hend, p, unpacked)) return error = "xz block header damaged", false;
            if ((bflags & 0x03) != 0) return error = "xz filter chain not supported (only LZMA2)", false;
            std::uint64_t id = 0, prop_size = 0;
            if (!varint(d, hend, p, id) || !varint(d, hend, p, prop_size))
                return error = "xz block header damaged", false;
            if (id != 0x21 || prop_size != 1 || p >= hend)
                return error = "xz filter chain not supported (only LZMA2)", false;
            const unsigned bits = d[p++];
            if (bits > 40) return error = "xz dictionary size not valid", false;
            const std::uint32_t dict = bits == 40 ? 0xFFFFFFFFu : (2u | (bits & 1u)) << (bits / 2 + 11);
            for (; p < hend; ++p)
                if (d[p] != 0) return error = "xz block header damaged", false;
            const std::size_t data_at = pos + header;
            const std::size_t limit = has_packed ? (packed > size - data_at ? size : data_at + std::size_t(packed)) : size;
            const std::size_t before = out.size();
            if (has_unpacked && unpacked <= max_output - before) out.reserve(before + std::size_t(unpacked));
            Lzma2Decoder lz(out, max_output);
            std::size_t used = 0;
            if (!lz.block(d + data_at, limit - data_at, dict, used, error)) return false;
            if (has_packed && used != packed) return error = "xz block size does not match", false;
            const std::size_t made = out.size() - before;
            if (has_unpacked && made != unpacked) return error = "xz block size does not match", false;
            pos = data_at + used;
            while ((pos - (data_at - header)) % 4 != 0) {
                if (pos >= size || d[pos] != 0) return error = "xz block padding damaged", false;
                ++pos;
            }
            if (pos + csize > size) return error = "xz file cut short", false;
            const std::string_view made_view(out.data() + before, made);
            bool good = true;
            if (check == 0x01) good = crc32(made_view) == le32(d + pos);
            else if (check == 0x04) good = crc64(made_view) == (std::uint64_t(le32(d + pos)) | std::uint64_t(le32(d + pos + 4)) << 32);
            else if (check == 0x0A) good = sha256_hex(made_view) == hex(d + pos, 32);
            if (!good) return error = "xz data check does not match", false;
            records.push_back({header + used + csize, made});
            pos += csize;
        }
        // The index.
        const std::size_t index_at = pos++;
        std::uint64_t count = 0;
        if (!varint(d, size, pos, count) || count != records.size()) return error = "xz index damaged", false;
        for (const Record& r : records) {
            std::uint64_t a = 0, b = 0;
            if (!varint(d, size, pos, a) || !varint(d, size, pos, b) || a != r.unpadded || b != r.uncompressed)
                return error = "xz index damaged", false;
        }
        while ((pos - index_at) % 4 != 0) {
            if (pos >= size || d[pos] != 0) return error = "xz index damaged", false;
            ++pos;
        }
        if (pos + 4 + 12 > size) return error = "xz file cut short", false;
        if (crc32(view(index_at, pos - index_at)) != le32(d + pos)) return error = "xz index damaged", false;
        pos += 4;
        const std::size_t index_size = pos - index_at;
        // The footer.
        if (crc32(view(pos + 4, 6)) != le32(d + pos)) return error = "xz stream footer damaged", false;
        if ((std::size_t(le32(d + pos + 4)) + 1) * 4 != index_size) return error = "xz stream footer damaged", false;
        if (d[pos + 8] != 0 || d[pos + 9] != flags || d[pos + 10] != 'Y' || d[pos + 11] != 'Z')
            return error = "xz stream footer damaged", false;
        pos += 12;
        ++streams;
    }
    if (streams == 0) return error = "not an xz file", false;
    return true;
}

// ---------------------------------------------------------------------------
// tar: POSIX ustar headers of 512 bytes, pax extended headers (`x`, `g`) and
// GNU long names (`L`).

namespace {

std::string field(const unsigned char* p, std::size_t n) {
    std::size_t len = 0;
    while (len < n && p[len] != 0) ++len;
    return std::string(reinterpret_cast<const char*>(p), len);
}

bool octal(const unsigned char* p, std::size_t n, std::uint64_t& v) {
    v = 0;
    std::size_t i = 0;
    while (i < n && (p[i] == ' ' || p[i] == 0)) ++i;
    if (i < n && (p[i] & 0x80)) return false; // base-256 sizes: not for files this small
    bool any = false;
    for (; i < n && p[i] >= '0' && p[i] <= '7'; ++i) {
        if (v > (std::uint64_t(1) << 60)) return false;
        v = v * 8 + (p[i] - '0');
        any = true;
    }
    for (; i < n; ++i)
        if (p[i] != ' ' && p[i] != 0) return false;
    return any;
}

// The `path` record of a pax header: lines "<length> <key>=<value>\n".
bool pax_path(std::string_view data, std::string& path) {
    std::size_t pos = 0;
    while (pos < data.size()) {
        std::size_t len = 0, i = pos;
        while (i < data.size() && data[i] >= '0' && data[i] <= '9') len = len * 10 + std::size_t(data[i++] - '0');
        if (i >= data.size() || data[i] != ' ' || len == 0 || pos + len > data.size() || data[pos + len - 1] != '\n')
            return false;
        const std::string_view rec = data.substr(i + 1, pos + len - 1 - (i + 1));
        const std::size_t eq = rec.find('=');
        if (eq != std::string_view::npos && rec.substr(0, eq) == "path") path = std::string(rec.substr(eq + 1));
        pos += len;
    }
    return true;
}

} // namespace

bool tar_read(std::string_view text, std::vector<TarEntry>& files, std::vector<std::string>& skipped,
              std::string& error) {
    const auto* d = reinterpret_cast<const unsigned char*>(text.data());
    const std::size_t size = text.size();
    std::size_t pos = 0;
    std::string long_name;
    while (pos + 512 <= size) {
        const unsigned char* h = d + pos;
        bool zero = true;
        for (int i = 0; i < 512 && zero; ++i) zero = h[i] == 0;
        if (zero) break; // the end blocks
        std::uint64_t sum = 0, stored = 0;
        for (int i = 0; i < 512; ++i) sum += (i >= 148 && i < 156) ? ' ' : h[i];
        if (!octal(h + 148, 8, stored) || stored != sum) return error = "tar header damaged", false;
        std::uint64_t length = 0;
        if (!octal(h + 124, 12, length)) return error = "tar header damaged", false;
        pos += 512;
        if (length > size - pos) return error = "tar file cut short", false;
        const std::string_view body(text.data() + pos, std::size_t(length));
        pos += (std::size_t(length) + 511) / 512 * 512;
        if (pos > size) pos = size;
        const char type = char(h[156]);
        if (type == 'L') { // GNU long name for the next entry
            long_name = field(reinterpret_cast<const unsigned char*>(body.data()), body.size());
            continue;
        }
        if (type == 'x') { // pax header for the next entry
            if (!pax_path(body, long_name)) return error = "tar pax header damaged", false;
            continue;
        }
        if (type == 'g') continue; // pax global header: nothing the pack needs
        std::string name = field(h, 100);
        if (std::memcmp(h + 257, "ustar", 5) == 0) {
            const std::string prefix = field(h + 345, 155);
            if (!prefix.empty()) name = prefix + "/" + name;
        }
        if (!long_name.empty()) {
            name = long_name;
            long_name.clear();
        }
        if (type == '0' || type == '\0' || type == '7') {
            files.push_back({name, std::string(body)});
        } else if (type != '5') {
            skipped.push_back(name);
        }
    }
    return true;
}

} // namespace tisma
