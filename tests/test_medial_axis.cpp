#include "geom/medial_axis.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <algorithm>
#include <cmath>

using namespace tisma;
using Catch::Matchers::WithinAbs;

namespace {

Polygon rect(double x0, double y0, double w, double h, bool clockwise = false) {
    Polygon p = {{to_units(x0), to_units(y0)},
                 {to_units(x0 + w), to_units(y0)},
                 {to_units(x0 + w), to_units(y0 + h)},
                 {to_units(x0), to_units(y0 + h)}};
    if (clockwise)
        std::reverse(p.begin(), p.end());
    return p;
}

double max_radius(const std::vector<MedialEdge>& edges) {
    double r = 0.0;
    for (const MedialEdge& e : edges)
        r = std::max({r, e.radius_a, e.radius_b});
    return r;
}

bool has_point(const std::vector<MedialEdge>& edges, double x, double y) {
    for (const MedialEdge& e : edges)
        if ((std::hypot(e.a.x - x, e.a.y - y) < 1e-6) || (std::hypot(e.b.x - x, e.b.y - y) < 1e-6))
            return true;
    return false;
}

} // namespace

TEST_CASE("medial axis of a square: four edges from the centre to the corners", "[medial]") {
    const auto edges = medial_axis({rect(0, 0, 10, 10)});
    REQUIRE(edges.size() == 4);
    for (const MedialEdge& e : edges) {
        CHECK_FALSE(e.curved);
        const bool a_is_centre = std::hypot(e.a.x - 5, e.a.y - 5) < 1e-6;
        const MedialEdge& m = e;
        CHECK_THAT(a_is_centre ? m.radius_a : m.radius_b, WithinAbs(5.0, 1e-6));
        CHECK_THAT(a_is_centre ? m.radius_b : m.radius_a, WithinAbs(0.0, 1e-6));
    }
    CHECK(has_point(edges, 0, 0));
    CHECK(has_point(edges, 10, 10));
}

TEST_CASE("medial axis of a rectangle: a centre line at half the width", "[medial]") {
    const auto edges = medial_axis({rect(0, 0, 20, 10)});
    CHECK(edges.size() == 5);
    CHECK(has_point(edges, 5, 5));
    CHECK(has_point(edges, 15, 5));
    CHECK_THAT(max_radius(edges), WithinAbs(5.0, 1e-6));
}

TEST_CASE("medial axis of a square ring bends around the hole's corners", "[medial]") {
    // 10 mm square with a centred 4 mm hole: walls 3 mm thick. Along a wall the
    // axis is 1.5 mm from both sides; near each corner of the hole it bends
    // into a parabola (equidistant from the hole's corner and the outer walls),
    // and on the diagonal it reaches t = sqrt(2) (3 - t), t = 3 sqrt(2) / (1 + sqrt(2)).
    const auto edges = medial_axis({rect(0, 0, 10, 10), rect(3, 3, 4, 4, true)});
    const double t = 3.0 * std::sqrt(2.0) / (1.0 + std::sqrt(2.0));
    CHECK_THAT(max_radius(edges), WithinAbs(t, 1e-6));
    CHECK(has_point(edges, t, t));
    CHECK(has_point(edges, 10.0 - t, 10.0 - t));
    CHECK(std::any_of(edges.begin(), edges.end(), [](const MedialEdge& e) { return e.curved; }));
    for (const MedialEdge& e : edges) {
        // Nothing inside the hole, and every end at least as close as the wall middle allows.
        CHECK_FALSE((e.a.x > 3.0 + 1e-6 && e.a.x < 7.0 - 1e-6 && e.a.y > 3.0 + 1e-6 && e.a.y < 7.0 - 1e-6));
        CHECK(e.radius_a <= t + 1e-6);
    }
}

TEST_CASE("medial axis of an empty region", "[medial]") {
    CHECK(medial_axis({}).empty());
}
