#include "mesh/indexed_mesh.hpp"

#include <algorithm>
#include <bit>

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

IndexedMesh weld_exact(const TriangleMesh& mesh) {
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

} // namespace tisma
