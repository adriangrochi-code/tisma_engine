// Pixel grids of a layer's area: scanline filling, disk morphology with word
// shifts, and outlines traced along pixel edges. Own design; see PROVENANCE.md.

#include "geom/bitgrid.hpp"

#include <oneapi/tbb/parallel_for.h>

#include <algorithm>
#include <bit>
#include <cmath>
#include <utility>

namespace tisma {
namespace {

// dst |= src moved s bits up (s > 0: bit x takes bit x - s) or down.
void or_shifted(std::uint64_t* dst, const std::uint64_t* src, int words, int s) {
    if (s == 0) {
        for (int w = 0; w < words; ++w)
            dst[w] |= src[w];
    } else if (s > 0) {
        const int ws = s / 64, bs = s % 64;
        for (int w = words - 1; w >= ws; --w) {
            std::uint64_t v = src[w - ws] << bs;
            if (bs != 0 && w - ws - 1 >= 0)
                v |= src[w - ws - 1] >> (64 - bs);
            dst[w] |= v;
        }
    } else {
        const int ws = -s / 64, bs = -s % 64;
        for (int w = 0; w + ws < words; ++w) {
            std::uint64_t v = src[w + ws] >> bs;
            if (bs != 0 && w + ws + 1 < words)
                v |= src[w + ws + 1] << (64 - bs);
            dst[w] |= v;
        }
    }
}

// out = the row with every set bit spread k bits each way, by doubling
// spans: [-(2^m - 1), 2^m - 1], then one last pair of shifts to reach k.
void spread(std::uint64_t* out, const std::uint64_t* in, int words, int k, std::vector<std::uint64_t>& scratch) {
    std::copy(in, in + words, out);
    scratch.resize(std::size_t(words));
    int span = 1;
    while (2 * span - 1 <= k) {
        std::copy(out, out + words, scratch.begin());
        or_shifted(out, scratch.data(), words, span);
        or_shifted(out, scratch.data(), words, -span);
        span *= 2;
    }
    if (const int rest = k - (span - 1); rest > 0) {
        std::copy(out, out + words, scratch.begin());
        or_shifted(out, scratch.data(), words, rest);
        or_shifted(out, scratch.data(), words, -rest);
    }
}

// ceil(a / b) for b > 0.
std::int64_t ceil_div(std::int64_t a, std::int64_t b) { return a >= 0 ? (a + b - 1) / b : -((-a) / b); }

constexpr std::uint8_t kEast = 1, kNorth = 2, kWest = 4, kSouth = 8;

std::uint8_t left_of(std::uint8_t d) {
    return d == kEast ? kNorth : d == kNorth ? kWest : d == kWest ? kSouth : kEast;
}

} // namespace

BitGrid::BitGrid(std::int64_t x0, std::int64_t y0, std::int64_t pixel, int width, int height)
    : x0_(x0), y0_(y0), pixel_(std::max<std::int64_t>(pixel, 1)), width_(std::max(width, 0)),
      height_(std::max(height, 0)), words_((std::max(width, 0) + 63) / 64),
      bits_(std::size_t(words_) * std::size_t(height_), 0) {}

bool BitGrid::get(int i, int j) const {
    if (i < 0 || j < 0 || i >= width_ || j >= height_)
        return false;
    return (row(j)[i / 64] >> (i % 64)) & 1u;
}

void BitGrid::set(int i, int j) {
    if (i >= 0 && j >= 0 && i < width_ && j < height_)
        row(j)[i / 64] |= std::uint64_t(1) << (i % 64);
}

bool BitGrid::empty() const {
    return std::all_of(bits_.begin(), bits_.end(), [](std::uint64_t w) { return w == 0; });
}

std::size_t BitGrid::count() const {
    std::size_t n = 0;
    for (const std::uint64_t w : bits_)
        n += std::size_t(std::popcount(w));
    return n;
}

void BitGrid::clear() { std::fill(bits_.begin(), bits_.end(), 0); }

void BitGrid::set_run(int j, int from, int to) {
    from = std::max(from, 0), to = std::min(to, width_);
    std::uint64_t* r = row(j);
    for (int i = from; i < to;) {
        const int w = i / 64, b = i % 64, n = std::min(64 - b, to - i);
        const std::uint64_t mask = (n == 64 ? ~std::uint64_t(0) : ((std::uint64_t(1) << n) - 1)) << b;
        r[w] |= mask;
        i += n;
    }
}

void BitGrid::trim() {
    if (width_ % 64 == 0)
        return;
    const std::uint64_t mask = (std::uint64_t(1) << (width_ % 64)) - 1;
    for (int j = 0; j < height_; ++j)
        row(j)[words_ - 1] &= mask;
}

void BitGrid::fill(const Polygons& region) {
    if (width_ == 0 || height_ == 0)
        return;
    // Every edge's crossings with the rows' centre lines, a row taking the
    // edges whose y range [low, high) holds its centre.
    std::vector<std::vector<std::pair<double, int>>> crossings(static_cast<std::size_t>(height_));
    const std::int64_t p = pixel_;
    for (const Polygon& path : region) {
        const std::size_t n = path.size();
        for (std::size_t k = 0; k < n; ++k) {
            const Clipper2Lib::Point64& a = path[k];
            const Clipper2Lib::Point64& b = path[(k + 1) % n];
            if (a.y == b.y)
                continue;
            const int dir = b.y > a.y ? 1 : -1;
            const std::int64_t low = std::min(a.y, b.y), high = std::max(a.y, b.y);
            // Centre y0 + (j + 1/2) p in [low, high).
            const std::int64_t j0 = std::max<std::int64_t>(ceil_div(2 * (low - y0_) - p, 2 * p), 0);
            const std::int64_t j1 = std::min<std::int64_t>(ceil_div(2 * (high - y0_) - p, 2 * p), height_);
            const double slope = double(b.x - a.x) / double(b.y - a.y);
            for (std::int64_t j = j0; j < j1; ++j) {
                const double cy = double(y0_) + (double(j) + 0.5) * double(p);
                crossings[std::size_t(j)].emplace_back(double(a.x) + (cy - double(a.y)) * slope, dir);
            }
        }
    }
    tbb::parallel_for(0, height_, [&](int j) {
        auto& row_crossings = crossings[std::size_t(j)];
        std::sort(row_crossings.begin(), row_crossings.end());
        int winding = 0;
        double start = 0.0;
        for (const auto& [x, dir] : row_crossings) {
            const int before = winding;
            winding += dir;
            if (before == 0 && winding != 0) {
                start = x;
            } else if (before != 0 && winding == 0) {
                // Centres x0 + (i + 1/2) p in [start, x).
                const double from = std::ceil((start - double(x0_)) / double(p) - 0.5);
                const double to = std::ceil((x - double(x0_)) / double(p) - 0.5);
                if (to > 0.0 && from < double(width_))
                    set_run(j, int(std::max(from, 0.0)), int(std::min(to, double(width_))));
            }
        }
    });
}

BitGrid& BitGrid::operator|=(const BitGrid& other) {
    for (std::size_t k = 0; k < bits_.size() && k < other.bits_.size(); ++k)
        bits_[k] |= other.bits_[k];
    return *this;
}

void BitGrid::subtract(const BitGrid& other) {
    for (std::size_t k = 0; k < bits_.size() && k < other.bits_.size(); ++k)
        bits_[k] &= ~other.bits_[k];
}

void BitGrid::dilate(int radius) {
    if (radius <= 0 || width_ == 0 || height_ == 0)
        return;
    // Row dy away spreads by the disk's half-width there: k*k + dy*dy <= r*r.
    std::vector<int> half(std::size_t(radius) + 1);
    for (int dy = 0; dy <= radius; ++dy) {
        int k = int(std::sqrt(double(radius * radius - dy * dy)));
        while ((k + 1) * (k + 1) + dy * dy <= radius * radius)
            ++k;
        while (k > 0 && k * k + dy * dy > radius * radius)
            --k;
        half[std::size_t(dy)] = k;
    }
    std::vector<int> widths = half;
    std::sort(widths.begin(), widths.end());
    widths.erase(std::unique(widths.begin(), widths.end()), widths.end());
    const std::size_t stride = std::size_t(words_) * std::size_t(height_);
    std::vector<std::uint64_t> spreads(widths.size() * stride);
    tbb::parallel_for(0, height_, [&](int j) {
        std::vector<std::uint64_t> scratch;
        for (std::size_t w = 0; w < widths.size(); ++w)
            spread(spreads.data() + w * stride + std::size_t(j) * std::size_t(words_), row(j), words_, widths[w],
                   scratch);
    });
    std::vector<std::size_t> which(half.size());
    for (std::size_t dy = 0; dy < half.size(); ++dy)
        which[dy] = std::size_t(std::lower_bound(widths.begin(), widths.end(), half[dy]) - widths.begin());
    tbb::parallel_for(0, height_, [&](int j) {
        std::uint64_t* out = row(j);
        std::fill(out, out + words_, 0);
        for (int dy = -radius; dy <= radius; ++dy) {
            const int from = j + dy;
            if (from < 0 || from >= height_)
                continue;
            const std::uint64_t* in =
                spreads.data() + which[std::size_t(std::abs(dy))] * stride + std::size_t(from) * std::size_t(words_);
            for (int w = 0; w < words_; ++w)
                out[w] |= in[w];
        }
    });
    trim();
}

void BitGrid::erode(int radius) {
    if (radius <= 0 || width_ == 0 || height_ == 0)
        return;
    for (std::uint64_t& w : bits_)
        w = ~w;
    trim();
    dilate(radius);
    for (std::uint64_t& w : bits_)
        w = ~w;
    trim();
    // A pixel nearer the edge than the radius has part of its disk outside.
    for (int j = 0; j < height_; ++j) {
        if (j < radius || j >= height_ - radius) {
            std::fill(row(j), row(j) + words_, 0);
            continue;
        }
        std::uint64_t* r = row(j);
        for (int i = 0; i < std::min(radius, width_); ++i)
            r[i / 64] &= ~(std::uint64_t(1) << (i % 64));
        for (int i = std::max(width_ - radius, 0); i < width_; ++i)
            r[i / 64] &= ~(std::uint64_t(1) << (i % 64));
    }
}

Polygons BitGrid::outline() const {
    Polygons out;
    if (width_ == 0 || height_ == 0)
        return out;
    // The boundary's unit edges, each leaving a corner (i, j) with the set
    // pixel on its left; a corner leaves by two only where two set pixels
    // touch diagonally.
    const std::size_t corners_wide = std::size_t(width_) + 1;
    const auto corner = [&](int i, int j) { return std::size_t(j) * corners_wide + std::size_t(i); };
    std::vector<std::uint8_t> edges(corners_wide * (std::size_t(height_) + 1), 0);
    const std::vector<std::uint64_t> none(std::size_t(words_), 0);
    for (int j = 0; j < height_; ++j) {
        const std::uint64_t* here = row(j);
        const std::uint64_t* below = j > 0 ? row(j - 1) : none.data();
        const std::uint64_t* above = j + 1 < height_ ? row(j + 1) : none.data();
        for (int w = 0; w < words_; ++w) {
            const std::uint64_t c = here[w];
            if (c == 0)
                continue;
            const std::uint64_t west = (c << 1) | (w > 0 ? here[w - 1] >> 63 : 0);
            const std::uint64_t east = (c >> 1) | (w + 1 < words_ ? here[w + 1] << 63 : 0);
            const auto each = [&](std::uint64_t bits, auto&& add) {
                while (bits != 0) {
                    const int b = std::countr_zero(bits);
                    bits &= bits - 1;
                    add(w * 64 + b);
                }
            };
            each(c & ~below[w], [&](int i) { edges[corner(i, j)] |= kEast; });
            each(c & ~above[w], [&](int i) { edges[corner(i + 1, j + 1)] |= kWest; });
            each(c & ~west, [&](int i) { edges[corner(i, j + 1)] |= kSouth; });
            each(c & ~east, [&](int i) { edges[corner(i + 1, j)] |= kNorth; });
        }
    }
    const std::vector<std::uint8_t> all = edges;
    const auto step = [&](std::size_t v, std::uint8_t d) {
        switch (d) {
        case kEast:
            return v + 1;
        case kWest:
            return v - 1;
        case kNorth:
            return v + corners_wide;
        default:
            return v - corners_wide;
        }
    };
    for (std::size_t v = 0; v < edges.size(); ++v)
        while (edges[v] != 0) {
            const std::uint8_t first = std::uint8_t(edges[v] & -edges[v]);
            Polygon path;
            std::size_t at = v;
            std::uint8_t d = first;
            std::uint8_t last = 0;
            while (true) {
                edges[at] &= std::uint8_t(~d);
                if (d != last)
                    path.push_back({x0_ + std::int64_t(at % corners_wide) * pixel_,
                                    y0_ + std::int64_t(at / corners_wide) * pixel_});
                last = d;
                at = step(at, d);
                // Left first where two leave: diagonal neighbours stay apart.
                const std::uint8_t options = all[at];
                const std::uint8_t next = std::popcount(options) == 1 ? options : left_of(d);
                if (at == v && next == first)
                    break;
                d = next;
            }
            if (last == first && path.size() > 1) // the start lies along a straight side
                path.erase(path.begin());
            out.push_back(std::move(path));
        }
    return out;
}

} // namespace tisma
