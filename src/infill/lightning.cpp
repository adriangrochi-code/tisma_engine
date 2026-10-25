// Lightning infill, after Tricard, Claux and Lefebvre 2020; own design. See
// PROVENANCE.md.

#include "infill/lightning.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <numbers>

namespace tisma {

namespace {

using Clipper2Lib::Point64;

struct Node {
    Point2d p;
    int parent = kRoot;
    static constexpr int kRoot = -1;   // on the wall
    static constexpr int kLoose = -2;  // its branch lost its way to the wall: to join again
};

double distance(Point2d a, Point2d b) { return std::hypot(a.x - b.x, a.y - b.y); }

// The point of segment ab nearest to p, and where along it (0 at a, 1 at b).
Point2d nearest_on(Point2d p, Point2d a, Point2d b, double& t) {
    const double dx = b.x - a.x, dy = b.y - a.y, l2 = dx * dx + dy * dy;
    t = l2 > 0.0 ? std::clamp(((p.x - a.x) * dx + (p.y - a.y) * dy) / l2, 0.0, 1.0) : 0.0;
    return {a.x + t * dx, a.y + t * dy};
}

Point64 units(Point2d p) { return {to_units(p.x), to_units(p.y)}; }

// Inside the region: inside an odd number of its paths (outlines hold their holes).
bool inside(const Polygons& region, Point2d p) {
    const Point64 q = units(p);
    int count = 0;
    for (const Polygon& path : region)
        count += Clipper2Lib::PointInPolygon(q, path) != Clipper2Lib::PointInPolygonResult::IsOutside;
    return count % 2 == 1;
}

// The point of the region's boundary nearest to p.
Point2d nearest_boundary(const Polygons& region, Point2d p, double& best) {
    best = std::numeric_limits<double>::infinity();
    Point2d out = p;
    for (const Polygon& path : region)
        for (std::size_t i = 0; i < path.size(); ++i) {
            const Point64& a = path[i];
            const Point64& b = path[(i + 1) % path.size()];
            double t = 0.0;
            const Point2d q = nearest_on(p, {to_mm(a.x), to_mm(a.y)}, {to_mm(b.x), to_mm(b.y)}, t);
            if (const double d = distance(p, q); d < best) {
                best = d;
                out = q;
            }
        }
    return out;
}

// Whether segment ab stays inside the region all along.
bool stays_inside(const Polygons& region, Point2d a, Point2d b) {
    Clipper2Lib::Clipper64 clipper;
    clipper.AddOpenSubject({{units(a), units(b)}});
    clipper.AddClip(region);
    Polygons closed, open;
    clipper.Execute(Clipper2Lib::ClipType::Intersection, Clipper2Lib::FillRule::NonZero, closed, open);
    if (open.size() != 1 || open.front().size() != 2)
        return false;
    const Point64& p = open.front()[0];
    const Point64& q = open.front()[1];
    return std::abs(std::hypot(to_mm(q.x - p.x), to_mm(q.y - p.y)) - distance(a, b)) < 1e-3;
}

// Takes out the nodes marked dead, keeping the parents' indices right.
void compact(std::vector<Node>& nodes, const std::vector<char>& alive) {
    std::vector<int> to(nodes.size(), Node::kLoose);
    std::size_t kept = 0;
    for (std::size_t i = 0; i < nodes.size(); ++i)
        if (alive[i] != 0)
            to[i] = int(kept++);
    std::vector<Node> out;
    out.reserve(kept);
    for (std::size_t i = 0; i < nodes.size(); ++i)
        if (alive[i] != 0) {
            Node n = nodes[i];
            if (n.parent >= 0)
                n.parent = to[std::size_t(n.parent)];
            out.push_back(n);
        }
    nodes = std::move(out);
}

std::vector<int> child_counts(const std::vector<Node>& nodes) {
    std::vector<int> count(nodes.size(), 0);
    for (const Node& n : nodes)
        if (n.parent >= 0)
            ++count[std::size_t(n.parent)];
    return count;
}

// Pulls every tip back along its branch by `by` mm; a branch shorter than
// that goes, and so does a wall's root left with no branch.
void pull_back(std::vector<Node>& nodes, double by) {
    std::vector<int> children = child_counts(nodes);
    std::vector<char> alive(nodes.size(), 1);
    std::vector<std::size_t> tips;
    for (std::size_t i = 0; i < nodes.size(); ++i)
        if (children[i] == 0)
            tips.push_back(i);
    for (std::size_t tip : tips) {
        std::size_t at = tip;
        double left = by;
        for (;;) {
            Node& n = nodes[at];
            if (n.parent < 0) {
                alive[at] = 0; // a root or a loose point with nothing on it
                break;
            }
            const auto up = std::size_t(n.parent);
            const double e = distance(n.p, nodes[up].p);
            if (e > left) {
                const double f = left / e;
                n.p = {n.p.x + f * (nodes[up].p.x - n.p.x), n.p.y + f * (nodes[up].p.y - n.p.y)};
                break;
            }
            alive[at] = 0;
            left -= e;
            if (--children[up] > 0)
                break;
            at = up;
        }
    }
    compact(nodes, alive);
}

// Which nodes reach the wall through their parents.
std::vector<char> anchored(const std::vector<Node>& nodes) {
    std::vector<char> state(nodes.size(), 2); // 2: not known yet
    std::vector<std::size_t> path;
    for (std::size_t i = 0; i < nodes.size(); ++i) {
        std::size_t at = i;
        path.clear();
        while (state[at] == 2) {
            const int up = nodes[at].parent;
            if (up < 0) {
                state[at] = up == Node::kRoot;
                break;
            }
            path.push_back(at);
            at = std::size_t(up);
        }
        for (std::size_t p : path)
            state[p] = state[at];
    }
    return state;
}

// Joins node i to the nearest anchored branch, or to the wall if that is
// nearer or the way to the branch leaves the region.
void join(std::vector<Node>& nodes, std::vector<char>& held, std::size_t i, const Polygons& region) {
    double wall = 0.0;
    const Point2d p = nodes[i].p;
    const Point2d on_wall = nearest_boundary(region, p, wall);
    double best = wall, best_t = 0.0;
    std::size_t edge = nodes.size();
    Point2d on_branch{};
    for (std::size_t c = 0; c < nodes.size(); ++c) {
        if (c == i || held[c] == 0 || nodes[c].parent < 0)
            continue;
        double t = 0.0;
        const Point2d q = nearest_on(p, nodes[c].p, nodes[std::size_t(nodes[c].parent)].p, t);
        if (const double d = distance(p, q); d < best) {
            best = d;
            best_t = t;
            edge = c;
            on_branch = q;
        }
    }
    if (edge < nodes.size() && stays_inside(region, p, on_branch)) {
        const auto up = std::size_t(nodes[edge].parent);
        std::size_t to = 0;
        if (best_t < 1e-9) {
            to = edge;
        } else if (best_t > 1.0 - 1e-9) {
            to = up;
        } else {
            // A fork on the branch.
            nodes.push_back({on_branch, int(up)});
            held.push_back(1);
            to = nodes.size() - 1;
            nodes[edge].parent = int(to);
        }
        nodes[i].parent = int(to);
    } else {
        nodes.push_back({on_wall, Node::kRoot});
        held.push_back(1);
        nodes[i].parent = int(nodes.size() - 1);
    }
}

// How far p is from the anchored branches.
double from_branches(const std::vector<Node>& nodes, const std::vector<char>& held, Point2d p) {
    double best = std::numeric_limits<double>::infinity();
    for (std::size_t c = 0; c < nodes.size(); ++c)
        if (held[c] != 0 && nodes[c].parent >= 0) {
            double t = 0.0;
            best = std::min(best, distance(p, nearest_on(p, nodes[c].p, nodes[std::size_t(nodes[c].parent)].p, t)));
        }
    return best;
}

// The branches as polylines from tips and forks up to the next fork or the
// wall, clipped to the region shrunk by half a bead.
std::vector<Toolpath> branches(const std::vector<Node>& nodes, const Polygons& region, double width) {
    const std::vector<int> children = child_counts(nodes);
    Polygons chains;
    for (std::size_t i = 0; i < nodes.size(); ++i) {
        if (children[i] == 1 || nodes[i].parent < 0)
            continue;
        Polygon chain{units(nodes[i].p)};
        std::size_t at = i;
        do {
            at = std::size_t(nodes[at].parent);
            chain.push_back(units(nodes[at].p));
        } while (nodes[at].parent >= 0 && children[at] == 1);
        chains.push_back(std::move(chain));
    }
    std::vector<Toolpath> out;
    const Polygons inner = offset(region, -width / 2.0 + 1e-5);
    if (chains.empty() || inner.empty())
        return out;
    Clipper2Lib::Clipper64 clipper;
    clipper.AddOpenSubject(chains);
    clipper.AddClip(inner);
    Polygons closed, open;
    clipper.Execute(Clipper2Lib::ClipType::Intersection, Clipper2Lib::FillRule::NonZero, closed, open);
    for (const Polygon& piece : open) {
        Toolpath t;
        double length = 0.0;
        for (std::size_t k = 0; k < piece.size(); ++k) {
            const Point2d q{to_mm(piece[k].x), to_mm(piece[k].y)};
            if (k > 0)
                length += distance(q, t.points.back().p);
            t.points.push_back({q, width});
        }
        if (t.points.size() >= 2 && length >= width)
            out.push_back(std::move(t));
    }
    return out;
}

} // namespace

double lightning_reach(double width, double density) { return density > 0.0 ? width / density : 0.0; }

std::vector<std::vector<Toolpath>> lightning_layers(const std::vector<Polygons>& sparse,
                                                    const std::vector<Polygons>& regions,
                                                    const std::vector<Layer>& layers, const LightningParams& params,
                                                    const std::vector<Polygons>* print) {
    const std::size_t count = std::min({sparse.size(), regions.size(), layers.size()});
    std::vector<std::vector<Toolpath>> out(count);
    if (!(params.reach > 0.0))
        return out;
    const double lean = std::tan(std::clamp(params.overhang, 0.0, 85.0) * std::numbers::pi / 180.0);
    // Points to hold up on a square grid fixed to the origin, `reach` apart: a
    // point lies at most reach / sqrt(2) from one. A point gets a branch unless
    // one passes within the rest of the reach, so every point to hold up has a
    // branch within the reach.
    const double grid = params.reach;
    const double near = params.reach * (1.0 - 1.0 / std::numbers::sqrt2);
    std::vector<Node> nodes;
    for (std::size_t l = count; l-- > 0;) {
        const Polygons& region = sparse[l];
        if (region.empty()) {
            nodes.clear();
            continue;
        }
        if (l + 1 < count)
            pull_back(nodes, (layers[l + 1].top - layers[l + 1].bottom) * lean);
        // What lies outside this layer's sparse infill goes; branches cut off
        // from their wall, and roots no longer at a wall, join again.
        {
            std::vector<char> alive(nodes.size(), 1);
            for (std::size_t i = 0; i < nodes.size(); ++i) {
                if (nodes[i].parent == Node::kRoot) {
                    double d = 0.0;
                    nearest_boundary(region, nodes[i].p, d);
                    if (d <= params.width / 2.0)
                        continue;
                    // The wall moved out (the root joins again) or in past it.
                    if (inside(region, nodes[i].p))
                        nodes[i].parent = Node::kLoose;
                    else
                        alive[i] = 0;
                } else if (!inside(region, nodes[i].p)) {
                    alive[i] = 0;
                }
            }
            for (Node& n : nodes)
                if (n.parent >= 0 && alive[std::size_t(n.parent)] == 0)
                    n.parent = Node::kLoose;
            compact(nodes, alive);
        }
        std::vector<char> held = anchored(nodes);
        for (std::size_t i = 0, n = nodes.size(); i < n; ++i)
            if (nodes[i].parent == Node::kLoose) {
                join(nodes, held, i, region);
                held = anchored(nodes);
            }
        // What the layer above has over this layer's sparse infill.
        if (l + 1 < count) {
            Polygons above = Clipper2Lib::Difference(regions[l + 1], sparse[l + 1], Clipper2Lib::FillRule::NonZero);
            above = Clipper2Lib::Intersect(above, region, Clipper2Lib::FillRule::NonZero);
            above = offset(offset(above, -params.width / 2.0), params.width / 2.0);
            if (!above.empty()) {
                const Clipper2Lib::Rect64 box = Clipper2Lib::GetBounds(above);
                struct Need {
                    Point2d p;
                    double wall;
                };
                std::vector<Need> points;
                const auto first_row = std::int64_t(std::ceil(to_mm(box.top) / grid));
                const auto first_column = std::int64_t(std::ceil(to_mm(box.left) / grid));
                for (auto j = first_row; double(j) * grid <= to_mm(box.bottom); ++j)
                    for (auto i = first_column; double(i) * grid <= to_mm(box.right); ++i) {
                        const Point2d p{double(i) * grid, double(j) * grid};
                        if (!inside(above, p))
                            continue;
                        double wall = 0.0;
                        nearest_boundary(region, p, wall);
                        points.push_back({p, wall});
                    }
                // The points nearest the walls first: the branches grow in from them.
                std::stable_sort(points.begin(), points.end(),
                                 [](const Need& a, const Need& b) { return a.wall < b.wall; });
                for (const Need& q : points) {
                    if (from_branches(nodes, held, q.p) <= near)
                        continue;
                    nodes.push_back({q.p, Node::kLoose});
                    held.push_back(0);
                    const std::size_t at = nodes.size() - 1;
                    join(nodes, held, at, region);
                    held[at] = 1;
                }
            }
        }
        out[l] = branches(nodes, print != nullptr && l < print->size() ? (*print)[l] : region, params.width);
    }
    return out;
}

} // namespace tisma
