#include "gcode/order.hpp"
#include "gcode/writer.hpp"
#include "mesh/indexed_mesh.hpp"
#include "mesh/paint.hpp"
#include "slice/painted.hpp"
#include "slice/pipeline.hpp"
#include "support/support.hpp"
#include "test_printer.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <numbers>
#include <regex>
#include <sstream>
#include <stdexcept>

using namespace tisma;
using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

namespace {

// A box from (x0, y0, z0) with sides (sx, sy, sz), facing out: bottom (2
// triangles), top (2), then the sides from -Y counter-clockwise (2 each).
TriangleMesh box(float x0, float y0, float z0, float sx, float sy, float sz) {
    const Vec3f v[8] = {{x0, y0, z0},           {x0 + sx, y0, z0},           {x0 + sx, y0 + sy, z0},
                        {x0, y0 + sy, z0},      {x0, y0, z0 + sz},           {x0 + sx, y0, z0 + sz},
                        {x0 + sx, y0 + sy, z0 + sz}, {x0, y0 + sy, z0 + sz}};
    TriangleMesh m;
    const auto quad = [&](int a, int b, int c, int d) {
        m.add_triangle(v[a], v[b], v[c]);
        m.add_triangle(v[a], v[c], v[d]);
    };
    quad(0, 3, 2, 1);
    quad(4, 5, 6, 7);
    for (int i = 0; i < 4; ++i)
        quad(i, (i + 1) % 4, 4 + (i + 1) % 4, 4 + i);
    return m;
}

IndexedMesh indexed(const TriangleMesh& m) {
    IndexedMesh out;
    for (std::size_t t = 0; t < m.triangle_count(); ++t) {
        std::array<std::uint32_t, 3> tri{};
        for (int k = 0; k < 3; ++k) {
            tri[std::size_t(k)] = std::uint32_t(out.vertices.size());
            out.vertices.push_back(m.corner(t, k));
        }
        out.triangles.push_back(tri);
    }
    return out;
}

// The painted area with this mark, mm2.
double painted_area(const Painting& p, PaintChannel channel, const TriangleMesh& m, std::uint8_t mark) {
    double sum = 0.0;
    for (std::size_t t = 0; t < m.triangle_count(); ++t)
        for_each_painted(p, channel, t, m.corner(t, 0), m.corner(t, 1), m.corner(t, 2),
                         [&](const Vec3f& a, const Vec3f& b, const Vec3f& c, std::uint8_t k) {
                             if (k != mark)
                                 return;
                             const double ux = b.x - a.x, uy = b.y - a.y, uz = b.z - a.z;
                             const double vx = c.x - a.x, vy = c.y - a.y, vz = c.z - a.z;
                             const double nx = uy * vz - uz * vy, ny = uz * vx - ux * vz, nz = ux * vy - uy * vx;
                             sum += 0.5 * std::sqrt(nx * nx + ny * ny + nz * nz);
                         });
    return sum;
}

Polygons rect(double x0, double y0, double x1, double y1) {
    return {{{to_units(x0), to_units(y0)}, {to_units(x1), to_units(y0)}, {to_units(x1), to_units(y1)}, {to_units(x0), to_units(y1)}}};
}

bool contains(const Polygons& area, double x, double y) {
    const Clipper2Lib::Point64 q{to_units(x), to_units(y)};
    int winding = 0;
    for (const Polygon& p : area)
        if (Clipper2Lib::PointInPolygon(q, p) != Clipper2Lib::PointInPolygonResult::IsOutside)
            winding += Clipper2Lib::IsPositive(p) ? 1 : -1;
    return winding != 0;
}

Toolpath square_loop() {
    Toolpath t;
    t.closed = true;
    for (const auto& [x, y] : {std::pair{0.0, 0.0}, {10.0, 0.0}, {10.0, 10.0}, {0.0, 10.0}})
        t.points.push_back({{x, y}, 0.4});
    return t;
}

TriangleMesh joined(const TriangleMesh& a, const TriangleMesh& b) {
    TriangleMesh m = a;
    for (std::size_t t = 0; t < b.triangle_count(); ++t)
        m.add_triangle(b.corner(t, 0), b.corner(t, 1), b.corner(t, 2));
    return m;
}

std::vector<Layer> layers_of(double height, double top) {
    std::vector<Layer> out;
    for (double z = 0.0; z < top - 1e-9; z += height)
        out.push_back({z, z + height});
    return out;
}

} // namespace

TEST_CASE("paint: a round brush paints a disc on a large face and erases it again", "[paint]") {
    const TriangleMesh m = box(0, 0, 0, 40, 40, 10);
    Painting p;
    Brush brush;
    brush.centre = {20, 20, 10};
    brush.radius = 5.0;
    brush.detail = 0.1;
    brush.view = {0, 0, -1}; // looking down
    CHECK(paint(p, PaintChannel::Support, m, brush) == 2); // both top triangles
    const double disc = std::numbers::pi * 25.0;
    CHECK_THAT(painted_area(p, PaintChannel::Support, m, kPaintEnforce), WithinRel(disc, 0.03));
    CHECK(p.has(PaintChannel::Support, kPaintEnforce));
    CHECK_FALSE(p.has(PaintChannel::Support, kPaintBlock));
    CHECK(p.empty(PaintChannel::Seam));
    // The bottom faces away from the eye; the sides are farther than the radius.
    CHECK(p.tree(PaintChannel::Support, 0).empty());
    CHECK(p.tree(PaintChannel::Support, 4).empty());
    // Painting the same again changes nothing; blocking half over it takes its share.
    CHECK(paint(p, PaintChannel::Support, m, brush) == 0);
    Brush block = brush;
    block.mark = kPaintBlock;
    block.centre = {25, 20, 10};
    paint(p, PaintChannel::Support, m, block);
    const double both = painted_area(p, PaintChannel::Support, m, kPaintEnforce) +
                        painted_area(p, PaintChannel::Support, m, kPaintBlock);
    // Two discs of radius 5 with centres 5 apart cover 2 pi r2 - lens.
    const double lens = 2.0 * 25.0 * std::acos(0.5) - 2.5 * std::sqrt(100.0 - 25.0);
    CHECK_THAT(both, WithinRel(2.0 * disc - lens, 0.03));
    // Erasing with a larger brush leaves nothing, and the trees fold back up.
    Brush erase = brush;
    erase.mark = kPaintNone;
    erase.radius = 12.0;
    paint(p, PaintChannel::Support, m, erase);
    CHECK(p.empty());
    CHECK(p.size(PaintChannel::Support) == 0);
}

TEST_CASE("paint: a brush larger than a triangle marks it whole", "[paint]") {
    const TriangleMesh m = box(0, 0, 0, 10, 10, 10);
    Painting p;
    Brush brush;
    brush.centre = {5, 5, 5};
    brush.radius = 20.0;
    brush.mark = 3; // the third extruder
    CHECK(paint(p, PaintChannel::Color, m, brush) == 12);
    for (std::size_t t = 0; t < 12; ++t)
        CHECK(p.tree(PaintChannel::Color, t) == Painting::Tree{3});
    CHECK_THAT(painted_area(p, PaintChannel::Color, m, 3), WithinAbs(600.0, 1e-3));
}

TEST_CASE("paint: the painting is kept as bytes, and joins other meshes' paintings", "[paint]") {
    const TriangleMesh m = box(0, 0, 0, 30, 30, 10);
    Painting p;
    Brush brush;
    brush.centre = {10, 10, 10};
    brush.radius = 3.0;
    paint(p, PaintChannel::Seam, m, brush);
    brush.mark = kPaintBlock;
    brush.centre = {0, 15, 5};
    paint(p, PaintChannel::Support, m, brush);
    const std::string bytes = painting_to_bytes(p);
    CHECK(painting_from_bytes(bytes, 12) == p);
    CHECK(painting_from_bytes(painting_to_bytes(Painting{}), 0).empty());
    // Damage: cut short, a mark out of range, a tree with bytes left over.
    CHECK_THROWS_AS(painting_from_bytes(bytes.substr(0, bytes.size() - 1), 12), std::invalid_argument);
    std::string bad = painting_to_bytes(Painting{});
    bad[0] = 1; // one tree in the support channel, and nothing after
    CHECK_THROWS_AS(painting_from_bytes(bad, 12), std::invalid_argument);
    Painting one;
    one.set_tree(PaintChannel::Fuzzy, 2, {kPaintEnforce});
    std::string extra = painting_to_bytes(one);
    CHECK(painting_from_bytes(extra, 3) == one);
    // A triangle past the mesh is refused before anything is made for it.
    CHECK_THROWS_AS(painting_from_bytes(extra, 2), std::invalid_argument);
    // Support and seam counts, the fuzzy count, index and length (at byte 16),
    // then the tree: made two bytes long, a leaf followed by junk.
    REQUIRE(extra.size() == 25);
    extra[16] = 2;
    extra.insert(extra.begin() + 21, char(kPaintEnforce));
    CHECK_THROWS_AS(painting_from_bytes(extra, 3), std::invalid_argument);
    Painting deep;
    deep.set_tree(PaintChannel::Support, 0, Painting::Tree(40, Painting::kSplit));
    CHECK_THROWS_AS(painting_from_bytes(painting_to_bytes(deep), 1), std::invalid_argument);
    // Joined after 12 triangles of another mesh.
    Painting plate;
    plate.set_tree(PaintChannel::Seam, 1, {kPaintEnforce});
    plate.append(p, 12);
    CHECK(plate.tree(PaintChannel::Seam, 1) == Painting::Tree{kPaintEnforce});
    for (std::size_t t = 0; t < 12; ++t) {
        CHECK(plate.tree(PaintChannel::Seam, 12 + t) == p.tree(PaintChannel::Seam, t));
        CHECK(plate.tree(PaintChannel::Support, 12 + t) == p.tree(PaintChannel::Support, t));
    }
}

TEST_CASE("paint: painted faces fall in the layers they reach, grown", "[paint]") {
    const TriangleMesh m = box(0, 0, 0, 20, 20, 10);
    const IndexedMesh im = indexed(m);
    const std::vector<Layer> layers = layers_of(0.2, 10.0);
    Painting p;
    // The whole top (its two triangles).
    p.set_tree(PaintChannel::Support, 2, {kPaintEnforce});
    p.set_tree(PaintChannel::Support, 3, {kPaintEnforce});
    std::vector<Polygons> areas = painted_areas(painted_pieces(im, p, PaintChannel::Support, kPaintEnforce), layers, 0.5);
    REQUIRE(areas.size() == 50);
    for (std::size_t l = 0; l + 1 < areas.size(); ++l)
        CHECK(areas[l].empty());
    CHECK_THAT(area_mm2(areas.back()), WithinRel(21.0 * 21.0, 0.01)); // grown by 0.5 all round (mitred corners)
    // A spot on the -X wall around (0, 10, 5): a band along X = 0 in the layers from 3 to 7.
    Painting wall;
    Brush spot;
    spot.centre = {0, 10, 5};
    spot.radius = 2.0;
    spot.detail = 0.1;
    paint(wall, PaintChannel::Seam, m, spot);
    areas = painted_areas(painted_pieces(im, wall, PaintChannel::Seam, kPaintEnforce), layers, 0.3);
    for (std::size_t l = 0; l < areas.size(); ++l) {
        const double z = layers[l].slice_z();
        CAPTURE(l, z);
        if (z < 2.9 || z > 7.1) {
            CHECK(areas[l].empty());
            continue;
        }
        if (z < 3.1 || z > 6.9)
            continue; // the edge of the spot
        REQUIRE_FALSE(areas[l].empty());
        const Clipper2Lib::Rect64 b = Clipper2Lib::GetBounds(areas[l]);
        CHECK_THAT(to_mm(b.left), WithinAbs(-0.3, 0.01));
        CHECK_THAT(to_mm(b.right), WithinAbs(0.3, 0.01));
        // The longest chord of the spot in the layer (at its height nearest the
        // spot's middle), grown by 0.3 at each end. Rect64's top is its least Y.
        const double off = std::max(0.0, std::fabs(z - 5.0) - 0.1);
        const double half = std::sqrt(std::max(0.0, 4.0 - off * off));
        CHECK_THAT(to_mm(b.bottom - b.top), WithinAbs(2.0 * half + 0.6, 0.1));
    }
    // Nothing with another mark.
    CHECK(painted_pieces(im, wall, PaintChannel::Seam, kPaintBlock).empty());
}

TEST_CASE("paint: support goes where it is enforced and not where it is blocked (supports_more.md, 3 to 5)", "[paint]") {
    // A 10 mm square for 10 layers, then 20 mm wide: the part from X 10 to 20 overhangs.
    std::vector<Polygons> regions;
    std::vector<double> heights(15, 0.2);
    for (int l = 0; l < 15; ++l)
        regions.push_back(l < 10 ? rect(0, 0, 10, 10) : rect(0, 0, 20, 10));
    SupportParams sp;
    sp.z_gap_layers = 1;
    const std::vector<Polygons> plain = support_areas(regions, heights, sp);
    CHECK(contains(plain[5], 15, 2));
    CHECK(contains(plain[5], 15, 8));
    // Blocked under the half at Y < 5.
    std::vector<Polygons> block(15);
    block[10] = rect(10, -1, 21, 5);
    sp.block = &block;
    std::vector<Polygons> areas = support_areas(regions, heights, sp);
    CHECK_FALSE(contains(areas[5], 15, 2));
    CHECK(contains(areas[5], 15, 8));
    // Automatic off: nothing without an enforcer; with one, only in it.
    sp.block = nullptr;
    sp.automatic = false;
    areas = support_areas(regions, heights, sp);
    for (const Polygons& a : areas)
        CHECK(a.empty());
    std::vector<Polygons> enforce(15);
    enforce[10] = rect(10, 5, 21, 11);
    sp.enforce = &enforce;
    areas = support_areas(regions, heights, sp);
    CHECK_FALSE(contains(areas[5], 15, 2));
    CHECK(contains(areas[5], 15, 8));
    // A slope gentler than the automatic support needs (0.1 mm a layer, half a
    // bead allowed) gets support only where enforced, however little it reaches.
    regions.clear();
    for (int l = 0; l < 15; ++l)
        regions.push_back(rect(0, 0, 10 + 0.1 * l, 10));
    sp.automatic = true;
    sp.enforce = nullptr;
    for (const Polygons& a : support_areas(regions, heights, sp))
        CHECK(a.empty());
    for (Polygons& e : enforce)
        e = rect(-1, -1, 30, 30);
    sp.enforce = &enforce;
    sp.xy_gap = 0.0;
    areas = support_areas(regions, heights, sp);
    CHECK_FALSE(areas[2].empty());
}

TEST_CASE("paint: a painted seam starts the loop inside the paint, in every seam mode", "[paint]") {
    // A spot on the middle of the right side, away from every corner.
    const Polygons spot = rect(9.5, 4.5, 10.5, 5.5);
    SeamPaint paint;
    paint.enforce = &spot;
    for (const SeamMode mode : {SeamMode::Aligned, SeamMode::Rear, SeamMode::Random, SeamMode::Nearest}) {
        CAPTURE(int(mode));
        SeamMemory memory;
        Point2d cursor{0, 0};
        const std::vector<Toolpath> out = order_paths({square_loop()}, cursor, mode, 7, memory, &paint);
        REQUIRE(out.size() == 1);
        const Point2d start = out[0].points.front().p;
        CHECK(contains(spot, start.x, start.y));
        if (mode == SeamMode::Aligned || mode == SeamMode::Rear) { // the middle of the painted stretch
            CHECK(start.x == 10.0);
            CHECK_THAT(start.y, WithinAbs(5.0, kSeamPaintStep));
        }
        CHECK(out[0].points.size() == 5); // the seam point added on the side
        CHECK(cursor.x == start.x);
    }
    // Blocked along the back: the rear seam leaves it, on a side just below it.
    const Polygons back = rect(-1, 9, 11, 11);
    SeamPaint blocked;
    blocked.block = &back;
    SeamMemory memory;
    Point2d cursor{0, 0};
    std::vector<Toolpath> out = order_paths({square_loop()}, cursor, SeamMode::Rear, 7, memory, &blocked);
    Point2d start = out[0].points.front().p;
    CHECK(start.y < 9.0);
    CHECK(start.y > 8.5);
    CHECK((start.x == 0.0 || start.x == 10.0));
    // Paint away from the loop changes nothing.
    const Polygons far = rect(50, 50, 51, 51);
    SeamPaint away;
    away.enforce = &far;
    away.block = &far;
    SeamMemory m1, m2;
    Point2d c1{0, 0}, c2{0, 0};
    const std::vector<Toolpath> a = order_paths({square_loop()}, c1, SeamMode::Rear, 7, m1, &away);
    const std::vector<Toolpath> b = order_paths({square_loop()}, c2, SeamMode::Rear, 7, m2);
    REQUIRE(a[0].points.size() == b[0].points.size());
    for (std::size_t i = 0; i < a[0].points.size(); ++i)
        CHECK((a[0].points[i].p.x == b[0].points[i].p.x && a[0].points[i].p.y == b[0].points[i].p.y));
}

TEST_CASE("paint: the slicer follows painted support and seams", "[paint]") {
    Settings s = test_printer();
    s.support = true;
    // A 10 mm cube with a 10 x 10 x 2 mm shelf on its +X side, 8 mm up.
    const TriangleMesh part = joined(box(0, 0, 0, 10, 10, 10), box(10, 0, 8, 10, 10, 2));
    const IndexedMesh mesh = weld_exact(part);
    const auto layer_at = [](const SlicedModel& m, double z) {
        std::size_t l = 0;
        while (l + 1 < m.layers.size() && m.layers[l].top < z)
            ++l;
        return l;
    };
    SlicedModel plain = slice_model(mesh, s);
    std::size_t l = layer_at(plain, 4.0);
    CHECK(contains(plain.support_areas[l], 15, 1.5));
    CHECK(contains(plain.support_areas[l], 15, 8));
    // A blocker painted on the shelf's bottom around (15, 0).
    Painting p;
    Brush brush;
    brush.centre = {15, 0, 8};
    brush.radius = 4.0;
    brush.view = {0, 0, 1}; // looking up at it
    brush.mark = kPaintBlock;
    CHECK(paint(p, PaintChannel::Support, part, brush) > 0);
    SlicedModel blocked = slice_model(mesh, s, nullptr, nullptr, &p);
    CHECK_FALSE(contains(blocked.support_areas[l], 15, 1.5));
    CHECK(contains(blocked.support_areas[l], 15, 8));
    // Automatic support off, an enforcer around (15, 8): support there only.
    s.support_automatic = false;
    Painting q;
    brush.centre = {15, 8, 8};
    brush.radius = 2.0;
    brush.mark = kPaintEnforce;
    paint(q, PaintChannel::Support, part, brush);
    SlicedModel enforced = slice_model(mesh, s, nullptr, nullptr, &q);
    CHECK(contains(enforced.support_areas[l], 15, 8));
    CHECK_FALSE(contains(enforced.support_areas[l], 15, 2));
    // The painted area and the margin round it (supports_skirt.md, rule 2), not the whole shelf.
    CHECK_FALSE(contains(enforced.support_areas[l], 10.8, 8));
    // Without paint, automatic off means no support at all.
    for (const Polygons& a : slice_model(mesh, s).support_areas)
        CHECK(a.empty());

    // A seam painted down the +X side of a 20 mm cube at Y = 15.
    Settings c = test_printer();
    const TriangleMesh cube = box(0, 0, 0, 20, 20, 20);
    Painting seam;
    for (double z = 0.0; z <= 20.0; z += 0.5) {
        Brush b;
        b.centre = {20, 15, float(z)};
        b.radius = 1.0;
        b.view = {-1, 0, 0};
        paint(seam, PaintChannel::Seam, cube, b);
    }
    const SlicedModel sliced = slice_model(weld_exact(cube), c, nullptr, nullptr, &seam);
    REQUIRE(sliced.seam_enforce.size() == sliced.layers.size());
    std::ostringstream gcode;
    write_gcode(gcode, sliced, c);
    // The last position before each outer wall is where it starts.
    const std::regex xy(R"(^G[01] .*X([-0-9.]+) Y([-0-9.]+))");
    std::istringstream in(gcode.str());
    std::string line;
    double x = 0.0, y = 0.0;
    int outer = 0, on_seam = 0;
    std::smatch m;
    while (std::getline(in, line)) {
        if (std::regex_search(line, m, xy))
            x = std::stod(m[1]), y = std::stod(m[2]);
        if (line == ";TYPE:External perimeter") {
            ++outer;
            on_seam += x > 19.5 && std::fabs(y - 15.0) < 0.3 ? 1 : 0;
        }
    }
    CHECK(outer >= 90);
    CHECK(on_seam == outer);
}
