// Layer planning and multi-level slicing with triangles bucketed by level.
// Own design; see PROVENANCE.md.

#include "slice/layers.hpp"

#include <oneapi/tbb/blocked_range.h>
#include <oneapi/tbb/parallel_for.h>

#include <algorithm>
#include <stdexcept>

namespace tisma {

std::vector<Layer> plan_layers(double min_z, double max_z, const LayerSettings& settings) {
    if (!(settings.first_layer_height > 0.0) || !(settings.layer_height > 0.0))
        throw std::invalid_argument("layer heights must be positive");
    std::vector<Layer> layers;
    // Heights come from the layer index, not from a running sum, so rounding
    // errors do not grow with the number of layers.
    for (std::size_t i = 0;; ++i) {
        const double bottom =
            i == 0 ? min_z : min_z + settings.first_layer_height + double(i - 1) * settings.layer_height;
        const double top = min_z + settings.first_layer_height + double(i) * settings.layer_height;
        const Layer layer{bottom, top};
        if (layer.slice_z() >= max_z)
            break;
        layers.push_back(layer);
    }
    return layers;
}

void triangle_level_ranges(const IndexedMesh& mesh, std::span<const float> values, std::span<const float> isos,
                           std::vector<std::uint32_t>& first, std::vector<std::uint32_t>& end) {
    first.resize(mesh.triangles.size());
    end.resize(mesh.triangles.size());
    for (std::size_t t = 0; t < mesh.triangles.size(); ++t) {
        const auto& tri = mesh.triangles[t];
        const float a = values[tri[0]], b = values[tri[1]], c = values[tri[2]];
        const auto lo = std::upper_bound(isos.begin(), isos.end(), std::min({a, b, c})) - isos.begin();
        const auto hi = std::upper_bound(isos.begin(), isos.end(), std::max({a, b, c})) - isos.begin();
        first[t] = static_cast<std::uint32_t>(lo);
        end[t] = static_cast<std::uint32_t>(std::max(lo, hi));
    }
}

std::vector<std::vector<Contour>> slice_levels(const IndexedMesh& mesh, std::span<const float> values,
                                               std::span<const float> isos, double max_gap) {
    const std::size_t level_count = isos.size();
    // Bucket the triangles by level in compressed rows: first count per level,
    // then fill.
    std::vector<std::uint32_t> first_level, end_level;
    triangle_level_ranges(mesh, values, isos, first_level, end_level);
    std::vector<std::uint32_t> row_start(level_count + 1, 0);
    for (std::size_t t = 0; t < mesh.triangles.size(); ++t)
        for (std::uint32_t l = first_level[t]; l < end_level[t]; ++l)
            ++row_start[l + 1];
    for (std::size_t l = 0; l < level_count; ++l)
        row_start[l + 1] += row_start[l];
    std::vector<std::uint32_t> bucket(row_start[level_count]);
    std::vector<std::uint32_t> fill(row_start.begin(), row_start.end() - 1);
    for (std::size_t t = 0; t < mesh.triangles.size(); ++t)
        for (std::uint32_t l = first_level[t]; l < end_level[t]; ++l)
            bucket[fill[l]++] = static_cast<std::uint32_t>(t);

    // Levels are independent: slice them in parallel, one segment buffer per chunk.
    std::vector<std::vector<Contour>> out(level_count);
    tbb::parallel_for(tbb::blocked_range<std::size_t>(0, level_count), [&](const tbb::blocked_range<std::size_t>& r) {
        std::vector<LevelSegment> segments;
        for (std::size_t l = r.begin(); l != r.end(); ++l) {
            segments.clear();
            const std::span<const std::uint32_t> tris(bucket.data() + row_start[l], row_start[l + 1] - row_start[l]);
            level_set_segments(mesh, values, isos[l], tris, segments);
            out[l] = join_segments(segments);
            if (max_gap > 0.0)
                close_gaps(out[l], max_gap);
        }
    });
    return out;
}

} // namespace tisma
