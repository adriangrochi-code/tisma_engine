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

TEST_CASE("nearest seams among many loops and lines: the pick of looking at every point", "[order]") {
    // Loops of 3 to 8 points and short lines on a 1 mm grid (many ties), more
    // of each than the grid's threshold: the loops' boxes prune, the lines'
    // ends are filed.
    std::mt19937 random(23);
    std::uniform_int_distribution<int> coordinate(0, 30), size(1, 3), corners(3, 8);
    std::vector<Toolpath> paths;
    for (int i = 0; i < 300; ++i) {
        Toolpath t;
        if (i % 2 == 0) {
            t.closed = true;
            const double cx = coordinate(random), cy = coordinate(random), r = size(random);
            const int n = corners(random);
            for (int k = 0; k < n; ++k)
                t.points.push_back({{std::round(cx + r * std::cos(6.283185307179586 * k / n)),
                                     std::round(cy + r * std::sin(6.283185307179586 * k / n))},
                                    0.4});
        } else {
            const double x = coordinate(random), y = coordinate(random);
            t.points = {{{x, y}, 0.4}, {{x + size(random), y}, 0.4}};
        }
        paths.push_back(std::move(t));
    }
    // Every path looked at each time: a loop at its nearest point (the first
    // of equals), a line at its nearer end; between equals the first path,
    // its front before its back.
    std::vector<Toolpath> want;
    {
        std::vector<bool> done(paths.size(), false);
        Point2d cursor{-3.0, 7.0};
        for (std::size_t k = 0; k < paths.size(); ++k) {
            std::size_t best = paths.size(), seam = 0;
            bool back = false;
            double nearest = 1e300;
            for (std::size_t i = 0; i < paths.size(); ++i) {
                if (done[i])
                    continue;
                const auto& pts = paths[i].points;
                const auto d = [&](std::size_t j) { return std::hypot(cursor.x - pts[j].p.x, cursor.y - pts[j].p.y); };
                if (paths[i].closed) {
                    std::size_t j = 0;
                    for (std::size_t q = 1; q < pts.size(); ++q)
                        if (d(q) < d(j))
                            j = q;
                    if (d(j) < nearest)
                        nearest = d(j), best = i, back = false, seam = j;
                } else {
                    if (d(0) < nearest)
                        nearest = d(0), best = i, back = false;
                    if (d(1) < nearest)
                        nearest = d(1), best = i, back = true;
                }
            }
            done[best] = true;
            Toolpath t = paths[best];
            if (t.closed)
                std::rotate(t.points.begin(), t.points.begin() + std::ptrdiff_t(seam), t.points.end());
            else if (back)
                std::reverse(t.points.begin(), t.points.end());
            cursor = t.closed ? t.points.front().p : t.points.back().p;
            want.push_back(std::move(t));
        }
    }
    Point2d cursor{-3.0, 7.0};
    SeamMemory memory;
    const std::vector<Toolpath> got = order_paths(paths, cursor, SeamMode::Nearest, 1, memory);
    REQUIRE(got.size() == want.size());
    int differ = 0;
    for (std::size_t k = 0; k < got.size(); ++k)
        for (std::size_t q = 0; q < got[k].points.size(); ++q)
            differ += got[k].points[q].p.x != want[k].points[q].p.x || got[k].points[q].p.y != want[k].points[q].p.y;
    CHECK(differ == 0);
}

namespace {

// The aligned seam's candidates by the plain walk: from every point, one point
// after another until one is kSeamSpan away (or the loop comes back).
std::vector<std::size_t> plain_candidates(const Toolpath& loop) {
    const std::size_t n = loop.points.size();
    double area = 0.0;
    for (std::size_t i = 0; i < n; ++i) {
        const Point2d& a = loop.points[i].p;
        const Point2d& b = loop.points[(i + 1) % n].p;
        area += a.x * b.y - b.x * a.y;
    }
    const auto walk = [&](std::size_t i, bool forward) {
        std::size_t j = i;
        for (std::size_t k = 0; k + 1 < n; ++k) {
            j = forward ? (j + 1) % n : (j + n - 1) % n;
            if (std::hypot(loop.points[j].p.x - loop.points[i].p.x, loop.points[j].p.y - loop.points[i].p.y) >= kSeamSpan)
                break;
        }
        return j;
    };
    const double limit = std::sin(kSeamCorner * 3.14159265358979323846 / 180.0);
    std::vector<std::size_t> corners, all;
    for (std::size_t i = 0; i < n; ++i) {
        all.push_back(i);
        const Point2d& a = loop.points[walk(i, false)].p;
        const Point2d& b = loop.points[i].p;
        const Point2d& c = loop.points[walk(i, true)].p;
        const double ux = b.x - a.x, uy = b.y - a.y, vx = c.x - b.x, vy = c.y - b.y;
        const double lu = std::hypot(ux, uy), lv = std::hypot(vx, vy);
        if (lu == 0.0 || lv == 0.0)
            continue;
        if ((ux * vy - uy * vx) / (lu * lv) * (area > 0.0 ? -1.0 : 1.0) > limit)
            corners.push_back(i);
    }
    return corners.empty() ? all : corners;
}

} // namespace

TEST_CASE("the aligned seam's candidates are the plain walk's, points crowded or not", "[order]") {
    std::mt19937 random(7);
    std::uniform_real_distribution<double> unit(0.0, 1.0);
    for (int trial = 0; trial < 300; ++trial) {
        // A star with rounded tips: arcs of crowded points at every tip and
        // notch, sometimes repeated points, sometimes a loop smaller than kSeamSpan.
        Toolpath loop;
        loop.closed = true;
        const int tips = 3 + int(unit(random) * 9);
        const double scale = trial % 10 == 0 ? 0.01 : 1.0 + unit(random) * 20.0;
        for (int k = 0; k < 2 * tips; ++k) {
            const double angle = 2.0 * 3.14159265358979323846 * k / (2 * tips);
            const double radius = scale * (k % 2 ? 0.4 + 0.3 * unit(random) : 1.0);
            const int crowd = int(unit(random) * 40);
            for (int c = 0; c <= crowd; ++c) {
                const double a = angle + (c - crowd / 2.0) * 0.002 * unit(random);
                loop.points.push_back({{radius * std::cos(a), radius * std::sin(a)}, 0.4});
                if (unit(random) < 0.05)
                    loop.points.push_back(loop.points.back());
            }
        }
        if (trial % 2)
            std::reverse(loop.points.begin(), loop.points.end());
        CHECK(seam_candidates(loop) == plain_candidates(loop));
    }
}

TEST_CASE("candidates found beforehand pick the same aligned seams", "[order]") {
    Toolpath l;
    l.closed = true;
    l.points = {{{0, 0}, 0.4}, {{10, 0}, 0.4}, {{10, 4}, 0.4}, {{4, 4}, 0.4}, {{4, 10}, 0.4}, {{0, 10}, 0.4}};
    const std::vector<Toolpath> paths{l, square_loop(20, 0, 5), square_loop(40, 0, 5, 1)};
    SeamMemory with, without;
    with.below[0] = without.below[0] = {{24.0, 6.0}};
    std::vector<std::vector<std::size_t>> found;
    for (const Toolpath& t : paths)
        found.push_back(seam_candidates(t));
    const std::vector<const std::vector<std::size_t>*> given{&found[0], &found[1], nullptr};
    Point2d a{0, 0}, b{0, 0};
    const std::vector<Toolpath> one = order_paths(paths, a, SeamMode::Aligned, 1, with, nullptr, given);
    const std::vector<Toolpath> two = order_paths(paths, b, SeamMode::Aligned, 1, without);
    REQUIRE(one.size() == two.size());
    for (std::size_t k = 0; k < one.size(); ++k)
        CHECK(one[k].points.front().p.x == two[k].points.front().p.x);
    REQUIRE(with.current.size() == without.current.size());
    for (const auto& [index, seams] : with.current) {
        REQUIRE(seams.size() == without.current.at(index).size());
        for (std::size_t k = 0; k < seams.size(); ++k)
            CHECK((seams[k].x == without.current.at(index)[k].x && seams[k].y == without.current.at(index)[k].y));
    }
}
