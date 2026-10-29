#include "mesh/indexed_mesh.hpp"

#include <algorithm>
#include <atomic>
#include <bit>
#include <functional>
#include <memory>

#include <tbb/blocked_range.h>
#include <tbb/parallel_for.h>
#include <tbb/parallel_reduce.h>
#include <tbb/parallel_scan.h>
#include <tbb/parallel_sort.h>

namespace tisma {
namespace {

// Bit pattern of a point: equal patterns mean equal vertices. -0.0f and 0.0f
// differ in bits, so they are normalised first.
struct PointBits {
    std::uint32_t x, y, z;
    friend bool operator==(const PointBits&, const PointBits&) = default;
};

std::uint32_t bits(float f) { return std::bit_cast<std::uint32_t>(f == 0.0f ? 0.0f : f); }

PointBits point_bits(const Vec3f& p) { return {bits(p.x), bits(p.y), bits(p.z)}; }

std::uint64_t hash(const PointBits& p) {
    // Multiply-xorshift mix of the three words.
    std::uint64_t h = (std::uint64_t(p.x) << 32 | p.y) * 0x9E3779B97F4A7C15ull;
    h ^= (h >> 29) + std::uint64_t(p.z) * 0xC2B2AE3D27D4EB4Full;
    h ^= h >> 32;
    return h;
}

constexpr std::uint32_t kEmpty = 0xFFFFFFFFu;

} // namespace

namespace {

IndexedMesh weld_serial(const TriangleMesh& mesh) {
    const std::vector<Vec3f>& corners = mesh.corners();
    const std::size_t n = corners.size();

    // Open-addressing hash table from point bits to vertex id, at most half
    // full. Vertices are numbered in order of first appearance, which keeps the
    // result deterministic and the vertices of nearby triangles close in memory.
    // Measured about 2x faster than a parallel sort of the corners on 4 cores
    // (see PROGRESS.md).
    const std::size_t table_size = std::bit_ceil(std::max<std::size_t>(2 * n, 16));
    const std::size_t mask = table_size - 1;
    std::vector<std::uint32_t> table(table_size, kEmpty);
    std::vector<PointBits> vertex_bits;

    IndexedMesh out;
    out.triangles.resize(mesh.triangle_count());
    for (std::size_t c = 0; c < n; ++c) {
        const PointBits key = point_bits(corners[c]);
        std::size_t slot = std::size_t(hash(key)) & mask;
        while (table[slot] != kEmpty && !(vertex_bits[table[slot]] == key))
            slot = (slot + 1) & mask;
        if (table[slot] == kEmpty) {
            table[slot] = static_cast<std::uint32_t>(out.vertices.size());
            out.vertices.push_back(corners[c]);
            vertex_bits.push_back(key);
        }
        out.triangles[c / 3][c % 3] = table[slot];
    }
    return out;
}

// Corners from which welding runs on all cores: below, the threads' start
// costs more than they save.
constexpr std::size_t kParallelCorners = 300000;

} // namespace

IndexedMesh weld_exact_parallel(const TriangleMesh& mesh) {
    const std::vector<Vec3f>& corners = mesh.corners();
    const std::size_t n = corners.size();
    // The same table as the serial weld's, shared by the threads: each slot
    // holds, of the corners with its point, the first one (the lowest index,
    // kept by compare-and-swap whatever order the threads come in).
    const std::size_t table_size = std::bit_ceil(std::max<std::size_t>(2 * n, 16));
    const std::size_t mask = table_size - 1;
    std::unique_ptr<std::atomic<std::uint32_t>[]> table(new std::atomic<std::uint32_t>[table_size]);
    tbb::parallel_for(std::size_t(0), table_size, std::size_t(1) << 16, [&](std::size_t from) {
        for (std::size_t i = from; i < std::min(table_size, from + (std::size_t(1) << 16)); ++i)
            table[i].store(kEmpty, std::memory_order_relaxed);
    });
    std::vector<std::uint32_t> slot_of(n);
    tbb::parallel_for(tbb::blocked_range<std::size_t>(0, n), [&](const tbb::blocked_range<std::size_t>& r) {
        for (std::size_t c = r.begin(); c != r.end(); ++c) {
            const PointBits key = point_bits(corners[c]);
            const auto me = static_cast<std::uint32_t>(c);
            std::size_t slot = std::size_t(hash(key)) & mask;
            for (;;) {
                std::uint32_t held = table[slot].load(std::memory_order_acquire);
                if (held == kEmpty) {
                    if (table[slot].compare_exchange_strong(held, me, std::memory_order_acq_rel))
                        break;
                    continue; // taken meanwhile: look at the slot again
                }
                if (point_bits(corners[held]) == key) {
                    while (me < held && !table[slot].compare_exchange_weak(held, me, std::memory_order_acq_rel)) {
                    }
                    break;
                }
                slot = (slot + 1) & mask;
            }
            slot_of[c] = static_cast<std::uint32_t>(slot);
        }
    });
    // Each corner's first: a new vertex where it is the corner itself,
    // numbered in order of the corners by a prefix sum.
    std::vector<std::uint32_t> first(n), id(n);
    tbb::parallel_for(std::size_t(0), n, [&](std::size_t c) { first[c] = table[slot_of[c]].load(std::memory_order_relaxed); });
    const std::uint32_t count = tbb::parallel_scan(
        tbb::blocked_range<std::size_t>(0, n), std::uint32_t(0),
        [&](const tbb::blocked_range<std::size_t>& r, std::uint32_t sum, bool final) {
            for (std::size_t c = r.begin(); c != r.end(); ++c) {
                if (final)
                    id[c] = sum;
                sum += first[c] == c ? 1u : 0u;
            }
            return sum;
        },
        std::plus<std::uint32_t>());
    IndexedMesh out;
    out.vertices.resize(count);
    out.triangles.resize(mesh.triangle_count());
    tbb::parallel_for(std::size_t(0), n, [&](std::size_t c) {
        if (first[c] == c)
            out.vertices[id[c]] = corners[c];
        out.triangles[c / 3][c % 3] = id[first[c]];
    });
    return out;
}

IndexedMesh weld_exact(const TriangleMesh& mesh) {
    return mesh.corners().size() < kParallelCorners ? weld_serial(mesh) : weld_exact_parallel(mesh);
}

std::size_t orient_consistently(IndexedMesh& mesh) {
    const std::size_t n = mesh.triangles.size();
    // Every edge once per triangle, keyed by its two vertices in order; the
    // direction it runs in that triangle.
    struct EdgeUse {
        std::uint64_t key;
        std::uint32_t triangle;
        bool forward; // a -> b with a < b
    };
    // On all cores: three uses a triangle, in place; an edge of a triangle
    // with two equal corners gets the largest key and is cut off after the
    // sort.
    constexpr std::uint64_t kNone = ~std::uint64_t(0);
    std::vector<EdgeUse> uses(3 * n);
    tbb::parallel_for(std::size_t(0), n, [&](std::size_t t) {
        for (std::size_t k = 0; k < 3; ++k) {
            const std::uint32_t a = mesh.triangles[t][k], b = mesh.triangles[t][(k + 1) % 3];
            uses[3 * t + k] = a == b ? EdgeUse{kNone, std::uint32_t(t), false}
                                     : EdgeUse{(std::uint64_t(std::min(a, b)) << 32) | std::max(a, b),
                                               std::uint32_t(t), a < b};
        }
    });
    // In parallel: the edges of a large mesh are millions (the order is total,
    // so the result is the same as a sequential sort's).
    tbb::parallel_sort(uses.begin(), uses.end(), [](const EdgeUse& l, const EdgeUse& r) {
        return l.key < r.key || (l.key == r.key && l.triangle < r.triangle);
    });
    uses.erase(std::lower_bound(uses.begin(), uses.end(), kNone,
                                [](const EdgeUse& u, std::uint64_t key) { return u.key < key; }),
               uses.end());
    // A manifold edge: two uses of one key by two triangles, the first of
    // them at i. Whether its triangles agree (a shared edge runs opposite
    // ways in two triangles wound alike).
    const auto pair_at = [&](std::size_t i) {
        return i + 1 < uses.size() && uses[i].key == uses[i + 1].key && (i == 0 || uses[i - 1].key != uses[i].key) &&
               (i + 2 == uses.size() || uses[i + 2].key != uses[i].key) && uses[i].triangle != uses[i + 1].triangle;
    };
    const auto each_pair = [&](auto&& visit) {
        for (std::size_t i = 0; i < uses.size(); ++i)
            if (pair_at(i))
                visit(uses[i], uses[i + 1], uses[i].forward != uses[i + 1].forward);
    };
    const bool any_disagree = tbb::parallel_reduce(
        tbb::blocked_range<std::size_t>(0, uses.size()), false,
        [&](const tbb::blocked_range<std::size_t>& r, bool found) {
            for (std::size_t i = r.begin(); i != r.end() && !found; ++i)
                found = pair_at(i) && uses[i].forward == uses[i + 1].forward;
            return found;
        },
        [](bool x, bool y) { return x || y; });
    if (!any_disagree)
        return 0; // the common case: nothing to turn, and no links built
    // Pairs of triangles across manifold edges.
    struct Link {
        std::uint32_t other;
        bool agree;
    };
    std::vector<std::vector<Link>> links(n);
    each_pair([&](const EdgeUse& a, const EdgeUse& b, bool agree) {
        links[a.triangle].push_back({b.triangle, agree});
        links[b.triangle].push_back({a.triangle, agree});
    });
    // Each connected piece: turned to agree with its first triangle, then as a
    // whole if its volume comes out negative.
    std::vector<std::int8_t> turn(n, -1); // -1 unseen, 0 keep, 1 turn
    std::vector<std::uint32_t> queue, piece;
    std::size_t turned = 0;
    for (std::size_t start = 0; start < n; ++start) {
        if (turn[start] >= 0)
            continue;
        turn[start] = 0;
        queue.assign(1, std::uint32_t(start));
        piece.clear();
        while (!queue.empty()) {
            const std::uint32_t t = queue.back();
            queue.pop_back();
            piece.push_back(t);
            for (const Link& l : links[t])
                if (turn[l.other] < 0) {
                    turn[l.other] = std::int8_t(l.agree ? turn[t] : 1 - turn[t]);
                    queue.push_back(l.other);
                }
        }
        double volume = 0.0; // six times the signed volume, as the piece will be wound
        for (const std::uint32_t t : piece) {
            const auto& tri = mesh.triangles[t];
            const Vec3f& a = mesh.vertices[tri[0]];
            const Vec3f& b = mesh.vertices[tri[turn[t] ? 2 : 1]];
            const Vec3f& c = mesh.vertices[tri[turn[t] ? 1 : 2]];
            volume += double(a.x) * (double(b.y) * c.z - double(b.z) * c.y) -
                      double(a.y) * (double(b.x) * c.z - double(b.z) * c.x) +
                      double(a.z) * (double(b.x) * c.y - double(b.y) * c.x);
        }
        // A piece that already agreed keeps its winding: an open surface or
        // an inside-out file is the fill modes' to read, as before.
        const bool repaired = std::any_of(piece.begin(), piece.end(), [&](std::uint32_t t) { return turn[t] != 0; });
        const bool flip_all = repaired && volume < 0.0;
        for (const std::uint32_t t : piece)
            if ((turn[t] != 0) != flip_all) {
                std::swap(mesh.triangles[t][1], mesh.triangles[t][2]);
                ++turned;
            }
    }
    return turned;
}

} // namespace tisma
