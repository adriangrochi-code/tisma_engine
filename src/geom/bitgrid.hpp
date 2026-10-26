#pragma once

#include "geom/polygons.hpp"

#include <cstdint>
#include <span>
#include <vector>

namespace tisma {

// An area of a layer as a grid of square pixels, a bit each, in rows of
// 64-bit words: pixel (i, j) covers [x0 + i p, x0 + (i + 1) p] x
// [y0 + j p, y0 + (j + 1) p], p the pixel size in units. Morphology on it
// costs the same however detailed the area is, where polygon offsets grow
// with every overlapping piece; and the same bits come out of the CPU and,
// later, the GPU (PLAN.md, «Diseño para la visión»). Own design.
class BitGrid {
public:
    BitGrid() = default;
    BitGrid(std::int64_t x0, std::int64_t y0, std::int64_t pixel, int width, int height);

    int width() const { return width_; }
    int height() const { return height_; }
    std::int64_t pixel() const { return pixel_; }
    std::int64_t x0() const { return x0_; }
    std::int64_t y0() const { return y0_; }

    // The bits, row after row, words_per_row() 64-bit words a row: pixel i
    // of a row is bit i % 64 of its word i / 64, and the bits past the width
    // stay unset (what the GPU kernels read and write, 32 bits at a time).
    int words_per_row() const { return words_; }
    std::span<std::uint64_t> words() { return bits_; }
    std::span<const std::uint64_t> words() const { return bits_; }

    bool get(int i, int j) const;
    void set(int i, int j);
    bool empty() const;
    std::size_t count() const;
    void clear();

    // Sets every pixel whose centre lies inside the region (non-zero rule).
    void fill(const Polygons& region);

    // With another grid of the same size: union, and taking it away.
    BitGrid& operator|=(const BitGrid& other);
    void subtract(const BitGrid& other);

    // Dilation by a disk: every pixel whose centre lies within `radius`
    // pixels of a set pixel's centre. Erosion: only the pixels whose whole
    // disk is set; outside the grid counts as unset.
    void dilate(int radius);
    void erode(int radius);

    // The set pixels as polygons along the pixel edges: outer boundaries
    // counter-clockwise, holes clockwise, a vertex only where the boundary
    // turns. Pixels that touch only at a corner are separate pieces.
    Polygons outline() const;

    friend bool operator==(const BitGrid&, const BitGrid&) = default;

private:
    std::uint64_t* row(int j) { return bits_.data() + std::size_t(j) * std::size_t(words_); }
    const std::uint64_t* row(int j) const { return bits_.data() + std::size_t(j) * std::size_t(words_); }
    void set_run(int j, int from, int to); // pixels [from, to) of row j
    void trim();                           // clears the bits past the width

    std::int64_t x0_ = 0, y0_ = 0, pixel_ = 1;
    int width_ = 0, height_ = 0, words_ = 0;
    std::vector<std::uint64_t> bits_;
};

} // namespace tisma
