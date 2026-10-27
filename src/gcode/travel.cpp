// Travel planning inside the part (docs/spec/BEHAVIOR.md, "Viajes y
// retracción"). Own design; see PROVENANCE.md.

#include "gcode/travel.hpp"

#include <algorithm>
#include <cmath>
#include <functional>

namespace tisma {
namespace {

constexpr double kMargin = 0.01;  // mm, how much the shortcut region exceeds the travel region
constexpr double kInward = 0.001; // mm, how far a hop lands past the travel region's boundary

double cross(Point2d o, Point2d a, Point2d b) { return (a.x - o.x) * (b.y - o.y) - (a.y - o.y) * (b.x - o.x); }

double distance(Point2d a, Point2d b) { return std::hypot(b.x - a.x, b.y - a.y); }

// The segments ab and cd touch or cross (collinear overlaps included).
bool touch(Point2d a, Point2d b, Point2d c, Point2d d) {
    const double d1 = cross(c, d, a), d2 = cross(c, d, b), d3 = cross(a, b, c), d4 = cross(a, b, d);
    if (((d1 > 0 && d2 < 0) || (d1 < 0 && d2 > 0)) && ((d3 > 0 && d4 < 0) || (d3 < 0 && d4 > 0)))
        return true;
    auto on = [](Point2d p, Point2d q, Point2d r, double o) { // r on the segment pq, given it is on its line
        return o == 0.0 && std::min(p.x, q.x) <= r.x && r.x <= std::max(p.x, q.x) && std::min(p.y, q.y) <= r.y &&
               r.y <= std::max(p.y, q.y);
    };
    return on(c, d, a, d1) || on(c, d, b, d2) || on(a, b, c, d3) || on(a, b, d, d4);
}

// Crossing number of p against one ring.
bool in_ring(const std::vector<Point2d>& ring, Point2d p) {
    bool in = false;
    for (std::size_t i = 0, j = ring.size() - 1; i < ring.size(); j = i++) {
        const Point2d a = ring[i], b = ring[j];
        if ((a.y > p.y) != (b.y > p.y) && p.x < a.x + (p.y - a.y) * (b.x - a.x) / (b.y - a.y))
            in = !in;
    }
    return in;
}

double path_length(Point2d start, const std::vector<Point2d>& points) {
    double length = 0.0;
    for (const Point2d& p : points) {
        length += distance(start, p);
        start = p;
    }
    return length;
}

} // namespace

std::vector<TravelPlanner::Ring> TravelPlanner::rings_of(const Polygons& polygons) {
    std::vector<Ring> rings;
    for (const Polygon& polygon : polygons) {
        if (polygon.size() < 3)
            continue;
        Ring r{{}, 1e300, 1e300, -1e300, -1e300};
        r.points.reserve(polygon.size());
        for (const auto& q : polygon) {
            const Point2d p{to_mm(q.x), to_mm(q.y)};
            r.points.push_back(p);
            r.min_x = std::min(r.min_x, p.x), r.min_y = std::min(r.min_y, p.y);
            r.max_x = std::max(r.max_x, p.x), r.max_y = std::max(r.max_y, p.y);
        }
        rings.push_back(std::move(r));
    }
    return rings;
}

TravelPlanner::TravelPlanner(const Polygons& region, double bead, TravelRouting routing)
    : snap_(1.5 * bead), routing_(routing) {
    region_ = rings_of(region);
    if (!routing_.inside) // every travel straight: no route to look for
        return;
    margin_ = rings_of(offset(region, -(bead - kMargin)));
    // The travel region as a tree: every outer contour with its holes is one piece.
    Clipper2Lib::Clipper64 clipper;
    clipper.AddSubject(offset(region, -bead));
    Clipper2Lib::PolyTree64 tree;
    clipper.Execute(Clipper2Lib::ClipType::Union, Clipper2Lib::FillRule::NonZero, tree);
    std::function<void(const Clipper2Lib::PolyPath64&)> walk = [&](const Clipper2Lib::PolyPath64& outer) {
        std::vector<std::size_t> piece;
        auto add = [&](const Polygon& polygon) {
            std::vector<Ring> one = rings_of({polygon});
            if (one.empty())
                return false;
            piece.push_back(travel_.size());
            travel_.push_back(std::move(one.front()));
            return true;
        };
        if (!add(outer.Polygon()))
            return;
        for (const auto& hole : outer) {
            add(hole->Polygon());
            for (const auto& island : *hole)
                walk(*island);
        }
        pieces_.push_back(std::move(piece));
    };
    for (const auto& outer : tree)
        walk(*outer);
}

bool TravelPlanner::inside(const std::vector<Ring>& rings, Point2d p) {
    bool in = false;
    for (const Ring& r : rings)
        if (p.x >= r.min_x && p.x <= r.max_x && p.y >= r.min_y && p.y <= r.max_y && in_ring(r.points, p))
            in = !in;
    return in;
}

// The segment ab stays strictly inside the rings: it touches none of their
// edges and a is inside.
bool TravelPlanner::visible(const std::vector<Ring>& rings, Point2d a, Point2d b) {
    const double x0 = std::min(a.x, b.x), x1 = std::max(a.x, b.x), y0 = std::min(a.y, b.y), y1 = std::max(a.y, b.y);
    for (const Ring& r : rings) {
        if (r.max_x < x0 || r.min_x > x1 || r.max_y < y0 || r.min_y > y1)
            continue;
        const std::vector<Point2d>& p = r.points;
        for (std::size_t i = 0, j = p.size() - 1; i < p.size(); j = i++) {
            if (std::max(p[i].x, p[j].x) < x0 || std::min(p[i].x, p[j].x) > x1 ||
                std::max(p[i].y, p[j].y) < y0 || std::min(p[i].y, p[j].y) > y1)
                continue;
            if (touch(a, b, p[j], p[i]))
                return false;
        }
    }
    return inside(rings, a);
}

int TravelPlanner::piece_of(Point2d p) const {
    for (std::size_t k = 0; k < pieces_.size(); ++k) {
        const std::vector<std::size_t>& rings = pieces_[k];
        const Ring& outer = travel_[rings.front()];
        if (p.x < outer.min_x || p.x > outer.max_x || p.y < outer.min_y || p.y > outer.max_y ||
            !in_ring(outer.points, p))
            continue;
        bool in_hole = false;
        for (std::size_t h = 1; h < rings.size() && !in_hole; ++h)
            in_hole = in_ring(travel_[rings[h]].points, p);
        if (!in_hole)
            return int(k);
    }
    return -1;
}

// The piece of the travel region the point is in; a point outside it within
// snap_ hops to the closest point of its boundary, a little inside.
TravelPlanner::Located TravelPlanner::locate(Point2d p) const {
    Located l;
    l.p = p;
    l.piece = piece_of(p);
    if (l.piece >= 0)
        return l;
    double best = snap_;
    Point2d closest{};
    bool found = false;
    for (const Ring& r : travel_) {
        if (p.x < r.min_x - best || p.x > r.max_x + best || p.y < r.min_y - best || p.y > r.max_y + best)
            continue;
        for (std::size_t i = 0, j = r.points.size() - 1; i < r.points.size(); j = i++) {
            const Point2d a = r.points[j], b = r.points[i];
            const double dx = b.x - a.x, dy = b.y - a.y, len2 = dx * dx + dy * dy;
            const double t = len2 > 0.0 ? std::clamp(((p.x - a.x) * dx + (p.y - a.y) * dy) / len2, 0.0, 1.0) : 0.0;
            const Point2d q{a.x + t * dx, a.y + t * dy};
            const double d = distance(p, q);
            if (d < best)
                best = d, closest = q, found = true;
        }
    }
    if (!found || best < 1e-9)
        return l;
    const Point2d in{closest.x + (closest.x - p.x) / best * kInward, closest.y + (closest.y - p.y) / best * kInward};
    l.piece = piece_of(in);
    l.p = in;
    l.hopped = l.piece >= 0;
    return l;
}

// From a to b inside one piece: the segment, with every stretch outside the
// piece replaced by the boundary it crossed, the shorter way round; then cut
// short wherever a straight line stays inside. Appends the points after a.
bool TravelPlanner::route(int piece, Point2d a, Point2d b, std::vector<Point2d>& out) const {
    std::vector<Point2d> path{a};
    if (!visible(margin_, a, b)) {
        struct Crossing {
            double t;
            std::size_t ring, edge; // the edge from point edge to point edge + 1
            Point2d p;
        };
        std::vector<Crossing> crossings;
        const double rx = b.x - a.x, ry = b.y - a.y;
        for (std::size_t ring : pieces_[std::size_t(piece)]) {
            const std::vector<Point2d>& p = travel_[ring].points;
            for (std::size_t i = 0; i < p.size(); ++i) {
                const Point2d c = p[i], d = p[(i + 1) % p.size()];
                const double sx = d.x - c.x, sy = d.y - c.y, denom = rx * sy - ry * sx;
                if (denom == 0.0)
                    continue;
                const double t = ((c.x - a.x) * sy - (c.y - a.y) * sx) / denom;
                const double u = ((c.x - a.x) * ry - (c.y - a.y) * rx) / denom;
                if (t > 0.0 && t < 1.0 && u >= 0.0 && u < 1.0)
                    crossings.push_back({t, ring, i, {a.x + t * rx, a.y + t * ry}});
            }
        }
        std::sort(crossings.begin(), crossings.end(), [](const Crossing& x, const Crossing& y) { return x.t < y.t; });
        // Out and back in through the same contour, in pairs.
        if (crossings.size() % 2 != 0)
            return false;
        for (std::size_t k = 0; k < crossings.size(); k += 2) {
            const Crossing& exit = crossings[k];
            const Crossing& entry = crossings[k + 1];
            if (exit.ring != entry.ring || exit.edge == entry.edge)
                return false;
            const std::vector<Point2d>& p = travel_[exit.ring].points;
            const std::size_t n = p.size();
            std::vector<Point2d> forward{exit.p}, backward{exit.p};
            for (std::size_t i = (exit.edge + 1) % n;; i = (i + 1) % n) {
                forward.push_back(p[i]);
                if (i == entry.edge)
                    break;
            }
            for (std::size_t i = exit.edge;; i = (i + n - 1) % n) {
                backward.push_back(p[i]);
                if (i == (entry.edge + 1) % n)
                    break;
            }
            forward.push_back(entry.p);
            backward.push_back(entry.p);
            const std::vector<Point2d>& way =
                path_length(exit.p, forward) <= path_length(exit.p, backward) ? forward : backward;
            path.insert(path.end(), way.begin(), way.end());
        }
    }
    path.push_back(b);
    // Shortcuts: from each point, as far along the route as a straight line stays inside.
    for (std::size_t i = 0; i + 1 < path.size();) {
        std::size_t j = i + 1;
        while (j + 1 < path.size() && visible(margin_, path[i], path[j + 1]))
            ++j;
        out.push_back(path[j]);
        i = j;
    }
    return true;
}

TravelPlan TravelPlanner::plan(Point2d from, Point2d to) const {
    TravelPlan plan;
    auto straight = [&](bool leaves) {
        plan.points = {to};
        plan.leaves_part = leaves;
        plan.length = distance(from, to);
        return plan;
    };
    if (visible(region_, from, to))
        return straight(false);
    if (!routing_.inside)
        return straight(true);
    const Located a = locate(from), b = locate(to);
    if (a.piece < 0 || a.piece != b.piece)
        return straight(true);
    if (a.hopped)
        plan.points.push_back(a.p);
    if (!route(a.piece, a.p, b.p, plan.points))
        return straight(true);
    if (b.hopped)
        plan.points.push_back(to);
    plan.length = path_length(from, plan.points);
    // A detour longer than allowed: straight across the walls.
    if (routing_.max_detour > 0.0) {
        const double direct = distance(from, to);
        const double allowed = routing_.percent ? direct * routing_.max_detour / 100.0 : routing_.max_detour;
        if (plan.length - direct > allowed)
            return straight(true);
    }
    return plan;
}

} // namespace tisma
