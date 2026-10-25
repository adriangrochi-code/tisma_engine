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

std::string unprintable_reason(const IndexedMesh& input, const Settings& s) {
    if (input.triangles.empty())
        return "the model has no triangles";
    const IndexedMesh mesh = compensated(input, s); // what will be printed
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
    // The engine's coordinates: within 2 m of the origin (the medial axis
    // works in 32-bit nanometres, about 2147 mm), and no taller. Past that,
    // integer overflows or runaway layer counts (the review of 2026-10-25).
    constexpr double kReach = 2000.0;
    for (const Vec3f& v : mesh.vertices)
        if (!(std::fabs(double(v.x)) <= kReach && std::fabs(double(v.y)) <= kReach)) {
            char text[200];
            std::snprintf(text, sizeof text,
                          "the model reaches x %.6g, y %.6g: the engine works within %.0f mm of the origin", double(v.x),
                          double(v.y), kReach);
            return text;
        }
    if (!(double(top) - double(bottom) <= kReach)) {
        char text[160];
        std::snprintf(text, sizeof text, "the model is %.6g mm tall: the engine prints up to %.0f mm",
                      double(top) - double(bottom), kReach);
        return text;
    }
    if (s.max_height > 0.0 && double(top) - double(bottom) > s.max_height) {
        char text[160];
        std::snprintf(text, sizeof text, "the model is %.3f mm tall, more than machine.max_height (%.3f mm)",
                      double(top) - double(bottom), s.max_height);
        return text;
    }
    return {};
}

std::string unprintable_reason(const SlicedModel& model, const Settings& s) {
    const bool any = std::any_of(model.regions.begin(), model.regions.end(), [](const Polygons& r) { return !r.empty(); });
    if (!any)
        return "no layer of the model encloses any area (is the mesh flat or open?)";
    // The skirt and the brim go around the model: they must fit on the bed too.
    if (!s.bed.empty())
        for (const auto* paths : {&model.skirt, &model.brim})
            for (const Toolpath& t : *paths)
                for (const ToolpathPoint& p : t.points)
                    if (!on_bed(s.bed, p.p.x, p.p.y)) {
                        char text[200];
                        std::snprintf(text, sizeof text,
                                      "the %s goes off the bed (machine.bed) at x %.3f, y %.3f: move the model or "
                                      "shrink adhesion.%s",
                                      paths == &model.skirt ? "skirt" : "brim", p.p.x, p.p.y,
                                      paths == &model.skirt ? "skirt_distance" : "brim_width");
                        return text;
                    }
    // Something to extrude: a wall, infill of any kind, or gap fill. (The
    // skirt and the brim alone are not a print.)
    auto has = [](const std::vector<Toolpath>& paths) {
        return std::any_of(paths.begin(), paths.end(), [](const Toolpath& t) { return t.points.size() >= 2; });
    };
    for (std::size_t l = 0; l < model.layers.size(); ++l) {
        if (l < model.walls.size() && has(model.walls[l].toolpaths))
            return {};
        if (l < model.fills.size()) {
            const LayerFill& f = model.fills[l];
            if (has(f.top) || has(f.solid) || has(f.sparse) || has(f.bridge) || has(f.gaps))
                return {};
        }
    }
    return "nothing would be printed: every part of the model is thinner than the lines (check walls.width, "
           "slicing.xy_offset and the model's scale)";
}

} // namespace tisma
