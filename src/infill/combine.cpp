// Combining the sparse infill of several layers. The rules are the feature
// spec's (docs/spec/features/infill_patterns.md, 18 to 20); the runs over
// variable layer heights are own design. See PROVENANCE.md.

#include "infill/combine.hpp"

#include <algorithm>

namespace tisma {

double combine_max_height(const Settings& s) {
    const double asked = s.infill_combine_max_height.percent ? s.nozzle_diameter * s.infill_combine_max_height.value / 100.0
                                                             : s.infill_combine_max_height.value;
    return std::min(asked, s.nozzle_diameter);
}

std::vector<std::array<std::size_t, 2>> combine_runs(const std::vector<Layer>& layers, const Settings& s) {
    std::vector<std::array<std::size_t, 2>> runs;
    if (!s.infill_combine_auto && s.infill_combine_layers < 2)
        return runs;
    const double most = combine_max_height(s) + 1e-9;
    const std::size_t count = s.infill_combine_auto ? layers.size() : std::size_t(s.infill_combine_layers);
    for (std::size_t first = 1; first < layers.size();) {
        std::size_t last = first;
        double height = layers[first].top - layers[first].bottom;
        while (last + 1 < layers.size() && last + 1 - first < count &&
               height + (layers[last + 1].top - layers[last + 1].bottom) <= most) {
            ++last;
            height += layers[last].top - layers[last].bottom;
        }
        if (last > first)
            runs.push_back({first, last});
        first = last + 1;
    }
    return runs;
}

} // namespace tisma
