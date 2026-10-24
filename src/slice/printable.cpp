// Checks that a model can be printed (docs/spec/BEHAVIOR.md, "Errores y
// avisos"). Own design; see PROVENANCE.md.

#include "slice/printable.hpp"

#include "slice/pipeline.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace tisma {
namespace {

constexpr double kOnEdge = 1e-6; // mm

// Inside the polygon or within kOnEdge of its boundary.
bool on_bed(const std::vector<std::array<double, 2>>& bed, double x, double y) {
    bool in = false;
    for (std::size_t i = 0, j = bed.size() - 1; i < bed.size(); j = i++) {
        const double ax = bed[j][0], ay = bed[j][1], bx = bed[i][0], by = bed[i][1];
        const double dx = bx - ax, dy = by - ay, len2 = dx * dx + dy * dy;
        const double t = len2 > 0.0 ? std::clamp(((x - ax) * dx + (y - ay) * dy) / len2, 0.0, 1.0) : 0.0;
        if (std::hypot(x - (ax + t * dx), y - (ay + t * dy)) <= kOnEdge)
            return true;
        if ((ay > y) != (by > y) && x < ax + (y - ay) * (bx - ax) / (by - ay))
            in = !in;
    }
    return in;
}

} // namespace

std::string unprintable_reason(const IndexedMesh& mesh, const Settings& s) {
    if (mesh.triangles.empty())
        return "the model has no triangles";
    float top = -std::numeric_limits<float>::max(), bottom = std::numeric_limits<float>::max();
    for (const Vec3f& v : mesh.vertices) {
        top = std::max(top, v.z), bottom = std::min(bottom, v.z);
        if (!s.bed.empty() && !on_bed(s.bed, v.x, v.y)) {
            char text[160];
            std::snprintf(text, sizeof text, "the model is outside the bed (machine.bed): a point at x %.3f, y %.3f",
                          double(v.x), double(v.y));
            return text;
        }
    }
    if (s.max_height > 0.0 && double(top) - double(bottom) > s.max_height) {
        char text[160];
        std::snprintf(text, sizeof text, "the model is %.3f mm tall, more than machine.max_height (%.3f mm)",
                      double(top) - double(bottom), s.max_height);
        return text;
    }
    return {};
}

std::string unprintable_reason(const SlicedModel& model) {
    const bool any = std::any_of(model.regions.begin(), model.regions.end(), [](const Polygons& r) { return !r.empty(); });
    return any ? std::string() : "no layer of the model encloses any area (is the mesh flat or open?)";
}

} // namespace tisma
