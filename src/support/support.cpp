// Overhangs and support areas. Own design; see PROVENANCE.md.

#include "support/support.hpp"

#include "geom/bitgrid.hpp"
#include "gpu/compute.hpp"

#include <oneapi/tbb/parallel_for.h>

#include <algorithm>
#include <climits>
#include <cmath>
#include <numbers>

namespace tisma {

namespace {

// mm past the layer below that an enforced face needs to get support: less is
// a wall that only looks slanted by rounding.
constexpr double kAnyReach = 0.01;

// mm: a step this much past the allowed reach is still no overhang.
constexpr double kTouching = 1e-3;

// The support column's grid: pixels a bead across (the closing's radius).
constexpr int kPixelsPerBead = 8;

} // namespace

std::vector<Polygons> overhangs(const std::vector<Polygons>& regions, const std::vector<double>& heights,
                                const SupportParams& params) {
    std::vector<Polygons> out(regions.size());
    tbb::parallel_for(std::size_t(1), std::max<std::size_t>(regions.size(), 1), [&](std::size_t l) {
        if (regions[l].empty())
            return;
        // Overhanging means reaching out MORE than this (supports_skirt.md,
        // rule 1): a 45° face with layers half a bead high is held, and
        // rounding in the mesh must not make it an overhang.
        const double reach = (params.threshold_angle > 0.0
                                  ? heights[l] / std::tan(params.threshold_angle * std::numbers::pi / 180.0)
                                  : params.bead / 2.0) +
                             kTouching;
        out[l] = Clipper2Lib::Difference(regions[l], offset(regions[l - 1], reach), Clipper2Lib::FillRule::NonZero);
    });
    return out;
}

std::vector<Polygons> supported_overhangs(const std::vector<Polygons>& regions, const std::vector<double>& heights,
                                          const SupportParams& params) {
    const std::size_t n = regions.size();
    std::vector<Polygons> over = params.automatic ? overhangs(regions, heights, params) : std::vector<Polygons>(n);
    if (params.enforce || params.block) {
        const auto at = [&](const std::vector<Polygons>* areas, std::size_t l) -> const Polygons* {
            return areas && l < areas->size() && !(*areas)[l].empty() ? &(*areas)[l] : nullptr;
        };
        tbb::parallel_for(std::size_t(1), std::max<std::size_t>(n, 1), [&](std::size_t l) {
            if (const Polygons* enforce = at(params.enforce, l)) {
                // Any reach at all: a slope too steep for the automatic support.
                const Polygons reach = Clipper2Lib::Difference(regions[l], offset(regions[l - 1], kAnyReach),
                                                               Clipper2Lib::FillRule::NonZero);
                over[l] = Clipper2Lib::Union(over[l], Clipper2Lib::Intersect(reach, *enforce, Clipper2Lib::FillRule::NonZero),
                                             Clipper2Lib::FillRule::NonZero);
            }
            if (const Polygons* block = at(params.block, l))
                over[l] = Clipper2Lib::Difference(over[l], *block, Clipper2Lib::FillRule::NonZero);
        });
    }
    return over;
}

std::vector<Polygons> support_areas(const std::vector<Polygons>& regions, const std::vector<double>& heights,
                                    const SupportParams& params) {
    const std::size_t n = regions.size();
    const std::vector<Polygons> over = supported_overhangs(regions, heights, params);
    std::vector<Polygons> out(n);
    // The column on a grid of an eighth of a bead (bitgrid.hpp): closing it
    // by polygon offsets cost as much as every overlapping speck of a
    // detailed part, and the grid is what the GPU takes. The column never
    // leaves the overhangs' bounds (closing fills in, it does not grow), so
    // the grid holds them, with room around for the closing.
    Clipper2Lib::Rect64 bounds(INT64_MAX, INT64_MAX, INT64_MIN, INT64_MIN);
    std::size_t highest = 0; // above the highest layer that takes an overhang, no column
    for (std::size_t l = 0; l < n; ++l)
        for (const Polygon& path : over[l])
            for (const Clipper2Lib::Point64& q : path) {
                bounds.left = std::min(bounds.left, q.x), bounds.right = std::max(bounds.right, q.x);
                bounds.top = std::min(bounds.top, q.y), bounds.bottom = std::max(bounds.bottom, q.y);
                highest = std::max(highest, l);
            }
    if (bounds.left > bounds.right)
        return out;
    const std::int64_t pixel = std::max<std::int64_t>(to_units(params.bead / double(kPixelsPerBead)), 1);
    const int radius = kPixelsPerBead; // the closing: one bead
    const int margin = 2 * radius + 2; // the overhangs grow a pixel, then the closing
    const std::int64_t x0 = bounds.left - margin * pixel, y0 = bounds.top - margin * pixel;
    const int width = int((bounds.right - bounds.left) / pixel) + 2 * margin + 1;
    const int height = int((bounds.bottom - bounds.top) / pixel) + 2 * margin + 1;
    const auto above_of = [&](std::size_t l) { return l + 1 + std::size_t(std::max(params.z_gap_layers, 0)); };
    const auto adds = [&](std::size_t l) { return above_of(l) < n && !over[above_of(l)].empty(); };
    // From the top down, a chunk of layers at a time: their grids filled in
    // parallel; the column, in order, grows with every overhang (gap layers
    // lower), is closed by a bead (each overhang leaves out the band its
    // layer holds by itself, so the overhangs of a slope project as rings
    // with thin gaps between them; support needs no detail) and loses what
    // the part occupies, where it stands on the part; then, in parallel, its
    // outline kept xy_gap away from the part.
    constexpr std::size_t kChunk = 32;
    std::vector<BitGrid> part(kChunk), add(kChunk), kept(kChunk);
    BitGrid column(x0, y0, pixel, width, height);
    for (std::size_t top = std::min(n, highest); top > 0;) {
        const std::size_t bottom = top > kChunk ? top - kChunk : 0;
        tbb::parallel_for(bottom, top, [&](std::size_t l) {
            BitGrid& p = part[l - bottom];
            p = BitGrid(x0, y0, pixel, width, height);
            p.fill(regions[l]);
            BitGrid& a = add[l - bottom];
            a = BitGrid();
            if (adds(l)) {
                // Grown by a pixel's half-diagonal first: an overhang thinner
                // than a pixel (a slope just past the threshold) still sets
                // the pixels it crosses.
                a = BitGrid(x0, y0, pixel, width, height);
                a.fill(offset(over[above_of(l)], 0.75 * to_mm(pixel)));
            }
        });
        const std::size_t count = top - bottom;
        const std::span<const BitGrid> parts(part.data(), count), adding(add.data(), count);
        if (params.gpu)
            params.gpu->support_column(column, parts, adding, std::span<BitGrid>(kept.data(), count), radius);
        else
            support_column(column, parts, adding, std::span<BitGrid>(kept.data(), count), radius);
        tbb::parallel_for(bottom, top, [&](std::size_t l) {
            const BitGrid& c = kept[l - bottom];
            if (c.width() == 0 || c.empty())
                return;
            // Simplified to half a bead (support needs no detail, and its
            // lines follow the outline), then kept xy_gap off the part, exactly.
            out[l] = Clipper2Lib::Difference(simplify(c.outline(), params.bead / 2.0), offset(regions[l], params.xy_gap),
                                             Clipper2Lib::FillRule::NonZero);
        });
        top = bottom;
    }
    return out;
}

void support_column(BitGrid& column, std::span<const BitGrid> parts, std::span<const BitGrid> adds,
                    std::span<BitGrid> kept, int radius) {
    for (std::size_t k = parts.size(); k-- > 0;) {
        if (adds[k].width() > 0) {
            column |= adds[k];
            column.dilate(radius);
            column.erode(radius);
        }
        column.subtract(parts[k]);
        kept[k] = column;
    }
}

} // namespace tisma
