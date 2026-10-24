// Medial axis from the segment Voronoi diagram (Boost.Polygon, BSL-1.0).
// Own design; see PROVENANCE.md.

#include "geom/medial_axis.hpp"

#include <boost/polygon/voronoi.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>

namespace tisma {
namespace {

struct Segment {
    Clipper2Lib::Point64 p, q;
};

double distance_to_segment(double x, double y, const Segment& s) {
    const double px = double(s.p.x), py = double(s.p.y), qx = double(s.q.x), qy = double(s.q.y);
    const double dx = qx - px, dy = qy - py;
    const double len2 = dx * dx + dy * dy;
    double t = len2 > 0.0 ? ((x - px) * dx + (y - py) * dy) / len2 : 0.0;
    t = std::clamp(t, 0.0, 1.0);
    return std::hypot(x - (px + t * dx), y - (py + t * dy));
}

// Non-zero winding of the region at (x, y), in units.
bool inside(const Polygons& region, double x, double y) {
    int winding = 0;
    for (const Polygon& path : region) {
        const std::size_t n = path.size();
        for (std::size_t i = 0; i < n; ++i) {
            const auto& a = path[i];
            const auto& b = path[(i + 1) % n];
            const double ay = double(a.y), by = double(b.y);
            const double cross = (double(b.x) - double(a.x)) * (y - ay) - (x - double(a.x)) * (by - ay);
            if (ay <= y) {
                if (by > y && cross > 0)
                    ++winding;
            } else if (by <= y && cross < 0) {
                --winding;
            }
        }
    }
    return winding != 0;
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

std::vector<MedialEdge> medial_axis(const Polygons& region) {
    std::vector<Segment> segments;
    constexpr std::int64_t kLimit = std::numeric_limits<std::int32_t>::max();
    for (const Polygon& path : region)
        for (std::size_t i = 0; i < path.size(); ++i) {
            const auto& p = path[i];
            const auto& q = path[(i + 1) % path.size()];
            if (std::llabs(p.x) > kLimit || std::llabs(p.y) > kLimit)
                throw std::out_of_range("medial_axis: coordinates beyond the 32-bit range");
            if (p != q)
                segments.push_back({p, q});
        }

    boost::polygon::voronoi_diagram<double> vd;
    boost::polygon::construct_voronoi(segments.begin(), segments.end(), &vd);

    // Distance from a point to the site of a cell (a segment or one of its ends).
    auto radius = [&](const boost::polygon::voronoi_diagram<double>::cell_type& cell, double x, double y) {
        const Segment& s = segments[cell.source_index()];
        switch (cell.source_category()) {
        case boost::polygon::SOURCE_CATEGORY_SEGMENT_START_POINT:
            return std::hypot(x - double(s.p.x), y - double(s.p.y));
        case boost::polygon::SOURCE_CATEGORY_SEGMENT_END_POINT:
            return std::hypot(x - double(s.q.x), y - double(s.q.y));
        default:
            return distance_to_segment(x, y, s);
        }
    };

    std::vector<MedialEdge> out;
    for (const auto& edge : vd.edges()) {
        // Each edge is stored twice (one per side); keep one of the pair.
        if (&edge > edge.twin() || !edge.is_finite() || !edge.is_primary())
            continue;
        const auto* v0 = edge.vertex0();
        const auto* v1 = edge.vertex1();
        if (!inside(region, v0->x(), v0->y()) && !inside(region, v1->x(), v1->y()))
            continue;
        // Both ends must be inside or on the boundary.
        const bool in0 = inside(region, v0->x(), v0->y()) || radius(*edge.cell(), v0->x(), v0->y()) < 1.0;
        const bool in1 = inside(region, v1->x(), v1->y()) || radius(*edge.cell(), v1->x(), v1->y()) < 1.0;
        if (!in0 || !in1)
            continue;
        MedialEdge m;
        m.a = {v0->x() / kUnitsPerMm, v0->y() / kUnitsPerMm};
        m.b = {v1->x() / kUnitsPerMm, v1->y() / kUnitsPerMm};
        m.radius_a = radius(*edge.cell(), v0->x(), v0->y()) / kUnitsPerMm;
        m.radius_b = radius(*edge.cell(), v1->x(), v1->y()) / kUnitsPerMm;
        m.curved = edge.is_curved();
        out.push_back(m);
    }
    return out;
}

} // namespace tisma
