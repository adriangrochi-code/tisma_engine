// The aerodynamic analysis (aerodynamics.md; AE1 to AE6). The cases that
// run the lattice at a useful size are tagged [.aero-full]: they run as their
// own CTest test in optimized builds only (tests/CMakeLists.txt).

#include "aero/aero.hpp"
#include "aero/lbm.hpp"
#include "mesh/shapes.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <nlohmann/json.hpp>

#include <cmath>
#include <cstdio>
#include <numbers>

using namespace tisma;
using namespace tisma::aero;
using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

namespace {

void add_quad(TriangleMesh& m, Vec3f a, Vec3f b, Vec3f c, Vec3f d) {
    m.add_triangle(a, b, c);
    m.add_triangle(a, c, d);
}

// A prism along +x from x0 to x1, its cross-section h by h centred on the
// axis; with a nose, a pyramid from x0 - nose to the front face.
TriangleMesh prism(float x0, float x1, float h, float nose = 0.0f) {
    TriangleMesh m;
    const float s = h / 2;
    const Vec3f a{x0, -s, -s}, b{x0, s, -s}, c{x0, s, s}, d{x0, -s, s};
    const Vec3f e{x1, -s, -s}, f{x1, s, -s}, g{x1, s, s}, k{x1, -s, s};
    add_quad(m, e, f, g, k);       // back, +x
    add_quad(m, a, e, k, d);       // -y
    add_quad(m, b, c, g, f);       // +y
    add_quad(m, a, b, f, e);       // -z
    add_quad(m, d, k, g, c);       // +z
    if (nose <= 0.0f) {
        add_quad(m, a, d, c, b);   // front, -x
    } else {
        const Vec3f apex{x0 - nose, 0, 0};
        m.add_triangle(apex, b, a);
        m.add_triangle(apex, c, b);
        m.add_triangle(apex, d, c);
        m.add_triangle(apex, a, d);
    }
    return m;
}

AeroResult run(TriangleMesh mesh, std::array<double, 3> direction, int cells, double flow_throughs = 3.0) {
    AeroInput in;
    in.mesh = std::move(mesh);
    in.direction = direction;
    in.speed = 10.0;
    in.cells = cells;
    in.flow_throughs = flow_throughs;
    AeroResult r = analyze(in);
    std::printf("cd %.3f (pressure %.3f, coarse %.3f at %d, faces %.3f, simulated total %.3f, blockage %.3f, friction %.4f, "
                "range %.3f to %.3f), grid %dx%dx%d, %ld steps%s, %.1f s\n",
                r.cd, r.cd_pressure, r.cd_pressure_coarse, r.cells_coarse, r.cd_pressure_surface, r.cd_total_simulated, r.blockage,
                r.cd_friction, r.cd_low, r.cd_high, r.cells_x, r.cells_y, r.cells_z, r.steps, r.stopped_early ? " (early)" : "", r.seconds);
    return r;
}

} // namespace

TEST_CASE("aero: the roughness of printed faces (AE6's numbers)", "[aero]") {
    // Walls and flat faces: the bead's rounding, 0.05 h.
    CHECK_THAT(face_roughness(0.0, 0.2), WithinAbs(0.01, 1e-12));
    CHECK_THAT(face_roughness(1.0, 0.2), WithinAbs(0.01, 1e-12));
    CHECK_THAT(face_roughness(-1.0, 0.2), WithinAbs(0.01, 1e-12));
    // At 45°: the staircase, h |n_z| / 4 = 35 µm.
    CHECK_THAT(face_roughness(std::sqrt(0.5), 0.2), WithinAbs(0.2 * std::sqrt(0.5) / 4.0, 1e-12));
    // Nearly a wall: never under the rounding.
    CHECK_THAT(face_roughness(0.05, 0.2), WithinAbs(0.01, 1e-12));
}

TEST_CASE("aero: AE5, laminar friction of a 6:1 prism within 5 % of Blasius", "[aero]") {
    // 60 mm long, 10 by 10 across, the air along it at 10 m/s: Re_L about
    // 40 000, laminar; the layers' roughness is far too small to trip it.
    AeroInput in;
    in.mesh = prism(0, 60, 10);
    in.quality = AeroQuality::Friction;
    in.speed = 10.0;
    const AeroResult r = analyze(in);
    REQUIRE(r.ok);
    CHECK_FALSE(r.simulated);
    CHECK(r.cd_pressure == 0.0);
    CHECK_THAT(r.frontal_area_mm2, WithinRel(100.0, 0.01));
    const double re_l = 10.0 * 0.060 / in.viscosity;
    const double blasius = 1.328 / std::sqrt(re_l) * 4.0 * 60.0 * 10.0 / 100.0; // the four sides, over the frontal area
    INFO(r.cd_friction << " vs " << blasius);
    CHECK_THAT(r.cd_friction, WithinRel(blasius, 0.05));
    CHECK(r.roughness_share < 0.01);
    // Faster and longer: turbulent past Re_x = 5e5, more friction than the
    // laminar law would give there.
    CHECK(friction_coefficient(900, 1000, 10.0, 0.0, 1.516e-5) > 0.664 / std::sqrt(10.0 * 0.95 / 1.516e-5));
    // Rough enough (U ks / nu > 300): fully rough from the start.
    CHECK(friction_coefficient(10, 20, 30.0, 0.2, 1.516e-5) > friction_coefficient(10, 20, 30.0, 0.0, 1.516e-5));
}

TEST_CASE("aero: AE6, a prism printed at 45° is rougher and rubs more", "[aero]") {
    // The same prism turned 45° about y, the air still along x... in print
    // coordinates the air goes along the prism's axis.
    TriangleMesh upright = prism(0, 60, 10), tilted;
    const float c = float(std::sqrt(0.5));
    for (std::size_t t = 0; t < upright.triangle_count(); ++t) {
        Vec3f p[3];
        for (int k = 0; k < 3; ++k) {
            const Vec3f v = upright.corner(t, k);
            p[k] = {c * v.x + c * v.z, v.y, -c * v.x + c * v.z};
        }
        tilted.add_triangle(p[0], p[1], p[2]);
    }
    AeroInput in;
    in.quality = AeroQuality::Friction;
    in.speed = 40.0; // U ks / nu = 370 on the stepped faces: fully rough
    in.mesh = upright;
    in.direction = {1, 0, 0};
    const AeroResult flat = analyze(in);
    in.mesh = tilted;
    in.direction = {c, 0, -c};
    const AeroResult steep = analyze(in);
    REQUIRE(flat.ok);
    REQUIRE(steep.ok);
    CHECK_THAT(flat.ra_mean_um, WithinAbs(10.0, 0.01));
    INFO(steep.ra_mean_um);
    CHECK(steep.ra_mean_um > 15.0);
    CHECK(steep.ra_mean_um < 36.0);
    INFO(flat.cd_friction << " " << steep.cd_friction);
    CHECK(steep.cd_friction > 1.5 * flat.cd_friction);
    CHECK(steep.roughness_share > 0.3);
    // The same shadow either way.
    CHECK_THAT(steep.frontal_area_mm2, WithinRel(flat.frontal_area_mm2, 0.02));
}

TEST_CASE("aero: inputs refused, and the result document", "[aero]") {
    AeroInput in;
    in.mesh = make_box(10, 10, 10);
    in.quality = AeroQuality::Friction;
    in.speed = 0.0;
    CHECK_FALSE(analyze(in).ok);
    in.speed = 150.0;
    CHECK_FALSE(analyze(in).ok);
    in.speed = 10.0;
    in.direction = {0, 0, 0};
    CHECK_FALSE(analyze(in).ok);
    in.direction = {0, 0, -1};
    const AeroResult r = analyze(in);
    REQUIRE(r.ok);
    CHECK(r.cp.size() == in.mesh.triangle_count());
    CHECK(r.ra_um.size() == in.mesh.triangle_count());
    const nlohmann::json j = nlohmann::json::parse(result_json(r, in));
    CHECK(j["format"] == "tisma-aero-result");
    CHECK(j["inputs"]["quality"] == "friction");
    CHECK(j["simulated"] == false);
    CHECK(j["cp"].size() == in.mesh.triangle_count());
    AeroQuality q;
    CHECK(quality_named("high", q));
    CHECK(q == AeroQuality::High);
    CHECK(quality_cells(q) == 32);
    CHECK_FALSE(quality_named("ultra", q));
}

TEST_CASE("aero: the lattice runs, stays stable and pushes on the face the air meets", "[aero]") {
    // Tiny: so it runs in the debug build with the sanitizers too.
    const AeroResult r = run(make_box(20, 20, 20), {1, 0, 0}, 6, 0.6);
    REQUIRE(r.ok);
    CHECK(r.simulated);
    CHECK(r.stable);
    CHECK(r.cd_pressure > 0.2);
    CHECK(r.cd_pressure < 3.0);
    // Stagnation on the front face (the four triangles facing -x), suction
    // or less on the back.
    TriangleMesh box = make_box(20, 20, 20);
    double front = 0, back = 0;
    int nf = 0, nb = 0;
    for (std::size_t t = 0; t < box.triangle_count(); ++t) {
        const Vec3f a = box.corner(t, 0), b = box.corner(t, 1), c = box.corner(t, 2);
        const float nx = (b.y - a.y) * (c.z - a.z) - (b.z - a.z) * (c.y - a.y);
        if (nx < -1e-3f)
            front += r.cp[t], ++nf;
        if (nx > 1e-3f)
            back += r.cp[t], ++nb;
    }
    REQUIRE(nf > 0);
    REQUIRE(nb > 0);
    CHECK(front / nf > back / nb);
    // Streamlines: from in front of the part, downstream, finite, with a
    // pressure at each point; those through the middle go round the cube.
    REQUIRE_FALSE(r.streamlines.empty());
    float lowest_x = 1e9f;
    for (const AeroResult::Streamline& s : r.streamlines) {
        REQUIRE(s.points.size() == s.cp.size());
        REQUIRE(s.points.size() >= 2);
        CHECK(s.points.back()[0] > s.points.front()[0]);
        for (const auto& p : s.points)
            CHECK((std::isfinite(p[0]) && std::isfinite(p[1]) && std::isfinite(p[2])));
        lowest_x = std::min(lowest_x, s.points.front()[0]);
    }
    CHECK(lowest_x < box.bounding_box().min.x);
    // Stopped: no result.
    AeroInput in;
    in.mesh = make_box(20, 20, 20);
    in.cells = 6;
    in.flow_throughs = 0.6;
    const AeroResult stopped = analyze(in, [](double done) { return done < 0.2; });
    CHECK_FALSE(stopped.ok);
    CHECK_FALSE(stopped.error.empty());
}

TEST_CASE("aero: AE1 to AE4, drag coefficients against Hoerner", "[.aero-full]") {
    const int cells = 12;
    SECTION("AE1 and AE2: a square plate and a cube face on") {
        const AeroResult plate = run(prism(0, 2.5f, 20), {1, 0, 0}, cells);
        const AeroResult cube = run(prism(0, 20, 20), {1, 0, 0}, cells);
        REQUIRE(plate.ok);
        REQUIRE(cube.ok);
        CHECK(plate.cd > 0.9); // Hoerner 1.17 to 1.2; 1.09 at 12 cells (2026-11-03): a plate of one or two cells
        CHECK(plate.cd < 1.4);
        CHECK(cube.cd > 0.95); // Hoerner 1.05; the domain's blockage adds 5 to 10 %
        CHECK(cube.cd < 1.3);
        // The likely range holds Hoerner's values, and the resolution check ran.
        CHECK(cube.resolution_checked);
        CHECK(cube.cd_low < 1.05);
        CHECK(cube.cd_high > 1.05);
        CHECK(plate.cd_high > 1.17);
        // AE3: the cube with the air along another axis.
        const AeroResult side = run(prism(0, 20, 20), {0, -1, 0}, cells);
        REQUIRE(side.ok);
        CHECK_THAT(side.cd, WithinRel(cube.cd, 0.04));
    }
    SECTION("AE4: a 3:1 prism, blunt and with a pyramid nose") {
        const AeroResult blunt = run(prism(0, 60, 20), {1, 0, 0}, cells);
        const AeroResult nosed = run(prism(20, 60, 20, 20), {1, 0, 0}, cells);
        REQUIRE(blunt.ok);
        REQUIRE(nosed.ok);
        CHECK(blunt.cd > 0.8); // Tisma 1.06
        CHECK(blunt.cd < 1.3);
        CHECK(nosed.cd < 0.7 * blunt.cd); // Tisma 0.34: the nose lowers the drag
    }
}

TEST_CASE("aero: EP9, a 20 mm cube in air along +X at 10 m/s, fast quality", "[.aero-full]") {
    AeroInput in;
    in.mesh = make_box(20, 20, 20);
    in.direction = {1, 0, 0};
    in.speed = 10.0;
    in.quality = AeroQuality::Fast;
    const AeroResult r = analyze(in);
    std::printf("EP9: cd %.3f (pressure %.3f, coarse %.3f), range %.3f to %.3f, %ld steps%s, %.1f s\n", r.cd, r.cd_pressure,
                r.cd_pressure_coarse, r.cd_low, r.cd_high, r.steps, r.stopped_early ? " (early)" : "", r.seconds);
    REQUIRE(r.ok);
    CHECK(r.cd > 0.8);
    CHECK(r.cd < 1.3);
    // The face the air meets in the map's top band (red: Cp from 0.66).
    double front = 0;
    int n = 0;
    for (std::size_t t = 0; t < in.mesh.triangle_count(); ++t) {
        const Vec3f a = in.mesh.corner(t, 0), b = in.mesh.corner(t, 1), c = in.mesh.corner(t, 2);
        const float nx = (b.y - a.y) * (c.z - a.z) - (b.z - a.z) * (c.y - a.y);
        if (nx < -1e-3f)
            front += r.cp[t], ++n;
    }
    REQUIRE(n > 0);
    INFO(front / n);
    CHECK(front / n > 0.66);
}
