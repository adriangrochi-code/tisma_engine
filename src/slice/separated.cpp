// The parts of a sliced solid, each with the centre of its 3D box
// (orca_2_5.md, rule 12). Own design: islands joined layer to layer where
// they overlap, with a union-find; see PROVENANCE.md.

#include "slice/separated.hpp"

#include "gcode/order.hpp"

#include <algorithm>
#include <array>
#include <limits>
#include <numeric>

namespace tisma {

std::vector<std::vector<PartIsland>> separated_parts(const std::vector<Polygons>& regions, const std::vector<Layer>& layers) {
    const std::size_t n = std::min(regions.size(), layers.size());
    std::vector<std::vector<Polygons>> islands(n);
    std::vector<std::size_t> first(n + 1, 0);
    for (std::size_t l = 0; l < n; ++l) {
        islands[l] = split_islands(regions[l]);
        first[l + 1] = first[l] + islands[l].size();
    }
    // Each island's box, to skip pairs that cannot overlap.
    std::vector<std::array<std::int64_t, 4>> box(first[n]);
    for (std::size_t l = 0; l < n; ++l)
        for (std::size_t i = 0; i < islands[l].size(); ++i) {
            auto& b = box[first[l] + i];
            b = {std::numeric_limits<std::int64_t>::max(), std::numeric_limits<std::int64_t>::max(),
                 std::numeric_limits<std::int64_t>::lowest(), std::numeric_limits<std::int64_t>::lowest()};
            for (const Polygon& p : islands[l][i])
                for (const auto& q : p)
                    b = {std::min(b[0], q.x), std::min(b[1], q.y), std::max(b[2], q.x), std::max(b[3], q.y)};
        }
    std::vector<std::size_t> parent(first[n]);
    std::iota(parent.begin(), parent.end(), std::size_t(0));
    const auto find = [&](std::size_t a) {
        while (parent[a] != a)
            a = parent[a] = parent[parent[a]];
        return a;
    };
    for (std::size_t l = 1; l < n; ++l)
        for (std::size_t i = 0; i < islands[l].size(); ++i)
            for (std::size_t j = 0; j < islands[l - 1].size(); ++j) {
                const auto& a = box[first[l] + i];
                const auto& b = box[first[l - 1] + j];
                if (a[0] > b[2] || b[0] > a[2] || a[1] > b[3] || b[1] > a[3])
                    continue;
                if (Clipper2Lib::Intersect(islands[l][i], islands[l - 1][j], Clipper2Lib::FillRule::NonZero).empty())
                    continue;
                parent[find(first[l] + i)] = find(first[l - 1] + j);
            }
    // Each part's 3D box.
    struct Box {
        std::int64_t x0 = std::numeric_limits<std::int64_t>::max(), y0 = x0;
        std::int64_t x1 = std::numeric_limits<std::int64_t>::lowest(), y1 = x1;
        double z0 = std::numeric_limits<double>::max(), z1 = std::numeric_limits<double>::lowest();
    };
    std::vector<Box> parts(first[n]);
    for (std::size_t l = 0; l < n; ++l)
        for (std::size_t i = 0; i < islands[l].size(); ++i) {
            const auto& b = box[first[l] + i];
            Box& p = parts[find(first[l] + i)];
            p.x0 = std::min(p.x0, b[0]), p.y0 = std::min(p.y0, b[1]);
            p.x1 = std::max(p.x1, b[2]), p.y1 = std::max(p.y1, b[3]);
            p.z0 = std::min(p.z0, layers[l].bottom), p.z1 = std::max(p.z1, layers[l].top);
        }
    std::vector<std::vector<PartIsland>> out(n);
    for (std::size_t l = 0; l < n; ++l)
        for (std::size_t i = 0; i < islands[l].size(); ++i) {
            const Box& p = parts[find(first[l] + i)];
            out[l].push_back({std::move(islands[l][i]),
                              {0.5 * (to_mm(p.x0) + to_mm(p.x1)), 0.5 * (to_mm(p.y0) + to_mm(p.y1))},
                              0.5 * (p.z0 + p.z1)});
        }
    return out;
}

} // namespace tisma
