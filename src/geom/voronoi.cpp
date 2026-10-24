// Topology-oriented incremental Voronoi diagram (Sugihara and Iri 1992; Held
// and Huber 2009, Section 2; Huber 2008, Chapter 2). Own implementation; see
// PROVENANCE.md.

#include "geom/voronoi.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>
#include <utility>

namespace tisma {
namespace {

constexpr double kDummyScale = 10.0; // dummy sites this many box radii from the centre
constexpr double kFarScale = 1000.0; // far nodes this many box radii away

double dist2(double ax, double ay, double bx, double by) {
    const double dx = ax - bx, dy = ay - by;
    return dx * dx + dy * dy;
}

} // namespace

std::int32_t VoronoiDiagram::new_node(double x, double y, double r) {
    std::int32_t n;
    if (!free_nodes_.empty()) {
        n = free_nodes_.back();
        free_nodes_.pop_back();
        nodes_[std::size_t(n)] = Node{};
        marked_[std::size_t(n)] = 0;
    } else {
        n = std::int32_t(nodes_.size());
        nodes_.emplace_back();
        marked_.push_back(0);
    }
    Node& node = nodes_[std::size_t(n)];
    node.x = x, node.y = y, node.r = r;
    return n;
}

// A new edge from node a to node b: the half-edge a -> b has `left` on its
// left; its twin b -> a has `right`. Links are set by the caller.
std::int32_t VoronoiDiagram::new_edge(std::int32_t a, std::int32_t b, std::int32_t left, std::int32_t right) {
    std::int32_t h;
    if (!free_edges_.empty()) {
        h = free_edges_.back();
        free_edges_.pop_back();
    } else {
        h = std::int32_t(halves_.size());
        halves_.emplace_back();
        halves_.emplace_back();
    }
    halves_[std::size_t(h)] = Half{a, -1, -1, left, true};
    halves_[std::size_t(twin(h))] = Half{b, -1, -1, right, true};
    return h;
}

void VoronoiDiagram::link(std::int32_t h, std::int32_t next) {
    halves_[std::size_t(h)].next = next;
    halves_[std::size_t(next)].prev = h;
}

std::size_t VoronoiDiagram::grid_cell(double x, double y) const {
    const double gx = std::clamp((x - grid_x_) * grid_scale_, 0.0, double(kGrid - 1));
    const double gy = std::clamp((y - grid_y_) * grid_scale_, 0.0, double(kGrid - 1));
    return std::size_t(gy) * kGrid + std::size_t(gx);
}

VoronoiDiagram::VoronoiDiagram(double min_x, double min_y, double max_x, double max_y) {
    grid_x_ = min_x, grid_y_ = min_y;
    grid_scale_ = double(kGrid) / std::max({max_x - min_x, max_y - min_y, 1e-300});
    grid_.assign(std::size_t(kGrid) * kGrid, -1);
    const double cx = (min_x + max_x) / 2.0, cy = (min_y + max_y) / 2.0;
    const double radius = std::max(std::hypot(max_x - min_x, max_y - min_y) / 2.0, 1.0);
    // Three dummy sites counter-clockwise at 90, 210 and 330 degrees; the
    // centre is their one node, with a ray to a far node between each pair.
    const double dummy[3] = {90.0, 210.0, 330.0};
    for (double a : dummy) {
        const double t = a * std::numbers::pi / 180.0;
        sites_.push_back({SiteKind::Dummy, cx + kDummyScale * radius * std::cos(t),
                          cy + kDummyScale * radius * std::sin(t), -1});
    }
    marked_count_.assign(3, 0);
    face_in_.assign(3, -1);
    face_out_.assign(3, -1);
    new_edge_of_.assign(3, -1);
    const std::int32_t centre = new_node(cx, cy, kDummyScale * radius);
    std::int32_t far[3];
    for (int i = 0; i < 3; ++i) { // the ray between dummy i and dummy i + 1, at the angle between them
        const double t = (dummy[i] + 60.0) * std::numbers::pi / 180.0;
        const double fx = cx + kFarScale * radius * std::cos(t), fy = cy + kFarScale * radius * std::sin(t);
        far[i] = new_node(fx, fy, std::sqrt(dist2(fx, fy, sites_[std::size_t(i)].x, sites_[std::size_t(i)].y)));
        nodes_[std::size_t(far[i])].fixed = true;
    }
    std::int32_t ray[3], outer[3];
    for (int i = 0; i < 3; ++i) {
        const int j = (i + 1) % 3;
        // Going out along the ray between i and j, dummy j is on the left.
        ray[i] = new_edge(centre, far[i], j, i);
        // Going counter-clockwise from far[i] to far[j], the cell of j is inside (left).
        outer[i] = new_edge(far[i], far[j], j, -1);
    }
    // Links, around each cell counter-clockwise.
    for (int i = 0; i < 3; ++i) {
        const int j = (i + 1) % 3;
        // Cell of j: out along ray i, along the outside from far[i] to far[j], back along ray j.
        link(ray[i], outer[i]);
        link(outer[i], twin(ray[j]));
        link(twin(ray[j]), ray[i]);
        // Outside: far[j] -> far[i], clockwise round the whole.
        link(twin(outer[j]), twin(outer[i]));
        sites_[std::size_t(j)].half = ray[i];
    }
    nodes_[std::size_t(centre)].half = ray[0];
    for (int i = 0; i < 3; ++i)
        nodes_[std::size_t(far[i])].half = twin(ray[i]);
}

double VoronoiDiagram::site_distance2(std::int32_t site, double x, double y) const {
    const Site& s = sites_[std::size_t(site)];
    return dist2(s.x, s.y, x, y);
}

// Greedy walk over neighbouring cells towards (x, y): on the Delaunay graph of
// points it ends at the nearest site.
std::int32_t VoronoiDiagram::nearest_site(double x, double y, std::int32_t start) const {
    std::int32_t site = start;
    double best = site_distance2(site, x, y);
    for (bool moved = true; moved;) {
        moved = false;
        const std::int32_t first = sites_[std::size_t(site)].half;
        std::int32_t h = first;
        do {
            const std::int32_t other = halves_[std::size_t(twin(h))].site;
            if (other >= 0) {
                const double d = site_distance2(other, x, y);
                if (d < best) {
                    best = d, site = other, moved = true;
                    break;
                }
            }
            h = halves_[std::size_t(h)].next;
        } while (h != first);
    }
    return site;
}

// Whether node w, next to the marked node `from`, may be marked for deletion
// when (x, y) is inserted: its clearance disk holds the new point, and marking
// it keeps the marked nodes a tree, keeps a node in every cell around it, and
// keeps the marked nodes of every cell contiguous (Sugihara and Iri 1992).
bool VoronoiDiagram::can_mark(std::int32_t w, std::int32_t from, double x, double y) const {
    const Node& node = nodes_[std::size_t(w)];
    if (node.fixed || marked_[std::size_t(w)] || dist2(node.x, node.y, x, y) >= node.r * node.r)
        return false;
    std::int32_t out[3];
    out[0] = node.half;
    out[1] = clockwise(out[0]);
    out[2] = clockwise(out[1]);
    int towards = -1; // the half-edge from w to `from`
    for (int i = 0; i < 3; ++i) {
        const std::int32_t d = dest(out[i]);
        if (d == from)
            towards = i;
        else if (marked_[std::size_t(d)])
            return false; // a cycle
    }
    if (towards < 0)
        return false;
    for (int i = 0; i < 3; ++i) {
        const std::int32_t face = halves_[std::size_t(out[i])].site;
        if (face < 0)
            continue;
        // The cell between out[i] and the next half counter-clockwise; out
        // runs clockwise, so the cells holding the edge to `from` are the ones
        // left of out[towards] and left of its counter-clockwise neighbour.
        const bool holds_edge = i == towards || i == (towards + 1) % 3;
        if (!holds_edge && marked_count_[std::size_t(face)] > 0)
            return false; // a second run of marked nodes in that cell
        bool kept = false;
        const std::int32_t first = out[i];
        for (std::int32_t h = halves_[std::size_t(first)].next; h != first; h = halves_[std::size_t(h)].next)
            if (!marked_[std::size_t(halves_[std::size_t(h)].origin)]) {
                kept = true;
                break;
            }
        if (!kept)
            return false;
    }
    return true;
}

std::int32_t VoronoiDiagram::insert_point(double x, double y) {
    const std::size_t cell = grid_cell(x, y);
    const std::int32_t near = nearest_site(x, y, grid_[cell] >= 0 ? grid_[cell] : last_);
    if (site_distance2(near, x, y) == 0.0)
        throw std::invalid_argument("VoronoiDiagram: the point is already a site");
    const std::int32_t site = std::int32_t(sites_.size());
    sites_.push_back({SiteKind::Point, x, y, -1});
    marked_count_.push_back(0);
    face_in_.push_back(-1);
    face_out_.push_back(-1);
    new_edge_of_.push_back(-1);

    // Seed: the node of the nearest cell whose clearance disk the point violates most.
    std::int32_t seed = -1;
    double worst = 0.0;
    {
        const std::int32_t first = sites_[std::size_t(near)].half;
        std::int32_t h = first;
        do {
            const Node& n = nodes_[std::size_t(halves_[std::size_t(h)].origin)];
            if (!n.fixed) {
                const double v = std::sqrt(dist2(n.x, n.y, x, y)) - n.r;
                if (seed < 0 || v < worst)
                    seed = halves_[std::size_t(h)].origin, worst = v;
            }
            h = halves_[std::size_t(h)].next;
        } while (h != first);
    }
    if (seed < 0)
        throw std::logic_error("VoronoiDiagram: no seed node");

    // Mark the tree of nodes to delete.
    std::vector<std::int32_t>& marked = scratch_marked_;
    std::vector<std::int32_t>& touched = scratch_touched_;
    marked.clear(), touched.clear();
    auto mark = [&](std::int32_t n) {
        marked_[std::size_t(n)] = 1;
        marked.push_back(n);
        std::int32_t h = nodes_[std::size_t(n)].half;
        for (int i = 0; i < 3; ++i, h = clockwise(h)) {
            const std::int32_t face = halves_[std::size_t(h)].site;
            if (face >= 0) {
                if (marked_count_[std::size_t(face)]++ == 0)
                    touched.push_back(face);
            }
        }
    };
    mark(seed);
    for (std::size_t k = 0; k < marked.size(); ++k) {
        const std::int32_t u = marked[k];
        std::int32_t h = nodes_[std::size_t(u)].half;
        for (int i = 0; i < 3; ++i, h = clockwise(h)) {
            const std::int32_t w = dest(h);
            if (can_mark(w, u, x, y))
                mark(w);
        }
    }

    // Each half-edge leaving a marked node towards an unmarked one gets a new
    // node where the new point is as far as the edge's two sites; the edge
    // is straight (both sites are points), and the difference of squared
    // distances is linear along it.
    std::vector<std::int32_t>& doomed = scratch_doomed_;
    doomed.clear();
    std::vector<std::pair<std::int32_t, std::int32_t>>& runs = scratch_runs_; // per cell: (in, out)
    for (std::int32_t face : touched)
        face_in_[std::size_t(face)] = face_out_[std::size_t(face)] = -1;
    for (std::int32_t u : marked) {
        std::int32_t h = nodes_[std::size_t(u)].half;
        std::int32_t out[3] = {h, clockwise(h), clockwise(clockwise(h))};
        for (std::int32_t o : out) {
            const std::int32_t w = dest(o);
            if (marked_[std::size_t(w)]) {
                doomed.push_back(o);
                continue;
            }
            const Node& a = nodes_[std::size_t(u)];
            const Node& b = nodes_[std::size_t(w)];
            const Site& s = sites_[std::size_t(halves_[std::size_t(o)].site)];
            const double fa = dist2(a.x, a.y, x, y) - dist2(a.x, a.y, s.x, s.y);
            const double fb = dist2(b.x, b.y, x, y) - dist2(b.x, b.y, s.x, s.y);
            const double t = fa < fb ? std::clamp(fa / (fa - fb), 0.0, 1.0) : 0.5;
            const double nx = a.x + t * (b.x - a.x), ny = a.y + t * (b.y - a.y);
            const std::int32_t n = new_node(nx, ny, std::sqrt(dist2(nx, ny, x, y)));
            halves_[std::size_t(o)].origin = n; // the edge now runs from the new node
            nodes_[std::size_t(n)].half = o;
            // o leaves the run of marked nodes in the cell on its left; its
            // twin enters the run in the cell on its right.
            const std::int32_t left = halves_[std::size_t(o)].site;
            const std::int32_t right = halves_[std::size_t(twin(o))].site;
            face_out_[std::size_t(left)] = o;
            face_in_[std::size_t(right)] = twin(o);
        }
    }
    // A new edge in every cell that had marked nodes, from the node where the
    // run starts to the node where it ends; its twin bounds the new cell.
    runs.clear();
    for (std::int32_t face : touched) {
        const std::int32_t in = face_in_[std::size_t(face)], out = face_out_[std::size_t(face)];
        if (in < 0 || out < 0)
            throw std::logic_error("VoronoiDiagram: a cell lost its run ends");
        const std::int32_t g = new_edge(dest(in), halves_[std::size_t(out)].origin, face, site);
        link(in, g);
        link(g, out);
        new_edge_of_[std::size_t(face)] = g;
        sites_[std::size_t(face)].half = out;
        runs.emplace_back(face, g);
    }
    for (const auto& [face, g] : runs) {
        // Around the new cell: after g's twin (which ends at the start of the
        // run in `face`) comes the new edge of the cell across the edge that
        // carries that node.
        const std::int32_t in = face_in_[std::size_t(face)];
        const std::int32_t across = halves_[std::size_t(twin(in))].site;
        link(twin(g), twin(new_edge_of_[std::size_t(across)]));
    }
    sites_[std::size_t(site)].half = twin(runs.front().second);

    for (std::int32_t o : doomed) {
        if (!halves_[std::size_t(o)].alive)
            continue;
        halves_[std::size_t(o)].alive = halves_[std::size_t(twin(o))].alive = false;
        free_edges_.push_back(o & ~1);
    }
    for (std::int32_t u : marked) {
        nodes_[std::size_t(u)].alive = false;
        marked_[std::size_t(u)] = 0;
        free_nodes_.push_back(u);
    }
    for (std::int32_t face : touched)
        marked_count_[std::size_t(face)] = 0;
    last_ = site;
    grid_[cell] = site;
    return site;
}

std::string VoronoiDiagram::check() const {
    for (std::size_t h = 0; h < halves_.size(); ++h) {
        const Half& e = halves_[h];
        if (!e.alive)
            continue;
        const std::string at = " at half-edge " + std::to_string(h);
        if (!halves_[h ^ 1].alive)
            return "dead twin" + at;
        if (e.next < 0 || e.prev < 0 || !halves_[std::size_t(e.next)].alive || !halves_[std::size_t(e.prev)].alive)
            return "dead or missing link" + at;
        if (halves_[std::size_t(e.next)].prev != std::int32_t(h) || halves_[std::size_t(e.prev)].next != std::int32_t(h))
            return "next and prev disagree" + at;
        if (halves_[std::size_t(e.next)].site != e.site)
            return "a cell changes along its boundary" + at;
        if (dest(std::int32_t(h)) != halves_[std::size_t(e.next)].origin)
            return "next does not start where the half-edge ends" + at;
        if (!nodes_[std::size_t(e.origin)].alive)
            return "dead origin" + at;
    }
    for (std::size_t n = 0; n < nodes_.size(); ++n) {
        if (!nodes_[n].alive)
            continue;
        const std::int32_t h = nodes_[n].half;
        if (h < 0 || !halves_[std::size_t(h)].alive || halves_[std::size_t(h)].origin != std::int32_t(n))
            return "bad node half-edge at node " + std::to_string(n);
        const std::int32_t h3 = clockwise(clockwise(clockwise(h)));
        if (h3 != h)
            return "node of degree other than three at node " + std::to_string(n);
    }
    for (std::size_t s = 0; s < sites_.size(); ++s) {
        const std::int32_t h = sites_[s].half;
        if (h < 0 || !halves_[std::size_t(h)].alive || halves_[std::size_t(h)].site != std::int32_t(s))
            return "bad cell half-edge at site " + std::to_string(s);
    }
    return {};
}

} // namespace tisma
