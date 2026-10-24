// Top, bottom and sparse infill by layer, with Clipper2 (BSL-1.0). Own design;
// see PROVENANCE.md.

#include "surfaces/surfaces.hpp"

#include <oneapi/tbb/parallel_for.h>

namespace tisma {
namespace {

using Clipper2Lib::FillRule;

// What layers i + 1 .. i + count (or i - 1 .. i - count with step -1) all
// cover; nothing when one of them is past the first or last layer.
Polygons covered_by_all(const std::vector<Polygons>& regions, std::size_t i, int count, int step) {
    Polygons common;
    for (int k = 1; k <= count; ++k) {
        const std::ptrdiff_t j = std::ptrdiff_t(i) + step * k;
        if (j < 0 || j >= std::ptrdiff_t(regions.size()))
            return {};
        common = k == 1 ? regions[std::size_t(j)]
                        : Clipper2Lib::Intersect(common, regions[std::size_t(j)], FillRule::NonZero);
        if (common.empty())
            return {};
    }
    return common;
}

} // namespace

std::vector<LayerSurfaces> classify_surfaces(const std::vector<Polygons>& regions, const std::vector<Polygons>& infill,
                                             int top_layers, int bottom_layers) {
    std::vector<LayerSurfaces> out(infill.size());
    tbb::parallel_for(std::size_t(0), infill.size(), [&](std::size_t i) {
        LayerSurfaces& s = out[i];
        if (infill[i].empty())
            return;
        Polygons rest = infill[i];
        if (top_layers > 0) {
            s.top = Clipper2Lib::Difference(rest, covered_by_all(regions, i, top_layers, +1), FillRule::NonZero);
            rest = Clipper2Lib::Difference(rest, s.top, FillRule::NonZero);
        }
        if (bottom_layers > 0) {
            s.bottom = Clipper2Lib::Difference(rest, covered_by_all(regions, i, bottom_layers, -1), FillRule::NonZero);
            rest = Clipper2Lib::Difference(rest, s.bottom, FillRule::NonZero);
        }
        s.sparse = std::move(rest);
    });
    return out;
}

} // namespace tisma
