// Top, bottom and sparse infill by layer, with Clipper2 (BSL-1.0). Own design;
// see PROVENANCE.md.

#include "surfaces/surfaces.hpp"

#include <oneapi/tbb/parallel_for.h>

#include <cmath>

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

// The solid part with its pieces narrower than min_width grown by min_width,
// within the infill region.
Polygons widen_thin(const Polygons& solid, const Polygons& infill, double min_width) {
    if (min_width <= 0.0 || solid.empty())
        return solid;
    const Polygons wide = offset(offset(solid, -min_width / 2.0), min_width / 2.0);
    const Polygons thin = Clipper2Lib::Difference(solid, wide, FillRule::NonZero);
    if (thin.empty())
        return solid;
    const Polygons grown = Clipper2Lib::Intersect(offset(thin, min_width), infill, FillRule::NonZero);
    return Clipper2Lib::Union(solid, grown, FillRule::NonZero);
}

// What of layer i's infill lies closer than the shell thickness to the
// outside through the layers within that height above (step +1) or below
// (step -1): the shell's height is the next count layers'. A point at height
// dz over (or under) layer i is far enough from layer j's edge when it is
// sqrt(t^2 - dz^2) inside it. Past the first or last layer: nothing, the top
// and bottom rules already take that.
Polygons too_thin(const std::vector<Polygons>& cover, const std::vector<double>& z, std::size_t i, int count, int step,
                  const Polygons& infill) {
    const std::ptrdiff_t far = std::ptrdiff_t(i) + step * count;
    if (count <= 0 || far < 0 || far >= std::ptrdiff_t(z.size()) || far >= std::ptrdiff_t(cover.size()))
        return {};
    const double t = std::abs(z[std::size_t(far)] - z[i]);
    Polygons deep = infill;
    for (int k = 1; k <= count && !deep.empty(); ++k) {
        const std::size_t j = std::size_t(std::ptrdiff_t(i) + step * k);
        const double dz = std::abs(z[j] - z[i]);
        const double inside = dz < t ? std::sqrt(t * t - dz * dz) : 0.0;
        deep = Clipper2Lib::Intersect(deep, inside > 0.0 ? offset(cover[j], -inside) : cover[j], FillRule::NonZero);
    }
    return Clipper2Lib::Difference(infill, deep, FillRule::NonZero);
}

} // namespace

std::vector<LayerSurfaces> classify_surfaces(const std::vector<Polygons>& regions, const std::vector<Polygons>& infill,
                                             int top_layers, int bottom_layers, double min_width,
                                             const ShellCheck* shell) {
    return classify_surfaces(regions, infill, std::vector<int>(infill.size(), top_layers),
                             std::vector<int>(infill.size(), bottom_layers), min_width, shell);
}

std::vector<LayerSurfaces> classify_surfaces(const std::vector<Polygons>& regions, const std::vector<Polygons>& infill,
                                             const std::vector<int>& top_counts, const std::vector<int>& bottom_counts,
                                             double min_width, const ShellCheck* shell) {
    const std::size_t n = infill.size();
    const bool check = shell && shell->mode != ShellCheck::Off && shell->z.size() >= n;
    std::vector<LayerSurfaces> out(n);
    const auto top_at = [&](std::size_t i) { return i < top_counts.size() ? top_counts[i] : 0; };
    const auto bottom_at = [&](std::size_t i) { return i < bottom_counts.size() ? bottom_counts[i] : 0; };
    // A layer whose infill region, counts and every region they reach are the
    // layer below's has its surfaces too (prismatic parts; the speed
    // report's V10): only the first of such a run is worked out.
    std::vector<char> same_region(regions.size(), 0), same(n, 0);
    tbb::parallel_for(std::size_t(1), regions.size(), [&](std::size_t j) { same_region[j] = regions[j] == regions[j - 1]; });
    tbb::parallel_for(std::size_t(1), n, [&](std::size_t i) {
        const int top = top_at(i), bottom = bottom_at(i);
        if (top != top_at(i - 1) || bottom != bottom_at(i - 1) || i < std::size_t(bottom) + 1 ||
            i + std::size_t(top) >= regions.size() || !(infill[i] == infill[i - 1]))
            return;
        for (std::size_t j = i - std::size_t(bottom); j <= i + std::size_t(top); ++j)
            if (j != i && !same_region[j])
                return;
        same[i] = 1;
    });
    tbb::parallel_for(std::size_t(0), n, [&](std::size_t i) {
        LayerSurfaces& s = out[i];
        if (same[i] || infill[i].empty())
            return;
        Polygons rest = infill[i];
        const int top_layers = top_at(i);
        const int bottom_layers = bottom_at(i);
        if (top_layers > 0) {
            s.top = Clipper2Lib::Difference(rest, covered_by_all(regions, i, top_layers, +1), FillRule::NonZero);
            s.top = widen_thin(s.top, rest, min_width);
            rest = Clipper2Lib::Difference(rest, s.top, FillRule::NonZero);
            if (top_layers > 1) {
                // Right under the air: what the next layer does not cover.
                Polygons exposed =
                    Clipper2Lib::Difference(s.top, covered_by_all(regions, i, 1, +1), FillRule::NonZero);
                exposed = widen_thin(exposed, s.top, min_width);
                s.top_under = Clipper2Lib::Difference(s.top, exposed, FillRule::NonZero);
            }
        }
        if (check && top_layers > 0) {
            // Under a slope: solid infill, not a top surface.
            const Polygons thin =
                too_thin(shell->mode == ShellCheck::Full ? infill : regions, shell->z, i, top_layers, +1, rest);
            if (!thin.empty()) {
                s.top = Clipper2Lib::Union(s.top, thin, FillRule::NonZero);
                s.top_under = Clipper2Lib::Union(s.top_under, thin, FillRule::NonZero);
                rest = Clipper2Lib::Difference(rest, thin, FillRule::NonZero);
            }
        }
        if (bottom_layers > 0) {
            s.bottom = Clipper2Lib::Difference(rest, covered_by_all(regions, i, bottom_layers, -1), FillRule::NonZero);
            s.bottom = widen_thin(s.bottom, rest, min_width);
            rest = Clipper2Lib::Difference(rest, s.bottom, FillRule::NonZero);
            s.bottom_exposed =
                i == 0 ? s.bottom : Clipper2Lib::Difference(s.bottom, regions[i - 1], FillRule::NonZero);
        }
        if (check && shell->mode == ShellCheck::Full && bottom_layers > 0) {
            // Over an overhang: solid infill resting on what is printed below.
            const Polygons thin = too_thin(infill, shell->z, i, bottom_layers, -1, rest);
            if (!thin.empty()) {
                s.bottom = Clipper2Lib::Union(s.bottom, thin, FillRule::NonZero);
                rest = Clipper2Lib::Difference(rest, thin, FillRule::NonZero);
            }
        }
        s.sparse = std::move(rest);
    });
    for (std::size_t i = 1; i < n; ++i)
        if (same[i])
            out[i] = out[i - 1];
    return out;
}

} // namespace tisma
