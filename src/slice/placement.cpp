// Placing the model on the bed. Own design on docs/spec/features/
// placement.md; see PROVENANCE.md.

#include "slice/placement.hpp"

#include <algorithm>

namespace tisma {

void place_on_bed(TriangleMesh& mesh, const Settings& s) {
    if (!s.placement_arrange || s.bed.empty() || mesh.empty())
        return;
    std::array<double, 2> target{};
    if (s.placement_center) {
        target = *s.placement_center;
    } else {
        double lo[2] = {s.bed.front()[0], s.bed.front()[1]}, hi[2] = {lo[0], lo[1]};
        for (const auto& p : s.bed)
            for (int a = 0; a < 2; ++a)
                lo[a] = std::min(lo[a], p[std::size_t(a)]), hi[a] = std::max(hi[a], p[std::size_t(a)]);
        target = {0.5 * (lo[0] + hi[0]), 0.5 * (lo[1] + hi[1])};
    }
    const BoundingBox box = mesh.bounding_box();
    const double dx = target[0] - 0.5 * (double(box.min.x) + double(box.max.x));
    const double dy = target[1] - 0.5 * (double(box.min.y) + double(box.max.y));
    if (dx == 0.0 && dy == 0.0)
        return;
    mesh.translate(float(dx), float(dy), 0.0f);
}

} // namespace tisma
