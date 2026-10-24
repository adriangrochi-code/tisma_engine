// Medial axis from the segment Voronoi diagram (Boost.Polygon, BSL-1.0).
// Discretization of parabolic edges and the central marking follow Kuipers et
// al. 2020, Sections 3.2 and 3.3. Own design; see PROVENANCE.md.

#include "geom/medial_axis.hpp"

#include <boost/polygon/voronoi.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <map>
#include <numbers>
#include <stdexcept>

namespace tisma {
namespace {

struct Segment {
    Clipper2Lib::Point64 p, q;
    std::uint32_t prev = 0, next = 0; // neighbouring segments along the same contour
};

double distance_to_segment(double x, double y, const Segment& s) {
    const double px = double(s.p.x), py = double(s.p.y), qx = double(s.q.x), qy = double(s.q.y);
    const double dx = qx - px, dy = qy - py;
    const double len2 = dx * dx + dy * dy;
    double t = len2 > 0.0 ? ((x - px) * dx + (y - py) * dy) / len2 : 0.0;
    t = std::clamp(t, 0.0, 1.0);
    return std::hypot(x - (px + t * dx), y - (py + t * dy));
}

// Twice the signed area of (a, b, c): positive when c is left of a -> b.
double cross(double ax, double ay, double bx, double by, double cx, double cy) {
    return (bx - ax) * (cy - ay) - (cx - ax) * (by - ay);
}

} // namespace

} // namespace tisma

// Boost.Polygon reads our segments through these traits.
namespace boost::polygon {
template <>
struct geometry_concept<Clipper2Lib::Point64> {
    using type = point_concept;
};
template <>
struct point_traits<Clipper2Lib::Point64> {
    using coordinate_type = std::int32_t;
    static coordinate_type get(const Clipper2Lib::Point64& p, orientation_2d o) {
        return static_cast<std::int32_t>(o == HORIZONTAL ? p.x : p.y);
    }
};
template <>
struct geometry_concept<tisma::Segment> {
    using type = segment_concept;
};
template <>
struct segment_traits<tisma::Segment> {
    using coordinate_type = std::int32_t;
    using point_type = Clipper2Lib::Point64;
    static point_type get(const tisma::Segment& s, direction_1d d) { return d.to_int() ? s.q : s.p; }
};
} // namespace boost::polygon

namespace tisma {
namespace {

using Diagram = boost::polygon::voronoi_diagram<double>;

class Builder {
public:
    explicit Builder(const Polygons& region) {
        constexpr std::int64_t kLimit = std::numeric_limits<std::int32_t>::max();
        for (const Polygon& path : region) {
            const std::uint32_t first = static_cast<std::uint32_t>(segments_.size());
            for (std::size_t i = 0; i < path.size(); ++i) {
                const auto& p = path[i];
                const auto& q = path[(i + 1) % path.size()];
                if (std::llabs(p.x) > kLimit || std::llabs(p.y) > kLimit)
                    throw std::out_of_range("medial_axis: coordinates beyond the 32-bit range");
                if (p != q)
                    segments_.push_back({p, q});
            }
            const std::uint32_t count = static_cast<std::uint32_t>(segments_.size()) - first;
            for (std::uint32_t k = 0; k < count; ++k) {
                segments_[first + k].prev = first + (k + count - 1) % count;
                segments_[first + k].next = first + (k + 1) % count;
            }
        }
        boost::polygon::construct_voronoi(segments_.begin(), segments_.end(), &vd_);
    }

    // Calls emit(piece) for every straight piece of every interior edge (in units).
    template <typename Emit>
    void for_each_piece(Emit emit) const {
        for (const auto& edge : vd_.edges()) {
            // Each edge is stored twice (one per side); keep one of the pair.
            if (&edge > edge.twin() || !edge.is_finite() || !edge.is_primary())
                continue;
            if (!interior(edge) || !interior(*edge.twin()))
                continue;
            const Point2d a{edge.vertex0()->x(), edge.vertex0()->y()};
            const Point2d b{edge.vertex1()->x(), edge.vertex1()->y()};
            const std::vector<Point2d> chain = edge.is_curved() ? discretize(edge, a, b) : std::vector<Point2d>{a, b};
            const auto& c0 = *edge.cell();
            const auto& c1 = *edge.twin()->cell();
            for (std::size_t i = 0; i + 1 < chain.size(); ++i) {
                MedialEdge m;
                m.a = chain[i];
                m.b = chain[i + 1];
                m.radius_a = radius(c0, m.a);
                m.radius_b = radius(c0, m.b);
                m.curved = edge.is_curved();
                m.support_a[0] = support(c0, m.a);
                m.support_a[1] = support(c1, m.a);
                m.support_b[0] = support(c0, m.b);
                m.support_b[1] = support(c1, m.b);
                emit(m);
            }
        }
    }

private:
    Clipper2Lib::Point64 site_point(const Diagram::cell_type& cell) const {
        const Segment& s = segments_[cell.source_index()];
        return cell.source_category() == boost::polygon::SOURCE_CATEGORY_SEGMENT_START_POINT ? s.p : s.q;
    }

    // Distance from a point to the site of a cell (a segment or one of its ends).
    double radius(const Diagram::cell_type& cell, const Point2d& v) const {
        if (cell.contains_point()) {
            const auto c = site_point(cell);
            return std::hypot(v.x - double(c.x), v.y - double(c.y));
        }
        return distance_to_segment(v.x, v.y, segments_[cell.source_index()]);
    }

    // Closest point of a cell's site to v: the corner, or the foot of the
    // perpendicular on the segment (clamped to it).
    Point2d support(const Diagram::cell_type& cell, const Point2d& v) const {
        if (cell.contains_point()) {
            const auto c = site_point(cell);
            return {double(c.x), double(c.y)};
        }
        const Segment& s = segments_[cell.source_index()];
        const double px = double(s.p.x), py = double(s.p.y), dx = double(s.q.x) - px, dy = double(s.q.y) - py;
        const double len2 = dx * dx + dy * dy;
        const double t = len2 > 0.0 ? std::clamp(((v.x - px) * dx + (v.y - py) * dy) / len2, 0.0, 1.0) : 0.0;
        return {px + t * dx, py + t * dy};
    }

    // Whether the part of the edge in this cell lies inside the region. The
    // interior is on the left of every contour segment (outer contours
    // counter-clockwise, holes clockwise). A segment's cell is inside on the
    // segment's left; a corner's cell is inside only at a reflex corner.
    bool interior(const Diagram::edge_type& edge) const {
        const Diagram::cell_type& cell = *edge.cell();
        const Segment& s = segments_[cell.source_index()];
        if (cell.contains_point()) {
            const bool start = cell.source_category() == boost::polygon::SOURCE_CATEGORY_SEGMENT_START_POINT;
            const Segment& in = start ? segments_[s.prev] : s;
            const Segment& out = start ? s : segments_[s.next];
            // Reflex corner: the contour turns right at it.
            return cross(double(in.p.x), double(in.p.y), double(in.q.x), double(in.q.y), double(out.q.x),
                         double(out.q.y)) < 0.0;
        }
        const double px = double(s.p.x), py = double(s.p.y), qx = double(s.q.x), qy = double(s.q.y);
        return cross(px, py, qx, qy, edge.vertex0()->x(), edge.vertex0()->y()) >= -1e-6 &&
               cross(px, py, qx, qy, edge.vertex1()->x(), edge.vertex1()->y()) >= -1e-6;
    }

    // Points along a parabolic edge (equidistant from a corner and a segment's
    // line), at most kMaxCurvedPiece apart.
    std::vector<Point2d> discretize(const Diagram::edge_type& edge, const Point2d& a, const Point2d& b) const {
        const Diagram::cell_type* point_cell = edge.cell();
        const Diagram::cell_type* segment_cell = edge.twin()->cell();
        if (!point_cell->contains_point())
            std::swap(point_cell, segment_cell);
        const auto focus = site_point(*point_cell);
        const Segment& s = segments_[segment_cell->source_index()];
        // Frame on the segment's line: u along it, n towards the focus.
        const double sx = double(s.p.x), sy = double(s.p.y);
        double ux = double(s.q.x) - sx, uy = double(s.q.y) - sy;
        const double len = std::hypot(ux, uy);
        ux /= len;
        uy /= len;
        double nx = -uy, ny = ux;
        const double fx = double(focus.x) - sx, fy = double(focus.y) - sy;
        double fv = fx * nx + fy * ny;
        if (fv < 0.0) {
            nx = -nx;
            ny = -ny;
            fv = -fv;
        }
        const double fu = fx * ux + fy * uy;
        auto along = [&](const Point2d& p) { return (p.x - sx) * ux + (p.y - sy) * uy; };
        // On the parabola, height above the line v = ((u - fu)^2 + fv^2) / (2 fv).
        auto at = [&](double u) {
            const double v = ((u - fu) * (u - fu) + fv * fv) / (2.0 * fv);
            return Point2d{sx + u * ux + v * nx, sy + u * uy + v * ny};
        };
        const double u0 = along(a), u1 = along(b);
        // Arc length is at least |u1 - u0|; refine until every chord is short enough.
        const double max_piece = kMaxCurvedPiece * kUnitsPerMm;
        std::size_t pieces = std::max<std::size_t>(1, std::size_t(std::ceil(std::fabs(u1 - u0) / max_piece)));
        std::vector<Point2d> chain;
        for (;;) {
            chain.clear();
            chain.push_back(a);
            for (std::size_t i = 1; i < pieces; ++i)
                chain.push_back(at(u0 + (u1 - u0) * double(i) / double(pieces)));
            chain.push_back(b);
            bool short_enough = true;
            for (std::size_t i = 0; i + 1 < chain.size(); ++i)
                short_enough = short_enough &&
                               std::hypot(chain[i + 1].x - chain[i].x, chain[i + 1].y - chain[i].y) <= max_piece;
            if (short_enough || pieces > 100000)
                return chain;
            pieces *= 2;
        }
    }

    std::vector<Segment> segments_;
    Diagram vd_;
};

} // namespace

std::vector<MedialEdge> medial_axis(const Polygons& region) {
    std::vector<MedialEdge> out;
    if (region.empty())
        return out;
    const Builder builder(region);
    builder.for_each_piece([&](MedialEdge m) {
        auto mm = [](const Point2d& p) { return Point2d{p.x / kUnitsPerMm, p.y / kUnitsPerMm}; };
        m.a = mm(m.a);
        m.b = mm(m.b);
        m.radius_a /= kUnitsPerMm;
        m.radius_b /= kUnitsPerMm;
        for (int k = 0; k < 2; ++k) {
            m.support_a[k] = mm(m.support_a[k]);
            m.support_b[k] = mm(m.support_b[k]);
        }
        out.push_back(m);
    });
    return out;
}

MedialGraph medial_graph(const Polygons& region) {
    MedialGraph g;
    // Nodes keyed by their position rounded to 1e-9 mm (0.001 units).
    std::map<std::pair<std::int64_t, std::int64_t>, std::uint32_t> index;
    auto node = [&](const Point2d& p, double r) {
        const auto key = std::make_pair(std::llround(p.x * 1e6), std::llround(p.y * 1e6));
        const auto [it, inserted] = index.emplace(key, static_cast<std::uint32_t>(g.nodes.size()));
        if (inserted)
            g.nodes.push_back({p, r, false});
        return it->second;
    };
    for (const MedialEdge& e : medial_axis(region)) {
        const std::uint32_t a = node(e.a, e.radius_a);
        const std::uint32_t b = node(e.b, e.radius_b);
        if (a != b)
            g.edges.push_back({a, b, e.curved, false, {e.support_a[0], e.support_a[1]}, {e.support_b[0], e.support_b[1]}});
    }
    return g;
}

void mark_central(MedialGraph& g, double alpha_max_degrees) {
    const double limit = std::cos(alpha_max_degrees * std::numbers::pi / 360.0); // cos(alpha_max / 2)
    std::vector<double> best_neighbour(g.nodes.size(), -1.0);
    for (MedialGraph::Edge& e : g.edges) {
        const MedialGraph::Node& a = g.nodes[e.a];
        const MedialGraph::Node& b = g.nodes[e.b];
        const double len = std::hypot(b.p.x - a.p.x, b.p.y - a.p.y);
        e.central = len > 0.0 && std::fabs(b.radius - a.radius) / len < limit;
        best_neighbour[e.a] = std::max(best_neighbour[e.a], b.radius);
        best_neighbour[e.b] = std::max(best_neighbour[e.b], a.radius);
    }
    for (std::size_t i = 0; i < g.nodes.size(); ++i)
        g.nodes[i].central = best_neighbour[i] >= 0.0 && g.nodes[i].radius > best_neighbour[i];
    for (const MedialGraph::Edge& e : g.edges)
        if (e.central)
            g.nodes[e.a].central = g.nodes[e.b].central = true;
}

void filter_marking(MedialGraph& g, double max_length) {
    std::vector<std::vector<std::uint32_t>> incident(g.nodes.size());
    for (std::uint32_t e = 0; e < g.edges.size(); ++e) {
        incident[g.edges[e].a].push_back(e);
        incident[g.edges[e].b].push_back(e);
    }
    auto other = [&](std::uint32_t e, std::uint32_t n) { return g.edges[e].a == n ? g.edges[e].b : g.edges[e].a; };
    auto length = [&](std::uint32_t e) {
        const Point2d& a = g.nodes[g.edges[e].a].p;
        const Point2d& b = g.nodes[g.edges[e].b].p;
        return std::hypot(b.x - a.x, b.y - a.y);
    };
    // Depth-first walk up along non-central edges, bounded by max_length.
    std::vector<std::uint32_t> path;
    std::vector<std::uint32_t> to_mark;
    auto walk = [&](auto&& self, std::uint32_t node, double travelled) -> void {
        for (std::uint32_t e : incident[node]) {
            if (g.edges[e].central)
                continue;
            const std::uint32_t next = other(e, node);
            if (g.nodes[next].radius <= g.nodes[node].radius)
                continue; // only upward
            const double total = travelled + length(e);
            if (total > max_length)
                continue;
            path.push_back(e);
            if (g.nodes[next].central)
                to_mark.insert(to_mark.end(), path.begin(), path.end());
            else
                self(self, next, total);
            path.pop_back();
        }
    };
    for (std::uint32_t n = 0; n < g.nodes.size(); ++n)
        if (g.nodes[n].central)
            walk(walk, n, 0.0);
    for (std::uint32_t e : to_mark) {
        g.edges[e].central = true;
        g.nodes[g.edges[e].a].central = g.nodes[g.edges[e].b].central = true;
    }
}

} // namespace tisma
