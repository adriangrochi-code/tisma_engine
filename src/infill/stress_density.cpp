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
    // Each row's runs of loaded voxels as rectangles, then one union.
    Polygons boxes;
    for (int j = 0; j < g.ny; ++j) {
        int start = -1;
        for (int i = 0; i <= g.nx; ++i) {
            const bool loaded = i < g.nx && f.load[g.index(i, j, k)] >= float(level);
            if (loaded && start < 0)
                start = i;
            if (!loaded && start >= 0) {
                const double x0 = g.x0 + start * g.hx, x1 = g.x0 + i * g.hx;
                const double y0 = g.y0 + j * g.hy, y1 = y0 + g.hy;
                boxes.push_back({{to_units(x0), to_units(y0)}, {to_units(x1), to_units(y0)},
                                 {to_units(x1), to_units(y1)}, {to_units(x0), to_units(y1)}});
                start = -1;
            }
        }
    }
    if (boxes.empty())
        return {};
    const Polygons joined = Clipper2Lib::Union(boxes, Clipper2Lib::FillRule::NonZero);
    const double half = 0.5 * std::max(g.hx, g.hy);
    return offset(offset(joined, half), -half);
}

std::vector<DensityBand> density_bands(double plain, double most) {
    if (!(most > plain))
        return {};
    return {{0.15, plain + (most - plain) / 3.0}, {0.35, plain + 2.0 * (most - plain) / 3.0}, {0.6, most}};
}

} // namespace tisma
