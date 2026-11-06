// The distance transform of a bit grid. Own code; see distance.hpp and
// PROVENANCE.md.

#include "geom/distance.hpp"

#include <cstdint>

#include <algorithm>

#include <cmath>
#include <limits>
#include <unordered_map>

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

Polygons distance_level(const BitGrid& grid, const std::vector<float>& squared, double pixels) {
    const int w = grid.width(), h = grid.height();
    if (w == 0 || h == 0)
        return {};
    // The distance to the edge at a pixel centre: half a pixel less than to
    // the nearest unset centre; unset pixels and the outside, -1/2.
    const auto value = [&](int i, int j) -> double {
        if (i < 0 || j < 0 || i >= w || j >= h)
            return -0.5;
        const float d = squared[std::size_t(j) * std::size_t(w) + std::size_t(i)];
        return d > 0.0f ? std::sqrt(double(d)) - 0.5 : -0.5;
    };
    // Which centres are in, each once and without a square root: for a level
    // above -1/2, value >= pixels is d >= (pixels + 1/2)^2.
    const std::size_t row = std::size_t(w) + 2;
    std::vector<std::uint8_t> inside(row * (std::size_t(h) + 2), 0);
    if (pixels <= -0.5) {
        std::fill(inside.begin(), inside.end(), 1);
    } else {
        const double limit = (pixels + 0.5) * (pixels + 0.5);
        for (int j = 0; j < h; ++j)
            for (int i = 0; i < w; ++i) {
                const float d = squared[std::size_t(j) * std::size_t(w) + std::size_t(i)];
                inside[std::size_t(j + 1) * row + std::size_t(i + 1)] = d > 0.0f && double(d) >= limit;
            }
    }
    const auto is_in = [&](int i, int j) { return inside[std::size_t(j + 1) * row + std::size_t(i + 1)] != 0; };
    const double p = double(grid.pixel());
    const auto x_of = [&](double i) { return double(grid.x0()) + (i + 0.5) * p; };
    const auto y_of = [&](double j) { return double(grid.y0()) + (j + 0.5) * p; };
    // Edges between centres, each once: (i, j) to (i + 1, j) and (i, j) to
    // (i, j + 1), i and j from -1.
    const auto edge_id = [&](int i, int j, bool vertical) {
        return (std::int64_t(j + 1) * std::int64_t(w + 2) + std::int64_t(i + 1)) * 2 + (vertical ? 1 : 0);
    };
    struct Segment {
        std::int64_t start = 0, to = 0;
        Clipper2Lib::Point64 from;
    };
    std::vector<Segment> segments;
    for (int j = -1; j < h; ++j)
        for (int i = -1; i < w; ++i) {
            // The cell's corners counter-clockwise (y up), and its edges after them.
            const int ci[4] = {i, i + 1, i + 1, i}, cj[4] = {j, j, j + 1, j + 1};
            bool in[4];
            for (int k = 0; k < 4; ++k)
                in[k] = is_in(ci[k], cj[k]);
            if (in[0] == in[1] && in[1] == in[2] && in[2] == in[3])
                continue;
            double v[4];
            for (int k = 0; k < 4; ++k)
                v[k] = value(ci[k], cj[k]);
            const std::int64_t ids[4] = {edge_id(i, j, false), edge_id(i + 1, j, true), edge_id(i, j + 1, false),
                                         edge_id(i, j, true)};
            Clipper2Lib::Point64 at[4];
            for (int k = 0; k < 4; ++k) {
                const int n = (k + 1) % 4;
                if (in[k] == in[n])
                    continue;
                const double t = (pixels - v[k]) / (v[n] - v[k]);
                at[k] = {std::llround(x_of(ci[k] + t * (ci[n] - ci[k]))), std::llround(y_of(cj[k] + t * (cj[n] - cj[k])))};
            }
            // Each crossing from inside to outside (going round the cell) to
            // the next from outside to inside: the inside on the left.
            for (int k = 0; k < 4; ++k) {
                if (!(in[k] && !in[(k + 1) % 4]))
                    continue;
                for (int s = 1; s < 4; ++s) {
                    const int e = (k + s) % 4;
                    if (!in[e] && in[(e + 1) % 4]) {
                        segments.push_back({ids[k], ids[e], at[k]});
                        break;
                    }
                }
            }
        }
    // The segments joined end to start into closed rings (sorted by where
    // they start, found by halving).
    std::sort(segments.begin(), segments.end(), [](const Segment& a, const Segment& b) { return a.start < b.start; });
    std::vector<bool> used(segments.size(), false);
    const auto find = [&](std::int64_t start) -> std::ptrdiff_t {
        const auto it = std::lower_bound(segments.begin(), segments.end(), start,
                                         [](const Segment& s, std::int64_t v) { return s.start < v; });
        return it != segments.end() && it->start == start ? it - segments.begin() : -1;
    };
    Polygons out;
    for (std::size_t first = 0; first < segments.size(); ++first) {
        if (used[first])
            continue;
        Polygon ring;
        std::ptrdiff_t at = std::ptrdiff_t(first);
        for (std::size_t guard = 0; guard <= segments.size() && at >= 0 && !used[std::size_t(at)]; ++guard) {
            used[std::size_t(at)] = true;
            ring.push_back(segments[std::size_t(at)].from);
            at = find(segments[std::size_t(at)].to);
        }
        if (ring.size() > 2)
            out.push_back(std::move(ring));
    }
    return out;
}

} // namespace tisma
