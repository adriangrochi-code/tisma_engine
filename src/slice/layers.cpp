// Layer planning and multi-level slicing with triangles bucketed by level.
// Own design; see PROVENANCE.md.

#include "slice/layers.hpp"

#include "gpu/compute.hpp"

#include <oneapi/tbb/blocked_range.h>
#include <oneapi/tbb/parallel_for.h>

#include <algorithm>
#include <array>
#include <limits>
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
        // A layer is added while its slicing plane is inside the part or
        // right on its top (slicing.md, rule 3).
        if (layer.slice_z() > max_z + kOnTop)
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

std::vector<char> same_crossings(std::span<const float> values, std::span<const float> isos) {
    const std::size_t n = isos.size();
    std::vector<char> same(n, 0);
    if (n < 2 || !std::is_sorted(isos.begin(), isos.end()))
        return same;
    std::fill(same.begin() + 1, same.end(), char(1));
    // A vertex changes sides between isos[l - 1] and isos[l] when
    // isos[l - 1] <= v < isos[l]: l is the first level above it.
    for (const float v : values) {
        const auto l = std::size_t(std::upper_bound(isos.begin(), isos.end(), v) - isos.begin());
        if (l >= 1 && l < n)
            same[l] = 0;
    }
    return same;
}

namespace {

// Corners in one line seen from above, exactly: products of float
// differences, which double holds without rounding.
bool upright(const IndexedMesh& mesh, const std::array<std::uint32_t, 3>& tri) {
    const Vec3f &a = mesh.vertices[tri[0]], &b = mesh.vertices[tri[1]], &c = mesh.vertices[tri[2]];
    const double ux = double(b.x) - a.x, uy = double(b.y) - a.y, vx = double(c.x) - a.x, vy = double(c.y) - a.y;
    return ux * vy == uy * vx;
}

} // namespace

std::vector<char> same_sections(const IndexedMesh& mesh, std::span<const float> values, std::span<const float> isos) {
    std::vector<char> same = same_crossings(values, isos);
    const std::size_t n = isos.size();
    if (values.size() != mesh.vertices.size()) {
        std::fill(same.begin(), same.end(), char(0));
        return same;
    }
    // No level crosses what the one below does (a sphere): nothing to rule out.
    if (std::find(same.begin(), same.end(), char(1)) == same.end())
        return same;
    // The levels crossing an edge whose crossing moves in x and y: one of a
    // triangle that does not stand upright, with its ends apart seen from
    // above. Counted in a difference array.
    std::vector<int> moving(n + 1, 0);
    for (const auto& tri : mesh.triangles) {
        if (upright(mesh, tri))
            continue;
        for (std::size_t k = 0; k < 3; ++k) {
            const std::uint32_t a = tri[k], b = tri[(k + 1) % 3];
            const Vec3f &pa = mesh.vertices[a], &pb = mesh.vertices[b];
            if (pa.x == pb.x && pa.y == pb.y)
                continue;
            // Crossed by the levels in (lo, hi].
            const float lo = std::min(values[a], values[b]), hi = std::max(values[a], values[b]);
            const auto k0 = std::upper_bound(isos.begin(), isos.end(), lo) - isos.begin();
            const auto k1 = std::upper_bound(isos.begin(), isos.end(), hi) - isos.begin();
            if (k0 < k1)
                ++moving[std::size_t(k0)], --moving[std::size_t(k1)];
        }
    }
    int count = 0;
    for (std::size_t l = 0; l < n; ++l) {
        count += moving[l];
        if (count > 0)
            same[l] = 0;
    }
    return same;
}

namespace {

// Which levels slice_levels cuts: the keys, first of each run of levels with
// the same crossings. The others follow their run's key.
struct LevelPlan {
    std::vector<char> same;    // same_crossings
    std::vector<char> section; // same_sections, when asked
    std::vector<std::size_t> keys;
    std::vector<float> key_isos;
    std::vector<std::size_t> key_of; // each level's run, as an index into keys
};

LevelPlan plan_levels(const IndexedMesh& mesh, std::span<const float> values, std::span<const float> isos,
                      bool sections) {
    LevelPlan plan;
    plan.same = same_crossings(values, isos);
    if (sections)
        plan.section = same_sections(mesh, values, isos);
    plan.key_of.resize(isos.size());
    for (std::size_t l = 0; l < isos.size(); ++l) {
        if (!plan.same[l]) {
            plan.keys.push_back(l);
            plan.key_isos.push_back(isos[l]);
        }
        plan.key_of[l] = plan.keys.size() - 1;
    }
    return plan;
}

// The contours of every level: each key's segments (segments_of(k, buffer),
// for key k) joined, and the other levels of its run moved along the key's
// edges.
template <class Segments>
std::vector<std::vector<Contour>> assemble(const IndexedMesh& mesh, std::span<const float> values,
                                           std::span<const float> isos, const LevelPlan& plan, double max_gap,
                                           std::vector<char>* same_section, const Segments& segments_of) {
    const std::size_t level_count = isos.size(), key_count = plan.keys.size();
    std::vector<std::vector<Contour>> out(level_count);
    struct Run {
        std::vector<std::vector<EdgeKey>> edges; // per contour, per point
        std::vector<char> closed;                // per contour
        bool all_closed = true;
    };
    std::vector<Run> runs(key_count);
    tbb::parallel_for(tbb::blocked_range<std::size_t>(0, key_count), [&](const tbb::blocked_range<std::size_t>& r) {
        std::vector<LevelSegment> buffer;
        for (std::size_t k = r.begin(); k != r.end(); ++k) {
            const std::size_t l = plan.keys[k];
            const bool alone = l + 1 == level_count || !plan.same[l + 1];
            out[l] = join_segments(segments_of(k, buffer), alone ? nullptr : &runs[k].edges);
            for (const Contour& c : out[l]) {
                runs[k].closed.push_back(c.closed ? 1 : 0);
                runs[k].all_closed = runs[k].all_closed && c.closed;
            }
            if (max_gap > 0.0)
                close_gaps(out[l], max_gap);
        }
    });
    if (same_section)
        same_section->assign(level_count, 0);
    tbb::parallel_for(std::size_t(0), level_count, [&](std::size_t l) {
        if (!plan.same[l])
            return;
        const Run& run = runs[plan.key_of[l]];
        if (same_section && plan.section[l] && run.all_closed) {
            (*same_section)[l] = 1;
            return;
        }
        std::vector<Contour>& contours = out[l];
        contours.resize(run.edges.size());
        for (std::size_t c = 0; c < run.edges.size(); ++c) {
            contours[c].closed = run.closed[c] != 0;
            contours[c].points.reserve(run.edges[c].size());
            for (const EdgeKey e : run.edges[c])
                contours[c].points.push_back(edge_crossing(mesh, values, isos[l], e));
        }
        if (max_gap > 0.0)
            close_gaps(contours, max_gap);
    });
    return out;
}

} // namespace

std::vector<std::vector<Contour>> slice_levels(const IndexedMesh& mesh, std::span<const float> values,
                                               std::span<const float> isos, double max_gap,
                                               std::vector<char>* same_section) {
    const LevelPlan plan = plan_levels(mesh, values, isos, same_section != nullptr);
    const std::size_t key_count = plan.keys.size();
    // Bucket the triangles by key level in compressed rows: first count per
    // level, then fill.
    std::vector<std::uint32_t> first_level, end_level;
    triangle_level_ranges(mesh, values, plan.key_isos, first_level, end_level);
    // 64-bit counts: tall triangles times thin layers pass 2^32 crossings.
    std::vector<std::size_t> row_start(key_count + 1, 0);
    for (std::size_t t = 0; t < mesh.triangles.size(); ++t)
        for (std::uint32_t k = first_level[t]; k < end_level[t]; ++k)
            ++row_start[k + 1];
    for (std::size_t k = 0; k < key_count; ++k)
        row_start[k + 1] += row_start[k];
    std::vector<std::uint32_t> bucket(row_start[key_count]);
    std::vector<std::size_t> fill(row_start.begin(), row_start.end() - 1);
    for (std::size_t t = 0; t < mesh.triangles.size(); ++t)
        for (std::uint32_t k = first_level[t]; k < end_level[t]; ++k)
            bucket[fill[k]++] = static_cast<std::uint32_t>(t);

    // Levels are independent: slice them in parallel, one segment buffer per chunk.
    return assemble(mesh, values, isos, plan, max_gap, same_section,
                    [&](std::size_t k, std::vector<LevelSegment>& segments) {
                        segments.clear();
                        const std::span<const std::uint32_t> tris(bucket.data() + row_start[k],
                                                                  row_start[k + 1] - row_start[k]);
                        level_set_segments(mesh, values, plan.key_isos[k], tris, segments);
                        return std::span<const LevelSegment>(segments);
                    });
}

std::vector<std::vector<Contour>> slice_levels(const IndexedMesh& mesh, std::span<const float> values,
                                               std::span<const float> isos, double max_gap, gpu::Context* gpu,
                                               std::vector<char>* same_section) {
    if (!gpu)
        return slice_levels(mesh, values, isos, max_gap, same_section);
    const LevelPlan plan = plan_levels(mesh, values, isos, same_section != nullptr);
    const std::size_t key_count = plan.keys.size();
    std::vector<std::uint32_t> first, end, offsets;
    std::vector<LevelSegment> by_triangle;
    gpu->triangle_level_ranges(mesh, values, plan.key_isos, first, end);
    // The GPU's offsets are 32-bit: past 2^32 crossings, the CPU path.
    std::size_t crossings = 0;
    for (std::size_t t = 0; t < mesh.triangles.size(); ++t)
        crossings += end[t] - first[t];
    if (crossings > std::numeric_limits<std::uint32_t>::max())
        return slice_levels(mesh, values, isos, max_gap, same_section);
    gpu->level_segments_by_triangle(mesh, values, plan.key_isos, first, end, offsets, by_triangle);

    // Regroup by level, keeping triangle order inside each level, as the CPU
    // path does: counting sort.
    std::vector<std::size_t> row_start(key_count + 1, 0);
    for (std::size_t t = 0; t < mesh.triangles.size(); ++t)
        for (std::uint32_t k = first[t]; k < end[t]; ++k)
            ++row_start[k + 1];
    for (std::size_t k = 0; k < key_count; ++k)
        row_start[k + 1] += row_start[k];
    std::vector<LevelSegment> by_level(by_triangle.size());
    std::vector<std::size_t> fill(row_start.begin(), row_start.end() - 1);
    for (std::size_t t = 0; t < mesh.triangles.size(); ++t)
        for (std::uint32_t k = first[t]; k < end[t]; ++k)
            by_level[fill[k]++] = by_triangle[offsets[t] + (k - first[t])];

    return assemble(mesh, values, isos, plan, max_gap, same_section, [&](std::size_t k, std::vector<LevelSegment>&) {
        return std::span<const LevelSegment>(by_level.data() + row_start[k], row_start[k + 1] - row_start[k]);
    });
}

} // namespace tisma
