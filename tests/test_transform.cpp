#include "slice/transform.hpp"

#include "cli/cli.hpp"
#include "mesh/indexed_mesh.hpp"
#include "test_temp.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <string>

using namespace tisma;
using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

namespace {

// A box from lo to hi, its triangles facing out.
void add_box(TriangleMesh& m, Vec3f lo, Vec3f hi, bool inward = false) {
    const Vec3f v[8] = {{lo.x, lo.y, lo.z}, {hi.x, lo.y, lo.z}, {hi.x, hi.y, lo.z}, {lo.x, hi.y, lo.z},
                        {lo.x, lo.y, hi.z}, {hi.x, lo.y, hi.z}, {hi.x, hi.y, hi.z}, {lo.x, hi.y, hi.z}};
    const int f[12][3] = {{0, 2, 1}, {0, 3, 2}, {4, 5, 6}, {4, 6, 7}, {0, 1, 5}, {0, 5, 4},
                          {1, 2, 6}, {1, 6, 5}, {2, 3, 7}, {2, 7, 6}, {3, 0, 4}, {3, 4, 7}};
    for (const auto& t : f)
        if (inward)
            m.add_triangle(v[t[0]], v[t[2]], v[t[1]]);
        else
            m.add_triangle(v[t[0]], v[t[1]], v[t[2]]);
}

TriangleMesh box(Vec3f lo, Vec3f hi) {
    TriangleMesh m;
    add_box(m, lo, hi);
    return m;
}

// A cone of radius r and height h on z = 0, with n sides.
TriangleMesh cone(double r, double h, int n) {
    TriangleMesh m;
    const Vec3f apex{0, 0, float(h)}, centre{0, 0, 0};
    for (int i = 0; i < n; ++i) {
        const double a0 = 2.0 * M_PI * i / n, a1 = 2.0 * M_PI * ((i + 1) % n) / n;
        const Vec3f p0{float(r * std::cos(a0)), float(r * std::sin(a0)), 0};
        const Vec3f p1{float(r * std::cos(a1)), float(r * std::sin(a1)), 0};
        m.add_triangle(p0, p1, apex);
        m.add_triangle(centre, p1, p0);
    }
    return m;
}

std::array<double, 3> size_of(const TriangleMesh& m) {
    const BoundingBox b = m.bounding_box();
    return {double(b.max.x) - b.min.x, double(b.max.y) - b.min.y, double(b.max.z) - b.min.z};
}

// Every edge shared by exactly two triangles, once each way: a closed mesh.
bool closed(const TriangleMesh& m) {
    const IndexedMesh im = weld_exact(m);
    std::map<std::pair<std::uint32_t, std::uint32_t>, int> edges;
    for (const auto& t : im.triangles)
        for (int k = 0; k < 3; ++k)
            ++edges[{t[std::size_t(k)], t[std::size_t((k + 1) % 3)]}];
    for (const auto& [e, count] : edges)
        if (count != 1 || edges.count({e.second, e.first}) == 0 || edges.at({e.second, e.first}) != 1)
            return false;
    return true;
}

const Vec3f kLo{5, 7, 3}, kHi{35, 27, 13};

} // namespace

TEST_CASE("transform: scale about the box's centre and to fit (MT14, MT15)", "[transform]") {
    TriangleMesh m = box(kLo, kHi);
    scale_mesh(m, 2, 2, 2);
    CHECK(size_of(m) == std::array<double, 3>{60, 40, 20});
    CHECK_THAT(m.signed_volume(), WithinRel(48000.0, 1e-6));
    CHECK(box_centre(m) == std::array<double, 3>{20, 17, 8});
    m = box(kLo, kHi);
    scale_mesh(m, 1.5, 1.5, 1.5);
    CHECK_THAT(m.signed_volume(), WithinRel(20250.0, 1e-6));
    m = box(kLo, kHi);
    CHECK(fit_factor(m, {60, 60, 60}) == 2.0);
    CHECK(fit_factor(m, {15, 100, 100}) == 0.5);
    CHECK(fit_factor(TriangleMesh{}, {1, 1, 1}) == 1.0);
}

TEST_CASE("transform: turns about the box's centre (MT16)", "[transform]") {
    const auto turned = [](double x, double y, double z) {
        TriangleMesh m = box(kLo, kHi);
        rotate_mesh(m, x, y, z);
        return size_of(m);
    };
    std::array<double, 3> s = turned(0, 0, 90);
    CHECK_THAT(s[0], WithinAbs(20, 1e-4));
    CHECK_THAT(s[1], WithinAbs(30, 1e-4));
    s = turned(0, 0, 45);
    CHECK_THAT(s[0], WithinAbs(35.355, 1e-3));
    CHECK_THAT(s[1], WithinAbs(35.355, 1e-3));
    s = turned(90, 0, 0);
    CHECK_THAT(s[1], WithinAbs(10, 1e-4));
    CHECK_THAT(s[2], WithinAbs(20, 1e-4));
    s = turned(0, 90, 0);
    CHECK_THAT(s[0], WithinAbs(10, 1e-4));
    CHECK_THAT(s[2], WithinAbs(30, 1e-4));
    TriangleMesh m = box(kLo, kHi);
    rotate_mesh(m, 30, 40, 50);
    CHECK_THAT(m.signed_volume(), WithinRel(6000.0, 1e-5));
}

TEST_CASE("transform: mirrored twice is the same mesh, facing out (MT20)", "[transform]") {
    const TriangleMesh start = cone(10, 10, 32);
    TriangleMesh m = start;
    mirror_mesh(m, 0);
    CHECK_THAT(m.signed_volume(), WithinRel(start.signed_volume(), 1e-6)); // still facing out
    CHECK(m.corners() != start.corners());
    mirror_mesh(m, 0);
    REQUIRE(m.triangle_count() == start.triangle_count());
    for (std::size_t i = 0; i < m.corners().size(); ++i) {
        CHECK_THAT(m.corners()[i].x, WithinAbs(start.corners()[i].x, 1e-5));
        CHECK(m.corners()[i].y == start.corners()[i].y);
    }
}

TEST_CASE("transform: a horizontal cut closes both halves (MT17)", "[transform]") {
    const CutResult b = cut_mesh(box(kLo, kHi), 8.0);
    CHECK(closed(b.below));
    CHECK(closed(b.above));
    CHECK_THAT(b.below.signed_volume(), WithinRel(3000.0, 1e-5));
    CHECK_THAT(b.above.signed_volume(), WithinRel(3000.0, 1e-5));
    CHECK(size_of(b.below) == std::array<double, 3>{30, 20, 5});
    // The cone of the spec: a frustum and a tip.
    const TriangleMesh c = cone(10, 10, 256);
    const CutResult k = cut_mesh(c, 5.0);
    CHECK(closed(k.below));
    CHECK(closed(k.above));
    CHECK_THAT(k.below.signed_volume() + k.above.signed_volume(), WithinRel(c.signed_volume(), 1e-5));
    CHECK_THAT(k.below.signed_volume(), WithinAbs(916.3, 1.0));
    CHECK_THAT(k.above.signed_volume(), WithinAbs(130.9, 0.3));
    // A plane that misses: one side empty.
    CHECK(cut_mesh(box(kLo, kHi), 20.0).above.empty());
}

TEST_CASE("transform: a cut through a hole closes it with the hole open", "[transform]") {
    // A ring: a 20 mm square with a 10 mm square hole, 10 mm tall, its top
    // and bottom filled by the triangulation itself.
    const std::vector<std::array<double, 2>> out_ring = {{0, 0}, {20, 0}, {20, 20}, {0, 20}};
    const std::vector<std::array<double, 2>> in_ring = {{5, 5}, {5, 15}, {15, 15}, {15, 5}};
    std::vector<std::array<double, 2>> all = out_ring;
    all.insert(all.end(), in_ring.begin(), in_ring.end());
    TriangleMesh ring;
    const auto at = [&](std::size_t i, float z) { return Vec3f{float(all[i][0]), float(all[i][1]), z}; };
    for (const auto& t : triangulate_polygon(out_ring, {in_ring})) {
        ring.add_triangle(at(t[0], 10), at(t[1], 10), at(t[2], 10));
        ring.add_triangle(at(t[0], 0), at(t[2], 0), at(t[1], 0));
    }
    for (const std::size_t first : {std::size_t(0), std::size_t(4)})
        for (std::size_t k = 0; k < 4; ++k) {
            const std::size_t i = first + k, j = first + (k + 1) % 4;
            ring.add_triangle(at(i, 0), at(j, 0), at(j, 10));
            ring.add_triangle(at(i, 0), at(j, 10), at(i, 10));
        }
    REQUIRE(closed(ring));
    CHECK_THAT(ring.signed_volume(), WithinRel(3000.0, 1e-6));
    const CutResult r = cut_mesh(ring, 4.0);
    CHECK(closed(r.below));
    CHECK(closed(r.above));
    CHECK_THAT(r.below.signed_volume(), WithinRel(4.0 * 300.0, 1e-5));
    CHECK_THAT(r.above.signed_volume(), WithinRel(6.0 * 300.0, 1e-5));
    // One outline with a hole: the triangulation fills the ring only.
    const std::vector<std::array<double, 2>> outer = {{0, 0}, {20, 0}, {20, 20}, {0, 20}};
    const std::vector<std::array<double, 2>> hole = {{5, 5}, {5, 15}, {15, 15}, {15, 5}};
    const auto tris = triangulate_polygon(outer, {hole});
    double area = 0;
    std::vector<std::array<double, 2>> pts = outer;
    pts.insert(pts.end(), hole.begin(), hole.end());
    for (const auto& t : tris) {
        const auto& p = pts[t[0]];
        const auto& q = pts[t[1]];
        const auto& s = pts[t[2]];
        const double ar = 0.5 * ((q[0] - p[0]) * (s[1] - p[1]) - (q[1] - p[1]) * (s[0] - p[0]));
        CHECK(ar >= 0.0);
        area += ar;
    }
    CHECK_THAT(area, WithinAbs(300.0, 1e-9));
}

TEST_CASE("transform: split into loose pieces, cavities kept (MT18)", "[transform]") {
    TriangleMesh two;
    add_box(two, {0, 0, 0}, {10, 10, 10});
    add_box(two, {30, 0, 0}, {40, 10, 5});
    const std::vector<TriangleMesh> pieces = split_mesh(two);
    REQUIRE(pieces.size() == 2);
    CHECK_THAT(pieces[0].signed_volume(), WithinRel(1000.0, 1e-6));
    CHECK_THAT(pieces[1].signed_volume(), WithinRel(500.0, 1e-6));
    // A hollow box: its cavity stays with it.
    TriangleMesh hollow;
    add_box(hollow, {0, 0, 0}, {10, 10, 10});
    add_box(hollow, {3, 3, 3}, {7, 7, 7}, true);
    add_box(hollow, {20, 0, 0}, {25, 5, 5});
    const std::vector<TriangleMesh> parts = split_mesh(hollow);
    REQUIRE(parts.size() == 2);
    CHECK_THAT(parts[0].signed_volume(), WithinRel(1000.0 - 64.0, 1e-6));
    CHECK(parts[0].triangle_count() == 24);
    CHECK(split_mesh(TriangleMesh{}).empty());
}

namespace {

// The command line's slice of a model, with the base settings of the spec's
// tests (two walls, no infill or solid layers, 0.2 mm layers, centred on
// 100, 100) and these groups: its output, and the X range of the G-code's
// moves.
struct Sliced {
    int code = 0;
    std::string out, err;
    double x_lo = 1e9, x_hi = -1e9;
};

std::string ascii_stl(const TriangleMesh& m) {
    std::ostringstream s;
    s << "solid t\n";
    for (std::size_t t = 0; t < m.triangle_count(); ++t) {
        s << "facet normal 0 0 0\nouter loop\n";
        for (int k = 0; k < 3; ++k)
            s << "vertex " << m.corner(t, k).x << ' ' << m.corner(t, k).y << ' ' << m.corner(t, k).z << '\n';
        s << "endloop\nendfacet\n";
    }
    s << "endsolid t\n";
    return s.str();
}

Sliced slice_with(const TriangleMesh& model, const std::string& name, const std::string& groups) {
    const std::filesystem::path dir = test_temp_dir();
    const std::filesystem::path stl = dir / (name + ".stl"), config = dir / (name + ".json"),
                                gcode = dir / (name + ".gcode");
    std::ofstream(stl) << ascii_stl(model);
    std::ofstream(config) << R"({"machine": {"bed": [[0, 0], [200, 0], [200, 200], [0, 200]]},
                                 "adhesion": {"skirt_loops": 0}, "walls": {"count": 2},
                                 "infill": {"density": 0}, "surfaces": {"top_layers": 0, "bottom_layers": 0})"
                          << (groups.empty() ? "" : ", ") << groups << "}";
    const std::vector<std::string> args = {"slice", stl.string(), "--config", config.string(), "-o", gcode.string()};
    const std::vector<std::string_view> views(args.begin(), args.end());
    std::ostringstream out, err;
    Sliced r;
    r.code = run_cli(views, out, err);
    r.out = out.str(), r.err = err.str();
    std::ifstream g(gcode);
    for (std::string line; std::getline(g, line);)
        if (line.rfind("G1 X", 0) == 0) {
            const double x = std::stod(line.substr(4));
            r.x_lo = std::min(r.x_lo, x), r.x_hi = std::max(r.x_hi, x);
        }
    return r;
}

double number_after(const std::string& text, const std::string& key) {
    const std::size_t at = text.find(key);
    REQUIRE(at != std::string::npos);
    return std::stod(text.substr(at + key.size()));
}

} // namespace

TEST_CASE("transform: the command line scales, turns, cuts and splits (MT14 to MT18)", "[transform][cli]") {
    const TriangleMesh model = box(kLo, kHi);
    // MT14: twice as big, 100 layers, 48 000 mm³; 150 % as "150%".
    Sliced r = slice_with(model, "mt14", R"("transform": {"scale": "200%"})");
    REQUIRE(r.code == 0);
    CHECK(number_after(r.out, "layers: ") == 100);
    CHECK_THAT(number_after(r.out, "sliced_volume: "), WithinRel(48000.0, 1e-3));
    r = slice_with(model, "mt14b", R"("transform": {"scale": 1.5})");
    CHECK_THAT(number_after(r.out, "sliced_volume: "), WithinRel(20250.0, 1e-3));
    // MT15: fit in (15, 100, 100): half size.
    r = slice_with(model, "mt15", R"("transform": {"scale_to_fit": [15, 100, 100]})");
    CHECK(number_after(r.out, "layers: ") == 25);
    CHECK_THAT(number_after(r.out, "sliced_volume: "), WithinRel(750.0, 1e-3));
    // MT16: 45° about Z, X from 82.6 to 117.4; 90° about X, 100 layers.
    r = slice_with(model, "mt16", R"("transform": {"rotate_z": 45})");
    CHECK_THAT(r.x_lo, WithinAbs(82.6, 0.3));
    CHECK_THAT(r.x_hi, WithinAbs(117.4, 0.3));
    r = slice_with(model, "mt16b", R"("transform": {"rotate_x": 90})");
    CHECK(number_after(r.out, "layers: ") == 100);
    // MT17: cut at 5 mm, two halves of 3000 mm³, both printed, 25 layers.
    r = slice_with(model, "mt17", R"("transform": {"cut_z": 5})");
    REQUIRE(r.code == 0);
    CHECK(number_after(r.out, "layers: ") == 25);
    CHECK_THAT(number_after(r.out, "sliced_volume: "), WithinRel(6000.0, 1e-3));
    // MT18: two loose boxes 20 mm apart; split, they are arranged 6 mm apart.
    TriangleMesh pair;
    add_box(pair, {0, 0, 0}, {10, 10, 10});
    add_box(pair, {30, 0, 0}, {40, 10, 5});
    r = slice_with(pair, "mt18", "");
    CHECK_THAT(r.x_lo, WithinAbs(80.2, 0.3));
    CHECK_THAT(r.x_hi, WithinAbs(119.8, 0.3));
    r = slice_with(pair, "mt18b", R"("transform": {"split": true})");
    CHECK_THAT(r.x_lo, WithinAbs(87.2, 0.3));
    CHECK_THAT(r.x_hi, WithinAbs(112.8, 0.3));
    // Pieces and copies do not go together.
    const std::filesystem::path dir = test_temp_dir();
    std::ofstream(dir / "mt18c.json") << R"({"transform": {"split": true}})";
    std::ofstream(dir / "mt18c.stl") << ascii_stl(pair);
    const std::vector<std::string> args = {"slice", (dir / "mt18c.stl").string(), "--config", (dir / "mt18c.json").string(),
                                           "-o", (dir / "mt18c.gcode").string(), "--copies", "2"};
    const std::vector<std::string_view> views(args.begin(), args.end());
    std::ostringstream out, err;
    CHECK(run_cli(views, out, err) == kExitUsage);
    CHECK(err.str().find("--copies") != std::string::npos);
}
