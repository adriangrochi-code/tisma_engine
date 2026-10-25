#include "gcode/order.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <algorithm>
#include <cmath>
#include <random>

using namespace tisma;
using Catch::Matchers::WithinAbs;

namespace {

Toolpath square_loop(double x, double y, double side, int index = 0) {
    Toolpath t;
    t.closed = true;
    t.index = index;
    t.points = {{{x, y}, 0.4}, {{x + side, y}, 0.4}, {{x + side, y + side}, 0.4}, {{x, y + side}, 0.4}};
    return t;
}

double travel(const std::vector<Toolpath>& paths, Point2d from) {
    double sum = 0.0;
    for (const Toolpath& t : paths) {
        sum += std::hypot(t.points.front().p.x - from.x, t.points.front().p.y - from.y);
        from = t.closed ? t.points.front().p : t.points.back().p;
    }
    return sum;
}

} // namespace

TEST_CASE("ordering a shuffled grid of squares shortens the travel", "[order]") {
    std::vector<Toolpath> grid;
    for (int i = 0; i < 6; ++i)
        for (int j = 0; j < 6; ++j)
            grid.push_back(square_loop(i * 10.0, j * 10.0, 2.0));
    std::mt19937 rng(7);
    std::shuffle(grid.begin(), grid.end(), rng);
    SeamMemory memory;
    Point2d cursor{0, 0};
    const double before = travel(grid, cursor);
    const std::vector<Toolpath> ordered = order_paths(grid, cursor, SeamMode::Nearest, 1, memory);
    REQUIRE(ordered.size() == grid.size());
    const double after = travel(ordered, {0, 0});
    CHECK(after < 0.5 * before);
    // Open lines may run backwards when their far end is nearer.
    Toolpath line;
    line.points = {{{5, 0}, 0.4}, {{0, 0}, 0.4}};
    Point2d at{0, 0};
    const std::vector<Toolpath> one = order_paths({line}, at, SeamMode::Nearest, 1, memory);
    CHECK(one[0].points.front().p.x == 0.0);
    CHECK(at.x == 5.0);
}

TEST_CASE("seams: rear, nearest, random, aligned", "[order]") {
    SeamMemory memory;
    const Toolpath sq = square_loop(0, 0, 10);
    // rear: the largest Y, then the smallest X
    const std::size_t rear = seam_point(sq, SeamMode::Rear, {0, 0}, 1, memory);
    CHECK(sq.points[rear].p.y == 10.0);
    CHECK(sq.points[rear].p.x == 0.0);
    // nearest: the corner nearest to where the last move ended
    const std::size_t near = seam_point(sq, SeamMode::Nearest, {11, -1}, 1, memory);
    CHECK(sq.points[near].p.x == 10.0);
    CHECK(sq.points[near].p.y == 0.0);
    // random: the same seed, the same point
    CHECK(seam_point(sq, SeamMode::Random, {0, 0}, 42, memory) == seam_point(sq, SeamMode::Random, {0, 0}, 42, memory));
    // aligned: an L-shaped loop has one concave corner, at (4, 4)
    Toolpath l;
    l.closed = true;
    l.points = {{{0, 0}, 0.4}, {{10, 0}, 0.4}, {{10, 4}, 0.4}, {{4, 4}, 0.4}, {{4, 10}, 0.4}, {{0, 10}, 0.4}};
    const std::size_t corner = seam_point(l, SeamMode::Aligned, {0, 0}, 1, memory);
    CHECK(l.points[corner].p.x == 4.0);
    CHECK(l.points[corner].p.y == 4.0);
    // aligned on a square (no concave corner): near the seam of the layer below
    memory.below[0] = {{-20.0, -20.0}, {9.5, 9.0}}; // the nearest seam below counts
    const std::size_t aligned = seam_point(sq, SeamMode::Aligned, {0, 0}, 1, memory);
    CHECK(sq.points[aligned].p.x == 10.0);
    CHECK(sq.points[aligned].p.y == 10.0);
    // order_paths starts loops at their seam and remembers it
    Point2d cursor{0, 0};
    SeamMemory fresh;
    const std::vector<Toolpath> ordered = order_paths({sq}, cursor, SeamMode::Rear, 1, fresh);
    CHECK(ordered[0].points.front().p.y == 10.0);
    CHECK(fresh.current.at(0).front().y == 10.0);
    // next_layer: this layer's seams go below; an index not used keeps its older seams
    fresh.below[5] = {{1.0, 1.0}};
    fresh.next_layer();
    CHECK(fresh.current.empty());
    CHECK(fresh.below.at(0).front().y == 10.0);
    CHECK(fresh.below.at(5).front().x == 1.0);
}

TEST_CASE("islands: connected pieces of a region, an island in a hole on its own", "[order]") {
    auto square = [](double x0, double y0, double x1, double y1, bool hole = false) {
        Polygon p{{to_units(x0), to_units(y0)}, {to_units(x1), to_units(y0)}, {to_units(x1), to_units(y1)},
                  {to_units(x0), to_units(y1)}};
        if (hole)
            std::reverse(p.begin(), p.end());
        return p;
    };
    // A frame with a hole and an island in it, and a separate square.
    const Polygons region{square(0, 0, 30, 30), square(5, 5, 25, 25, true), square(12, 12, 18, 18), square(40, 0, 50, 10)};
    const std::vector<Polygons> islands = split_islands(region);
    REQUIRE(islands.size() == 3);
    const int frame = island_of(islands, {2, 2});
    const int inner = island_of(islands, {15, 15});
    const int apart = island_of(islands, {45, 5});
    CHECK(frame >= 0);
    CHECK(inner >= 0);
    CHECK(apart >= 0);
    CHECK(frame != inner);
    CHECK(frame != apart);
    CHECK(inner != apart);
    CHECK(islands[std::size_t(frame)].size() == 2); // with its hole
    CHECK(island_of(islands, {8, 8}) == -1);        // in the hole, outside the island
    CHECK(island_of(islands, {35, 5}) == -1);
    CHECK(island_of(islands, {0, 15}) == frame);    // on the boundary
}

TEST_CASE("ordering many paths through the grid picks what looking at every path picks", "[order]") {
    // Short lines with their ends on a 1 mm grid, so that many starts tie, and
    // a few loops; more than the grid's threshold.
    std::mt19937 random(11);
    std::uniform_int_distribution<int> coordinate(0, 40), length(1, 3);
    std::vector<Toolpath> paths;
    for (int i = 0; i < 400; ++i) {
        Toolpath t;
        const double x = coordinate(random), y = coordinate(random);
        t.points = {{{x, y}, 0.4}, {{x + length(random), y + (i % 3 == 0 ? 1.0 : 0.0)}, 0.4}};
        paths.push_back(std::move(t));
    }
    for (int i = 0; i < 20; ++i)
        paths.push_back(square_loop(coordinate(random), coordinate(random), 2.0));
    // The greedy order, every path looked at each time: the nearest start;
    // between equals the first path, its front before its back.
    std::vector<Toolpath> want;
    {
        std::vector<Toolpath> left = paths;
        std::vector<bool> done(left.size(), false);
        Point2d cursor{-5.0, -5.0};
        for (std::size_t k = 0; k < left.size(); ++k) {
            std::size_t best = left.size();
            bool back = false;
            double nearest = 1e300;
            for (std::size_t i = 0; i < left.size(); ++i) {
                if (done[i])
                    continue;
                const auto& pts = left[i].points;
                // A loop starts at its rear seam: the largest Y, then the smallest X.
                Point2d start = pts.front().p;
                if (left[i].closed)
                    for (const ToolpathPoint& q : pts)
                        if (q.p.y > start.y || (q.p.y == start.y && q.p.x < start.x))
                            start = q.p;
                const double front = std::hypot(cursor.x - start.x, cursor.y - start.y);
                if (front < nearest)
                    nearest = front, best = i, back = false;
                if (!left[i].closed) {
                    const double end = std::hypot(cursor.x - pts.back().p.x, cursor.y - pts.back().p.y);
                    if (end < nearest)
                        nearest = end, best = i, back = true;
                }
            }
            done[best] = true;
            Toolpath t = left[best];
            if (back)
                std::reverse(t.points.begin(), t.points.end());
            if (t.closed) {
                cursor = t.points.front().p;
                for (const ToolpathPoint& q : t.points)
                    if (q.p.y > cursor.y || (q.p.y == cursor.y && q.p.x < cursor.x))
                        cursor = q.p;
            } else {
                cursor = t.points.back().p;
            }
            want.push_back(std::move(t));
        }
    }
    Point2d cursor{-5.0, -5.0};
    SeamMemory memory;
    const std::vector<Toolpath> got = order_paths(paths, cursor, SeamMode::Rear, 1, memory);
    REQUIRE(got.size() == want.size());
    int differ = 0;
    for (std::size_t k = 0; k < got.size(); ++k)
        if (!want[k].closed)
            differ += got[k].points.front().p.x != want[k].points.front().p.x ||
                      got[k].points.front().p.y != want[k].points.front().p.y ||
                      got[k].points.back().p.x != want[k].points.back().p.x ||
                      got[k].points.back().p.y != want[k].points.back().p.y;
    CHECK(differ == 0);
}
