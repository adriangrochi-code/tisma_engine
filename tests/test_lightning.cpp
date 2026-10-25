#include "infill/lightning.hpp"

#include "gcode/check.hpp"
#include "gcode/writer.hpp"
#include "slice/pipeline.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <cstdint>
#include <limits>
#include <sstream>

using namespace tisma;

namespace {

Polygons square(double x0, double y0, double side) {
    const auto u = [](double v) { return to_units(v); };
    return {{{u(x0), u(y0)}, {u(x0 + side), u(y0)}, {u(x0 + side), u(y0 + side)}, {u(x0), u(y0 + side)}}};
}

double total(const std::vector<Toolpath>& paths) {
    double sum = 0.0;
    for (const Toolpath& t : paths)
        for (std::size_t i = 1; i < t.points.size(); ++i)
            sum += std::hypot(t.points[i].p.x - t.points[i - 1].p.x, t.points[i].p.y - t.points[i - 1].p.y);
    return sum;
}

double to_segment(Point2d p, Point2d a, Point2d b) {
    const double dx = b.x - a.x, dy = b.y - a.y, l2 = dx * dx + dy * dy;
    const double t = l2 > 0.0 ? std::clamp(((p.x - a.x) * dx + (p.y - a.y) * dy) / l2, 0.0, 1.0) : 0.0;
    return std::hypot(p.x - a.x - t * dx, p.y - a.y - t * dy);
}

double to_lines(Point2d p, const std::vector<Toolpath>& paths) {
    double best = std::numeric_limits<double>::infinity();
    for (const Toolpath& t : paths)
        for (std::size_t i = 1; i < t.points.size(); ++i)
            best = std::min(best, to_segment(p, t.points[i - 1].p, t.points[i].p));
    return best;
}

IndexedMesh cube(float side) {
    IndexedMesh m;
    m.vertices = {{0, 0, 0}, {side, 0, 0}, {side, side, 0}, {0, side, 0},
                  {0, 0, side}, {side, 0, side}, {side, side, side}, {0, side, side}};
    auto quad = [&](std::uint32_t a, std::uint32_t b, std::uint32_t c, std::uint32_t d) {
        m.triangles.push_back({a, b, c});
        m.triangles.push_back({a, c, d});
    };
    quad(0, 3, 2, 1);
    quad(4, 5, 6, 7);
    for (std::uint32_t i = 0; i < 4; ++i)
        quad(i, (i + 1) % 4, 4 + (i + 1) % 4, 4 + i);
    return m;
}

} // namespace

TEST_CASE("lightning: holds up the ceiling, grows in from the walls, draws back below", "[lightning]") {
    // A 30 mm box: walls all round, sparse infill from layer 0 to 99 (z 0 to
    // 20), a solid ceiling from layer 100 up.
    std::vector<Layer> layers;
    std::vector<Polygons> sparse, regions;
    for (int l = 0; l < 105; ++l) {
        layers.push_back({0.2 * l, 0.2 * (l + 1)});
        regions.push_back(square(-1, -1, 32));
        sparse.push_back(l < 100 ? square(0, 0, 30) : Polygons{});
    }
    LightningParams p;
    p.width = 0.4;
    p.reach = 1.0;
    const std::vector<std::vector<Toolpath>> lines = lightning_layers(sparse, regions, layers, p);
    REQUIRE(lines.size() == layers.size());
    const auto wall = [](Point2d q) { return std::min({q.x, q.y, 30.0 - q.x, 30.0 - q.y}); };
    // Right under the ceiling, every point of it within the reach of a branch
    // or a wall (the branches stop half a bead short of the walls).
    for (double y = 0.25; y < 30.0; y += 0.5)
        for (double x = 0.25; x < 30.0; x += 0.5) {
            INFO("at " << x << ", " << y);
            CHECK(std::min(to_lines({x, y}, lines[99]) - 1e-6, wall({x, y}) - p.width / 2.0) <= p.reach);
        }
    // Less and less lower down, nothing far below: the branches draw back 0.2
    // mm a layer, and none is much longer than 15 mm.
    CHECK(total(lines[80]) < total(lines[99]));
    CHECK(total(lines[40]) < total(lines[80]));
    CHECK(total(lines[0]) < 0.05 * total(lines[99]));
    // Printable: each branch reaches past the one under it by at most a
    // layer's lean (0.2 mm at 45 degrees), or starts at a wall.
    for (std::size_t l = 0; l + 1 < 100; l += 9)
        for (const Toolpath& t : lines[l + 1])
            for (const ToolpathPoint& q : t.points) {
                INFO("layer " << l + 1 << " at " << q.p.x << ", " << q.p.y);
                CHECK(std::min(to_lines(q.p, lines[l]), wall(q.p)) <= 0.2 + p.width + 1e-6);
            }
    // Above the infill, nothing.
    for (std::size_t l = 100; l < lines.size(); ++l)
        CHECK(lines[l].empty());
}

TEST_CASE("lightning: the spec's 20 mm cube, growing upwards (P7), sliced to checked G-code", "[lightning]") {
    const IndexedMesh m = cube(20.0f);
    Settings s;
    s.gpu = GpuMode::Off;
    s.nozzle_diameter = 0.4;
    s.layer_height = 0.2;
    s.first_layer_height = 0.2;
    s.wall_count = 2;
    s.top_layers = 3;
    s.bottom_layers = 3;
    s.infill_density = 20.0;
    s.infill_pattern = InfillPattern::Lightning;
    s.skirt_loops = 0;
    const SlicedModel sliced = slice_model(m, s);
    std::ostringstream out;
    write_gcode(out, sliced, s);
    const GcodeCheck check = check_gcode(out.str(), gcode_limits(sliced, s));
    INFO((check.problems.empty() ? std::string() : check.problems.front().message));
    CHECK(check.ok());
    REQUIRE(sliced.fills.size() == 100);
    const auto length = [&](std::size_t l) { return total(sliced.fills[l].sparse); };
    // Much under the top, little at the bottom.
    CHECK(length(95) > length(60));
    CHECK(length(60) > length(10));
    CHECK(length(95) > 3.0 * length(10));
}
