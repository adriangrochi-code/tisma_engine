#include "ui/plate.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <cstdint>

using namespace tisma;
using namespace tisma::ui;
using Catch::Matchers::WithinAbs;

namespace {

// A box from (x0, y0, z0) with sides (sx, sy, sz), facing out.
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

bool near(const Rotation& a, const Rotation& b) {
    for (std::size_t i = 0; i < 9; ++i)
        if (std::fabs(a[i] - b[i]) > 1e-9)
            return false;
    return true;
}

} // namespace

TEST_CASE("plate: a model is scaled, turned and set on the bed where it is asked", "[plate]") {
    // A 10 x 20 x 30 box anywhere in its file.
    PlateObject o{"box", box(-50, 7, 40, 10, 20, 30), {}, {}};
    o.place.x = 100, o.place.y = 120;
    BoundingBox b = placed_mesh(o).bounding_box();
    CHECK_THAT(b.min.x, WithinAbs(95, 1e-4));
    CHECK_THAT(b.max.x, WithinAbs(105, 1e-4));
    CHECK_THAT(b.min.y, WithinAbs(110, 1e-4));
    CHECK_THAT(b.max.y, WithinAbs(130, 1e-4));
    CHECK_THAT(b.min.z, WithinAbs(0, 1e-4));
    CHECK_THAT(b.max.z, WithinAbs(30, 1e-4));
    // Twice as big in X; a quarter turn about Z swaps its sides, about X lays it down.
    o.place.scale = {2.0, 1.0, 1.0};
    const auto size = placed_size(o);
    CHECK_THAT(size[0], WithinAbs(20, 1e-4));
    o.place.rotation = rotated(kNoRotation, 2, 90.0);
    b = placed_mesh(o).bounding_box();
    CHECK_THAT(b.max.x - b.min.x, WithinAbs(20, 1e-4)); // the 20 of Y now along X
    CHECK_THAT(b.max.y - b.min.y, WithinAbs(20, 1e-4)); // the doubled 10 now along Y
    o.place.rotation = rotated(kNoRotation, 0, 90.0);
    b = placed_mesh(o).bounding_box();
    CHECK_THAT(b.max.z - b.min.z, WithinAbs(20, 1e-4));
    CHECK_THAT(b.min.z, WithinAbs(0, 1e-4));
    // The volume follows the scale, and the triangles still face out.
    o.place.scale = {2.0, 1.5, 0.5};
    CHECK_THAT(placed_mesh(o).signed_volume(), WithinAbs(6000.0 * 1.5, 1e-1));
}

TEST_CASE("plate: angles for the panel give the same rotation back", "[plate]") {
    for (const auto& d : {std::array<double, 3>{0, 0, 0}, {30, 0, 0}, {0, -45, 0}, {10, 20, 30}, {-170, 80, 95}}) {
        CAPTURE(d[0], d[1], d[2]);
        const Rotation r = from_euler_degrees(d);
        CHECK(near(from_euler_degrees(euler_degrees(r)), r));
    }
    // Straight up: the X and Z angles turn the same way; the rotation still comes back.
    const Rotation up = from_euler_degrees({25, 90, 40});
    CHECK(near(from_euler_degrees(euler_degrees(up)), up));
    CHECK(near(rotated(rotated(kNoRotation, 1, 30), 1, -30), kNoRotation));
}

TEST_CASE("plate: lay on face turns the chosen face down", "[plate]") {
    PlateObject o{"box", box(0, 0, 0, 10, 20, 30), {}, {}};
    o.place.x = 50, o.place.y = 50;
    // Its +X face (normal +X), then a slanted normal, then the top (half a turn).
    for (const Vec3 n : {Vec3{1, 0, 0}, normalize(Vec3{1, 1, 1}), Vec3{0, 0, 1}, Vec3{0, 0, -1}}) {
        CAPTURE(n.x, n.y, n.z);
        const Rotation r = laid_on(kNoRotation, n);
        const Vec3 down{float(r[0] * n.x + r[1] * n.y + r[2] * n.z), float(r[3] * n.x + r[4] * n.y + r[5] * n.z),
                        float(r[6] * n.x + r[7] * n.y + r[8] * n.z)};
        CHECK_THAT(down.x, WithinAbs(0, 1e-6));
        CHECK_THAT(down.y, WithinAbs(0, 1e-6));
        CHECK_THAT(down.z, WithinAbs(-1, 1e-6));
    }
    o.place.rotation = laid_on(kNoRotation, {1, 0, 0});
    const BoundingBox b = placed_mesh(o).bounding_box();
    CHECK_THAT(b.max.z - b.min.z, WithinAbs(10, 1e-4)); // it lies on the 20 x 30 face
}

TEST_CASE("plate: arrange puts the models in a compact grid, centred (placement.md)", "[plate]") {
    // PL7: two 20 mm cubes, 6 mm apart, centred on (100, 100).
    std::vector<std::array<double, 2>> two = arrange({{20, 20}, {20, 20}}, {100, 100}, 6.0);
    CHECK_THAT(two[0][0], WithinAbs(87, 1e-9));
    CHECK_THAT(two[1][0], WithinAbs(113, 1e-9));
    CHECK_THAT(two[0][1], WithinAbs(100, 1e-9));
    CHECK_THAT(two[1][1], WithinAbs(100, 1e-9));
    // PL8: three in a 2 x 2 grid with one place empty, the grid from 77 to 123.
    const std::vector<std::array<double, 2>> three = arrange({{20, 20}, {20, 20}, {20, 20}}, {100, 100}, 6.0);
    double lo_x = 1e9, hi_x = -1e9, lo_y = 1e9, hi_y = -1e9;
    for (const auto& c : three)
        lo_x = std::min(lo_x, c[0] - 10), hi_x = std::max(hi_x, c[0] + 10), lo_y = std::min(lo_y, c[1] - 10),
        hi_y = std::max(hi_y, c[1] + 10);
    CHECK_THAT(lo_x, WithinAbs(77, 1e-9));
    CHECK_THAT(hi_x, WithinAbs(123, 1e-9));
    CHECK_THAT(lo_y, WithinAbs(77, 1e-9));
    CHECK_THAT(hi_y, WithinAbs(123, 1e-9));
    // Different sizes never overlap and keep the spacing.
    const std::vector<std::array<double, 2>> sizes = {{40, 10}, {10, 30}, {25, 25}, {5, 5}, {60, 8}};
    const auto at = arrange(sizes, {0, 0}, 6.0);
    for (std::size_t i = 0; i < sizes.size(); ++i)
        for (std::size_t j = i + 1; j < sizes.size(); ++j) {
            const double gap_x = std::fabs(at[i][0] - at[j][0]) - 0.5 * (sizes[i][0] + sizes[j][0]);
            const double gap_y = std::fabs(at[i][1] - at[j][1]) - 0.5 * (sizes[i][1] + sizes[j][1]);
            CHECK(std::max(gap_x, gap_y) >= 6.0 - 1e-9);
        }
    CHECK(arrange({}, {0, 0}, 6.0).empty());
}

TEST_CASE("plate: a click picks the nearest model and its face", "[plate]") {
    std::vector<TriangleMesh> placed = {box(0, 0, 0, 10, 10, 10), box(20, 0, 0, 10, 10, 20)};
    const auto hit = pick(placed, {25, 5, 100}, {0, 0, -1});
    REQUIRE(hit);
    CHECK(hit->object == 1);
    CHECK_THAT(hit->distance, WithinAbs(80, 1e-6));
    const Vec3 n = triangle_normal(placed[1], hit->triangle);
    CHECK_THAT(n.z, WithinAbs(1, 1e-6)); // its top
    CHECK_FALSE(pick(placed, {15, 5, 100}, {0, 0, -1})); // between them
    const TriangleMesh both = plate_mesh(placed);
    CHECK(both.triangle_count() == 24);
    CHECK_THAT(both.signed_volume(), WithinAbs(3000.0, 1e-3));
}

TEST_CASE("plate: the paint follows each model, and the plate joins it in order", "[plate][paint]") {
    std::vector<PlateObject> objects = {{"a", box(0, 0, 0, 10, 10, 10), {}, {}}, {"b", box(0, 0, 0, 20, 10, 5), {}, {}}};
    objects[0].place.x = 50, objects[0].place.y = 50;
    objects[1].place.x = 100, objects[1].place.y = 50;
    objects[1].place.rotation = rotated(kNoRotation, 2, 90.0);
    // The top of the second model, painted where it is on the bed.
    std::vector<TriangleMesh> placed = {placed_mesh(objects[0]), placed_mesh(objects[1])};
    Brush brush;
    brush.centre = {100, 50, 5};
    brush.radius = 3.0;
    brush.view = {0, 0, -1};
    REQUIRE(paint(objects[1].painting, PaintChannel::Seam, placed[1], brush) > 0);
    // Moved and turned, the paint is still on its top, around its middle.
    objects[1].place.x = 30;
    objects[1].place.rotation = kNoRotation;
    placed[1] = placed_mesh(objects[1]);
    const PaintMesh mesh = paint_mesh(objects, placed, PaintChannel::Seam);
    REQUIRE(mesh.groups.size() == 1);
    CHECK(mesh.groups[0].mark == kPaintEnforce);
    CHECK(mesh.groups[0].count == mesh.vertices.size() / 6);
    for (std::size_t k = 0; k < mesh.vertices.size(); k += 6) {
        // The pieces on the brush's edge go past it by up to a piece's size.
        CHECK(std::hypot(mesh.vertices[k] - 30.0f, mesh.vertices[k + 1] - 50.0f) < 3.0 + brush.detail);
        CHECK_THAT(mesh.vertices[k + 2], WithinAbs(5.0, 1e-4));
        CHECK_THAT(mesh.vertices[k + 5], WithinAbs(1.0, 1e-6)); // facing up
    }
    CHECK(paint_mesh(objects, placed, PaintChannel::Support).vertices.empty());
    // Joined after the first model's 12 triangles.
    const Painting plate = plate_painting(objects);
    for (std::size_t t = 0; t < 12; ++t) {
        CHECK(plate.tree(PaintChannel::Seam, t).empty());
        CHECK(plate.tree(PaintChannel::Seam, 12 + t) == objects[1].painting.tree(PaintChannel::Seam, t));
    }
}
