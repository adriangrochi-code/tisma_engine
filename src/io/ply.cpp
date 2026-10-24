// The surface of analysed voxels as a binary PLY mesh, from the public
// description of the format (Turk, "The PLY Polygon File Format"). Own
// implementation; see PROVENANCE.md.

#include "io/ply.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <ostream>
#include <vector>

namespace tisma {
namespace {

template <typename T>
void put(std::vector<char>& buffer, T value) {
    auto bytes = std::bit_cast<std::array<char, sizeof(T)>>(value);
    if constexpr (std::endian::native == std::endian::big)
        std::reverse(bytes.begin(), bytes.end());
    buffer.insert(buffer.end(), bytes.begin(), bytes.end());
}

// The neighbour across each face, and the face's corners as (dx, dy, dz) from
// the voxel's lowest corner, counter-clockwise seen from outside.
constexpr int kNeighbour[6][3] = {{-1, 0, 0}, {1, 0, 0}, {0, -1, 0}, {0, 1, 0}, {0, 0, -1}, {0, 0, 1}};
constexpr int kCorners[6][4][3] = {
    {{0, 0, 0}, {0, 0, 1}, {0, 1, 1}, {0, 1, 0}}, {{1, 0, 0}, {1, 1, 0}, {1, 1, 1}, {1, 0, 1}},
    {{0, 0, 0}, {1, 0, 0}, {1, 0, 1}, {0, 0, 1}}, {{0, 1, 0}, {0, 1, 1}, {1, 1, 1}, {1, 1, 0}},
    {{0, 0, 0}, {0, 1, 0}, {1, 1, 0}, {1, 0, 0}}, {{0, 0, 1}, {1, 0, 1}, {1, 1, 1}, {0, 1, 1}}};

} // namespace

std::array<std::uint8_t, 3> safety_colour(double safety) {
    if (!std::isfinite(safety))
        return {190, 190, 190};
    if (safety < 1.0)
        return {220, 40, 40};
    if (safety < 1.5)
        return {245, 140, 30};
    if (safety < 2.0)
        return {240, 210, 40};
    return {60, 170, 75};
}

void write_voxel_mesh_ply(std::ostream& out, const AnalysisReport& r) {
    const VoxelGrid& g = r.grid;
    std::size_t faces = 0;
    for (int k = 0; k < g.nz; ++k)
        for (int j = 0; j < g.ny; ++j)
            for (int i = 0; i < g.nx; ++i)
                if (g.at(i, j, k))
                    for (const auto& n : kNeighbour)
                        faces += !g.at(i + n[0], j + n[1], k + n[2]);
    out << "ply\n"
        << "format binary_little_endian 1.0\n"
        << "comment tisma-engine voxel surface\n"
        << "comment voxels analysed " << g.count() << " of a grid of " << g.nx << " x " << g.ny << " x " << g.nz
        << ", " << g.hx << " mm, " << r.rows_per_layer << " rows per layer\n"
        << "comment colour by safety: red below 1, orange to 1.5, yellow to 2, green from 2, grey unstressed\n"
        << "element vertex " << 4 * faces << "\n"
        << "property float x\nproperty float y\nproperty float z\n"
        << "property uchar red\nproperty uchar green\nproperty uchar blue\n"
        << "element face " << faces << "\n"
        << "property list uchar int vertex_indices\n"
        << "property float safety\n"
        << "property uchar mode\n"
        << "end_header\n";
    std::vector<char> buffer;
    buffer.reserve(faces * 4 * 15);
    for (int k = 0; k < g.nz; ++k)
        for (int j = 0; j < g.ny; ++j)
            for (int i = 0; i < g.nx; ++i) {
                if (!g.at(i, j, k))
                    continue;
                const auto colour = safety_colour(r.result.safety[g.index(i, j, k)]);
                for (int f = 0; f < 6; ++f) {
                    if (g.at(i + kNeighbour[f][0], j + kNeighbour[f][1], k + kNeighbour[f][2]))
                        continue;
                    for (const auto& c : kCorners[f]) {
                        put(buffer, float(g.x0 + (i + c[0]) * g.hx));
                        put(buffer, float(g.y0 + (j + c[1]) * g.hy));
                        put(buffer, float(g.z[std::size_t(k + c[2])]));
                        for (std::uint8_t channel : colour)
                            put(buffer, channel);
                    }
                }
            }
    out.write(buffer.data(), std::streamsize(buffer.size()));
    buffer.clear();
    std::uint32_t vertex = 0;
    for (int k = 0; k < g.nz; ++k)
        for (int j = 0; j < g.ny; ++j)
            for (int i = 0; i < g.nx; ++i) {
                if (!g.at(i, j, k))
                    continue;
                const std::size_t v = g.index(i, j, k);
                const float safety = float(std::isfinite(r.result.safety[v]) ? std::min(r.result.safety[v], 1e9) : 1e9);
                for (int f = 0; f < 6; ++f) {
                    if (g.at(i + kNeighbour[f][0], j + kNeighbour[f][1], k + kNeighbour[f][2]))
                        continue;
                    put(buffer, std::uint8_t(4));
                    for (int c = 0; c < 4; ++c)
                        put(buffer, std::int32_t(vertex++));
                    put(buffer, safety);
                    put(buffer, static_cast<std::uint8_t>(r.result.mode[v]));
                }
            }
    out.write(buffer.data(), std::streamsize(buffer.size()));
}

} // namespace tisma
