// Stress-guided infill density. Own design; see PROVENANCE.md.

#include "infill/stress_density.hpp"

#include "fea/analysis.hpp"

#include <algorithm>
#include <cmath>

namespace tisma {

StressField stress_field(const AnalysisReport& report) {
    StressField out;
    const std::vector<double>& safety = report.result.safety;
    if (!report.valid || safety.size() != report.grid.solid.size())
        return out;
    out.grid = report.grid;
    out.load.assign(safety.size(), 0.0f);
    double most = 0.0;
    for (std::size_t v = 0; v < safety.size(); ++v)
        if (report.grid.solid[v] != 0 && std::isfinite(safety[v]) && safety[v] > 0.0)
            most = std::max(most, 1.0 / safety[v]);
    if (!(most > 0.0))
        return {};
    for (std::size_t v = 0; v < safety.size(); ++v)
        if (report.grid.solid[v] != 0 && std::isfinite(safety[v]) && safety[v] > 0.0)
            out.load[v] = float(1.0 / safety[v] / most);
    return out;
}

StressField infill_stress_field(const IndexedMesh& mesh, const Settings& settings, const AnalysisCase& c,
                                gpu::Context* gpu) {
    AnalysisCase solid = c;
    solid.model = AnalysisModel::Solid;
    solid.resolution_check = false;
    return stress_field(run_analysis(mesh, settings, solid, gpu));
}

Polygons loaded_region(const StressField& f, double z, double level) {
    const VoxelGrid& g = f.grid;
    if (f.empty() || g.z.size() < 2 || z < g.z.front() || z >= g.z.back())
        return {};
    const auto k = int(std::upper_bound(g.z.begin(), g.z.end(), z) - g.z.begin()) - 1;
    std::vector<char> loaded(std::size_t(g.nx) * std::size_t(g.ny));
    for (int j = 0; j < g.ny; ++j)
        for (int i = 0; i < g.nx; ++i)
            loaded[std::size_t(j) * std::size_t(g.nx) + std::size_t(i)] = f.load[g.index(i, j, k)] >= float(level);
    return cell_region(loaded, g.nx, g.ny, g.x0, g.y0, g.hx, g.hy);
}

std::vector<DensityBand> density_bands(double plain, double most) {
    if (!(most > plain))
        return {};
    return {{0.15, plain + (most - plain) / 3.0}, {0.35, plain + 2.0 * (most - plain) / 3.0}, {0.6, most}};
}

} // namespace tisma
