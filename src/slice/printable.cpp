// Checks that a model can be printed (docs/spec/BEHAVIOR.md, "Errores y
// avisos"). Own design; see PROVENANCE.md.

#include "slice/printable.hpp"

#include "slice/pipeline.hpp"
#include "slice/wipe_tower.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>

namespace tisma {
namespace {

constexpr double kOnEdge = 1e-6; // mm

// The distance between two outlines, mm: 0 when they touch or overlap.
double outline_distance(const Polygon& a, const Polygon& b) {
    if (a.empty() || b.empty())
        return std::numeric_limits<double>::infinity();
    if (!Clipper2Lib::Intersect(Polygons{a}, Polygons{b}, Clipper2Lib::FillRule::NonZero).empty())
        return 0.0;
    double best = std::numeric_limits<double>::infinity();
    for (const auto* p : {&a, &b}) {
        const Polygon& from = *p;
        const Polygon& to = p == &a ? b : a;
        for (const auto& q : from)
            for (std::size_t i = 0; i < to.size(); ++i) {
                const auto& u = to[i];
                const auto& v = to[(i + 1) % to.size()];
                const double ux = to_mm(u.x), uy = to_mm(u.y), dx = to_mm(v.x) - ux, dy = to_mm(v.y) - uy;
                const double px = to_mm(q.x), py = to_mm(q.y), len2 = dx * dx + dy * dy;
                const double t = len2 > 0.0 ? std::clamp(((px - ux) * dx + (py - uy) * dy) / len2, 0.0, 1.0) : 0.0;
                best = std::min(best, std::hypot(px - ux - t * dx, py - uy - t * dy));
            }
    }
    return best;
}

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

// A belt printer's bed is only as wide as its belt, which runs on along y
// (belt_printer.md).
bool on_bed(const Settings& s, double x, double y) {
    if (!s.belt_enabled)
        return on_bed(s.bed, x, y);
    double lo = std::numeric_limits<double>::infinity(), hi = -lo;
    for (const auto& p : s.bed)
        lo = std::min(lo, p[0]), hi = std::max(hi, p[0]);
    return x >= lo - kOnEdge && x <= hi + kOnEdge;
}

} // namespace

std::string unprintable_reason(const IndexedMesh& input, const Settings& s) {
    if (input.triangles.empty())
        return "the model has no triangles";
    const IndexedMesh mesh = compensated(input, s); // what will be printed
    float top = -std::numeric_limits<float>::max(), bottom = std::numeric_limits<float>::max();
    for (const Vec3f& v : mesh.vertices) {
        top = std::max(top, v.z), bottom = std::min(bottom, v.z);
        if (!s.bed.empty() && !on_bed(s, v.x, v.y)) {
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
    if (!any) {
        // Say what emptied it: the negative volumes, a shrinking setting, or holes in the mesh.
        std::string reason = "no layer of the model encloses any area";
        if (model.cut_away > 0)
            reason += ": the negative volumes take all of it away";
        else if (s.xy_offset < 0.0) {
            char text[120];
            std::snprintf(text, sizeof text, ": slicing.xy_offset (%.3g mm) may have shrunk it away", s.xy_offset);
            reason += text;
        }
        else if (model.open_contours > 0)
            reason += ": " + std::to_string(model.open_contours) +
                      " open contours were dropped, so the mesh has holes; repair it";
        else
            reason += " (is the mesh flat or open?)";
        return reason;
    }
    // Object by object (sequential.md, rules 2 and 4): every object but the
    // last under the X axis, and the head's cylinder clear of the others.
    if (model.sequential) {
        char text[300];
        for (std::size_t k = 0; k + 1 < model.objects.size(); ++k)
            if (const SlicedObject& o = model.objects[k]; o.height > s.clearance_height + 1e-6) {
                std::snprintf(text, sizeof text,
                              "'%s' is %.2f mm tall, more than sequential.clearance_height (%.2f mm): the X axis would "
                              "hit it while the next objects print; only the last object may be taller",
                              o.label().c_str(), o.height, s.clearance_height);
                return text;
            }
        for (std::size_t i = 0; i < model.objects.size(); ++i)
            for (std::size_t j = i + 1; j < model.objects.size(); ++j)
                if (const double d = outline_distance(model.objects[i].outline, model.objects[j].outline);
                    d < s.clearance_radius - 1e-6) {
                    std::snprintf(text, sizeof text,
                                  "'%s' and '%s' are %.2f mm apart, less than sequential.clearance_radius (%.2f mm): the "
                                  "head would hit one while printing the other; place them farther apart",
                                  model.objects[i].label().c_str(), model.objects[j].label().c_str(), d, s.clearance_radius);
                    return text;
                }
    }
    // Vase mode lays one spiral (vase_mode.md, rule 6): several objects only one after another.
    if (s.vase && model.objects.size() > 1 && !model.sequential)
        return "vase mode prints one object, or several one after another (sequential.enabled)";
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
    // The wipe tower (multi_extruder.md, rule 10): on the bed, clear of the
    // part's first layer and its skirt and brim.
    if (const WipeTower tower = plan_wipe_tower(model, s, plan_tools(model, s)); !tower.empty()) {
        char text[240];
        for (const auto& q : tower.outline)
            if (!s.bed.empty() && !on_bed(s.bed, to_mm(q.x), to_mm(q.y))) {
                std::snprintf(text, sizeof text,
                              "the wipe tower goes off the bed (machine.bed) at x %.3f, y %.3f: move it "
                              "(wipe_tower.x, wipe_tower.y) or make it narrower",
                              to_mm(q.x), to_mm(q.y));
                return text;
            }
        // The skirt's and the brim's loops with what they go round.
        Polygons first = model.regions.empty() ? Polygons{} : model.regions.front();
        for (const auto* paths : {&model.skirt, &model.brim})
            for (const Toolpath& t : *paths) {
                Polygon loop;
                for (const ToolpathPoint& p : t.points)
                    loop.push_back({to_units(p.p.x), to_units(p.p.y)});
                if (loop.size() >= 3)
                    first.push_back(std::move(loop));
            }
        if (!Clipper2Lib::Intersect(Polygons{tower.outline}, first, Clipper2Lib::FillRule::NonZero).empty())
            return "the wipe tower lies on the part, its skirt or its brim: move it (wipe_tower.x, wipe_tower.y)";
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
