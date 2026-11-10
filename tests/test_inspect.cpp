#include "mesh/inspect.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>

using namespace tisma;
using Catch::Matchers::WithinAbs;

namespace {

// A box's 12 triangles, facing out.
void add_box(TriangleMesh& m, float x0, float y0, float z0, float x1, float y1, float z1) {
    const Vec3f p[8] = {{x0, y0, z0}, {x1, y0, z0}, {x1, y1, z0}, {x0, y1, z0},
                        {x0, y0, z1}, {x1, y0, z1}, {x1, y1, z1}, {x0, y1, z1}};
    const int quads[6][4] = {{0, 3, 2, 1}, {4, 5, 6, 7}, {0, 1, 5, 4}, {1, 2, 6, 5}, {2, 3, 7, 6}, {3, 0, 4, 7}};
    for (const auto& q : quads) {
        m.add_triangle(p[q[0]], p[q[1]], p[q[2]]);
        m.add_triangle(p[q[0]], p[q[2]], p[q[3]]);
    }
}

} // namespace

TEST_CASE("inspect: a cube's summary (orca_2_5.md, rule 26, OR26)", "[inspect]") {
    TriangleMesh cube;
    add_box(cube, 0, 0, 0, 20, 20, 20);
    const MeshInspection m = inspect_mesh(cube);
    CHECK_THAT(m.size[0], WithinAbs(20.0, 1e-9));
    CHECK_THAT(m.size[2], WithinAbs(20.0, 1e-9));
    CHECK_THAT(m.volume, WithinAbs(8000.0, 1e-6));
    CHECK(m.triangles == 12);
    CHECK(m.closed);
    CHECK(m.parts == 1);
    CHECK(inspection_json(m) ==
          R"({"size": [20, 20, 20], "volume": 8000, "triangles": 12, "closed": true, "open_edges": 0, "parts": 1})");
    // Two boxes apart, one face missing: two parts, open.
    TriangleMesh two = cube;
    add_box(two, 30, 0, 0, 40, 10, 10);
    TriangleMesh open(std::vector<Vec3f>(two.corners().begin(), two.corners().end() - 3));
    const MeshInspection o = inspect_mesh(open);
    CHECK(o.parts == 2);
    CHECK_FALSE(o.closed);
    CHECK(o.open_edges == 3);
    CHECK(inspect_mesh(TriangleMesh{}).triangles == 0);
}

TEST_CASE("inspect: the largest flat face goes on the bed (rule 25, OR25)", "[inspect]") {
    // An L lying on its short leg: the long leg's side (40 x 10) is the largest face.
    TriangleMesh l;
    add_box(l, 0, 0, 0, 10, 10, 40); // standing leg
    add_box(l, 10, 0, 0, 30, 10, 5); // foot
    const auto normal = largest_flat_face(l);
    REQUIRE(normal);
    CHECK_THAT(std::abs((*normal)[1]), WithinAbs(1.0, 1e-9)); // a side facing along Y: 40 x 10 and 20 x 5 in one plane
    const TriangleMesh laid = laid_on_face(l, *normal);
    const BoundingBox box = laid.bounding_box();
    CHECK_THAT(double(box.max.z) - box.min.z, WithinAbs(10.0, 1e-4)); // its thickness is now the height
    CHECK_THAT(inspect_mesh(laid).volume, WithinAbs(inspect_mesh(l).volume, 1e-3));
    // A face by a point on it, and by its normal.
    const auto through = face_through(l, {5.0, 5.0, 40.0}); // the top of the leg
    REQUIRE(through);
    CHECK_THAT((*through)[2], WithinAbs(1.0, 1e-9));
    CHECK_FALSE(face_through(l, {5.0, 5.0, 20.0})); // inside, on no face
    const TriangleMesh upside = laid_on_face(l, {0.0, 0.0, 1.0}); // half a turn
    CHECK_THAT(double(upside.bounding_box().max.z), WithinAbs(0.0, 1e-4));
    const TriangleMesh same = laid_on_face(l, {0.0, 0.0, -1.0});
    CHECK(same.corners() == l.corners());
}

TEST_CASE("inspect: what is painted, by kind (rule 26, --inspect-paint)", "[inspect]") {
    TriangleMesh cube;
    add_box(cube, 0, 0, 0, 20, 20, 20); // each triangle 200 mm2
    Painting p;
    CHECK(paint_inspection_json(cube, p) == R"({"support": {}, "seam": {}, "fuzzy": {}, "color": {}})");
    p.set_tree(PaintChannel::Support, 0, {kPaintEnforce});
    p.set_tree(PaintChannel::Support, 1, {kPaintBlock});
    p.set_tree(PaintChannel::Color, 2, {2});
    // A triangle split in four, one quarter fuzzy.
    p.set_tree(PaintChannel::Fuzzy, 3, {Painting::kSplit, kPaintEnforce, kPaintNone, kPaintNone, kPaintNone});
    CHECK(paint_inspection_json(cube, p) ==
          R"({"support": {"enforce": 200, "block": 200}, "seam": {}, "fuzzy": {"enforce": 50}, "color": {"extruder_2": 200}})");
}
