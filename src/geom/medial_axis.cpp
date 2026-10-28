// Medial axis from the segment Voronoi diagram: the own one (src/geom/voronoi.*),
// with Boost.Polygon's (BSL-1.0) kept as the reference the tests compare to.
// Discretization of parabolic edges and the central marking follow Kuipers et
// al. 2020, Sections 3.2 and 3.3. Own design; see PROVENANCE.md.

#include "geom/medial_axis.hpp"

#include "geom/voronoi.hpp"

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

// At most this many pieces per edge (a 20 m edge at 0.2 mm), against
// degenerate input.
constexpr std::size_t kMaxPieces = 100000;

// The region's contours as segments, each knowing its neighbours along the
// contour. Collinear points and spikes make degenerate cells (a corner on its
// neighbour segment's line, where the parabola has no width): they go.
// The contour without corners closer than kSpeck to the last one kept: an
// offset by a hair (a fraction of a micrometre) leaves zig-zags of 1 nm edges
// at its corners, which neither Voronoi diagram takes (fuzz, 2026-10-25), and
// which no bead could follow anyway.
constexpr std::int64_t kSpeck = 10; // nm

Polygon without_specks(const Polygon& path) {
    Polygon out;
    out.reserve(path.size());
    const auto near = [](const Clipper2Lib::Point64& a, const Clipper2Lib::Point64& b) {
        return std::llabs(a.x - b.x) <= kSpeck && std::llabs(a.y - b.y) <= kSpeck;
    };
    for (const auto& p : path)
        if (out.empty() || !near(p, out.back()))
            out.push_back(p);
    while (out.size() > 1 && near(out.back(), out.front()))
        out.pop_back();
    return out;
}

std::vector<Segment> contour_segments(const Polygons& region) {
    constexpr std::int64_t kLimit = std::numeric_limits<std::int32_t>::max();
    std::vector<Segment> segments;
    for (const Polygon& raw : region) {
        const Polygon path = Clipper2Lib::TrimCollinear(without_specks(raw), false);
        if (path.size() < 3)
            continue;
        const std::uint32_t first = static_cast<std::uint32_t>(segments.size());
        for (std::size_t i = 0; i < path.size(); ++i) {
            const auto& p = path[i];
            const auto& q = path[(i + 1) % path.size()];
            if (std::llabs(p.x) > kLimit || std::llabs(p.y) > kLimit)
                throw std::out_of_range("medial_axis: coordinates beyond the 32-bit range");
            if (p != q)
                segments.push_back({p, q});
        }
        const std::uint32_t count = static_cast<std::uint32_t>(segments.size()) - first;
        for (std::uint32_t k = 0; k < count; ++k) {
            segments[first + k].prev = first + (k + count - 1) % count;
            segments[first + k].next = first + (k + 1) % count;
        }
    }
    return segments;
}

// One side of a Voronoi edge: a segment, or the start or end point of one.
struct Cell {
    enum Kind : std::uint8_t { Segment, Start, End };
    std::uint32_t segment;
    Kind kind;
    bool point() const { return kind != Segment; }
};

// Turns Voronoi edges into medial-axis pieces: keeps those inside the region,
// cuts parabolas and corner-to-corner edges into short pieces, and gives each
// end its radius and supports. Works the same for either diagram.
class Pieces {
public:
    explicit Pieces(const std::vector<Segment>& segments) : segments_(segments) {}

    // Set when a parabolic edge ends off its parabola: the diagram placed a
    // node that is not equidistant from its sites. medial_axis then takes the
    // reference diagram for the whole region.
    mutable bool off_bisector = false;

    // The edge from a to b between the cells c0 and c1 (in units).
    template <typename Emit>
    void edge(const Point2d& a, const Point2d& b, Cell c0, Cell c1, bool curved, Emit&& emit) const {
        if (!interior(c0, a, b) || !interior(c1, a, b))
            return;
        std::vector<Point2d> chain;
        if (curved)
            chain = c0.point() ? discretize(c0, c1, a, b) : discretize(c1, c0, a, b);
        else if (c0.point() && c1.point())
            chain = subdivide(a, b); // straight, but R grows like a hyperbola along it
        else
            chain = {a, b};
        for (std::size_t i = 0; i + 1 < chain.size(); ++i) {
            MedialEdge m;
            m.a = chain[i];
            m.b = chain[i + 1];
            m.radius_a = radius(c0, m.a);
            m.radius_b = radius(c0, m.b);
            m.curved = curved;
            m.support_a[0] = support(c0, m.a);
            m.support_a[1] = support(c1, m.a);
            m.support_b[0] = support(c0, m.b);
            m.support_b[1] = support(c1, m.b);
            emit(m);
        }
    }

private:
    Clipper2Lib::Point64 site_point(Cell c) const {
        const Segment& s = segments_[c.segment];
        return c.kind == Cell::Start ? s.p : s.q;
    }

    // Distance from a point to the site of a cell (a segment or one of its ends).
    double radius(Cell c, const Point2d& v) const {
        if (c.point()) {
            const auto p = site_point(c);
            return std::hypot(v.x - double(p.x), v.y - double(p.y));
        }
        return distance_to_segment(v.x, v.y, segments_[c.segment]);
    }

    // Closest point of a cell's site to v: the corner, or the foot of the
    // perpendicular on the segment (clamped to it).
    Point2d support(Cell c, const Point2d& v) const {
        if (c.point()) {
            const auto p = site_point(c);
            return {double(p.x), double(p.y)};
        }
        const Segment& s = segments_[c.segment];
        const double px = double(s.p.x), py = double(s.p.y), dx = double(s.q.x) - px, dy = double(s.q.y) - py;
        const double len2 = dx * dx + dy * dy;
        double t = len2 > 0.0 ? std::clamp(((v.x - px) * dx + (v.y - py) * dy) / len2, 0.0, 1.0) : 0.0;
        // A foot within kAtEnd of an end is that end, exactly: the trapezoid
        // of the corner's own cell then names the same support point.
        constexpr double kAtEnd = 1e-3; // units
        const double len = std::sqrt(len2);
        if (t * len <= kAtEnd)
            t = 0.0;
        else if ((1.0 - t) * len <= kAtEnd)
            t = 1.0;
        return {px + t * dx, py + t * dy};
    }

    // Whether the part of the edge from a to b in this cell lies inside the
    // region. The interior is on the left of every contour segment (outer
    // contours counter-clockwise, holes clockwise). A segment's cell is inside
    // on the segment's left; a corner's cell is inside only at a reflex corner.
    bool interior(Cell c, const Point2d& a, const Point2d& b) const {
        const Segment& s = segments_[c.segment];
        if (c.point()) {
            const Segment& in = c.kind == Cell::Start ? segments_[s.prev] : s;
            const Segment& out = c.kind == Cell::Start ? s : segments_[s.next];
            // Reflex corner: the contour turns right at it.
            return cross(double(in.p.x), double(in.p.y), double(in.q.x), double(in.q.y), double(out.q.x),
                         double(out.q.y)) < 0.0;
        }
        // A vertex at the segment's end, such as the start of a spoke from a
        // convex corner, is computed in floating point and may land a hair to
        // the right: accept it within kOnLine of the line.
        constexpr double kOnLine = 1.0; // units (1 nm)
        const double px = double(s.p.x), py = double(s.p.y), qx = double(s.q.x), qy = double(s.q.y);
        const double slack = -kOnLine * std::hypot(qx - px, qy - py);
        return cross(px, py, qx, qy, a.x, a.y) >= slack && cross(px, py, qx, qy, b.x, b.y) >= slack;
    }

    // Points along the bisector of two corners, at most kMaxCurvedPiece apart:
    // the edge is straight but its radius is not linear along it, and later
    // steps interpolate the radius linearly between nodes.
    static std::vector<Point2d> subdivide(const Point2d& a, const Point2d& b) {
        const double max_piece = kMaxCurvedPiece * kUnitsPerMm;
        const double length = std::hypot(b.x - a.x, b.y - a.y);
        if (!std::isfinite(length) || length > kMaxPieces * max_piece)
            return {a, b}; // degenerate input (a hair-thin channel): leave the edge whole
        const std::size_t pieces = std::max<std::size_t>(1, std::size_t(std::ceil(length / max_piece)));
        std::vector<Point2d> chain{a};
        for (std::size_t i = 1; i < pieces; ++i) {
            const double t = double(i) / double(pieces);
            chain.push_back({a.x + t * (b.x - a.x), a.y + t * (b.y - a.y)});
        }
        chain.push_back(b);
        return chain;
    }

    // Points along a parabolic edge (equidistant from a corner and a segment's
    // line), at most kMaxCurvedPiece apart.
    std::vector<Point2d> discretize(Cell point_cell, Cell segment_cell, const Point2d& a, const Point2d& b) const {
        const auto focus = site_point(point_cell);
        const Segment& s = segments_[segment_cell.segment];
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
        if (!(fv > 1e-9 * len))
            return {a, b}; // the corner is on the segment's line: no parabola, a straight piece
        auto along = [&](const Point2d& p) { return (p.x - sx) * ux + (p.y - sy) * uy; };
        // On the parabola, height above the line v = ((u - fu)^2 + fv^2) / (2 fv).
        auto at = [&](double u) {
            const double v = ((u - fu) * (u - fu) + fv * fv) / (2.0 * fv);
            return Point2d{sx + u * ux + v * nx, sy + u * uy + v * ny};
        };
        const double u0 = along(a), u1 = along(b);
        // Both ends must be on the parabola, or the refinement below never
        // ends (the first and last chords do not shrink): 1 um of slack.
        constexpr double kOnParabola = 1000.0; // units
        const Point2d pa = at(u0), pb = at(u1);
        if (std::hypot(pa.x - a.x, pa.y - a.y) > kOnParabola || std::hypot(pb.x - b.x, pb.y - b.y) > kOnParabola) {
            off_bisector = true;
            return {a, b};
        }
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
            if (short_enough || pieces > kMaxPieces)
                return chain;
            pieces *= 2;
        }
    }

    const std::vector<Segment>& segments_;
};

// The pieces from Boost.Polygon's segment Voronoi diagram (the reference).
template <typename Emit>
void boost_pieces(const std::vector<Segment>& segments, Emit&& emit) {
    Diagram vd;
    boost::polygon::construct_voronoi(segments.begin(), segments.end(), &vd);
    const Pieces pieces(segments);
    auto cell_of = [](const Diagram::cell_type& c) {
        const Cell::Kind kind = !c.contains_point()                                                      ? Cell::Segment
                                : c.source_category() == boost::polygon::SOURCE_CATEGORY_SEGMENT_START_POINT ? Cell::Start
                                                                                                         : Cell::End;
        return Cell{static_cast<std::uint32_t>(c.source_index()), kind};
    };
    for (const auto& edge : vd.edges()) {
        // Each edge is stored twice (one per side); keep one of the pair.
        if (&edge > edge.twin() || !edge.is_finite() || !edge.is_primary())
            continue;
        const Point2d a{edge.vertex0()->x(), edge.vertex0()->y()};
        const Point2d b{edge.vertex1()->x(), edge.vertex1()->y()};
        pieces.edge(a, b, cell_of(*edge.cell()), cell_of(*edge.twin()->cell()), edge.is_curved(), emit);
    }
}

// A small deterministic mixer (the splitmix64 finaliser, public-domain
// constants), so that the insertion order is the same on every platform.
std::uint64_t mix(std::uint64_t z) {
    z += 0x9e3779b97f4a7c15ull;
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ull;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebull;
    return z ^ (z >> 31);
}

// Fisher-Yates with the mixer above.
void shuffle(std::vector<std::uint32_t>& order) {
    std::uint64_t state = 0x7461736d61ull; // fixed seed: the same diagram every run
    for (std::size_t i = order.size(); i > 1; --i) {
        state = mix(state);
        std::swap(order[i - 1], order[std::size_t(state % i)]);
    }
}

// The pieces from the own Voronoi diagram (src/geom/voronoi.*): corners first,
// then segments, each in a random order (Held and Huber 2009, 2.1).
template <typename Emit>
bool own_pieces(const std::vector<Segment>& segments, Emit&& emit) {
    if (segments.empty())
        return true;
    double min_x = 1e300, min_y = 1e300, max_x = -1e300, max_y = -1e300;
    for (const Segment& s : segments) {
        min_x = std::min(min_x, double(s.p.x)), max_x = std::max(max_x, double(s.p.x));
        min_y = std::min(min_y, double(s.p.y)), max_y = std::max(max_y, double(s.p.y));
    }
    VoronoiDiagram vd(min_x, min_y, max_x, max_y);
    // Every corner is the start of one segment; a corner where two contours
    // touch is one site, named after the first of its segments.
    std::vector<std::uint32_t> order(segments.size());
    for (std::uint32_t k = 0; k < order.size(); ++k)
        order[k] = k;
    shuffle(order);
    std::map<std::pair<std::int64_t, std::int64_t>, std::int32_t> site_at;
    std::vector<Cell> cell_of_site;
    auto cell_site = [&](std::int32_t site, Cell c) {
        if (std::size_t(site) >= cell_of_site.size())
            cell_of_site.resize(std::size_t(site) + 1, Cell{0, Cell::Segment});
        cell_of_site[std::size_t(site)] = c;
    };
    for (std::uint32_t k : order) {
        const auto& p = segments[k].p;
        if (site_at.count({p.x, p.y}))
            continue;
        const std::int32_t site = vd.insert_point(double(p.x), double(p.y));
        site_at[{p.x, p.y}] = site;
        cell_site(site, Cell{k, Cell::Start});
    }
    shuffle(order);
    for (std::uint32_t k : order) {
        const Segment& s = segments[k];
        const std::int32_t site = vd.insert_segment(site_at.at({s.p.x, s.p.y}), site_at.at({s.q.x, s.q.y}));
        cell_site(site, Cell{k, Cell::Segment});
    }
    const Pieces pieces(segments);
    const auto& sites = vd.sites();
    const auto& halves = vd.halves();
    const auto& nodes = vd.nodes();
    // A vertex where more than three edges meet (the centre of a regular
    // polygon) is a bunch of degree-three nodes joined by edges of next to
    // no length (Held and Huber 2009, 2.1); rounding can also leave them up
    // to a hair apart. Nodes joined by an edge shorter than kMerge become one
    // node, at the position of one of them, as in Boost's exact diagram.
    constexpr double kMerge = 1.0; // units (1 nm)
    const double merge = std::max(kMerge, 1e-9 * std::max({max_x - min_x, max_y - min_y, 1.0}));
    std::vector<std::int32_t> parent(nodes.size());
    for (std::size_t n = 0; n < parent.size(); ++n)
        parent[n] = std::int32_t(n);
    auto root = [&](std::int32_t n) {
        while (parent[std::size_t(n)] != n)
            n = parent[std::size_t(n)] = parent[std::size_t(parent[std::size_t(n)])];
        return n;
    };
    for (std::size_t h = 0; h < halves.size(); h += 2) {
        if (!halves[h].alive)
            continue;
        const std::int32_t a = halves[h].origin, b = halves[h + 1].origin;
        const auto& na = nodes[std::size_t(a)];
        const auto& nb = nodes[std::size_t(b)];
        if (std::hypot(nb.x - na.x, nb.y - na.y) <= merge) {
            const std::int32_t ra = root(a), rb = root(b);
            if (ra != rb)
                parent[std::size_t(std::max(ra, rb))] = std::min(ra, rb);
        }
    }
    for (std::size_t h = 0; h < halves.size(); h += 2) {
        if (!halves[h].alive)
            continue;
        const std::int32_t l = halves[h].site, r = halves[h + 1].site;
        if (l < 0 || r < 0)
            continue;
        const auto& sl = sites[std::size_t(l)];
        const auto& sr = sites[std::size_t(r)];
        using Kind = VoronoiDiagram::SiteKind;
        if (sl.kind == Kind::Dummy || sr.kind == Kind::Dummy)
            continue;
        const bool lseg = sl.kind == Kind::Segment, rseg = sr.kind == Kind::Segment;
        // A segment and its own end: the line at right angles through the end
        // (Boost calls it a secondary edge); not part of the medial axis.
        if ((lseg && !rseg && (r == sl.a || r == sl.b)) || (rseg && !lseg && (l == sr.a || l == sr.b)))
            continue;
        const std::int32_t ra = root(halves[h].origin), rb = root(halves[h + 1].origin);
        if (ra == rb)
            continue; // inside a bunch of coinciding nodes
        const auto& na = nodes[std::size_t(ra)];
        const auto& nb = nodes[std::size_t(rb)];
        if (na.fixed || nb.fixed)
            continue;
        pieces.edge({na.x, na.y}, {nb.x, nb.y}, cell_of_site[std::size_t(l)], cell_of_site[std::size_t(r)],
                    lseg != rseg, emit);
    }
    return !pieces.off_bisector;
}

std::vector<MedialEdge> to_millimetres(std::vector<MedialEdge> out) {
    auto mm = [](const Point2d& p) { return Point2d{p.x / kUnitsPerMm, p.y / kUnitsPerMm}; };
    for (MedialEdge& m : out) {
        m.a = mm(m.a);
        m.b = mm(m.b);
        m.radius_a /= kUnitsPerMm;
        m.radius_b /= kUnitsPerMm;
        for (int k = 0; k < 2; ++k) {
            m.support_a[k] = mm(m.support_a[k]);
            m.support_b[k] = mm(m.support_b[k]);
        }
    }
    return out;
}

} // namespace

std::vector<MedialEdge> medial_axis(const Polygons& region) {
    const std::vector<Segment> segments = contour_segments(region);
    std::vector<MedialEdge> out;
    bool good = false;
    try {
        good = own_pieces(segments, [&](const MedialEdge& m) { out.push_back(m); });
    } catch (const std::logic_error&) {
        // A region the own diagram cannot build (contours touching at corners
        // around a pocket, the review of 2026-10-25): the reference does.
    }
    if (!good) {
        out.clear();
        boost_pieces(segments, [&](const MedialEdge& m) { out.push_back(m); });
    }
    return to_millimetres(std::move(out));
}

std::vector<MedialEdge> medial_axis_reference(const Polygons& region) {
    std::vector<MedialEdge> out;
    boost_pieces(contour_segments(region), [&](const MedialEdge& m) { out.push_back(m); });
    return to_millimetres(std::move(out));
}

MedialGraph medial_graph(const Polygons& region) {
    MedialGraph g;
    // Edges that meet at a Voronoi vertex read the same coordinates, so their
    // ends are equal bit for bit: nodes are keyed by exact position.
    std::map<std::pair<double, double>, std::uint32_t> index;
    auto node = [&](const Point2d& p, double r) {
        const auto [it, inserted] = index.emplace(std::make_pair(p.x, p.y), static_cast<std::uint32_t>(g.nodes.size()));
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
        // A hair of slack: a wedge exactly as sharp as the limit is central.
        e.central = len > 0.0 && std::fabs(b.radius - a.radius) / len < limit * (1.0 + 1e-6);
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
    // Depth-first walk up along non-central edges, bounded by max_length,
    // with its own stack: a long chain of short edges would overflow the
    // call stack (the review of 2026-10-25).
    std::vector<std::uint32_t> path;
    std::vector<std::uint32_t> to_mark;
    struct Frame {
        std::uint32_t node;
        double travelled;
        std::size_t next_edge; // index into incident[node]
    };
    std::vector<Frame> stack;
    for (std::uint32_t n = 0; n < g.nodes.size(); ++n) {
        if (!g.nodes[n].central)
            continue;
        stack.push_back({n, 0.0, 0});
        while (!stack.empty()) {
            Frame& f = stack.back();
            if (f.next_edge == incident[f.node].size()) {
                stack.pop_back();
                if (!path.empty())
                    path.pop_back();
                continue;
            }
            const std::uint32_t e = incident[f.node][f.next_edge++];
            if (g.edges[e].central)
                continue;
            const std::uint32_t next = other(e, f.node);
            if (g.nodes[next].radius <= g.nodes[f.node].radius)
                continue; // only upward
            const double total = f.travelled + length(e);
            if (total > max_length)
                continue;
            path.push_back(e);
            if (g.nodes[next].central) {
                to_mark.insert(to_mark.end(), path.begin(), path.end());
                path.pop_back();
            } else {
                stack.push_back({next, total, 0}); // f is not used after this
            }
        }
    }
    for (std::uint32_t e : to_mark) {
        g.edges[e].central = true;
        g.nodes[g.edges[e].a].central = g.nodes[g.edges[e].b].central = true;
    }
}

} // namespace tisma
