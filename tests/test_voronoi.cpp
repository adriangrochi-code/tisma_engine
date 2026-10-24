#include "geom/voronoi.hpp"

#include <boost/polygon/voronoi.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <cstdint>
#include <random>
#include <set>
#include <utility>
#include <vector>

using namespace tisma;

namespace {

struct IntPoint {
    std::int32_t x, y;
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
            real = real && s >= 0 && vd.sites()[std::size_t(s)].kind == VoronoiDiagram::SiteKind::Point;
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
    auto in_box = [&](double x, double y) { return x >= min_x && x <= max_x && y >= min_y && y <= max_y; };
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
