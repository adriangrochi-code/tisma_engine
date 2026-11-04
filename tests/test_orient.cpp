// Automatic orientation, least support (orientation.md, rules 1 to 6; OR1, OR2).

#include "mesh/orient.hpp"
#include "mesh/shapes.hpp"
#include "config/settings.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>

using namespace tisma;
using Catch::Matchers::WithinAbs;

namespace {

// Where the part's own Z points after the rotation: its third column.
std::array<double, 3> part_z(const OrientResult& r) { return {r.rotation[2], r.rotation[5], r.rotation[8]}; }

void append(TriangleMesh& to, const TriangleMesh& from) {
    for (std::size_t t = 0; t < from.triangle_count(); ++t)
        to.add_triangle(from.corner(t, 0), from.corner(t, 1), from.corner(t, 2));
}

} // namespace

TEST_CASE("orient: OR1, a mushroom ends upside down with its cap on the bed", "[orient]") {
    TriangleMesh mushroom = make_box(8, 8, 20);
    TriangleMesh cap = make_box(30, 30, 3);
    cap.translate(0, 0, 20);
    append(mushroom, cap);
    const OrientResult r = orient_least_support(mushroom);
    INFO("cost " << r.cost << " before " << r.cost_before << ", " << r.candidates << " candidates");
    CHECK(part_z(r)[2] < -0.99);
    CHECK(r.cost < r.cost_before);
    // As it stood: the cap's underside (900 - 64 mm²) 20 mm over the bed,
    // minus the stem's 64 mm² on it.
    // (sampled in pieces: within 2 %).
    CHECK_THAT(r.cost_before, WithinAbs(836.0 * 20.0 - 64.0, 0.02 * 836.0 * 20.0));
    CHECK_THAT(r.bed_area_mm2, WithinAbs(900.0, 0.5));
}

TEST_CASE("orient: OR2, a standing bar lies down", "[orient]") {
    const OrientResult r = orient_least_support(make_box(5, 5, 60));
    CHECK(std::abs(part_z(r)[2]) < 0.01);
    CHECK_THAT(r.bed_area_mm2, WithinAbs(300.0, 0.5));
    // Rule 2: lying landscape, its 60 mm along X.
    CHECK(std::abs(part_z(r)[0]) > 0.99);
}

TEST_CASE("orient: never worse than it was, and the rotation is a rotation", "[orient]") {
    // A cube on the bed: nothing to gain; it stays as low as it is.
    const OrientResult cube = orient_least_support(make_box(10, 10, 10));
    CHECK(cube.cost <= cube.cost_before);
    const auto& m = cube.rotation;
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j) {
            double s = 0;
            for (int k = 0; k < 3; ++k)
                s += m[std::size_t(i * 3 + k)] * m[std::size_t(j * 3 + k)];
            CHECK_THAT(s, WithinAbs(i == j ? 1.0 : 0.0, 1e-9));
        }
    // Empty: the identity.
    const OrientResult none = orient_least_support(TriangleMesh{});
    CHECK(none.rotation[0] == 1.0);
    CHECK(none.candidates == 0);
    // A slab floating 10 mm over the bed: its whole underside needs support
    // down to the bed (the ray finds nothing below it).
    TriangleMesh slab = make_box(30, 10, 2);
    slab.translate(0, 0, 10);
    TriangleMesh post = make_box(2, 2, 1);
    post.translate(40, 0, 0); // something on the bed, away from it
    append(slab, post);
    const OrientCost c = orientation_cost(slab, {0, 0, 1});
    CHECK_THAT(c.support_mm3, WithinAbs(300.0 * 10.0, 1.0));
    CHECK_THAT(c.bed_area_mm2, WithinAbs(4.0, 1e-6));
}

namespace {

// OR3 and OR4's cup: 20 by 20 by 20 mm, walls and floor 2 mm, open on top.
TriangleMesh cup() {
    TriangleMesh m = make_box(20, 20, 2);
    const struct {
        double sx, sy, x, y;
    } walls[] = {{2, 20, -9, 0}, {2, 20, 9, 0}, {16, 2, 0, -9}, {16, 2, 0, 9}};
    for (const auto& w : walls) {
        TriangleMesh b = make_box(w.sx, w.sy, 20);
        b.translate(float(w.x), float(w.y), 0);
        append(m, b);
    }
    return m;
}

Settings resin(bool raised) {
    Settings s;
    s.machine_technology = MachineTechnology::Msla;
    s.resin_layer_height = 0.05;
    s.resin_supports = raised;
    s.resin_support_elevation = raised ? 5.0 : 0.0;
    s.resin_pad_around_object = false;
    return s;
}

} // namespace

TEST_CASE("orient: OR3, a cup on supports does not open towards the film", "[orient][resin]") {
    const PeelResult r = orient_least_peel(cup(), resin(true));
    INFO("cups " << r.cups << " (before " << r.cups_before << "), largest " << r.largest_layer_mm2 << ", " << r.candidates
                 << " candidates");
    CHECK(r.cups_before >= 1); // as modelled, open upwards: a cup
    CHECK(r.cups == 0);
    CHECK(part_z(r)[2] < 0.5);
    CHECK(r.candidates == 13); // max(4, 16 x 0.75) and the current one
}

TEST_CASE("orient: OR4, a cup on the platform lies on its side", "[orient][resin]") {
    const PeelResult r = orient_least_peel(cup(), resin(false));
    INFO("cups " << r.cups << " (before " << r.cups_before << "), largest " << r.largest_layer_mm2);
    CHECK(r.cups == 0);
    CHECK(std::abs(part_z(r)[2]) < 0.01);
}

