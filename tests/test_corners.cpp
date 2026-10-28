#include "config/settings.hpp"
#include "gcode/corners.hpp"
#include "gcode/writer.hpp"
#include "mesh/indexed_mesh.hpp"
#include "slice/pipeline.hpp"
#include "test_printer.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <sstream>
#include <string>
#include <vector>

using namespace tisma;

namespace {

// The distance from q to the segment a-b.
double to_segment(Point2d q, Point2d a, Point2d b) {
    const double dx = b.x - a.x, dy = b.y - a.y, l2 = dx * dx + dy * dy;
    const double u = l2 > 0.0 ? std::clamp(((q.x - a.x) * dx + (q.y - a.y) * dy) / l2, 0.0, 1.0) : 0.0;
    return std::hypot(q.x - a.x - u * dx, q.y - a.y - u * dy);
}

// How far each point of `path` lies from the polygon or polyline `shape`.
double farthest(const Toolpath& path, const std::vector<Point2d>& shape, bool closed) {
    double worst = 0.0;
    for (const ToolpathPoint& p : path.points) {
        double best = 1e9;
        const std::size_t n = shape.size();
        for (std::size_t i = 0; i + (closed ? 0 : 1) < n; ++i)
            best = std::min(best, to_segment(p.p, shape[i], shape[(i + 1) % n]));
        worst = std::max(worst, best);
    }
    return worst;
}

Toolpath path_of(const std::vector<Point2d>& points, bool closed) {
    Toolpath t;
    t.closed = closed;
    for (const Point2d& p : points)
        t.points.push_back({p, 0.45});
    return t;
}

} // namespace

TEST_CASE("corners: rounded within the deviation, the ends of an open path kept (toolpath_time.md, rule 7)",
          "[corners]") {
    const std::vector<Point2d> square = {{0, 0}, {10, 0}, {10, 10}, {0, 10}};
    const Toolpath round = round_corners(path_of(square, true), 0.02);
    CHECK(round.closed);
    CHECK(round.points.size() > square.size());
    CHECK(farthest(round, square, true) <= 0.02 + 1e-9);
    // Each corner cut by the full deviation: no point at a vertex.
    for (const Point2d& v : square)
        for (const ToolpathPoint& p : round.points)
            CHECK(std::hypot(p.p.x - v.x, p.p.y - v.y) > 0.019);
    // An open zigzag: its ends stay, its hairpin turns are rounded within it.
    const std::vector<Point2d> zigzag = {{0, 0}, {10, 0}, {10, 0.45}, {0, 0.45}, {0, 0.9}, {10, 0.9}};
    const Toolpath z = round_corners(path_of(zigzag, false), 0.05);
    CHECK(z.points.front().p.x == 0.0);
    CHECK(z.points.back().p.x == 10.0);
    CHECK(z.points.back().p.y == 0.9);
    CHECK(farthest(z, zigzag, false) <= 0.05 + 1e-9);
    // Straight on, or nothing to round: unchanged.
    const std::vector<Point2d> line = {{0, 0}, {5, 0}, {10, 0}};
    CHECK(round_corners(path_of(line, false), 0.02).points.size() == 3);
    CHECK(round_corners(path_of(square, true), 0.0).points.size() == 4);
}

TEST_CASE("corners: gcode.corner_smoothing rounds the inner walls, never the outer one (toolpath_time.md, TT7)",
          "[corners][gcode]") {
    IndexedMesh cube;
    {
        const auto quad = [&](std::uint32_t a, std::uint32_t b, std::uint32_t c, std::uint32_t d) {
            cube.triangles.push_back({a, b, c});
            cube.triangles.push_back({a, c, d});
        };
        for (float z : {0.0f, 4.0f})
            for (const auto& p : {std::array<float, 2>{0, 0}, {20, 0}, {20, 20}, {0, 20}})
                cube.vertices.push_back({p[0], p[1], z});
        quad(0, 3, 2, 1);
        quad(4, 5, 6, 7);
        for (std::uint32_t i = 0; i < 4; ++i)
            quad(i, (i + 1) % 4, 4 + (i + 1) % 4, 4 + i);
    }
    Settings s = test_printer();
    s.skirt_loops = 0;
    // Each type's lines of the G-code.
    const auto lines = [&](double smoothing, const std::string& type) {
        s.corner_smoothing = smoothing;
        std::ostringstream out;
        write_gcode(out, slice_model(cube, s), s);
        std::istringstream in(out.str());
        std::string current, text;
        for (std::string line; std::getline(in, line);) {
            if (line.rfind(";TYPE:", 0) == 0)
                current = line.substr(6);
            else if (current == type && line.rfind("G1 X", 0) == 0)
                text += line.substr(0, line.find(" E")) + "\n";
        }
        return text;
    };
    CHECK(lines(0.02, "External perimeter") == lines(0.0, "External perimeter"));
    const std::string sharp = lines(0.0, "Perimeter"), round = lines(0.02, "Perimeter");
    CHECK(round != sharp);
    CHECK(std::count(round.begin(), round.end(), '\n') > std::count(sharp.begin(), sharp.end(), '\n'));
}
