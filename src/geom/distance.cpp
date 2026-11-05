// The distance transform of a bit grid. Own code; see distance.hpp and
// PROVENANCE.md.

#include "geom/distance.hpp"

#include <limits>

namespace tisma {
namespace {

// One line of the second pass: f[q] the squared distance along the other
// axis at position q (or a large value), out[q] the squared distance in 2D.
// The parabolas q -> (q - p)^2 + f[p] are kept in order of their vertices p,
// each one from where it is lowest on (z[k] to z[k + 1]).
void envelope(const float* f, int n, float* out, std::vector<int>& v, std::vector<float>& z) {
    v.assign(std::size_t(n), 0);
    z.assign(std::size_t(n) + 1, 0.0f);
    constexpr float kInf = std::numeric_limits<float>::infinity();
    int k = -1;
    for (int q = 0; q < n; ++q) {
        if (f[q] >= kInf)
            continue;
        while (k >= 0) {
            const int p = v[std::size_t(k)];
            // Where the parabola of q meets that of p.
            const float s = ((f[q] + float(q) * float(q)) - (f[p] + float(p) * float(p))) / (2.0f * float(q - p));
            if (s > z[std::size_t(k)])
                break;
            --k;
        }
        ++k;
        v[std::size_t(k)] = q;
        z[std::size_t(k)] = k == 0 ? -kInf : ((f[q] + float(q) * float(q)) -
                                              (f[v[std::size_t(k - 1)]] + float(v[std::size_t(k - 1)]) * float(v[std::size_t(k - 1)]))) /
                                                 (2.0f * float(q - v[std::size_t(k - 1)]));
        z[std::size_t(k) + 1] = kInf;
    }
    if (k < 0) {
        for (int q = 0; q < n; ++q)
            out[q] = kInf;
        return;
    }
    int j = 0;
    for (int q = 0; q < n; ++q) {
        while (z[std::size_t(j) + 1] < float(q))
            ++j;
        const float d = float(q - v[std::size_t(j)]);
        out[q] = d * d + f[v[std::size_t(j)]];
    }
}

} // namespace

std::vector<float> squared_distance_inside(const BitGrid& grid) {
    const int w = grid.width(), h = grid.height();
    std::vector<float> out(std::size_t(w) * std::size_t(h), 0.0f);
    if (w == 0 || h == 0)
        return out;
    // Along each column: the distance to the nearest unset pixel above or
    // below, the row past either end counting as unset.
    std::vector<float> column(std::size_t(w) * std::size_t(h));
    for (int i = 0; i < w; ++i) {
        float run = 0.0f; // from above
        for (int j = 0; j < h; ++j) {
            run = grid.get(i, j) ? run + 1.0f : 0.0f;
            column[std::size_t(j) * std::size_t(w) + std::size_t(i)] = run;
        }
        run = 0.0f; // from below
        for (int j = h - 1; j >= 0; --j) {
            run = grid.get(i, j) ? run + 1.0f : 0.0f;
            float& c = column[std::size_t(j) * std::size_t(w) + std::size_t(i)];
            c = c < run ? c : run;
        }
    }
    // Along each row, with the columns' squares as the parabolas' heights;
    // the pixels just past the row's ends are unset (height 0).
    std::vector<float> f(std::size_t(w) + 2), line(std::size_t(w) + 2);
    std::vector<int> v;
    std::vector<float> z;
    for (int j = 0; j < h; ++j) {
        f[0] = f[std::size_t(w) + 1] = 0.0f;
        for (int i = 0; i < w; ++i) {
            const float c = column[std::size_t(j) * std::size_t(w) + std::size_t(i)];
            f[std::size_t(i) + 1] = c * c;
        }
        envelope(f.data(), w + 2, line.data(), v, z);
        for (int i = 0; i < w; ++i)
            out[std::size_t(j) * std::size_t(w) + std::size_t(i)] = grid.get(i, j) ? line[std::size_t(i) + 1] : 0.0f;
    }
    return out;
}

BitGrid at_least(const BitGrid& grid, const std::vector<float>& squared, double pixels) {
    BitGrid out(grid.x0(), grid.y0(), grid.pixel(), grid.width(), grid.height());
    const float limit = float(pixels * pixels);
    for (int j = 0; j < grid.height(); ++j)
        for (int i = 0; i < grid.width(); ++i)
            if (squared[std::size_t(j) * std::size_t(grid.width()) + std::size_t(i)] >= limit)
                out.set(i, j);
    return out;
}

} // namespace tisma
