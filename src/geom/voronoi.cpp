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
constexpr double kOnPoint = 1e-9;    // relative to the box radius: a node this close to its sites is on an input point
constexpr int kBisection = 100;      // steps at most when placing a node on an edge
constexpr double kPlace = 1e-15;     // relative to the box radius: how closely a node is placed
constexpr int kSamples = 32;         // samples when an edge shows no clean sign change

double dist2(double ax, double ay, double bx, double by) {
    const double dx = ax - bx, dy = ay - by;
    return dx * dx + dy * dy;
}

// Distance from (x, y) to the closed segment from (ax, ay) to (bx, by).
double segment_distance(double ax, double ay, double bx, double by, double x, double y) {
    const double dx = bx - ax, dy = by - ay, len2 = dx * dx + dy * dy;
    const double t = len2 > 0.0 ? std::clamp(((x - ax) * dx + (y - ay) * dy) / len2, 0.0, 1.0) : 0.0;
    return std::hypot(x - (ax + t * dx), y - (ay + t * dy));
}

} // namespace

std::int32_t VoronoiDiagram::add_site(const Site& s) {
    sites_.push_back(s);
    marked_count_.push_back(0);
    face_in_.push_back(-1);
    face_out_.push_back(-1);
    new_edge_of_.push_back(-1);
    return std::int32_t(sites_.size() - 1);
}

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
    node.on_point = r <= on_point_;
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
    on_point_ = kOnPoint * radius;
    // Three dummy sites counter-clockwise at 90, 210 and 330 degrees; the
    // centre is their one node, with a ray to a far node between each pair.
    const double dummy[3] = {90.0, 210.0, 330.0};
    for (double a : dummy) {
        const double t = a * std::numbers::pi / 180.0;
        Site s;
        s.kind = SiteKind::Dummy;
        s.x = cx + kDummyScale * radius * std::cos(t);
        s.y = cy + kDummyScale * radius * std::sin(t);
        add_site(s);
    }
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

double VoronoiDiagram::distance(std::int32_t site, double x, double y) const {
    const Site& s = sites_[std::size_t(site)];
    if (s.kind == SiteKind::Segment)
        return segment_distance(s.x, s.y, s.x2, s.y2, x, y);
    return std::sqrt(dist2(s.x, s.y, x, y));
}

// Strictly inside a segment's cone of influence: the slab between the lines
// through its endpoints at right angles to it (Held and Huber 2009, 1.4).
bool VoronoiDiagram::in_cone(const Site& s, double x, double y) const {
    const double dx = s.x2 - s.x, dy = s.y2 - s.y;
    return (x - s.x) * dx + (y - s.y) * dy > 0.0 && (x - s.x2) * dx + (y - s.y2) * dy < 0.0;
}

// Whether a site, inserted, takes the point (x, y) of clearance r: a point
// closer than r; a segment closer than r with (x, y) inside its cone of
// influence (outside it the segment's nearest point is an endpoint, which is
// a site already).
bool VoronoiDiagram::violated(double x, double y, double r, std::int32_t site) const {
    const Site& s = sites_[std::size_t(site)];
    if (s.kind != SiteKind::Segment)
        return dist2(s.x, s.y, x, y) < r * r;
    if (!in_cone(s, x, y))
        return false;
    const double dx = s.x2 - s.x, dy = s.y2 - s.y;
    const double line = std::abs(dx * (y - s.y) - dy * (x - s.x)) / std::hypot(dx, dy);
    return line < r;
}

// Greedy walk over neighbouring cells towards (x, y): on the Delaunay graph of
// points it ends at the nearest site. Only used while there are points alone.
std::int32_t VoronoiDiagram::nearest_site(double x, double y, std::int32_t start) const {
    std::int32_t site = start;
    double best = dist2(sites_[std::size_t(site)].x, sites_[std::size_t(site)].y, x, y);
    for (bool moved = true; moved;) {
        moved = false;
        const std::int32_t first = sites_[std::size_t(site)].half;
        std::int32_t h = first;
        do {
            const std::int32_t other = halves_[std::size_t(twin(h))].site;
            if (other >= 0) {
                const double d = dist2(sites_[std::size_t(other)].x, sites_[std::size_t(other)].y, x, y);
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

// Whether node w, next to the marked node `from`, may be marked for deletion:
// the new site violates its clearance disk, and marking it keeps the marked
// nodes a tree and keeps a node in every cell around it (Sugihara and Iri
// 1992; Held and Huber 2009, 2.2). For a new point, the marked nodes of every
// cell also stay contiguous: cells of points are convex. (A new segment may
// wrap round a point's cell and touch a cell twice.)
bool VoronoiDiagram::can_mark(std::int32_t w, std::int32_t from, std::int32_t site) const {
    const Node& node = nodes_[std::size_t(w)];
    if (node.fixed || node.on_point || node.split || marked_[std::size_t(w)] ||
        !violated(node.x, node.y, node.r, site))
        return false;
    const std::int32_t first = node.half;
    std::int32_t towards = -1; // the half-edge from w to `from`
    std::int32_t h = first;
    do {
        const std::int32_t d = dest(h);
        if (d == from)
            towards = h;
        else if (marked_[std::size_t(d)])
            return false; // a cycle
        h = clockwise(h);
    } while (h != first);
    if (towards < 0)
        return false;
    const bool point = sites_[std::size_t(site)].kind != SiteKind::Segment;
    const std::int32_t beside = clockwise(towards); // the cells left of `towards` and of `beside` hold the edge
    h = first;
    do {
        const std::int32_t face = halves_[std::size_t(h)].site;
        if (face >= 0) {
            if (point && h != towards && h != beside && marked_count_[std::size_t(face)] > 0)
                return false; // a second run of marked nodes in that cell
            bool kept = false;
            for (std::int32_t k = halves_[std::size_t(h)].next; k != h; k = halves_[std::size_t(k)].next)
                if (!marked_[std::size_t(halves_[std::size_t(k)].origin)]) {
                    kept = true;
                    break;
                }
            if (!kept)
                return false;
        }
        h = clockwise(h);
    } while (h != first);
    return true;
}

// Where to cut the edge of h so that a new segment does not take all of it
// (Held and Huber 2009, 2.2.3, for segments as in Held 2001): if one side of
// the edge is a point q (not an endpoint of the new segment), the point of
// the edge nearest to q, when it lies inside the edge and the new segment
// does not take it.
bool VoronoiDiagram::split_point(std::int32_t h, std::int32_t site, double& px, double& py) const {
    const Site& s = sites_[std::size_t(site)];
    const Node& u = nodes_[std::size_t(halves_[std::size_t(h)].origin)];
    const Node& w = nodes_[std::size_t(dest(h))];
    const std::int32_t sides[2] = {halves_[std::size_t(h)].site, halves_[std::size_t(twin(h))].site};
    for (int k = 0; k < 2; ++k) {
        const std::int32_t q = sides[k], other = sides[1 - k];
        if (q < 0 || other < 0 || q == s.a || q == s.b)
            continue;
        const Site& sq = sites_[std::size_t(q)];
        const Site& so = sites_[std::size_t(other)];
        if (sq.kind == SiteKind::Segment)
            continue;
        double cx, cy;
        if (so.kind != SiteKind::Segment) {
            // Two points: their bisector comes closest to q at their midpoint.
            const double mx = (sq.x + so.x) / 2.0, my = (sq.y + so.y) / 2.0;
            const double ex = w.x - u.x, ey = w.y - u.y, len2 = ex * ex + ey * ey;
            const double t = len2 > 0.0 ? ((mx - u.x) * ex + (my - u.y) * ey) / len2 : -1.0;
            if (!(t > 0.0 && t < 1.0))
                continue;
            cx = u.x + t * ex, cy = u.y + t * ey;
        } else {
            if (q == so.a || q == so.b)
                continue; // q's own segment: the bisector runs through q itself
            // A parabola with focus q: it comes closest to q at its apex.
            const double dx = so.x2 - so.x, dy = so.y2 - so.y, len = std::hypot(dx, dy);
            const double ux = dx / len, uy = dy / len;
            const double along = (sq.x - so.x) * ux + (sq.y - so.y) * uy;
            const double ox = so.x + along * ux, oy = so.y + along * uy;
            const double depth = std::hypot(sq.x - ox, sq.y - oy);
            if (depth <= on_point_)
                continue;
            const double tu = (u.x - ox) * ux + (u.y - oy) * uy, tw = (w.x - ox) * ux + (w.y - oy) * uy;
            if (!((tu < 0.0 && tw > 0.0) || (tu > 0.0 && tw < 0.0)))
                continue;
            cx = ox + (sq.x - ox) / 2.0, cy = oy + (sq.y - oy) / 2.0;
        }
        if (violated(cx, cy, std::sqrt(dist2(cx, cy, sq.x, sq.y)), site))
            continue;
        px = cx, py = cy;
        return true;
    }
    return false;
}

// Cuts the edge of h (u -> w) at (x, y) with a new degree-two node; h then
// ends there and a new edge carries on to w. Returns the new node.
std::int32_t VoronoiDiagram::split_edge(std::int32_t h, double x, double y) {
    const std::int32_t w = dest(h);
    const std::int32_t left = halves_[std::size_t(h)].site, right = halves_[std::size_t(twin(h))].site;
    const std::int32_t n = new_node(x, y, distance(left, x, y));
    nodes_[std::size_t(n)].split = true;
    nodes_[std::size_t(n)].on_point = false;
    const std::int32_t g = new_edge(n, w, left, right);
    link(g, halves_[std::size_t(h)].next);
    link(h, g);
    link(halves_[std::size_t(twin(h))].prev, twin(g));
    link(twin(g), twin(h));
    halves_[std::size_t(twin(h))].origin = n;
    if (nodes_[std::size_t(w)].half == twin(h))
        nodes_[std::size_t(w)].half = twin(g);
    nodes_[std::size_t(n)].half = g;
    return n;
}

// Removes a degree-two node, joining its two edges into one.
void VoronoiDiagram::merge_split(std::int32_t x) {
    const std::int32_t h1 = nodes_[std::size_t(x)].half; // x -> n1
    const std::int32_t h2 = clockwise(h1);               // x -> n2
    if (clockwise(h2) != h1)
        throw std::logic_error("VoronoiDiagram: a split node is not of degree two");
    const std::int32_t n2 = dest(h2);
    // twin(h1) becomes n1 -> n2 and h1 becomes n2 -> n1; the pair of h2 goes.
    link(twin(h1), halves_[std::size_t(h2)].next);
    link(halves_[std::size_t(twin(h2))].prev, h1);
    halves_[std::size_t(h1)].origin = n2;
    if (nodes_[std::size_t(n2)].half == twin(h2))
        nodes_[std::size_t(n2)].half = h1;
    const std::int32_t s2 = halves_[std::size_t(h2)].site, t2 = halves_[std::size_t(twin(h2))].site;
    if (s2 >= 0 && sites_[std::size_t(s2)].half == h2)
        sites_[std::size_t(s2)].half = twin(h1);
    if (t2 >= 0 && sites_[std::size_t(t2)].half == twin(h2))
        sites_[std::size_t(t2)].half = h1;
    halves_[std::size_t(h2)].alive = halves_[std::size_t(twin(h2))].alive = false;
    free_edges_.push_back(h2 & ~1);
    nodes_[std::size_t(x)].alive = false;
    free_nodes_.push_back(x);
}

// The point of the edge of h (from a node the new site takes to one it does
// not) as far from the new site as from the edge's two sites. Found by
// bisection along the edge, which is straight unless it lies between a
// point and a segment the point is not an end of: then it is a parabola,
// followed through the foot of its points on the segment's line.
void VoronoiDiagram::locate(std::int32_t h, std::int32_t site, double& x, double& y) const {
    const Node& u = nodes_[std::size_t(halves_[std::size_t(h)].origin)];
    const Node& w = nodes_[std::size_t(dest(h))];
    const std::int32_t left = halves_[std::size_t(h)].site, right = halves_[std::size_t(twin(h))].site;
    const Site& sn = sites_[std::size_t(site)];
    const Site& sl = sites_[std::size_t(left)];
    const Site& sr = sites_[std::size_t(right)];
    if (sn.kind != SiteKind::Segment && sl.kind != SiteKind::Segment && sr.kind != SiteKind::Segment) {
        // Points only: the difference of squared distances is linear along the straight edge.
        const double fa = dist2(u.x, u.y, sn.x, sn.y) - dist2(u.x, u.y, sl.x, sl.y);
        const double fb = dist2(w.x, w.y, sn.x, sn.y) - dist2(w.x, w.y, sl.x, sl.y);
        const double t = fa < fb ? std::clamp(fa / (fa - fb), 0.0, 1.0) : 0.5;
        x = u.x + t * (w.x - u.x), y = u.y + t * (w.y - u.y);
        return;
    }
    // The edge's carrier, from its two sites (not from its end nodes, which
    // carry rounding: some are far nodes a thousand box radii away): a
    // parabola, or a line through (px0, py0) along (dx0, dy0), unit length.
    bool parabola = false, line = false;
    double ox = 0.0, oy = 0.0, ux = 0.0, uy = 0.0, nx = 0.0, ny = 0.0, depth = 0.0, tu = 0.0, tw = 0.0;
    double px0 = 0.0, py0 = 0.0, dx0 = 0.0, dy0 = 0.0;
    auto unit = [](double& vx, double& vy) {
        const double len = std::hypot(vx, vy);
        if (len > 0.0)
            vx /= len, vy /= len;
        return len > 0.0;
    };
    const bool lseg = sl.kind == SiteKind::Segment, rseg = sr.kind == SiteKind::Segment;
    if (!lseg && !rseg) {
        // Two points: their perpendicular bisector.
        px0 = (sl.x + sr.x) / 2.0, py0 = (sl.y + sr.y) / 2.0;
        dx0 = -(sr.y - sl.y), dy0 = sr.x - sl.x;
        line = unit(dx0, dy0);
    } else if (lseg != rseg) {
        const std::int32_t qi = lseg ? right : left;
        const Site& q = sites_[std::size_t(qi)];
        const Site& t = lseg ? sl : sr;
        double tx = t.x2 - t.x, ty = t.y2 - t.y;
        unit(tx, ty);
        if (qi == t.a || qi == t.b) {
            // A segment and its own end: the line through the end at right angles.
            px0 = q.x, py0 = q.y, dx0 = -ty, dy0 = tx;
            line = true;
        } else {
            ux = tx, uy = ty;
            const double along = (q.x - t.x) * ux + (q.y - t.y) * uy;
            ox = t.x + along * ux, oy = t.y + along * uy;
            depth = std::hypot(q.x - ox, q.y - oy);
            if (depth > on_point_) {
                parabola = true;
                nx = (q.x - ox) / depth, ny = (q.y - oy) / depth;
                tu = (u.x - ox) * ux + (u.y - oy) * uy;
                tw = (w.x - ox) * ux + (w.y - oy) * uy;
            }
        }
    } else {
        // Two segments: one of the bisectors of their lines (the one closer to
        // the edge's end nodes), or the middle line of parallel ones.
        double ax = sl.x2 - sl.x, ay = sl.y2 - sl.y, bx = sr.x2 - sr.x, by = sr.y2 - sr.y;
        unit(ax, ay), unit(bx, by);
        const double cross = ax * by - ay * bx;
        if (std::abs(cross) < 1e-12) {
            const double off = (sr.x - sl.x) * -ay + (sr.y - sl.y) * ax; // signed distance between the lines
            px0 = sl.x - ay * off / 2.0, py0 = sl.y + ax * off / 2.0, dx0 = ax, dy0 = ay;
            line = true;
        } else {
            // Where the two lines meet.
            const double t = ((sr.x - sl.x) * by - (sr.y - sl.y) * bx) / cross;
            px0 = sl.x + t * ax, py0 = sl.y + t * ay;
            double c1x = ax + bx, c1y = ay + by, c2x = ax - bx, c2y = ay - by;
            unit(c1x, c1y), unit(c2x, c2y);
            auto off_line = [&](double cx, double cy, const Node& n) { return std::abs((n.x - px0) * cy - (n.y - py0) * cx); };
            const bool first = off_line(c1x, c1y, u) + off_line(c1x, c1y, w) <= off_line(c2x, c2y, u) + off_line(c2x, c2y, w);
            dx0 = first ? c1x : c2x, dy0 = first ? c1y : c2y;
            line = true;
        }
    }
    double lu = 0.0, lw = 0.0; // the end nodes projected on the line
    if (line) {
        lu = (u.x - px0) * dx0 + (u.y - py0) * dy0;
        lw = (w.x - px0) * dx0 + (w.y - py0) * dy0;
    }
    auto at = [&](double t, double& px, double& py) {
        if (parabola) {
            const double tau = tu + t * (tw - tu);
            const double rise = (tau * tau + depth * depth) / (2.0 * depth);
            px = ox + tau * ux + rise * nx, py = oy + tau * uy + rise * ny;
        } else if (line) {
            const double l = lu + t * (lw - lu);
            px = px0 + l * dx0, py = py0 + l * dy0;
        } else {
            px = u.x + t * (w.x - u.x), py = u.y + t * (w.y - u.y);
        }
    };
    // Negative where the new site takes the point. Outside a new segment's
    // cone of influence its nearest point is an endpoint, already a site: the
    // point is not taken (and the difference, zero there in exact arithmetic,
    // must not decide by rounding).
    auto f = [&](double t) {
        double px, py;
        at(t, px, py);
        if (sn.kind == SiteKind::Segment && !in_cone(sn, px, py))
            return 1.0;
        return distance(site, px, py) - distance(left, px, py);
    };
    double lo = 0.0, hi = 1.0;
    double flo = f(lo), fhi = f(hi);
    if (!(flo < 0.0 && fhi >= 0.0)) {
        // No clean change of sign (rounding): the sample closest to equidistance.
        double best = 0.5, best_f = std::abs(f(0.5));
        for (int k = 0; k <= kSamples; ++k) {
            const double t = double(k) / kSamples, v = std::abs(f(t));
            if (v < best_f)
                best = t, best_f = v;
        }
        at(best, x, y);
        return;
    }
    // Regula falsi with the Illinois change (the end kept twice in a row has
    // its value halved), always inside the bracket, with a bisection every
    // third step so that a jump of f cannot slow it down to a crawl. Stops
    // when the bracket is shorter than kPlace (relative to the box) or the
    // function is zero; the node goes where the bracket's upper end is.
    double ex, ey, fx, fy;
    at(0.0, ex, ey);
    at(1.0, fx, fy);
    const double tolerance = kPlace * on_point_ / kOnPoint / std::max(std::hypot(fx - ex, fy - ey), 1e-300);
    auto solve = [&](double a, double b, double fa, double fb, auto&& fn) {
        int kept = 0; // -1: a kept last time, +1: b kept
        for (int k = 0; k < kBisection && b - a > tolerance; ++k) {
            double t = (k % 3 == 2) ? (a + b) / 2.0 : (a * fb - b * fa) / (fb - fa);
            if (!(t > a && t < b))
                t = (a + b) / 2.0;
            const double ft = fn(t);
            if (ft < 0.0) {
                a = t, fa = ft;
                if (kept == -1)
                    fb /= 2.0;
                kept = -1;
            } else {
                b = t, fb = ft;
                if (kept == 1)
                    fa /= 2.0;
                kept = 1;
                if (ft == 0.0)
                    break;
            }
        }
        at(b, x, y);
    };
    if (sn.kind == SiteKind::Segment) {
        // A new segment: f jumps where the edge leaves its cone of influence,
        // and a search would only creep up on the jump by halving. The edge
        // crosses the cone's two side lines (through the segment's ends, at
        // right angles to it) where a line or parabola meets a line: worked
        // out exactly. Between those points the distances' difference is
        // continuous, and the search runs only there.
        const double sdx = sn.x2 - sn.x, sdy = sn.y2 - sn.y;
        double cuts[4];
        int count = 0;
        const auto add = [&](double t) {
            if (t > 0.0 && t < 1.0 && std::isfinite(t))
                cuts[count++] = t;
        };
        for (const auto& [cx, cy] : {std::pair{sn.x, sn.y}, std::pair{sn.x2, sn.y2}}) {
            // side(p) = (p - c) . d, zero on the side line.
            if (parabola) {
                // In tau: a2 tau^2 + a1 tau + a0, with t = (tau - tu) / (tw - tu).
                const double nd = nx * sdx + ny * sdy, ud = ux * sdx + uy * sdy;
                const double a2 = nd / (2.0 * depth), a1 = ud;
                const double a0 = (ox - cx) * sdx + (oy - cy) * sdy + depth / 2.0 * nd;
                if (tw == tu)
                    continue;
                if (a2 == 0.0) {
                    if (a1 != 0.0)
                        add((-a0 / a1 - tu) / (tw - tu));
                    continue;
                }
                const double disc = a1 * a1 - 4.0 * a2 * a0;
                if (disc < 0.0)
                    continue;
                // The root that does not lose digits, and the other from the product.
                const double q = -0.5 * (a1 + std::copysign(std::sqrt(disc), a1));
                if (q != 0.0) {
                    add((q / a2 - tu) / (tw - tu));
                    add((a0 / q - tu) / (tw - tu));
                } else {
                    add((0.0 - tu) / (tw - tu));
                }
            } else {
                // A straight carrier: side is linear in t.
                double s0, s1;
                if (line) {
                    s0 = (px0 + lu * dx0 - cx) * sdx + (py0 + lu * dy0 - cy) * sdy;
                    s1 = (px0 + lw * dx0 - cx) * sdx + (py0 + lw * dy0 - cy) * sdy;
                } else {
                    s0 = (u.x - cx) * sdx + (u.y - cy) * sdy;
                    s1 = (w.x - cx) * sdx + (w.y - cy) * sdy;
                }
                if (s0 != s1)
                    add(s0 / (s0 - s1));
            }
        }
        for (int i = 1; i < count; ++i) // at most four: in order by insertion
            for (int j = i; j > 0 && cuts[j - 1] > cuts[j]; --j)
                std::swap(cuts[j - 1], cuts[j]);
        // Without the cone: the distances' difference, continuous along the edge.
        auto g = [&](double t) {
            double px, py;
            at(t, px, py);
            return distance(site, px, py) - distance(left, px, py);
        };
        // From t = 0 (inside the cone, taken), piece by piece: when every
        // piece up to a cut is taken and the edge leaves the cone there, the
        // new site's part ends at the cut.
        double a = 0.0;
        for (int i = 0; i <= count; ++i) {
            const double b = i < count ? cuts[i] : 1.0;
            if (b <= a)
                continue;
            double mx, my;
            at((a + b) / 2.0, mx, my);
            if (!in_cone(sn, mx, my)) {
                if (a > 0.0) {
                    // The new site's part ends where the edge leaves the cone:
                    // the node on the side not taken, as the search placed it
                    // (rounding can put the cut itself just inside).
                    double t = a, step = tolerance;
                    at(t, x, y);
                    for (int k = 0; k < 64 && t < 1.0 && in_cone(sn, x, y); ++k, step *= 2.0) {
                        t = std::min(1.0, a + step);
                        at(t, x, y);
                    }
                    return;
                }
                break; // outside from the start: rounding; the search below decides
            }
            // The change inside a piece: the search below, over the whole
            // edge as always (a piece can hold more than one root of the
            // difference, and the one the search finds is the one kept).
            if (b >= 1.0 || g(b) >= 0.0)
                break;
            a = b;
        }
    }
    solve(lo, hi, flo, fhi, f);
}

// The node to start deleting from: for a point, in the cell of the nearest
// site; for a segment, in the cells of its endpoints, inside its cone of
// influence and not on an input point (Held and Huber 2009, 2.3). The one
// whose clearance disk the new site violates most.
std::int32_t VoronoiDiagram::seed_node(std::int32_t site, std::int32_t near) const {
    const Site& s = sites_[std::size_t(site)];
    std::int32_t seed = -1;
    double worst = 0.0;
    auto scan = [&](std::int32_t cell) {
        const std::int32_t first = sites_[std::size_t(cell)].half;
        std::int32_t h = first;
        do {
            const std::int32_t n = halves_[std::size_t(h)].origin;
            const Node& node = nodes_[std::size_t(n)];
            h = halves_[std::size_t(h)].next;
            if (node.fixed || node.on_point || node.split)
                continue;
            if (s.kind == SiteKind::Segment && !in_cone(s, node.x, node.y))
                continue;
            const double v = distance(site, node.x, node.y) - node.r;
            if (seed < 0 || v < worst)
                seed = n, worst = v;
        } while (h != first);
    };
    if (s.kind == SiteKind::Segment) {
        scan(s.a);
        scan(s.b);
    } else {
        scan(near);
    }
    if (seed < 0)
        throw std::logic_error("VoronoiDiagram: no seed node");
    return seed;
}

std::int32_t VoronoiDiagram::insert_point(double x, double y) {
    const std::size_t cell = grid_cell(x, y);
    const std::int32_t near = nearest_site(x, y, grid_[cell] >= 0 ? grid_[cell] : last_);
    if (dist2(sites_[std::size_t(near)].x, sites_[std::size_t(near)].y, x, y) == 0.0)
        throw std::invalid_argument("VoronoiDiagram: the point is already a site");
    Site s;
    s.kind = SiteKind::Point;
    s.x = x, s.y = y;
    const std::int32_t site = add_site(s);
    insert(site, near);
    last_ = site;
    grid_[cell] = site;
    return site;
}

std::int32_t VoronoiDiagram::insert_segment(std::int32_t a, std::int32_t b) {
    const Site& pa = sites_[std::size_t(a)];
    const Site& pb = sites_[std::size_t(b)];
    if (a == b || pa.kind != SiteKind::Point || pb.kind != SiteKind::Point)
        throw std::invalid_argument("VoronoiDiagram: a segment needs two different point sites");
    Site s;
    s.kind = SiteKind::Segment;
    s.x = pa.x, s.y = pa.y, s.x2 = pb.x, s.y2 = pb.y;
    s.a = a, s.b = b;
    const std::int32_t site = add_site(s);
    insert(site, a);
    return site;
}

// Held and Huber 2009, Section 2.1, and Huber 2008, Algorithms 1-4: mark the
// tree of nodes the new site takes, put a new node on every edge leaving the
// tree, and join the new nodes into the new site's cell.
void VoronoiDiagram::insert(std::int32_t site, std::int32_t near) {
    const bool segment = sites_[std::size_t(site)].kind == SiteKind::Segment;
    std::vector<std::int32_t>& marked = scratch_marked_;
    std::vector<std::int32_t>& touched = scratch_touched_;
    std::vector<std::int32_t>& splits = scratch_splits_;
    marked.clear(), touched.clear(), splits.clear();
    auto mark = [&](std::int32_t n) {
        marked_[std::size_t(n)] = 1;
        marked.push_back(n);
        const std::int32_t first = nodes_[std::size_t(n)].half;
        std::int32_t h = first;
        do {
            const std::int32_t face = halves_[std::size_t(h)].site;
            if (face >= 0 && marked_count_[std::size_t(face)]++ == 0)
                touched.push_back(face);
            h = clockwise(h);
        } while (h != first);
    };
    mark(seed_node(site, near));
    for (std::size_t k = 0; k < marked.size(); ++k) {
        const std::int32_t u = marked[k];
        const std::int32_t first = nodes_[std::size_t(u)].half;
        std::int32_t h = first;
        do {
            const std::int32_t w = dest(h);
            const Node& node = nodes_[std::size_t(w)];
            if (!marked_[std::size_t(w)] && !node.fixed && !node.on_point && !node.split &&
                violated(node.x, node.y, node.r, site)) {
                double px, py;
                if (segment && split_point(h, site, px, py))
                    splits.push_back(split_edge(h, px, py)); // the edge keeps its middle
                else if (can_mark(w, u, site))
                    mark(w);
            }
            h = clockwise(h);
        } while (h != first);
    }

    // Every half-edge leaving the tree gets a new node and now starts there.
    std::vector<std::int32_t>& doomed = scratch_doomed_;
    std::vector<std::int32_t>& outs = scratch_outs_;
    doomed.clear(), outs.clear();
    for (std::int32_t u : marked) {
        const std::int32_t first = nodes_[std::size_t(u)].half;
        std::int32_t leaving[64];
        int count = 0;
        std::int32_t h = first;
        do {
            if (count == 64)
                throw std::logic_error("VoronoiDiagram: a node of too high degree");
            leaving[count++] = h;
            h = clockwise(h);
        } while (h != first);
        for (int i = 0; i < count; ++i) {
            const std::int32_t o = leaving[i];
            if (marked_[std::size_t(dest(o))]) {
                doomed.push_back(o);
                continue;
            }
            double nx, ny;
            locate(o, site, nx, ny);
            const std::int32_t n = new_node(nx, ny, distance(halves_[std::size_t(o)].site, nx, ny));
            halves_[std::size_t(o)].origin = n;
            nodes_[std::size_t(n)].half = o;
            outs.push_back(o);
        }
    }
    // In every cell, each run of marked nodes is replaced by a new edge from
    // the new node where the run starts (the end of the half-edge entering
    // it, the twin of a leaving one) to the one where it ends (the start of
    // the next leaving half-edge along the cell). run_edge_ tags the leaving
    // half-edges with the new edge of their run.
    constexpr std::int32_t kNotLeaving = -2;
    if (run_edge_.size() < halves_.size())
        run_edge_.resize(halves_.size(), kNotLeaving);
    for (std::int32_t o : outs)
        run_edge_[std::size_t(o)] = -1;
    std::vector<std::pair<std::int32_t, std::int32_t>>& made = scratch_runs_; // (entering half, new edge)
    made.clear();
    for (std::int32_t o : outs) {
        const std::int32_t in = twin(o);
        std::int32_t out = halves_[std::size_t(in)].next;
        while (run_edge_[std::size_t(out)] == kNotLeaving)
            out = halves_[std::size_t(out)].next;
        const std::int32_t face = halves_[std::size_t(in)].site;
        const std::int32_t g = new_edge(dest(in), halves_[std::size_t(out)].origin, face, site);
        run_edge_[std::size_t(out)] = g;
        link(in, g);
        link(g, out);
        sites_[std::size_t(face)].half = out;
        made.emplace_back(in, g);
    }
    // Around the new cell: after g's twin (ending where its run starts) comes
    // the twin of the new edge that ends there, in the cell across.
    for (const auto& [in, g] : made)
        link(twin(g), twin(run_edge_[std::size_t(twin(in))]));
    for (std::int32_t o : outs)
        run_edge_[std::size_t(o)] = kNotLeaving;
    sites_[std::size_t(site)].half = twin(made.front().second);
    const std::vector<std::int32_t> faces_touched(touched.begin(), touched.end());

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
    for (std::int32_t face : faces_touched)
        marked_count_[std::size_t(face)] = 0;
    for (std::int32_t x : splits)
        merge_split(x);
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
