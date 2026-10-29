// Legacy VTK writer for analysis results, from Kitware's public description
// of the format ("VTK File Formats"). Own implementation; see PROVENANCE.md.

#include "io/vtk.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <ostream>
#include <string>
#include <vector>

namespace tisma {
namespace {

// Binary legacy VTK is big-endian.
template <typename T>
void put(std::vector<char>& buffer, T value) {
    auto bytes = std::bit_cast<std::array<char, sizeof(T)>>(value);
    if constexpr (std::endian::native == std::endian::little)
        std::reverse(bytes.begin(), bytes.end());
    buffer.insert(buffer.end(), bytes.begin(), bytes.end());
}

void flush(std::ostream& out, std::vector<char>& buffer) {
    out.write(buffer.data(), std::streamsize(buffer.size()));
    out << "\n";
    buffer.clear();
}

} // namespace

void write_analysis_vtk(std::ostream& out, const AnalysisReport& r) {
    const VoxelGrid& g = r.grid;
    const AsPrintedResult& result = r.result;
    std::vector<char> buffer;
    out << "# vtk DataFile Version 3.0\n"
        << "tisma-engine structural analysis\n"
        << "BINARY\n"
        << "DATASET RECTILINEAR_GRID\n"
        << "DIMENSIONS " << g.nx + 1 << " " << g.ny + 1 << " " << g.nz + 1 << "\n";
    out << "X_COORDINATES " << g.nx + 1 << " double\n";
    for (int i = 0; i <= g.nx; ++i)
        put(buffer, g.x0 + i * g.hx);
    flush(out, buffer);
    out << "Y_COORDINATES " << g.ny + 1 << " double\n";
    for (int j = 0; j <= g.ny; ++j)
        put(buffer, g.y0 + j * g.hy);
    flush(out, buffer);
    out << "Z_COORDINATES " << g.nz + 1 << " double\n";
    for (double z : g.z)
        put(buffer, z);
    flush(out, buffer);

    const std::size_t cells = g.solid.size();
    out << "CELL_DATA " << cells << "\n";
    out << "SCALARS solid unsigned_char 1\nLOOKUP_TABLE default\n";
    for (std::size_t v = 0; v < cells; ++v)
        put(buffer, static_cast<unsigned char>(g.solid[v] ? 1 : 0));
    flush(out, buffer);
    out << "SCALARS safety float 1\nLOOKUP_TABLE default\n";
    for (std::size_t v = 0; v < cells; ++v) {
        const double s = v < result.safety.size() ? result.safety[v] : 0.0;
        put(buffer, float(!g.solid[v] ? -1.0 : std::isfinite(s) ? std::min(s, 1e9) : 1e9));
    }
    flush(out, buffer);
    out << "SCALARS peak_safety float 1\nLOOKUP_TABLE default\n";
    for (std::size_t v = 0; v < cells; ++v) {
        const double s = v < result.peak_safety.size() ? result.peak_safety[v] : 0.0;
        put(buffer, float(!g.solid[v] ? -1.0 : std::isfinite(s) ? std::min(s, 1e9) : 1e9));
    }
    flush(out, buffer);
    out << "SCALARS mode unsigned_char 1\nLOOKUP_TABLE default\n";
    for (std::size_t v = 0; v < cells; ++v)
        put(buffer, static_cast<unsigned char>(v < result.mode.size() ? static_cast<unsigned char>(result.mode[v]) : 0));
    flush(out, buffer);
    out << "SCALARS von_mises float 1\nLOOKUP_TABLE default\n";
    for (std::size_t v = 0; v < cells; ++v)
        put(buffer, float(v < result.elasticity.von_mises.size() ? result.elasticity.von_mises[v] : 0.0));
    flush(out, buffer);
    out << "SCALARS direction float 1\nLOOKUP_TABLE default\n";
    for (std::size_t v = 0; v < cells; ++v) {
        const bool known = g.solid[v] && v < r.direction.size() && std::isfinite(r.direction[v]);
        float degrees = known ? float(double(r.direction[v]) * 180.0 / std::numbers::pi) : -1.0f;
        if (degrees >= 180.0f)
            degrees -= 180.0f; // an angle just under half a turn that rounded up
        put(buffer, degrees);
    }
    flush(out, buffer);

    const std::size_t points = std::size_t(g.nx + 1) * std::size_t(g.ny + 1) * std::size_t(g.nz + 1);
    out << "POINT_DATA " << points << "\n";
    out << "VECTORS displacement float\n";
    for (std::size_t q = 0; q < 3 * points; ++q)
        put(buffer, float(q < result.elasticity.displacement.size() ? result.elasticity.displacement[q] : 0.0));
    flush(out, buffer);
}

} // namespace tisma
