#pragma once

// A check of the support read from the G-code alone (the owner's request,
// 2026-11-16: "no tener soporte siempre es muy grave"). Each layer's beads go
// onto a raster; then
// - the part's beads that have nothing under them farther than a layer's
//   overhang allows, and are not a bridge, are unsupported;
// - the support's beads farther than `float_limit` from anything under them
//   float.
// Own design; see PROVENANCE.md.

#include "gcode/preview.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <string>
#include <vector>

namespace tisma::support_check {

struct Spot {
    std::size_t layer = 0;
    double z = 0.0, x = 0.0, y = 0.0; // a cell of the largest piece
    double area = 0.0;                // mm2
};

struct Report {
    double part_area = 0.0;        // mm2 of the part's beads, all layers
    double unsupported_area = 0.0; // mm2 of them with nothing under them
    Spot worst_unsupported;        // the largest connected piece
    double support_area = 0.0;     // mm2 of the support's beads
    double floating_area = 0.0;    // mm2 of them far from anything under them
    Spot worst_floating;
    std::size_t support_moves = 0;
    // Quality (point 2 of the owner's request): support fused to the part.
    double fused_side = 0.0;   // mm2 of support beads nearer the part in their layer than the xy gap allows
    double fused_top = 0.0;    // mm2 of support under part that starts less than the z gap above it
    double fused_bottom = 0.0; // mm2 of support on part that ends less than the gap below it
    Spot worst_fused;          // the support layer with the most fused area, and a cell of it
    // Of the part's cells resting on support, the share that rests on interface.
    double on_interface = 0.0, on_base = 0.0; // mm2
};

struct Options {
    double cell = 0.1;          // mm
    double overhang = -1.0;     // mm a layer may stick out past the one below; < 0: half the bead's width
    double z_gap = 0.2;         // the support's gap under the part (support.z_gap)
    double float_limit = 1.5;   // mm a support bead may stand past what is under it (QA's check, 2026-11-16)
    bool bridges = true;        // a bridge's part needs nothing under it
    double xy_gap = 0.225;      // the support's gap beside the part (support.xy_gap)
    double tolerance = 0.1;     // mm the gaps may come short (the raster, rounding)
};

namespace detail {

// The squared distance (in cells) from each cell to the nearest set one:
// Felzenszwalb and Huttenlocher, "Distance Transforms of Sampled Functions",
// Theory of Computing 8 (2012), the one-dimensional pass done along rows and
// then along columns.
inline void distance_1d(const double* f, double* d, int n, int* v, double* zz) {
    int k = 0;
    v[0] = 0;
    zz[0] = -std::numeric_limits<double>::infinity();
    zz[1] = std::numeric_limits<double>::infinity();
    const auto meet = [&](int q, int p) {
        return ((f[q] + double(q) * q) - (f[p] + double(p) * p)) / (2.0 * double(q - p));
    };
    for (int q = 1; q < n; ++q) {
        double s = meet(q, v[k]);
        while (s <= zz[k]) {
            --k;
            s = meet(q, v[k]);
        }
        ++k;
        v[k] = q;
        zz[k] = s;
        zz[k + 1] = std::numeric_limits<double>::infinity();
    }
    k = 0;
    for (int q = 0; q < n; ++q) {
        while (zz[k + 1] < double(q))
            ++k;
        const double t = double(q - v[k]);
        d[q] = t * t + f[v[k]];
    }
}

inline std::vector<float> squared_distance(const std::vector<std::uint8_t>& set, int nx, int ny) {
    const float far = 1e12f;
    std::vector<float> g(set.size());
    for (std::size_t i = 0; i < set.size(); ++i)
        g[i] = set[i] ? 0.0f : far;
    const int n = std::max(nx, ny);
    const std::size_t size = static_cast<std::size_t>(n);
    std::vector<double> f(size), d(size), zz(size + 1);
    std::vector<int> v(size);
    for (int y = 0; y < ny; ++y) {
        for (int x = 0; x < nx; ++x)
            f[std::size_t(x)] = g[std::size_t(y) * std::size_t(nx) + std::size_t(x)];
        distance_1d(f.data(), d.data(), nx, v.data(), zz.data());
        for (int x = 0; x < nx; ++x)
            g[std::size_t(y) * std::size_t(nx) + std::size_t(x)] = float(d[std::size_t(x)]);
    }
    for (int x = 0; x < nx; ++x) {
        for (int y = 0; y < ny; ++y)
            f[std::size_t(y)] = g[std::size_t(y) * std::size_t(nx) + std::size_t(x)];
        distance_1d(f.data(), d.data(), ny, v.data(), zz.data());
        for (int y = 0; y < ny; ++y)
            g[std::size_t(y) * std::size_t(nx) + std::size_t(x)] = float(d[std::size_t(y)]);
    }
    return g;
}

inline bool is_part(LineKind k) {
    switch (k) {
    case LineKind::OuterWall:
    case LineKind::InnerWall:
    case LineKind::OverhangWall:
    case LineKind::SparseInfill:
    case LineKind::SolidInfill:
    case LineKind::TopSolidInfill:
    case LineKind::BridgeInfill:
    case LineKind::GapFill:
        return true;
    default:
        return false;
    }
}

inline bool is_support(LineKind k) { return k == LineKind::Support || k == LineKind::SupportInterface; }

} // namespace detail

inline Report check(const GcodePreview& p, const Options& opt = {}) {
    Report r;
    if (p.layers.empty() || p.extrusions.empty())
        return r;
    const double c = opt.cell;
    const double x0 = p.min[0] - 2.0, y0 = p.min[1] - 2.0;
    const int nx = int(std::ceil((p.max[0] - x0 + 2.0) / c)) + 1, ny = int(std::ceil((p.max[1] - y0 + 2.0) / c)) + 1;
    const std::size_t cells = std::size_t(nx) * std::size_t(ny);
    // 0: empty; bits: 1 part, 2 support, 4 bridge infill, 8 overhang wall, 16 skirt or brim.
    std::vector<std::vector<std::uint8_t>> layers(p.layers.size());
    std::vector<float> width(p.layers.size(), 0.0f);
    for (std::size_t li = 0; li < p.layers.size(); ++li) {
        std::vector<std::uint8_t>& m = layers[li];
        m.assign(cells, 0);
        const PreviewLayer& L = p.layers[li];
        double wsum = 0.0, wn = 0.0;
        for (std::size_t e = L.first_extrusion; e < L.first_extrusion + L.extrusion_count; ++e) {
            const PreviewMove& mv = p.moves[p.extrusions[e]];
            std::uint8_t bits = 0;
            if (detail::is_part(mv.kind))
                bits = 1;
            else if (detail::is_support(mv.kind))
                bits = 2, ++r.support_moves;
            else if (mv.kind == LineKind::SkirtBrim)
                bits = 16;
            else
                continue;
            if (mv.kind == LineKind::BridgeInfill)
                bits |= 4;
            if (mv.kind == LineKind::OverhangWall)
                bits |= 8;
            if (mv.kind == LineKind::SupportInterface)
                bits |= 64;
            if (bits & 1)
                wsum += mv.width, wn += 1.0;
            // The bead: a capsule round the move, half its width wide.
            const double rad = std::max(0.5 * mv.width, 0.5 * c);
            const double ax = mv.x0, ay = mv.y0, bx = mv.x1, by = mv.y1;
            const int ix0 = std::max(0, int(std::floor((std::min(ax, bx) - rad - x0) / c)));
            const int ix1 = std::min(nx - 1, int(std::ceil((std::max(ax, bx) + rad - x0) / c)));
            const int iy0 = std::max(0, int(std::floor((std::min(ay, by) - rad - y0) / c)));
            const int iy1 = std::min(ny - 1, int(std::ceil((std::max(ay, by) + rad - y0) / c)));
            const double dx = bx - ax, dy = by - ay, len2 = dx * dx + dy * dy;
            for (int iy = iy0; iy <= iy1; ++iy)
                for (int ix = ix0; ix <= ix1; ++ix) {
                    const double px = x0 + (ix + 0.5) * c, py = y0 + (iy + 0.5) * c;
                    double t = len2 > 0.0 ? ((px - ax) * dx + (py - ay) * dy) / len2 : 0.0;
                    t = std::clamp(t, 0.0, 1.0);
                    const double qx = ax + t * dx - px, qy = ay + t * dy - py;
                    if (qx * qx + qy * qy <= rad * rad)
                        m[std::size_t(iy) * std::size_t(nx) + std::size_t(ix)] |= bits;
                }
        }
        width[li] = wn > 0.0 ? float(wsum / wn) : 0.45f;
    }
    // Each layer's footprint: its part's beads and what they enclose (the
    // sparse infill's gaps, a hollow's inside), bit 32. What lies on it is
    // held, as a top skin is over the infill.
    {
        std::vector<std::uint8_t> outside(cells);
        std::vector<std::size_t> todo;
        std::vector<std::uint8_t> part(cells);
        // Walls meet with hairline gaps (a seam, a wall broken at a corner):
        // the flood stops two tenths of a millimetre short of any bead.
        const double seal = 0.2 / c;
        for (std::vector<std::uint8_t>& m : layers) {
            for (std::size_t s = 0; s < cells; ++s)
                part[s] = m[s] & 1;
            const std::vector<float> near = detail::squared_distance(part, nx, ny);
            std::fill(outside.begin(), outside.end(), 0);
            todo.clear();
            const auto push = [&](int x, int y) {
                const std::size_t s = std::size_t(y) * std::size_t(nx) + std::size_t(x);
                if (double(near[s]) > seal * seal && !outside[s])
                    outside[s] = 1, todo.push_back(s);
            };
            for (int x = 0; x < nx; ++x)
                push(x, 0), push(x, ny - 1);
            for (int y = 0; y < ny; ++y)
                push(0, y), push(nx - 1, y);
            while (!todo.empty()) {
                const std::size_t q = todo.back();
                todo.pop_back();
                const int x = int(q % std::size_t(nx)), y = int(q / std::size_t(nx));
                if (x > 0) push(x - 1, y);
                if (x + 1 < nx) push(x + 1, y);
                if (y > 0) push(x, y - 1);
                if (y + 1 < ny) push(x, y + 1);
            }
            for (std::size_t s = 0; s < cells; ++s)
                if (!outside[s])
                    m[s] |= 32;
        }
    }
    const double area = c * c;
    std::vector<std::uint8_t> below(cells), flag(cells), seen(cells);
    std::vector<std::size_t> stack;
    // The largest connected piece of the flagged cells, by area.
    const auto largest = [&](std::size_t li, Spot& worst) {
        std::fill(seen.begin(), seen.end(), 0);
        for (std::size_t s = 0; s < cells; ++s) {
            if (!flag[s] || seen[s])
                continue;
            std::size_t count = 0, first = s;
            stack.assign(1, s);
            seen[s] = 1;
            while (!stack.empty()) {
                const std::size_t q = stack.back();
                stack.pop_back();
                ++count;
                const int x = int(q % std::size_t(nx)), y = int(q / std::size_t(nx));
                const int nb[4][2] = {{x + 1, y}, {x - 1, y}, {x, y + 1}, {x, y - 1}};
                for (const auto& n : nb) {
                    if (n[0] < 0 || n[1] < 0 || n[0] >= nx || n[1] >= ny)
                        continue;
                    const std::size_t o = std::size_t(n[1]) * std::size_t(nx) + std::size_t(n[0]);
                    if (flag[o] && !seen[o])
                        seen[o] = 1, stack.push_back(o);
                }
            }
            if (double(count) * area > worst.area)
                worst = {li, p.layers[li].z, x0 + (double(first % std::size_t(nx)) + 0.5) * c,
                         y0 + (double(first / std::size_t(nx)) + 0.5) * c, double(count) * area};
        }
    };
    for (std::size_t li = 0; li < p.layers.size(); ++li) {
        const std::vector<std::uint8_t>& m = layers[li];
        const double z = p.layers[li].z, h = p.layers[li].height;
        const bool first = li == 0 || z - h < 0.05;
        // What is under this layer: the layers whose top is between this
        // layer's bottom and the support's gap below it.
        std::fill(below.begin(), below.end(), 0);
        if (!first)
            for (std::size_t lj = li; lj-- > 0;) {
                const double top = p.layers[lj].z;
                if (top < z - h - opt.z_gap - p.layers[lj].height - 0.01)
                    break;
                if (top > z - h + 0.01)
                    continue;
                for (std::size_t s = 0; s < cells; ++s)
                    below[s] |= (layers[lj][s] & (2 | 16 | 32)) != 0;
            }
        // The part.
        std::size_t part = 0;
        for (std::size_t s = 0; s < cells; ++s)
            part += (m[s] & 1) != 0;
        r.part_area += double(part) * area;
        std::size_t sup = 0;
        for (std::size_t s = 0; s < cells; ++s)
            sup += (m[s] & 2) != 0;
        r.support_area += double(sup) * area;
        if (first)
            continue;
        const std::vector<float> d2 = detail::squared_distance(below, nx, ny);
        const double reach = (opt.overhang >= 0.0 ? opt.overhang : 0.5 * width[li]) / c;
        std::size_t flagged = 0;
        for (std::size_t s = 0; s < cells; ++s) {
            flag[s] = (m[s] & 1) && double(d2[s]) > reach * reach;
            flagged += flag[s];
        }
        // A bridge (supports_skirt.md, rule 1): in some direction, going both
        // ways from the point over this layer's footprint, the line comes to
        // held part on both sides. Directions every 22.5 degrees. (The rule's
        // 60 degrees at the ends is left out: the check is the lenient side.)
        if (flagged && opt.bridges) {
            const auto held = [&](std::size_t s) { return (m[s] & 1) && double(d2[s]) <= reach * reach; };
            std::vector<std::size_t> cleared;
            for (std::size_t s = 0; s < cells; ++s) {
                if (!flag[s])
                    continue;
                const double sx = double(s % std::size_t(nx)) + 0.5, sy = double(s / std::size_t(nx)) + 0.5;
                bool bridge = false;
                for (int k = 0; k < 8 && !bridge; ++k) {
                    const double a = k * 3.14159265358979323846 / 8.0, ux = std::cos(a), uy = std::sin(a);
                    bool ends = true;
                    for (int side = -1; side <= 1 && ends; side += 2) {
                        bool reached = false;
                        for (double t = 1.0;; t += 1.0) {
                            const int x = int(std::floor(sx + side * t * ux)), y = int(std::floor(sy + side * t * uy));
                            if (x < 0 || y < 0 || x >= nx || y >= ny)
                                break;
                            const std::size_t q = std::size_t(y) * std::size_t(nx) + std::size_t(x);
                            if (!(m[q] & 32))
                                break;
                            if (held(q)) {
                                reached = true;
                                break;
                            }
                        }
                        ends = reached;
                    }
                    bridge = ends;
                }
                if (bridge)
                    cleared.push_back(s);
            }
            for (std::size_t s : cleared)
                flag[s] = 0;
            flagged -= cleared.size();
        }
        // No room for support under this layer (the first layer and the z
        // gap fill it all): what hangs here cannot be held, only noted.
        if (z - h - opt.z_gap < p.layers[0].z - 1e-3)
            flagged = 0, std::fill(flag.begin(), flag.end(), 0);
        r.unsupported_area += double(flagged) * area;
        if (flagged)
            largest(li, r.worst_unsupported);
        // The support.
        const double limit = opt.float_limit / c;
        flagged = 0;
        for (std::size_t s = 0; s < cells; ++s) {
            flag[s] = (m[s] & 2) && double(d2[s]) > limit * limit;
            flagged += flag[s];
        }
        r.floating_area += double(flagged) * area;
        if (flagged)
            largest(li, r.worst_floating);
    }
    // Quality: the gaps between support and part, and what the part rests on.
    // A part layer over the support is a layer and the gap high
    // (supports_skirt.md, rule 4): its bottom, for the gap, is a common
    // layer's height under its top.
    std::vector<double> hs;
    for (const PreviewLayer& L : p.layers)
        hs.push_back(L.height);
    std::sort(hs.begin(), hs.end());
    const double common = hs[hs.size() / 2];
    std::vector<std::uint8_t> part_mask(cells);
    for (std::size_t li = 0; li < p.layers.size(); ++li) {
        const std::vector<std::uint8_t>& m = layers[li];
        bool any = false;
        for (std::size_t s = 0; s < cells && !any; ++s)
            any = (m[s] & 2) != 0;
        if (!any)
            continue;
        const double top = p.layers[li].z, bottom = top - p.layers[li].height;
        for (std::size_t s = 0; s < cells; ++s)
            part_mask[s] = m[s] & 1;
        const std::vector<float> d2 = detail::squared_distance(part_mask, nx, ny);
        const double side = std::max(0.0, opt.xy_gap - opt.tolerance) / c;
        for (std::size_t s = 0; s < cells; ++s)
            if ((m[s] & 2) && double(d2[s]) < side * side)
                r.fused_side += area;
        for (std::size_t lj = 0; lj < p.layers.size(); ++lj) {
            if (lj == li)
                continue;
            const double pt = p.layers[lj].z, pb = pt - std::min(p.layers[lj].height, common);
            const bool over = lj > li && pb > top - 0.01 && pb < top + opt.z_gap - opt.tolerance;
            const bool under = lj < li && pt < bottom + 0.01 && pt > bottom - opt.z_gap + opt.tolerance;
            if (!over && !under)
                continue;
            double here = 0.0;
            std::size_t at = 0;
            for (std::size_t s = 0; s < cells; ++s)
                if ((m[s] & 2) && (layers[lj][s] & 1))
                    (over ? r.fused_top : r.fused_bottom) += area, here += area, at = s;
            if (here > r.worst_fused.area)
                r.worst_fused = {li, top, x0 + (double(at % std::size_t(nx)) + 0.5) * c, y0 + (double(at / std::size_t(nx)) + 0.5) * c, here};
        }
    }
    // The part's cells over support: the support layer right under the gap.
    for (std::size_t lj = 1; lj < p.layers.size(); ++lj) {
        const double pb = p.layers[lj].z - std::min(p.layers[lj].height, common);
        for (std::size_t li = lj; li-- > 0;) {
            const double top = p.layers[li].z;
            if (top > pb - opt.z_gap + opt.tolerance)
                continue;
            if (top < pb - opt.z_gap - p.layers[li].height)
                break;
            for (std::size_t s = 0; s < cells; ++s)
                if ((layers[lj][s] & 1) && (layers[li][s] & 2))
                    ((layers[li][s] & 64) ? r.on_interface : r.on_base) += area;
            break;
        }
    }
    return r;
}

inline std::string describe(const Spot& s) {
    char buf[160];
    std::snprintf(buf, sizeof buf, "%.2f mm2 at layer %zu (z %.2f) near x %.1f y %.1f", s.area, s.layer, s.z, s.x, s.y);
    return buf;
}

} // namespace tisma::support_check
