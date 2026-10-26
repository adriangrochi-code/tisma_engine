// Overhangs and support areas. Own design; see PROVENANCE.md.

#include "support/support.hpp"

#include <oneapi/tbb/parallel_for.h>

#include <cmath>
#include <numbers>

namespace tisma {

namespace {

// mm past the layer below that an enforced face needs to get support: less is
// a wall that only looks slanted by rounding.
constexpr double kAnyReach = 0.01;

// mm: a step this much past the allowed reach is still no overhang.
constexpr double kTouching = 1e-3;

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

std::vector<Polygons> support_areas(const std::vector<Polygons>& regions, const std::vector<double>& heights,
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
    std::vector<Polygons> out(n);
    // From the top down: a column grows with every overhang (gap layers lower)
    // and loses what the part occupies, where it stands on the part.
    Polygons column;
    for (std::size_t l = n; l-- > 0;) {
        const std::size_t above = l + 1 + std::size_t(std::max(params.z_gap_layers, 0));
        if (above < n && !over[above].empty()) {
            // Each overhang leaves out the band its layer holds by itself, so
            // the overhangs of a slope project as rings with thin gaps between
            // them: closing by one bead fills those (support needs no detail).
            column = Clipper2Lib::Union(column, over[above], Clipper2Lib::FillRule::NonZero);
            column = offset(offset(column, params.bead), -params.bead);
        }
        if (column.empty())
            continue;
        column = Clipper2Lib::Difference(column, regions[l], Clipper2Lib::FillRule::NonZero);
        out[l] = Clipper2Lib::Difference(column, offset(regions[l], params.xy_gap), Clipper2Lib::FillRule::NonZero);
    }
    return out;
}

} // namespace tisma
