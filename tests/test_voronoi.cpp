#include "geom/voronoi.hpp"

#include <boost/polygon/voronoi.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <algorithm>
#include <cstdint>
#include <string>
#include <random>
#include <set>
#include <utility>
#include <vector>

using namespace tisma;

namespace {

struct IntPoint {
    std::int32_t x, y;
};
struct IntSegment {
    IntPoint p, q;
};

} // namespace

namespace boost::polygon {
template <>
struct geometry_concept<IntPoint> {
    using type = point_concept;
};
template <>
struct point_traits<IntPoint> {
    using coordinate_type = std::int32_t;
    static coordinate_type get(const IntPoint& p, orientation_2d o) { return o == HORIZONTAL ? p.x : p.y; }
};
template <>
struct geometry_concept<IntSegment> {
    using type = segment_concept;
};
template <>
struct segment_traits<IntSegment> {
    using coordinate_type = std::int32_t;
    using point_type = IntPoint;
    static point_type get(const IntSegment& s, direction_1d d) { return d.to_int() ? s.q : s.p; }
};
} // namespace boost::polygon

namespace {

// Nodes of the own diagram between three input points (no dummy site around them).
std::vector<std::pair<double, double>> real_nodes(const VoronoiDiagram& vd) {
    std::vector<std::pair<double, double>> out;
    for (const auto& n : vd.nodes()) {
        if (!n.alive || n.fixed)
            continue;
        bool real = true;
        std::int32_t h = n.half;
        for (int i = 0; i < 3; ++i, h = vd.clockwise(h)) {
            const std::int32_t s = vd.halves()[std::size_t(h)].site;
            real = real && s >= 0 && vd.sites()[std::size_t(s)].kind != VoronoiDiagram::SiteKind::Dummy;
        }
        if (real)
            out.emplace_back(n.x, n.y);
    }
    return out;
}

// Positions rounded to a grid of `tolerance`, so that coinciding nodes count once.
std::set<std::pair<long long, long long>> snapped(const std::vector<std::pair<double, double>>& points, double tolerance) {
    std::set<std::pair<long long, long long>> out;
    for (const auto& [x, y] : points)
        out.emplace(std::llround(x / tolerance), std::llround(y / tolerance));
    return out;
}

// Every point of `a` has one of `b` within `tolerance` (looked up in the 3 x 3 snapped neighbourhood).
bool covered(const std::vector<std::pair<double, double>>& a, const std::vector<std::pair<double, double>>& b,
             double tolerance) {
    const auto grid = snapped(b, tolerance);
    for (const auto& [x, y] : a) {
        const long long gx = std::llround(x / tolerance), gy = std::llround(y / tolerance);
        bool found = false;
        for (long long dx = -1; dx <= 1 && !found; ++dx)
            for (long long dy = -1; dy <= 1 && !found; ++dy)
                found = grid.count({gx + dx, gy + dy}) > 0;
        if (!found)
            return false;
    }
    return true;
}

// The own diagram and Boost's of the same points: the same Voronoi vertices.
void compare_with_boost(const std::vector<IntPoint>& points, double tolerance) {
    double min_x = 1e300, min_y = 1e300, max_x = -1e300, max_y = -1e300;
    for (const IntPoint& p : points) {
        min_x = std::min(min_x, double(p.x)), max_x = std::max(max_x, double(p.x));
        min_y = std::min(min_y, double(p.y)), max_y = std::max(max_y, double(p.y));
    }
    VoronoiDiagram vd(min_x, min_y, max_x, max_y);
    for (const IntPoint& p : points)
        vd.insert_point(p.x, p.y);
    REQUIRE(vd.check().empty());
    boost::polygon::voronoi_diagram<double> reference;
    boost::polygon::construct_voronoi(points.begin(), points.end(), &reference);
    // Within the input's box: far outside, circles through three input points
    // can hold a dummy site, and the own diagram is not meant to be exact there.
    auto in_box = [&](double x, double y) {
        return x >= min_x - tolerance && x <= max_x + tolerance && y >= min_y - tolerance && y <= max_y + tolerance;
    };
    std::vector<std::pair<double, double>> expected, own;
    for (const auto& v : reference.vertices())
        if (in_box(v.x(), v.y()))
            expected.emplace_back(v.x(), v.y());
    for (const auto& [x, y] : real_nodes(vd))
        if (in_box(x, y))
            own.emplace_back(x, y);
    CHECK(expected.size() > 0);
    CHECK(covered(own, expected, tolerance));
    CHECK(covered(expected, own, tolerance));
}

} // namespace

TEST_CASE("own Voronoi diagram: three points have one node at their circumcentre", "[voronoi]") {
    VoronoiDiagram vd(0, 0, 10, 10);
    vd.insert_point(0, 0);
    vd.insert_point(10, 0);
    vd.insert_point(0, 10);
    REQUIRE(vd.check().empty());
    const auto nodes = real_nodes(vd);
    REQUIRE(nodes.size() == 1);
    CHECK(std::abs(nodes[0].first - 5.0) < 1e-9);
    CHECK(std::abs(nodes[0].second - 5.0) < 1e-9);
    CHECK_THROWS(vd.insert_point(10, 0));
}

TEST_CASE("own Voronoi diagram of random points matches Boost", "[voronoi]") {
    std::mt19937 rng(12345);
    std::uniform_int_distribution<std::int32_t> coordinate(-1000000, 1000000);
    std::vector<IntPoint> points;
    std::set<std::pair<std::int32_t, std::int32_t>> seen;
    while (points.size() < 3000) {
        const IntPoint p{coordinate(rng), coordinate(rng)};
        if (seen.insert({p.x, p.y}).second)
            points.push_back(p);
    }
    compare_with_boost(points, 1e-3);
}

TEST_CASE("own Voronoi diagram of a grid (many four-point circles) matches Boost", "[voronoi]") {
    std::vector<IntPoint> points;
    for (int i = 0; i < 40; ++i)
        for (int j = 0; j < 40; ++j)
            points.push_back({i * 1000, j * 1000});
    compare_with_boost(points, 1e-3);
}

TEST_CASE("own Voronoi diagram of a polygon's corners: a 64-gon and a star", "[voronoi]") {
    std::vector<IntPoint> circle, star;
    for (int i = 0; i < 64; ++i) {
        const double a = 2.0 * 3.14159265358979323846 * i / 64;
        circle.push_back({std::int32_t(std::lround(5e6 * std::cos(a))), std::int32_t(std::lround(5e6 * std::sin(a)))});
        const double r = i % 2 == 0 ? 5e6 : 2e6;
        star.push_back({std::int32_t(std::lround(r * std::cos(a))), std::int32_t(std::lround(r * std::sin(a)))});
    }
    compare_with_boost(circle, 1.0);
    compare_with_boost(star, 1.0);
}

namespace {

// The own diagram of polygons' edges (corners first, then segments) and
// Boost's segment Voronoi diagram: the same vertices inside the input's box.
void compare_polygons(const std::vector<std::vector<IntPoint>>& polygons, double tolerance) {
    double min_x = 1e300, min_y = 1e300, max_x = -1e300, max_y = -1e300;
    std::vector<IntSegment> segments;
    for (const auto& polygon : polygons)
        for (std::size_t i = 0; i < polygon.size(); ++i) {
            const IntPoint& p = polygon[i];
            min_x = std::min(min_x, double(p.x)), max_x = std::max(max_x, double(p.x));
            min_y = std::min(min_y, double(p.y)), max_y = std::max(max_y, double(p.y));
            segments.push_back({p, polygon[(i + 1) % polygon.size()]});
        }
    VoronoiDiagram vd(min_x, min_y, max_x, max_y);
    std::vector<std::vector<std::int32_t>> ids;
    for (const auto& polygon : polygons) {
        ids.emplace_back();
        for (const IntPoint& p : polygon)
            ids.back().push_back(vd.insert_point(p.x, p.y));
    }
    REQUIRE(vd.check().empty());
    for (const auto& polygon : ids)
        for (std::size_t i = 0; i < polygon.size(); ++i) {
            vd.insert_segment(polygon[i], polygon[(i + 1) % polygon.size()]);
            const std::string problem = vd.check();
            INFO("after segment " << i << ": " << problem);
            REQUIRE(problem.empty());
        }
    boost::polygon::voronoi_diagram<double> reference;
    boost::polygon::construct_voronoi(segments.begin(), segments.end(), &reference);
    auto in_box = [&](double x, double y) {
        return x >= min_x - tolerance && x <= max_x + tolerance && y >= min_y - tolerance && y <= max_y + tolerance;
    };
    std::vector<std::pair<double, double>> expected, own;
    for (const auto& v : reference.vertices())
        if (in_box(v.x(), v.y()))
            expected.emplace_back(v.x(), v.y());
    for (const auto& [x, y] : real_nodes(vd))
        if (in_box(x, y))
            own.emplace_back(x, y);
    INFO("own " << own.size() << " nodes, Boost " << expected.size() << " vertices");
    CHECK(expected.size() > 0);
    CHECK(covered(own, expected, tolerance));
    CHECK(covered(expected, own, tolerance));
}

std::vector<IntPoint> regular(int n, double radius, double phase = 0.0) {
    std::vector<IntPoint> out;
    for (int i = 0; i < n; ++i) {
        const double a = phase + 2.0 * 3.14159265358979323846 * i / n;
        out.push_back({std::int32_t(std::lround(radius * std::cos(a))), std::int32_t(std::lround(radius * std::sin(a)))});
    }
    return out;
}

} // namespace

TEST_CASE("own segment Voronoi diagram: a square and an L", "[voronoi]") {
    compare_polygons({{{0, 0}, {10000000, 0}, {10000000, 10000000}, {0, 10000000}}}, 1.0);
    compare_polygons({{{0, 0}, {10000000, 0}, {10000000, 4000000}, {4000000, 4000000}, {4000000, 10000000}, {0, 10000000}}},
                     1.0);
}

TEST_CASE("own segment Voronoi diagram: a 64-gon and a star", "[voronoi]") {
    compare_polygons({regular(64, 5e6)}, 2.0);
    std::vector<IntPoint> star;
    for (int i = 0; i < 20; ++i) {
        const double a = 2.0 * 3.14159265358979323846 * i / 20, r = i % 2 == 0 ? 5e6 : 2e6;
        star.push_back({std::int32_t(std::lround(r * std::cos(a))), std::int32_t(std::lround(r * std::sin(a)))});
    }
    compare_polygons({star}, 2.0);
}

TEST_CASE("own segment Voronoi diagram: a square with a hole, and a comb", "[voronoi]") {
    std::vector<IntPoint> hole = regular(4, 3e6, 0.3);
    std::reverse(hole.begin(), hole.end()); // clockwise
    compare_polygons({{{-10000000, -10000000}, {10000000, -10000000}, {10000000, 10000000}, {-10000000, 10000000}}, hole},
                     2.0);
    std::vector<IntPoint> comb{{0, 0}, {21000000, 0}, {21000000, 8000000}};
    for (int k = 9; k >= 0; --k) {
        const std::int32_t x = k * 2000000 + 1000000;
        comb.push_back({x + 1000000, 8000000});
        comb.push_back({x + 1000000, 2000000});
        comb.push_back({x, 2000000});
        comb.push_back({x, 8000000});
    }
    comb.push_back({0, 8000000});
    compare_polygons({comb}, 2.0);
}
