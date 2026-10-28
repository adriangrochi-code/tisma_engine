#include "ui/plate.hpp"
#include "ui/measure.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <algorithm>
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

TEST_CASE("plate: the triangle index finds what looking at every triangle finds", "[plate]") {
    // A sphere of 20 000 triangles and a box beside it, as one mesh.
    TriangleMesh m = box(30, 0, 0, 10, 10, 10);
    const int n = 100;
    const auto at = [&](int i, int j) {
        const double th = 3.14159265358979 * i / n, ph = 2 * 3.14159265358979 * j / n;
        return Vec3f{float(10 * std::sin(th) * std::cos(ph)), float(10 * std::sin(th) * std::sin(ph)),
                     float(10 + 10 * std::cos(th))};
    };
    for (int i = 0; i < n; ++i)
        for (int j = 0; j < n; ++j) {
            m.add_triangle(at(i, j), at(i + 1, j), at(i + 1, j + 1));
            m.add_triangle(at(i, j), at(i + 1, j + 1), at(i, j + 1));
        }
    const TriangleIndex index(m);
    const std::vector<TriangleIndex> indexes{TriangleIndex(m)};
    int hits = 0;
    for (int k = 0; k < 400; ++k) {
        const float x = -15.0f + 0.15f * float(k % 20) * 3.7f, y = -12.0f + 0.13f * float(k / 20) * 9.1f;
        const Vec3 origin{x, y, 60.0f};
        const Vec3 direction = normalize(Vec3{0.1f * float(k % 7) - 0.3f, 0.05f * float(k % 5) - 0.1f, -1.0f});
        const auto slow = pick({m}, origin, direction);
        const auto fast = pick({m}, indexes, origin, direction);
        REQUIRE(slow.has_value() == fast.has_value());
        if (!slow)
            continue;
        ++hits;
        CHECK(fast->triangle == slow->triangle);
        CHECK(fast->distance == slow->distance);
    }
    CHECK(hits > 100);
    // Near a box: every triangle whose box meets it, and nothing far.
    const Vec3f lo{8, -2, 8}, hi{12, 2, 12};
    std::vector<std::uint32_t> near = index.near(lo, hi);
    std::sort(near.begin(), near.end());
    for (std::size_t t = 0; t < m.triangle_count(); ++t) {
        float x0 = 1e9f, x1 = -1e9f, y0 = 1e9f, y1 = -1e9f, z0 = 1e9f, z1 = -1e9f;
        for (int c = 0; c < 3; ++c) {
            const Vec3f& v = m.corner(t, c);
            x0 = std::min(x0, v.x), x1 = std::max(x1, v.x), y0 = std::min(y0, v.y), y1 = std::max(y1, v.y);
            z0 = std::min(z0, v.z), z1 = std::max(z1, v.z);
        }
        const bool meets = x0 <= hi.x && x1 >= lo.x && y0 <= hi.y && y1 >= lo.y && z0 <= hi.z && z1 >= lo.z;
        if (meets)
            CHECK(std::binary_search(near.begin(), near.end(), std::uint32_t(t)));
    }
    CHECK(near.size() < m.triangle_count() / 20);
    CHECK(TriangleIndex().near(lo, hi).empty());
    CHECK_FALSE(TriangleIndex().hit(m, {0, 0, 50}, {0, 0, -1}));
}

TEST_CASE("measure: planes, points, angles and the scale that makes a distance (MT25)", "[ui][measure]") {
    // The box of MT14: 30 × 20 × 10 mm from (5, 7, 3).
    TriangleMesh m;
    const Vec3f lo{5, 7, 3}, hi{35, 27, 13};
    const Vec3f v[8] = {{lo.x, lo.y, lo.z}, {hi.x, lo.y, lo.z}, {hi.x, hi.y, lo.z}, {lo.x, hi.y, lo.z},
                        {lo.x, lo.y, hi.z}, {hi.x, lo.y, hi.z}, {hi.x, hi.y, hi.z}, {lo.x, hi.y, hi.z}};
    const int f[12][3] = {{0, 2, 1}, {0, 3, 2}, {4, 5, 6}, {4, 6, 7}, {0, 1, 5}, {0, 5, 4},
                          {1, 2, 6}, {1, 6, 5}, {2, 3, 7}, {2, 7, 6}, {3, 0, 4}, {3, 4, 7}};
    for (const auto& t : f)
        m.add_triangle(v[t[0]], v[t[1]], v[t[2]]);
    // Triangle 6 is the +X side, 10 the −X side, 2 the top, 4 the front.
    const MeasureFeature right = plane_feature(m, 6), left = plane_feature(m, 10), top = plane_feature(m, 2);
    CHECK(right.normal[0] > 0.99);
    const Measurement across = measure(left, right);
    REQUIRE(across.distance);
    CHECK(std::abs(*across.distance - 30.0) < 1e-9);
    CHECK_FALSE(across.angle);
    // Writing 45: the box grows evenly to 45 × 30 × 15.
    const double factor = 45.0 / *across.distance;
    CHECK(std::abs(factor - 1.5) < 1e-12);
    const Measurement corner = measure(right, top);
    REQUIRE(corner.angle);
    CHECK(std::abs(*corner.angle - 90.0) < 1e-9);
    // Points: the nearest corner to the hit; a point to a plane.
    const MeasureFeature a = point_feature(m, 2, {34.0, 26.0, 13.0});
    CHECK(a.point == Point3{35, 27, 13});
    const MeasureFeature b = point_feature(m, 0, {5.5, 7.5, 3.0});
    CHECK(b.point == Point3{5, 7, 3});
    CHECK(std::abs(*measure(a, b).distance - std::sqrt(900.0 + 400.0 + 100.0)) < 1e-9);
    CHECK(std::abs(*measure(b, top).distance - 10.0) < 1e-9);
}

namespace {

// A cylinder (a cone when the top is smaller) of n sides between z = 0 and
// h, closed; turned inward it is a hole's wall.
void add_round(TriangleMesh& m, double r0, double r1, double h, int n, bool inward) {
    constexpr double kTau = 6.283185307179586;
    const auto at = [&](int i, double r, double z) {
        const double a = kTau * double(i % n) / n;
        return Vec3f{float(r * std::cos(a)), float(r * std::sin(a)), float(z)};
    };
    for (int i = 0; i < n; ++i) {
        Vec3f p[4] = {at(i, r0, 0), at(i + 1, r0, 0), at(i + 1, r1, h), at(i, r1, h)};
        if (inward)
            std::swap(p[1], p[3]);
        m.add_triangle(p[0], p[1], p[2]);
        m.add_triangle(p[0], p[2], p[3]);
    }
}

} // namespace

TEST_CASE("measure: a hole's diameter, a cylinder's and a cone's angle", "[ui][measure]") {
    // A boss of 10 mm and its side: a circle of 10.
    TriangleMesh boss;
    add_round(boss, 5, 5, 8, 64, false);
    const MeasureFeature c = surface_feature(boss, 10);
    REQUIRE(c.kind == MeasureFeature::Kind::Circle);
    CHECK(std::abs(c.diameter - 10.0) < 1e-3);
    CHECK_FALSE(c.hole);
    CHECK(std::abs(std::abs(c.normal[2]) - 1.0) < 1e-6);
    CHECK(std::abs(c.point[2] - 4.0) < 1e-4);
    // The same wall facing in: a hole of 10.
    TriangleMesh hole;
    add_round(hole, 5, 5, 8, 64, true);
    const MeasureFeature h = surface_feature(hole, 3);
    REQUIRE(h.kind == MeasureFeature::Kind::Circle);
    CHECK(h.hole);
    CHECK(std::abs(h.diameter - 10.0) < 1e-3);
    // A cone from 20 to 10 mm over 5 mm: 90° in all, its two diameters.
    TriangleMesh cone;
    add_round(cone, 10, 5, 5, 96, false);
    const MeasureFeature k = surface_feature(cone, 7);
    REQUIRE(k.kind == MeasureFeature::Kind::Cone);
    CHECK(std::abs(k.angle - 90.0) < 0.05);
    CHECK(std::abs(std::min(k.diameter, k.diameter_end) - 10.0) < 1e-2);
    CHECK(std::abs(std::max(k.diameter, k.diameter_end) - 20.0) < 1e-2);
    // A box's face is still its plane; between two circles, their centres.
    TriangleMesh two;
    add_round(two, 5, 5, 8, 64, false);
    CHECK(surface_feature(two, 0).kind == MeasureFeature::Kind::Circle);
    MeasureFeature other = c;
    other.point[0] += 30.0;
    CHECK(std::abs(*measure(c, other).distance - 30.0) < 1e-9);
}

TEST_CASE("plate: an object's own settings, added, edited and taken away (settings_levels.md, rules 10 and 11)", "[ui][plate]") {
    Settings profile;
    profile.infill_density = 15.0;
    std::string own;
    CHECK(own_keys(own).empty());
    // Added with the profile's value (rule 11).
    own = with_own_key(own, profile, "infill.density");
    own = with_own_key(own, profile, "walls.count");
    CHECK(own_keys(own) == std::vector<std::string>{"infill.density", "walls.count"});
    CHECK(own_settings(profile, own, "box").infill_density == 15.0);
    // Edited in the panel: only its keys are taken.
    Settings edited = own_settings(profile, own, "box");
    edited.infill_density = 40.0;
    edited.top_layers = 9; // not one of its keys
    own = own_values(own, edited);
    const Settings merged = own_settings(profile, own, "box");
    CHECK(merged.infill_density == 40.0);
    CHECK(merged.top_layers == profile.top_layers);
    // Taken away: back to the profile's; none left, no settings.
    own = without_own_key(own, "infill.density");
    CHECK(own_keys(own) == std::vector<std::string>{"walls.count"});
    own = without_own_key(own, "walls.count");
    CHECK(own.empty());
    // A key the engine does not have is not added.
    CHECK(with_own_key("", profile, "infill.no_such_key").empty());
    // The keys an object may set: walls, infill and surfaces only.
    const std::vector<std::string> keys = own_setting_keys(profile);
    CHECK(std::find(keys.begin(), keys.end(), "infill.pattern") != keys.end());
    CHECK(std::all_of(keys.begin(), keys.end(), [](const std::string& k) {
        return k.starts_with("walls.") || k.starts_with("infill.") || k.starts_with("surfaces.");
    }));
    for (const std::string& k : keys) {
        INFO(k);
        CHECK_NOTHROW(own_settings(profile, with_own_key("", profile, k), "box"));
    }
}

TEST_CASE("plate: modifiers and negative volumes follow their model's turn, scale and mirror", "[plate]") {
    // A model 20 x 10 x 10 mm and a 4 mm box 5 mm toward +X from its centre.
    PlateObject o;
    o.name = "block";
    o.mesh = box(0, 0, 0, 20, 10, 10);
    o.place.x = 100, o.place.y = 100;
    const PlateVolume v{PlateVolume::Kind::Modifier, PlateVolume::Shape::Box, {5.0, 0.0, 3.0}, {4.0, 4.0, 4.0}, {}};
    const auto centre = [](const TriangleMesh& m) {
        const BoundingBox b = m.bounding_box();
        return std::array<double, 3>{0.5 * (double(b.min.x) + b.max.x), 0.5 * (double(b.min.y) + b.max.y),
                                     0.5 * (double(b.min.z) + b.max.z)};
    };
    const auto near = [](const std::array<double, 3>& a, const std::array<double, 3>& b) {
        return std::abs(a[0] - b[0]) < 1e-3 && std::abs(a[1] - b[1]) < 1e-3 && std::abs(a[2] - b[2]) < 1e-3;
    };
    // As it stands: X 105, Z 3 to 7.
    CHECK(near(centre(volume_mesh(v, o, placed_mesh(o))), {105.0, 100.0, 5.0}));
    // Turned 90° about Z, the box goes to +Y; scaled twice, twice as far and as big.
    o.place.rotation = rotated(kNoRotation, 2, 90.0);
    CHECK(near(centre(volume_mesh(v, o, placed_mesh(o))), {100.0, 105.0, 5.0}));
    o.place.scale = {2.0, 2.0, 2.0};
    const TriangleMesh big = volume_mesh(v, o, placed_mesh(o));
    CHECK(near(centre(big), {100.0, 110.0, 10.0}));
    CHECK_THAT(double(big.bounding_box().max.x) - big.bounding_box().min.x, WithinAbs(8.0, 1e-3));
    // Saved as placed (the model's mesh as it stands), the volume stays put.
    PlateObject saved = o;
    saved.mesh = placed_mesh(o);
    saved.place.rotation = kNoRotation, saved.place.scale = {1.0, 1.0, 1.0};
    const PlateVolume carried = carried_volume(v, o.mesh.bounding_box(), placing(o, saved.mesh), saved.mesh.bounding_box());
    CHECK(near(centre(volume_mesh(carried, saved, placed_mesh(saved))), {100.0, 110.0, 10.0}));
    CHECK_THAT(carried.size[0], WithinAbs(8.0, 1e-9));
    // Mirrored in X (unturned): the box goes to the other side, upright.
    PlateObject flat;
    flat.mesh = box(0, 0, 0, 20, 10, 10);
    flat.place.x = 100, flat.place.y = 100;
    Affine mirror;
    mirror.a[0] = -1.0, mirror.b[0] = 20.0; // about X 10, the box's centre
    const BoundingBox box = flat.mesh.bounding_box();
    const PlateVolume mirrored = carried_volume(v, box, mirror, box);
    CHECK(near({mirrored.offset[0], mirrored.offset[1], mirrored.offset[2]}, {-5.0, 0.0, 3.0}));
    CHECK(mirrored.turn == kNoRotation);
}

TEST_CASE("plate: the rows under an object in the list (settings_levels.md, rules 11 and 13)", "[plate]") {
    PlateObject o;
    o.mesh = box(0, 0, 0, 10, 10, 10);
    CHECK(object_rows(o).empty());
    o.settings = R"({"infill": {"density": 15, "pattern": "grid"}, "walls": {"count": 3}})";
    o.volumes.push_back({});
    o.volumes.push_back({PlateVolume::Kind::Negative});
    o.ranges.push_back({});
    const std::vector<ObjectRow> rows = object_rows(o);
    REQUIRE(rows.size() == 5);
    CHECK(rows[0] == ObjectRow{ObjectRow::Kind::Settings, "walls", 0});
    CHECK(rows[1] == ObjectRow{ObjectRow::Kind::Settings, "infill", 0});
    CHECK(rows[2] == ObjectRow{ObjectRow::Kind::Volume, {}, 0});
    CHECK(rows[3] == ObjectRow{ObjectRow::Kind::Volume, {}, 1});
    CHECK(rows[4] == ObjectRow{ObjectRow::Kind::Range, {}, 0});
}
