// Which extruder lays what, in what order, and the wipe tower
// (docs/spec/features/multi_extruder.md, rules 2, 3 and 9 to 13). Own
// design; see PROVENANCE.md.

#include "slice/wipe_tower.hpp"

#include "slice/pipeline.hpp"

#include <oneapi/tbb/parallel_for.h>

#include <algorithm>
#include <cmath>
#include <numbers>

namespace tisma {

int path_tool(const Toolpath& path, int chosen, bool part, const Settings& settings) {
    if (settings.extruders <= 1)
        return 0;
    return std::clamp(chosen > 0 ? chosen : path.tool > 0 ? int(path.tool) : part ? 1 : 0, 0, kMaxExtruders);
}

std::array<bool, kMaxExtruders + 1> layer_tools(const SlicedModel& model, std::size_t layer, const Settings& s) {
    std::array<bool, kMaxExtruders + 1> used{};
    if (s.extruders <= 1)
        return used;
    const auto note = [&](const std::vector<Toolpath>& paths, int chosen, bool part) {
        for (const Toolpath& t : paths)
            if (!t.points.empty())
                used[std::size_t(path_tool(t, chosen, part, s))] = true;
    };
    if (layer < model.walls.size())
        note(model.walls[layer].toolpaths, s.extruder_walls, true), note(model.walls[layer].gaps, s.extruder_walls, true);
    if (layer < model.fills.size()) {
        const LayerFill& f = model.fills[layer];
        note(f.solid, s.extruder_solid_infill, true), note(f.top, s.extruder_solid_infill, true);
        note(f.bridge, s.extruder_solid_infill, true), note(f.gaps, s.extruder_solid_infill, true);
        note(f.sparse, s.extruder_infill, true);
        note(f.support, s.extruder_support, false), note(f.support_interface, s.extruder_support_interface, false);
        note(f.arcs, s.extruder_walls, true), note(f.overhang_walls, s.extruder_walls, true);
        note(f.ironing, s.extruder_solid_infill, true);
    }
    return used;
}

namespace {

// A path's volume as the G-code lays it (width x height x length), mm3.
double volume_of(const Toolpath& t, double height) {
    double v = 0.0;
    for (std::size_t k = 1; k < t.points.size(); ++k)
        v += 0.5 * (t.points[k - 1].width + t.points[k].width) *
             std::hypot(t.points[k].p.x - t.points[k - 1].p.x, t.points[k].p.y - t.points[k - 1].p.y);
    if (t.closed && t.points.size() > 2)
        v += 0.5 * (t.points.back().width + t.points.front().width) *
             std::hypot(t.points.front().p.x - t.points.back().p.x, t.points.front().p.y - t.points.back().p.y);
    return v * height;
}

// What the changes of each layer purge into objects (rule 13), and what is
// left for the tower.
void plan_sinks(ToolPlan& plan, const SlicedModel& model, const Settings& s) {
    const std::size_t n = model.layers.size();
    plan.sinks.assign(n, {});
    plan.tower_purge.assign(n, {});
    bool any = false;
    for (const SlicedObject& o : model.objects)
        any = any || o.wipe_into_infill || o.wipe_into_object;
    tbb::parallel_for(std::size_t(0), n, [&](std::size_t l) {
        const std::vector<int>& order = plan.order[l];
        plan.sinks[l].assign(order.size(), {});
        plan.tower_purge[l].assign(order.size(), 0.0);
        for (std::size_t g = 0; g < order.size(); ++g)
            if (plan.change_from[l][g] > 0)
                plan.tower_purge[l][g] = purge_volume(s, plan.change_from[l][g], order[g]);
        if (!any || !s.single_extruder_mmu || model.sequential)
            return;
        // The paths of the layer that objects let purge into, with their
        // group: sparse infill with wipe_into_infill, all with wipe_into_object.
        struct Candidate {
            ToolPlan::Sink sink;
            std::size_t group = 0;
            double volume = 0.0;
        };
        std::vector<Candidate> candidates;
        const double h = model.layers[l].top - model.layers[l].bottom;
        const auto consider = [&](const std::vector<Toolpath>& paths, int kind, int chosen, bool part) {
            for (const Toolpath& t : paths) {
                if (t.points.size() < 2)
                    continue;
                const Clipper2Lib::Point64 q{to_units(t.points.front().p.x), to_units(t.points.front().p.y)};
                for (std::size_t o = 0; o < model.objects.size(); ++o) {
                    const SlicedObject& object = model.objects[o];
                    if (!(object.wipe_into_object || (object.wipe_into_infill && kind == 4)) ||
                        Clipper2Lib::PointInPolygon(q, object.outline) == Clipper2Lib::PointInPolygonResult::IsOutside)
                        continue;
                    const int e = path_tool(t, chosen, part, s);
                    const auto at = std::find(order.begin(), order.end(), e);
                    candidates.push_back({{&t, kind, int(o)}, e == 0 || at == order.end() ? 0 : std::size_t(at - order.begin()),
                                          volume_of(t, h)});
                    break;
                }
            }
        };
        if (l < model.fills.size()) {
            const LayerFill& f = model.fills[l];
            consider(f.sparse, 4, s.extruder_infill, true);
            consider(f.solid, 0, s.extruder_solid_infill, true), consider(f.top, 1, s.extruder_solid_infill, true);
            consider(f.bridge, 2, s.extruder_solid_infill, true), consider(f.gaps, 3, s.extruder_solid_infill, true);
            consider(f.arcs, ToolPlan::kArc, s.extruder_walls, true);
            consider(f.overhang_walls, ToolPlan::kOverhangWall, s.extruder_walls, true);
            consider(f.ironing, ToolPlan::kIroning, s.extruder_solid_infill, true);
        }
        if (l < model.walls.size()) {
            consider(model.walls[l].gaps, ToolPlan::kWallGap, s.extruder_walls, true);
            consider(model.walls[l].toolpaths, ToolPlan::kWall, s.extruder_walls, true);
        }
        std::vector<char> taken(candidates.size(), 0);
        for (std::size_t g = 0; g < order.size(); ++g) {
            if (plan.change_from[l][g] == 0)
                continue;
            const double floor = per_extruder(s.min_purge_on_tower, std::size_t(order[g] - 1));
            double& tower = plan.tower_purge[l][g];
            for (std::size_t c = 0; c < candidates.size() && tower > floor; ++c) {
                if (taken[c] || candidates[c].group < g) // printed before the change
                    continue;
                taken[c] = 1;
                plan.sinks[l][g].push_back(candidates[c].sink);
                tower = std::max(floor, tower - candidates[c].volume);
            }
        }
    });
}

} // namespace

ToolPlan plan_tools(const SlicedModel& model, const Settings& s) {
    ToolPlan plan;
    const std::size_t n = model.layers.size();
    plan.order.assign(n, std::vector<int>{0});
    plan.change_from.assign(n, std::vector<int>{0});
    if (s.extruders <= 1) {
        plan.sinks.assign(n, {{}});
        plan.tower_purge.assign(n, {0.0});
        return plan;
    }
    std::vector<std::array<bool, kMaxExtruders + 1>> used(n);
    tbb::parallel_for(std::size_t(0), n, [&](std::size_t l) { used[l] = layer_tools(model, l, s); });
    for (std::size_t l = 0; l < n && plan.initial == 0; ++l)
        for (int e = 1; e <= kMaxExtruders && plan.initial == 0; ++e)
            if (used[l][std::size_t(e)])
                plan.initial = e;
    if (plan.initial == 0)
        plan.initial = 1;
    plan.used[std::size_t(plan.initial)] = true;
    int current = plan.initial;
    for (std::size_t l = 0; l < n; ++l) {
        std::vector<int>& order = plan.order[l];
        order.clear();
        if (used[l][std::size_t(current)])
            order.push_back(current);
        for (int e = 1; e <= kMaxExtruders; ++e)
            if (used[l][std::size_t(e)] && e != current)
                order.push_back(e);
        if (order.empty())
            order.push_back(0);
        std::vector<int>& from = plan.change_from[l];
        from.assign(order.size(), 0);
        for (std::size_t g = 0; g < order.size(); ++g) {
            if (order[g] == 0)
                continue;
            plan.used[std::size_t(order[g])] = true;
            if (order[g] != current)
                from[g] = current;
            current = order[g];
        }
    }
    plan_sinks(plan, model, s);
    return plan;
}

double purge_volume(const Settings& s, int from, int to) {
    if (from == to || from < 1 || to < 1)
        return 0.0;
    const auto i = std::size_t(from - 1), j = std::size_t(to - 1);
    const double volume =
        i < s.purge_volumes.size() && j < s.purge_volumes[i].size() ? s.purge_volumes[i][j] : 140.0;
    return volume * per_extruder(s.purge_multiplier, j, 100.0) / 100.0;
}

namespace {

// The tower's paths in its own frame (x across its width, y along its
// depth, from its corner), each bead `w` wide.
struct Frame {
    double width, depth, w, spacing;

    Toolpath path(std::initializer_list<Point2d> points, bool closed = false) const {
        Toolpath t;
        t.closed = closed;
        for (const Point2d& p : points)
            t.points.push_back({p, w});
        return t;
    }
    // A loop round the tower, `d` in from its edge (out with a negative d).
    Toolpath loop(double d) const {
        return path({{d, d}, {width - d, d}, {width - d, depth - d}, {d, depth - d}}, true);
    }
    // `n` lines across the inside from y0, `gap` apart (the spacing), one
    // zig-zag of beads `bead` wide (the tower's).
    Toolpath lines(double y0, int n, double bead = 0.0, double gap = 0.0) const {
        Toolpath t;
        if (!(bead > 0.0))
            bead = w;
        if (!(gap > 0.0))
            gap = spacing;
        const double a = 1.5 * w, b = width - 1.5 * w;
        for (int k = 0; k < n; ++k) {
            const double y = y0 + gap * (k + 0.5);
            const bool forward = k % 2 == 0;
            t.points.push_back({{forward ? a : b, y}, bead});
            t.points.push_back({{forward ? b : a, y}, bead});
        }
        return t;
    }
    // Columns along the depth from y0 to y1, at most `bridging` apart, joined
    // at their ends: what the blocks above bridge between.
    Toolpath columns(double y0, double y1, double bridging) const {
        Toolpath t;
        const double lo = y0 + 0.5 * w, hi = y1 - 0.5 * w;
        if (!(hi > lo))
            return t;
        const double a = 1.5 * w, b = width - 1.5 * w;
        const int count = std::max(2, int(std::ceil((b - a) / bridging - 1e-9)) + 1);
        for (int k = 0; k < count; ++k) {
            const double x = a + (b - a) * k / (count - 1);
            const bool up = k % 2 == 0;
            t.points.push_back({{x, up ? lo : hi}, w});
            t.points.push_back({{x, up ? hi : lo}, w});
        }
        return t;
    }
};

// A path cut into consecutive pieces of these lengths (what is left of the
// path past them is dropped).
std::vector<Toolpath> cut(const Toolpath& path, const std::vector<double>& lengths) {
    std::vector<Toolpath> pieces;
    std::size_t k = 1;
    Point2d at = path.points.empty() ? Point2d{} : path.points.front().p;
    const double width = path.points.empty() ? 0.0 : path.points.front().width;
    for (const double length : lengths) {
        Toolpath piece;
        piece.points.push_back({at, width});
        double left = length;
        while (left > 1e-9 && k < path.points.size()) {
            const Point2d to = path.points[k].p;
            const double d = std::hypot(to.x - at.x, to.y - at.y);
            if (d <= left) {
                at = to, left -= d, ++k;
            } else {
                at = {at.x + (to.x - at.x) * left / d, at.y + (to.y - at.y) * left / d};
                left = 0.0;
            }
            piece.points.push_back({at, width});
        }
        if (piece.points.size() > 1)
            pieces.push_back(std::move(piece));
    }
    return pieces;
}

} // namespace

WipeTower plan_wipe_tower(const SlicedModel& model, const Settings& s, const ToolPlan& tools) {
    WipeTower tower;
    if (!s.wipe_tower || s.extruders <= 1 || model.layers.empty())
        return tower;
    std::size_t last = model.layers.size();
    for (std::size_t l = 0; l < tools.change_from.size(); ++l)
        for (const int from : tools.change_from[l])
            if (from > 0)
                last = l;
    if (last == model.layers.size())
        return tower; // no change: no tower
    Frame f{s.wipe_tower_width, 0.0, s.wall_width * s.wipe_tower_extra_flow / 100.0, 0.0};
    f.spacing = f.w * s.wipe_tower_extra_spacing / 100.0;
    // A line across the inside, inside the outline's bead, its ends half a bead in.
    const double line = f.width - 3.0 * f.w;
    if (!(line > f.w))
        return tower; // too narrow for anything inside
    // Each change's lines: what its purge fills, or a few to prime; with an
    // MMU, the ramming's first (rule 15): beads 1.5 walls wide, each quarter
    // of a second as long as its flow asks.
    std::vector<std::vector<int>> counts(last + 1), ram_lines(last + 1);
    std::vector<std::vector<std::vector<double>>> ram_lengths(last + 1), ram_feeds(last + 1);
    const double ram_bead = 1.5 * s.wall_width;
    double deepest = kPrimeLines * f.spacing;
    for (std::size_t l = 0; l <= last; ++l) {
        const double h = model.layers[l].top - model.layers[l].bottom;
        double used = 0.0;
        for (std::size_t g = 0; g < tools.change_from[l].size(); ++g) {
            const int from = tools.change_from[l][g];
            if (from == 0)
                continue;
            // What the objects did not take of the purge (rule 13).
            const double volume = tools.tower_purge[l][g];
            const int n = s.single_extruder_mmu ? std::max(0, int(std::ceil(volume / (f.w * h * line) - 1e-9))) : kPrimeLines;
            counts[l].push_back(n);
            used += n * f.spacing;
            std::vector<double> lengths, feeds;
            double total = 0.0;
            if (s.single_extruder_mmu)
                for (const double flow : s.ramming)
                    if (flow > 0.0) {
                        feeds.push_back(flow / (ram_bead * h));
                        lengths.push_back(0.25 * feeds.back());
                        total += lengths.back();
                    }
            const int rams = total > 0.0 ? int(std::ceil(total / line - 1e-9)) : 0;
            ram_lines[l].push_back(rams);
            ram_lengths[l].push_back(std::move(lengths));
            ram_feeds[l].push_back(std::move(feeds));
            used += rams * ram_bead;
        }
        deepest = std::max(deepest, used);
    }
    f.depth = deepest + 2.0 * f.w;
    tower.width = f.width;
    tower.depth = f.depth;
    // From the tower's frame to the bed: turned about the corner, moved to it.
    const double turn = s.wipe_tower_rotation * std::numbers::pi / 180.0;
    const double c = std::cos(turn), sn = std::sin(turn);
    const auto place = [&](Toolpath t) {
        for (ToolpathPoint& p : t.points)
            p.p = {s.wipe_tower_x + c * p.p.x - sn * p.p.y, s.wipe_tower_y + sn * p.p.x + c * p.p.y};
        return t;
    };
    const int brim = s.wipe_tower_brim > 0.0 ? int(std::ceil(s.wipe_tower_brim / f.w - 1e-9)) : 0;
    {
        const double out = brim * f.w;
        for (const ToolpathPoint& p : place(f.path({{-out, -out}, {f.width + out, -out}, {f.width + out, f.depth + out},
                                                    {-out, f.depth + out}})).points)
            tower.outline.push_back({to_units(p.p.x), to_units(p.p.y)});
    }
    tower.layers.resize(last + 1);
    double top = model.layers.front().bottom; // the tower's top so far
    for (std::size_t l = 0; l <= last; ++l) {
        WipeTowerLayer& layer = tower.layers[l];
        const double h = model.layers[l].top - model.layers[l].bottom;
        if (s.wipe_tower_no_sparse_layers) {
            if (l > 0 && counts[l].empty())
                continue; // no change: no tower on this layer
            top += h;
            layer.drop = std::max(0.0, model.layers[l].top - top);
        }
        double y = f.w;
        for (std::size_t b = 0; b < counts[l].size(); ++b) {
            std::vector<Toolpath> ram;
            for (Toolpath& piece : cut(f.lines(y, ram_lines[l][b], ram_bead, ram_bead), ram_lengths[l][b]))
                ram.push_back(place(std::move(piece)));
            layer.ramming.push_back(std::move(ram));
            layer.ramming_feed.push_back(ram_feeds[l][b]);
            y += ram_lines[l][b] * ram_bead;
            layer.blocks.push_back({place(f.lines(y, counts[l][b]))});
            y += counts[l][b] * f.spacing;
        }
        if (l == 0)
            for (int k = brim; k-- > 0;) // the brim from the outside in, to the tower
                layer.shell.push_back(place(f.loop(-(k + 0.5) * f.w)));
        layer.shell.push_back(place(f.loop(0.5 * f.w)));
        // The rest of the inside: dense on the first layer, columns above.
        Toolpath fill = l == 0 ? f.lines(y, int(std::floor((f.depth - f.w - y) / f.spacing + 1e-9)))
                               : f.columns(y, f.depth - f.w, s.wipe_tower_bridging);
        if (fill.points.size() > 1)
            layer.shell.push_back(place(std::move(fill)));
    }
    return tower;
}

} // namespace tisma
