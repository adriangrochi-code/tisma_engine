// Organic (tree) supports (docs/spec/features/supports_more.md, rules 15 to
// 17). Own design; see PROVENANCE.md.

#include "support/tree.hpp"

#include "infill/rectilinear.hpp"

#include <oneapi/tbb/parallel_for.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>
#include <unordered_map>

namespace tisma {
namespace {

constexpr double kDegrees = std::numbers::pi / 180.0;

// The point of a region's boundary nearest to (x, y), and its distance.
struct Nearest {
    double x = 0.0, y = 0.0, d = std::numeric_limits<double>::infinity();
};
Nearest nearest_boundary(const Polygons& region, double x, double y) {
    Nearest best;
    for (const Polygon& ring : region) {
        const std::size_t n = ring.size();
        for (std::size_t i = 0, j = n - 1; i < n; j = i++) {
            const double ax = to_mm(ring[j].x), ay = to_mm(ring[j].y), bx = to_mm(ring[i].x), by = to_mm(ring[i].y);
            // A segment wholly farther than the best so far is passed over.
            if (std::min(ax, bx) - x > best.d || x - std::max(ax, bx) > best.d || std::min(ay, by) - y > best.d ||
                y - std::max(ay, by) > best.d)
                continue;
            const double dx = bx - ax, dy = by - ay, len2 = dx * dx + dy * dy;
            const double t = len2 > 0.0 ? std::clamp(((x - ax) * dx + (y - ay) * dy) / len2, 0.0, 1.0) : 0.0;
            const double qx = ax + t * dx, qy = ay + t * dy;
            const double d = std::hypot(qx - x, qy - y);
            if (d < best.d)
                best = {qx, qy, d};
        }
    }
    return best;
}

// Where a branch of radius r near (x, y) can stand clear of `forbidden`
// (the part grown by the gap): where it is if it already is, else pushed
// straight away from the nearest boundary. False if that is farther than
// `reach`.
bool keep_clear(const Polygons& forbidden, double& x, double& y, double r, double reach) {
    if (forbidden.empty())
        return true;
    const bool inside = region_contains(forbidden, x, y);
    const Nearest q = nearest_boundary(forbidden, x, y);
    if (!inside && q.d >= r)
        return true;
    if (!(q.d < std::numeric_limits<double>::infinity()))
        return true;
    double ux = q.x - x, uy = q.y - y; // towards the boundary
    double len = std::hypot(ux, uy);
    if (len < 1e-9) { // on the boundary: no direction from it; give up the move
        return false;
    }
    ux /= len, uy /= len;
    // Inside: past the boundary by r; outside but too near: back from it to r.
    const double nx = inside ? q.x + ux * (r + 1e-3) : q.x - ux * (r + 1e-3);
    const double ny = inside ? q.y + uy * (r + 1e-3) : q.y - uy * (r + 1e-3);
    if (std::hypot(nx - x, ny - y) > reach)
        return false;
    if (region_contains(forbidden, nx, ny) || nearest_boundary(forbidden, nx, ny).d < r * 0.98)
        return false; // a narrow gap: nowhere to stand
    x = nx, y = ny;
    return true;
}

// A ring as a closed support path.
Toolpath ring_path(const Polygon& ring, double bead) {
    Toolpath t;
    t.closed = true;
    t.points.reserve(ring.size());
    for (const auto& q : ring)
        t.points.push_back({{to_mm(q.x), to_mm(q.y)}, bead});
    return t;
}

struct Branch {
    TreeNode node;
    double grown = 0.0;   // mm below its tip
    std::vector<std::pair<std::size_t, std::size_t>> recent; // its last placements (layer, index)
    bool alive = true;
};

// The tips that cover an overhang not yet under a branch: candidates along
// its outline and on a grid inside it, each taken unless a branch or a tip
// already stands within the spacing, and moved off the part (`forbidden`)
// within `reach`; one that cannot be is left for the next candidate.
void add_tips(const Polygons& overhang, const Polygons& forbidden, double spacing, double tip_r, double reach,
              std::vector<Branch>& branches) {
    if (overhang.empty())
        return;
    std::vector<std::pair<double, double>> candidates;
    const double step = spacing * 0.5;
    for (const Polygon& ring : overhang) {
        const std::size_t n = ring.size();
        double carried = 0.0;
        for (std::size_t i = 0; i < n; ++i) {
            const double ax = to_mm(ring[i].x), ay = to_mm(ring[i].y);
            const double bx = to_mm(ring[(i + 1) % n].x), by = to_mm(ring[(i + 1) % n].y);
            const double len = std::hypot(bx - ax, by - ay);
            for (double t = carried; t < len; t += step)
                candidates.push_back({ax + (bx - ax) * t / len, ay + (by - ay) * t / len});
            carried = std::fmod(carried + step - std::fmod(len, step), step);
        }
    }
    Clipper2Lib::Rect64 box = Clipper2Lib::GetBounds(overhang);
    const double x0 = std::floor(to_mm(box.left) / step) * step, y0 = std::floor(to_mm(box.top) / step) * step;
    for (double y = y0; y <= to_mm(box.bottom); y += step)
        for (double x = x0; x <= to_mm(box.right); x += step)
            if (region_contains(overhang, x, y))
                candidates.push_back({x, y});
    // Interior points first: a tip in the middle of a wide overhang, then the
    // outline where nothing reaches.
    std::stable_partition(candidates.begin(), candidates.end(),
                          [&](const auto& c) { return nearest_boundary(overhang, c.first, c.second).d > tip_r; });
    for (const auto& [x, y] : candidates) {
        bool covered = false;
        for (const Branch& b : branches)
            if (b.alive && std::hypot(b.node.x - x, b.node.y - y) < spacing * 0.8) {
                covered = true;
                break;
            }
        double cx = x, cy = y;
        if (!covered && keep_clear(forbidden, cx, cy, tip_r, reach))
            branches.push_back({{cx, cy, tip_r}, 0.0, {}, true});
    }
}

} // namespace

double tree_tip_spacing(double tip_diameter, double top_rate, double branch_distance) {
    const double rate = std::clamp(top_rate, 1.0, 100.0) / 15.0;
    return 3.0 * std::pow(rate, -0.27) * std::pow((tip_diameter + 0.8) / 1.6, 0.3) * std::max(branch_distance, 0.1);
}

std::vector<std::vector<TreeNode>> tree_nodes(const std::vector<Polygons>& regions, const std::vector<double>& heights,
                                              const std::vector<Polygons>& overhangs, const TreeParams& params) {
    const std::size_t n = regions.size();
    std::vector<std::vector<TreeNode>> out(n);
    const double tip_r = std::max(params.tip_diameter * 0.5, 0.05);
    const double branch_r = std::max(params.branch_diameter * 0.5, tip_r);
    const double widen = std::tan(std::clamp(params.widen_angle, 0.0, 85.0) * kDegrees);
    const double lean = std::tan(std::clamp(params.angle_preferred, 0.0, 85.0) * kDegrees);
    const double dodge = std::tan(std::clamp(std::max(params.angle, params.angle_preferred), 0.0, 85.0) * kDegrees);
    const std::size_t gap = std::size_t(std::max(params.z_gap_layers, 0));
    const std::size_t interface = std::size_t(std::max(params.interface_layers, 0));
    // How high each layer is over the bed: a branch joins another only if
    // they can meet before reaching it.
    std::vector<double> base(n, 0.0);
    for (std::size_t l = 1; l < n; ++l)
        base[l] = base[l - 1] + (l - 1 < heights.size() ? heights[l - 1] : 0.0);
    std::vector<Branch> branches;
    const double reach_range = 10.0 * std::max(params.branch_diameter, 1.0); // branches farther apart stay apart
    for (std::size_t l = n; l-- > 0;) {
        // The tips under the interface of the overhang gap + interface + 1
        // layers up, off the part at this layer: one under a hole's rim, say,
        // may move as far as its own width to stand clear of it.
        if (const std::size_t a = l + 1 + gap + interface; a < n && a < overhangs.size() && !overhangs[a].empty()) {
            const Polygons forbidden = regions[l].empty() ? Polygons{} : offset(regions[l], params.xy_gap);
            add_tips(overhangs[a], forbidden, params.tip_spacing, tip_r, 2.0 * tip_r, branches);
        }
        for (Branch& b : branches) {
            if (!b.alive)
                continue;
            b.recent.push_back({l, out[l].size()});
            if (b.recent.size() > gap + 1)
                b.recent.erase(b.recent.begin());
            out[l].push_back(b.node);
        }
        if (l == 0)
            break;
        // Down a layer: lean towards the nearest branch that can be met before
        // the bed, join where they touch, widen, and keep off the part.
        const double down = l - 1 < heights.size() ? heights[l - 1] : 0.0;
        const double step = down * lean;
        std::vector<std::size_t> alive;
        for (std::size_t i = 0; i < branches.size(); ++i)
            if (branches[i].alive)
                alive.push_back(i);
        // A grid of cells of the reach, for the nearest neighbours.
        std::unordered_map<std::int64_t, std::vector<std::size_t>> cells;
        const auto cell_of = [&](double x, double y) {
            return (std::int64_t(std::floor(x / reach_range)) << 32) ^ (std::int64_t(std::floor(y / reach_range)) & 0xffffffff);
        };
        for (const std::size_t i : alive)
            cells[cell_of(branches[i].node.x, branches[i].node.y)].push_back(i);
        std::vector<std::pair<double, double>> moves(branches.size(), {0.0, 0.0});
        for (const std::size_t i : alive) {
            const TreeNode& a = branches[i].node;
            double best = std::numeric_limits<double>::infinity();
            std::size_t partner = branches.size();
            const std::int64_t cx = std::int64_t(std::floor(a.x / reach_range)), cy = std::int64_t(std::floor(a.y / reach_range));
            for (std::int64_t gx = cx - 1; gx <= cx + 1; ++gx)
                for (std::int64_t gy = cy - 1; gy <= cy + 1; ++gy) {
                    const auto it = cells.find((gx << 32) ^ (gy & 0xffffffff));
                    if (it == cells.end())
                        continue;
                    for (const std::size_t j : it->second) {
                        if (j == i)
                            continue;
                        const double d = std::hypot(branches[j].node.x - a.x, branches[j].node.y - a.y);
                        if (d < best || (d == best && j < partner))
                            best = d, partner = j;
                    }
                }
            // They meet halfway: each covers half the distance, before the bed.
            if (partner == branches.size() || best > reach_range || best * 0.5 > base[l] * lean + 1e-9)
                continue;
            const double go = std::min(step, best * 0.5);
            const TreeNode& b = branches[partner].node;
            moves[i] = {(b.x - a.x) / best * go, (b.y - a.y) / best * go};
        }
        for (const std::size_t i : alive) {
            branches[i].node.x += moves[i].first;
            branches[i].node.y += moves[i].second;
        }
        // Join the branches that now overlap: their area kept, at their centre.
        std::sort(alive.begin(), alive.end(), [&](std::size_t p, std::size_t q) {
            return branches[p].node.r > branches[q].node.r || (branches[p].node.r == branches[q].node.r && p < q);
        });
        for (std::size_t u = 0; u < alive.size(); ++u) {
            Branch& a = branches[alive[u]];
            if (!a.alive)
                continue;
            for (std::size_t v = u + 1; v < alive.size(); ++v) {
                Branch& b = branches[alive[v]];
                if (!b.alive)
                    continue;
                const double d = std::hypot(a.node.x - b.node.x, a.node.y - b.node.y);
                if (d > std::max(a.node.r, b.node.r))
                    continue;
                const double wa = a.node.r * a.node.r, wb = b.node.r * b.node.r;
                a.node.x = (a.node.x * wa + b.node.x * wb) / (wa + wb);
                a.node.y = (a.node.y * wa + b.node.y * wb) / (wa + wb);
                a.node.r = std::sqrt(wa + wb); // as thick as both together, as trees grow
                a.grown = std::max(a.grown, b.grown);
                b.alive = false;
            }
        }
        // Widen: a tip quickly to a branch, then by the widening angle.
        const Polygons below = regions[l - 1].empty() ? Polygons{} : offset(regions[l - 1], params.xy_gap);
        for (Branch& b : branches) {
            if (!b.alive)
                continue;
            b.grown += down;
            b.node.r = b.node.r < branch_r ? std::min(branch_r, b.node.r + down) : b.node.r + down * widen;
            if (keep_clear(below, b.node.x, b.node.y, b.node.r, down * dodge))
                continue;
            // As thin as a tip, it may still pass.
            const double r = b.node.r;
            b.node.r = tip_r;
            if (keep_clear(below, b.node.x, b.node.y, tip_r, down * dodge))
                continue;
            b.node.r = r;
            // It rests on the part: its lowest `gap` layers, right over the
            // part, stay empty.
            b.alive = false;
            for (std::size_t k = 0; k < std::min(gap, b.recent.size()); ++k) {
                const auto [layer, index] = b.recent[b.recent.size() - 1 - k];
                out[layer][index].r = 0.0;
            }
        }
        std::erase_if(branches, [](const Branch& b) { return !b.alive; });
    }
    for (auto& layer : out)
        std::erase_if(layer, [](const TreeNode& t) { return !(t.r > 0.0); });
    return out;
}

std::vector<Polygons> tree_support_areas(const std::vector<Polygons>& regions,
                                         const std::vector<std::vector<TreeNode>>& nodes,
                                         const std::vector<Polygons>& overhangs, const TreeParams& params) {
    std::vector<Polygons> out(nodes.size());
    const std::size_t gap = std::size_t(std::max(params.z_gap_layers, 0));
    tbb::parallel_for(std::size_t(0), nodes.size(), [&](std::size_t l) {
        Polygons discs;
        for (int k = 1; k <= params.interface_layers; ++k)
            if (const std::size_t a = l + gap + std::size_t(k); a < overhangs.size())
                for (const Polygon& p : overhangs[a])
                    discs.push_back(p);
        if (nodes[l].empty() && discs.empty())
            return;
        for (const TreeNode& t : nodes[l]) {
            const int sides = std::clamp(int(std::ceil(2.0 * std::numbers::pi * t.r / 0.4)), 12, 64);
            Polygon disc;
            for (int k = 0; k < sides; ++k) {
                const double a = 2.0 * std::numbers::pi * k / sides;
                disc.push_back({to_units(t.x + t.r * std::cos(a)), to_units(t.y + t.r * std::sin(a))});
            }
            discs.push_back(std::move(disc));
        }
        Polygons area = Clipper2Lib::Union(discs, Clipper2Lib::FillRule::NonZero);
        if (l < regions.size() && !regions[l].empty())
            area = Clipper2Lib::Difference(area, offset(regions[l], params.xy_gap), Clipper2Lib::FillRule::NonZero);
        out[l] = std::move(area);
    });
    return out;
}

TreeFill tree_fill(const Polygons& area, const Polygons& above, double bead, double spacing, double double_wall,
                   double angle) {
    TreeFill out;
    if (area.empty())
        return out;
    // The interface: dense lines all over, no wall (rule 17).
    const Polygons dense = above.empty() ? Polygons{} : Clipper2Lib::Intersect(area, above, Clipper2Lib::FillRule::NonZero);
    const Polygons branches = dense.empty() ? area : Clipper2Lib::Difference(area, above, Clipper2Lib::FillRule::NonZero);
    if (!dense.empty())
        out.interface = rectilinear_fill(dense, bead, angle + 45.0, bead, true);
    for (const Polygon& ring : offset(branches, -bead / 2.0))
        if (ring.size() > 2)
            out.support.push_back(ring_path(ring, bead));
    Polygons inside = offset(branches, -bead);
    // Where a disc of double_wall fits (a round branch that wide), a second wall.
    if (double_wall > 0.0 && !inside.empty()) {
        const Polygons wide = offset(offset(branches, -double_wall / 2.0), double_wall / 2.0);
        if (!wide.empty()) {
            for (const Polygon& ring : offset(wide, -bead * 1.5))
                if (ring.size() > 2)
                    out.support.push_back(ring_path(ring, bead));
            inside = Clipper2Lib::Union(Clipper2Lib::Difference(inside, offset(wide, -bead), Clipper2Lib::FillRule::NonZero),
                                        offset(wide, -bead * 2.0), Clipper2Lib::FillRule::NonZero);
        }
    }
    for (Toolpath& t : rectilinear_fill(inside, spacing, angle, bead, true))
        out.support.push_back(std::move(t));
    return out;
}

} // namespace tisma
