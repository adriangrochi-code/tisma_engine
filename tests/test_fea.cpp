#include "fea/as_printed.hpp"
#include "fea/elasticity.hpp"
#include "fea/hex8.hpp"
#include "fea/material.hpp"
#include "fea/materials.hpp"
#include "fea/multigrid.hpp"
#include "fea/operator.hpp"
#include "fea/voxels.hpp"
#include "io/stl.hpp"
#include "slice/pipeline.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <numbers>

using namespace tisma;
using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

namespace {

VoxelGrid block(int nx, int ny, int nz, double h) {
    VoxelGrid g;
    g.hx = g.hy = h, g.nx = nx, g.ny = ny, g.nz = nz;
    for (int k = 0; k <= nz; ++k)
        g.z.push_back(k * h);
    g.solid.assign(std::size_t(nx) * std::size_t(ny) * std::size_t(nz), 1);
    return g;
}

Box face(int axis, double at) {
    Box b;
    b.min[axis] = at - 1e-9;
    b.max[axis] = at + 1e-9;
    return b;
}

// An orthotropic material with every constant different, so that a mix-up
// of axes shows.
OrthotropicMaterial printed() {
    OrthotropicMaterial m;
    m.e1 = 3500.0, m.e2 = 3000.0, m.e3 = 2500.0;
    m.g12 = 1200.0, m.g13 = 1000.0, m.g23 = 900.0;
    m.nu12 = 0.35, m.nu13 = 0.30, m.nu23 = 0.32;
    m.xt = 50.0, m.xc = 60.0, m.yt = 30.0, m.yc = 55.0, m.zt = 20.0, m.zc = 50.0;
    m.s12 = 28.0, m.s13 = 18.0, m.s23 = 16.0;
    return m;
}

void check_same(const Stiffness& a, const Stiffness& b, double scale) {
    for (std::size_t i = 0; i < 36; ++i)
        CHECK_THAT(a[i], WithinAbs(b[i], 1e-12 * scale));
}

} // namespace

TEST_CASE("hexahedron: symmetric, and rigid motions cost no energy", "[fea]") {
    const Hex8 e = hex8(0.3);
    for (int r = 0; r < 24; ++r) {
        CHECK(e.stiffness[std::size_t(r) * 24 + std::size_t(r)] > 0.0);
        for (int c = 0; c < 24; ++c)
            CHECK_THAT(e.stiffness[std::size_t(r) * 24 + std::size_t(c)], WithinAbs(e.stiffness[std::size_t(c) * 24 + std::size_t(r)], 1e-12));
    }
    // Three translations and three small rotations about the centre.
    for (int mode = 0; mode < 6; ++mode) {
        double u[24];
        for (int a = 0; a < 8; ++a) {
            const double x = kHexCorner[a][0] - 0.5, y = kHexCorner[a][1] - 0.5, z = kHexCorner[a][2] - 0.5;
            const double w[3] = {mode == 3 ? 1.0 : 0.0, mode == 4 ? 1.0 : 0.0, mode == 5 ? 1.0 : 0.0};
            u[3 * a] = (mode == 0) + w[1] * z - w[2] * y;
            u[3 * a + 1] = (mode == 1) + w[2] * x - w[0] * z;
            u[3 * a + 2] = (mode == 2) + w[0] * y - w[1] * x;
        }
        for (int r = 0; r < 24; ++r) {
            double s = 0.0;
            for (int c = 0; c < 24; ++c)
                s += e.stiffness[std::size_t(r) * 24 + std::size_t(c)] * u[c];
            CHECK_THAT(s, WithinAbs(0.0, 1e-12));
        }
    }
}

TEST_CASE("voxels of the 10 mm cube", "[fea]") {
    const IndexedMesh cube = weld_exact(read_stl(std::filesystem::path(TISMA_TEST_DATA_DIR) / "cube_ascii.stl").mesh);
    CHECK(voxelize(cube, 1.0).count() == 1000);
    const VoxelGrid fine = voxelize(cube, 0.5);
    CHECK(fine.nx == 20);
    CHECK(fine.count() == 8000);
}

TEST_CASE("a bar in tension: the exact stretch, contraction and stress", "[fea]") {
    // 20 x 4 x 4 mm on rollers (each of the three planes through the origin
    // holds one direction), pulled with 100 N on its far end: a uniform stress
    // that trilinear elements represent exactly.
    const VoxelGrid g = block(20, 4, 4, 1.0);
    const Material m{3500.0, 0.35};
    const Fixture fixtures[] = {{face(0, 0.0), true, false, false}, {face(1, 0.0), false, true, false},
                                {face(2, 0.0), false, false, true}};
    SurfaceLoad load;
    load.box = face(0, 20.0);
    load.force[0] = 100.0;
    const ElasticityResult r = solve_elasticity(g, m, fixtures, std::span(&load, 1), {.tolerance = 1e-12});
    REQUIRE(r.converged);
    const double stress = 100.0 / 16.0;
    auto u = [&](int i, int j, int k, int axis) { return r.displacement[3 * ((std::size_t(k) * 5 + std::size_t(j)) * 21 + std::size_t(i)) + std::size_t(axis)]; };
    CHECK_THAT(u(20, 2, 2, 0), WithinRel(stress / m.young * 20.0, 1e-8));
    CHECK_THAT(u(20, 4, 4, 1), WithinRel(-m.poisson * stress / m.young * 4.0, 1e-8));
    CHECK_THAT(u(10, 1, 4, 2), WithinRel(-m.poisson * stress / m.young * 4.0, 1e-8));
    for (double vm : r.von_mises)
        CHECK_THAT(vm, WithinRel(stress, 1e-8));
}

TEST_CASE("a cantilever beam bends as beam theory says", "[fea]") {
    // 40 x 4 x 4 mm clamped at x = 0, 10 N down on its free end. Euler-Bernoulli
    // with Timoshenko's shear term: F L^3 / (3 E I) + F L / (k G A).
    const double h = 0.5, length = 40.0, side = 4.0, force = 10.0;
    const VoxelGrid g = block(80, 8, 8, h);
    const Material m{3500.0, 0.35};
    const Fixture clamp{face(0, 0.0)};
    SurfaceLoad load;
    load.box = face(0, length);
    load.force[2] = -force;
    const ElasticityResult r = solve_elasticity(g, m, std::span(&clamp, 1), std::span(&load, 1), {.tolerance = 1e-10});
    REQUIRE(r.converged);
    const double inertia = std::pow(side, 4) / 12.0, area = side * side, shear = m.young / (2.0 * (1.0 + m.poisson));
    const double expected = force * std::pow(length, 3) / (3.0 * m.young * inertia) + force * length / (5.0 / 6.0 * shear * area);
    // The tip's middle node.
    const std::size_t tip = (std::size_t(4) * 9 + 4) * 81 + 80;
    const double deflection = -r.displacement[3 * tip + 2];
    INFO("deflection " << deflection << " mm, beam theory " << expected << " mm, " << r.iterations << " iterations");
    CHECK_THAT(deflection, WithinRel(expected, 0.05));
    // The largest bending stress, at the clamp's top and bottom: M c / I.
    CHECK(r.max_von_mises > 0.8 * force * length * (side / 2.0) / inertia);
}

TEST_CASE("box voxels that follow the layers: still exact in tension", "[fea]") {
    // 4 voxels per 0.4 mm line across, 2 per 0.2 mm layer up, a first layer of
    // 0.3 mm: a 2 x 0.4 x 0.7 mm bar on rollers, pulled along x.
    const std::vector<Layer> layers{{0.0, 0.3}, {0.3, 0.5}, {0.5, 0.7}};
    VoxelGrid g;
    g.hx = g.hy = 0.1;
    g.z = layer_planes(layers, 2);
    REQUIRE(g.z.size() == 7);
    CHECK_THAT(g.z[1], WithinAbs(0.15, 1e-12));
    CHECK_THAT(g.z[3], WithinAbs(0.4, 1e-12));
    g.nx = 20, g.ny = 4, g.nz = 6;
    g.solid.assign(std::size_t(20 * 4 * 6), 1);
    const Material m{3500.0, 0.35};
    const Fixture fixtures[] = {{face(0, 0.0), true, false, false}, {face(1, 0.0), false, true, false},
                                {face(2, 0.0), false, false, true}};
    SurfaceLoad load;
    load.box = face(0, 2.0);
    load.force[0] = 2.8;
    const ElasticityResult r = solve_elasticity(g, m, fixtures, std::span(&load, 1), {.tolerance = 1e-12});
    REQUIRE(r.converged);
    const double stress = 2.8 / (0.4 * 0.7);
    auto u = [&](int i, int j, int k, int axis) { return r.displacement[3 * ((std::size_t(k) * 5 + std::size_t(j)) * 21 + std::size_t(i)) + std::size_t(axis)]; };
    CHECK_THAT(u(20, 2, 3, 0), WithinRel(stress / m.young * 2.0, 1e-8));
    CHECK_THAT(u(20, 2, 6, 2), WithinRel(-m.poisson * stress / m.young * 0.7, 1e-8));
    CHECK_THAT(u(20, 4, 6, 1), WithinRel(-m.poisson * stress / m.young * 0.4, 1e-8));
    for (double vm : r.von_mises)
        CHECK_THAT(vm, WithinRel(stress, 1e-8));
}

TEST_CASE("orthotropic material: the isotropic case, rotations and the layer average", "[fea]") {
    OrthotropicMaterial iso;
    iso.e1 = iso.e2 = iso.e3 = 3500.0;
    iso.g12 = iso.g13 = iso.g23 = 3500.0 / (2.0 * 1.35);
    iso.nu12 = iso.nu13 = iso.nu23 = 0.35;
    check_same(local_stiffness(iso), isotropic_stiffness(3500.0, 0.35), 1e4);
    check_same(global_stiffness(iso, 0.7), isotropic_stiffness(3500.0, 0.35), 1e4);

    const OrthotropicMaterial m = printed();
    const Stiffness d = local_stiffness(m);
    // The compliance's normal block times the stiffness's is the identity.
    const double s[3][3] = {{1.0 / m.e1, -m.nu12 / m.e1, -m.nu13 / m.e1},
                            {-m.nu12 / m.e1, 1.0 / m.e2, -m.nu23 / m.e2},
                            {-m.nu13 / m.e1, -m.nu23 / m.e2, 1.0 / m.e3}};
    for (int r = 0; r < 3; ++r)
        for (int c = 0; c < 3; ++c) {
            double v = 0.0;
            for (int k = 0; k < 3; ++k)
                v += s[r][k] * d[std::size_t(k * 6 + c)];
            CHECK_THAT(v, WithinAbs(r == c ? 1.0 : 0.0, 1e-12));
        }
    check_same(global_stiffness(m, 0.0), d, 1e4);
    // A quarter turn: the line along y, axis 2 along x.
    const Stiffness q = global_stiffness(m, std::numbers::pi / 2.0);
    CHECK_THAT(q[0], WithinRel(d[7], 1e-12));
    CHECK_THAT(q[7], WithinRel(d[0], 1e-12));
    CHECK_THAT(q[2], WithinRel(d[8], 1e-12));
    CHECK_THAT(q[3 * 6 + 3], WithinRel(m.g12, 1e-12));
    CHECK_THAT(q[4 * 6 + 4], WithinRel(m.g13, 1e-12));
    CHECK_THAT(q[5 * 6 + 5], WithinRel(m.g23, 1e-12));
    // Stresses in the line's axes two ways: rotate the global stresses, or
    // apply the local stiffness to the rotated strains.
    const double theta = 0.6;
    const double strain[6] = {1e-3, -4e-4, 2e-4, 7e-4, -3e-4, 5e-4};
    const Stiffness dg = global_stiffness(m, theta), ts = stress_rotation(theta), te = strain_rotation(theta);
    for (int r = 0; r < 6; ++r) {
        double a = 0.0, b = 0.0;
        for (int c = 0; c < 6; ++c)
            for (int k = 0; k < 6; ++k) {
                a += ts[std::size_t(r * 6 + k)] * dg[std::size_t(k * 6 + c)] * strain[c];
                b += d[std::size_t(r * 6 + k)] * te[std::size_t(k * 6 + c)] * strain[c];
            }
        CHECK_THAT(a, WithinAbs(b, 1e-9));
    }
    // The layer average is transversely isotropic about z.
    const Stiffness avg = layer_average_stiffness(m);
    CHECK_THAT(avg[0], WithinRel(avg[7], 1e-12));
    CHECK_THAT(avg[2], WithinRel(avg[8], 1e-12));
    CHECK_THAT(avg[3 * 6 + 3], WithinRel(0.5 * (avg[0] - avg[1]), 1e-12));
    CHECK_THAT(avg[4 * 6 + 4], WithinRel(avg[5 * 6 + 5], 1e-12));
    CHECK_THAT(avg[3], WithinAbs(0.0, 1e-9));
    check_same(layer_average_stiffness(m), [&] {
        // The average does not depend on where the angles start.
        Stiffness out{};
        for (int a = 0; a < 8; ++a) {
            const Stiffness r = global_stiffness(m, 0.3 + std::numbers::pi * a / 8.0);
            for (std::size_t i = 0; i < 36; ++i)
                out[i] += r[i] / 8.0;
        }
        return out;
    }(), 1e4);
}

TEST_CASE("Tsai-Wu: safety 1 at every strength, and what gives way", "[fea]") {
    const OrthotropicMaterial m = printed();
    auto safety = [&](int axis, double value) {
        Voigt s{};
        s[std::size_t(axis)] = value;
        return tsai_wu_safety(m, s);
    };
    CHECK_THAT(safety(0, m.xt), WithinRel(1.0, 1e-12));
    CHECK_THAT(safety(0, -m.xc), WithinRel(1.0, 1e-12));
    CHECK_THAT(safety(1, m.yt), WithinRel(1.0, 1e-12));
    CHECK_THAT(safety(1, -m.yc), WithinRel(1.0, 1e-12));
    CHECK_THAT(safety(2, m.zt), WithinRel(1.0, 1e-12));
    CHECK_THAT(safety(2, -m.zc), WithinRel(1.0, 1e-12));
    CHECK_THAT(safety(3, m.s12), WithinRel(1.0, 1e-12));
    CHECK_THAT(safety(4, -m.s23), WithinRel(1.0, 1e-12));
    CHECK_THAT(safety(5, m.s13), WithinRel(1.0, 1e-12));
    CHECK_THAT(safety(0, m.xt / 2.0), WithinRel(2.0, 1e-12));
    CHECK_THAT(safety(2, -m.zc / 4.0), WithinRel(4.0, 1e-12));
    CHECK(std::isinf(tsai_wu_safety(m, Voigt{})));
    // Tension and shear together fail sooner than either alone.
    CHECK(tsai_wu_safety(m, Voigt{0.0, 0.0, 0.7 * m.zt, 0.0, 0.7 * m.s23, 0.0}) < 1.0 / 0.7);

    CHECK(failure_mode(m, Voigt{10.0, 0.0, 0.0, 0.0, 0.0, 0.0}) == FailureMode::Line);
    CHECK(failure_mode(m, Voigt{0.0, -10.0, 0.0, 0.0, 0.0, 0.0}) == FailureMode::BetweenLines);
    CHECK(failure_mode(m, Voigt{0.0, 0.0, 0.0, 10.0, 0.0, 0.0}) == FailureMode::BetweenLines);
    CHECK(failure_mode(m, Voigt{0.0, 0.0, 10.0, 0.0, 0.0, 0.0}) == FailureMode::BetweenLayers);
    CHECK(failure_mode(m, Voigt{0.0, 0.0, 0.0, 0.0, 0.0, -10.0}) == FailureMode::BetweenLayers);
    CHECK(failure_mode(m, Voigt{20.0, 0.0, 10.0, 0.0, 0.0, 0.0}) == FailureMode::BetweenLayers);
    CHECK(failure_mode(m, Voigt{}) == FailureMode::None);
}

TEST_CASE("as printed: bars along the lines, across them and across the layers", "[fea]") {
    // 20 x 4 x 4 mm on rollers, a uniform 10 MPa along its length: exact for
    // trilinear elements in every orientation without shear coupling.
    const OrthotropicMaterial m = printed();
    const double sigma = 10.0, length = 20.0, side = 4.0;
    auto bar = [&](int axis, std::span<const float> angle) {
        const VoxelGrid g = block(axis == 0 ? 20 : 4, 4, axis == 2 ? 20 : 4, 1.0);
        const Fixture fixtures[] = {{face(0, 0.0), true, false, false}, {face(1, 0.0), false, true, false},
                                    {face(2, 0.0), false, false, true}};
        SurfaceLoad load;
        load.box = face(axis, length);
        load.force[axis] = sigma * side * side;
        return solve_as_printed(g, m, angle, fixtures, std::span(&load, 1), {.tolerance = 1e-12});
    };
    auto stretch = [](const AsPrintedResult& r, int nx, int ny, int i, int j, int k, int axis) {
        return r.elasticity.displacement[3 * ((std::size_t(k) * std::size_t(ny + 1) + std::size_t(j)) * std::size_t(nx + 1) + std::size_t(i)) + std::size_t(axis)];
    };
    const std::vector<float> along(320, 0.0f), across(320, float(std::numbers::pi / 2.0)),
        turned(320, float(3.0 * std::numbers::pi / 2.0)); // the same as across, half a turn later

    SECTION("pulled along the lines") {
        const AsPrintedResult r = bar(0, along);
        REQUIRE(r.elasticity.converged);
        CHECK_THAT(stretch(r, 20, 4, 20, 2, 2, 0), WithinRel(sigma / m.e1 * length, 1e-8));
        CHECK_THAT(stretch(r, 20, 4, 20, 4, 2, 1), WithinRel(-m.nu12 * sigma / m.e1 * side, 1e-8));
        CHECK_THAT(stretch(r, 20, 4, 20, 2, 4, 2), WithinRel(-m.nu13 * sigma / m.e1 * side, 1e-8));
        CHECK_THAT(r.min_safety, WithinRel(tsai_wu_safety(m, Voigt{sigma, 0, 0, 0, 0, 0}), 1e-8));
        for (FailureMode f : r.mode)
            CHECK(f == FailureMode::Line);
    }
    SECTION("pulled across the lines") {
        for (const auto* angle : {&across, &turned}) {
            const AsPrintedResult r = bar(0, *angle);
            REQUIRE(r.elasticity.converged);
            CHECK_THAT(stretch(r, 20, 4, 20, 2, 2, 0), WithinRel(sigma / m.e2 * length, 1e-8));
            CHECK_THAT(stretch(r, 20, 4, 20, 4, 2, 1), WithinRel(-m.nu12 * sigma / m.e1 * side, 1e-8));
            CHECK_THAT(stretch(r, 20, 4, 20, 2, 4, 2), WithinRel(-m.nu23 * sigma / m.e2 * side, 1e-8));
            CHECK_THAT(r.min_safety, WithinRel(tsai_wu_safety(m, Voigt{0, sigma, 0, 0, 0, 0}), 1e-8));
            for (FailureMode f : r.mode)
                CHECK(f == FailureMode::BetweenLines);
        }
    }
    SECTION("pulled across the layers") {
        const AsPrintedResult r = bar(2, along);
        REQUIRE(r.elasticity.converged);
        CHECK_THAT(stretch(r, 4, 4, 2, 2, 20, 2), WithinRel(sigma / m.e3 * length, 1e-8));
        CHECK_THAT(stretch(r, 4, 4, 4, 2, 20, 0), WithinRel(-m.nu13 * sigma / m.e1 * side, 1e-8));
        CHECK_THAT(r.min_safety, WithinRel(tsai_wu_safety(m, Voigt{0, 0, sigma, 0, 0, 0}), 1e-8));
        for (std::size_t v = 0; v < r.mode.size(); ++v) {
            CHECK(r.mode[v] == FailureMode::BetweenLayers);
            CHECK_THAT(r.safety[v], WithinRel(r.min_safety, 1e-8));
        }
    }
    SECTION("unknown directions: the layer average and the weakest direction") {
        const AsPrintedResult r = bar(0, {});
        REQUIRE(r.elasticity.converged);
        const double ex = stretch(r, 20, 4, 20, 2, 2, 0) / length;
        CHECK(ex > sigma / m.e1);
        CHECK(ex < sigma / m.e2);
        CHECK(r.min_safety <= tsai_wu_safety(m, Voigt{0, sigma, 0, 0, 0, 0}) * (1.0 + 1e-12));
        for (FailureMode f : r.mode)
            CHECK(f == FailureMode::BetweenLines);
    }
}

TEST_CASE("as printed: line directions of a sliced cube, four voxels per line", "[fea]") {
    const IndexedMesh cube = weld_exact(read_stl(std::filesystem::path(TISMA_TEST_DATA_DIR) / "cube_ascii.stl").mesh);
    const Settings settings;
    const SlicedModel model = slice_model(cube, settings);
    const VoxelGrid g = voxelize(cube, 0.1, 0.1, layer_planes(model.layers, 2));
    REQUIRE(g.nx == 100);
    REQUIRE(g.nz == 100);
    const std::vector<float> direction = line_directions(g, model);
    REQUIRE(direction.size() == g.solid.size());
    // The outer walls of layer 25 (voxel rows 50 and 51), at their middle.
    for (int k : {50, 51}) {
        const float x_wall = direction[g.index(2, 50, k)], y_wall = direction[g.index(50, 2, k)];
        CHECK_THAT(std::cos(x_wall), WithinAbs(0.0, 1e-3));
        CHECK_THAT(std::sin(y_wall), WithinAbs(0.0, 1e-3));
    }
    // Inside the walls: solid infill on layer 1, lines at 45 degrees; sparse
    // infill on layer 25, mostly gaps.
    auto inside = [&](int k, auto&& count) {
        int n = 0, hits = 0;
        for (int j = 25; j < 75; ++j)
            for (int i = 25; i < 75; ++i, ++n)
                hits += count(direction[g.index(i, j, k)]);
        return double(hits) / n;
    };
    CHECK(inside(2, [](float a) { return !std::isnan(a) && std::abs(std::cos(2.0 * a)) < 1e-3; }) > 0.98);
    const double sparse = inside(50, [](float a) { return !std::isnan(a); });
    INFO("covered fraction of the sparse layer " << sparse);
    CHECK(sparse > 0.1);
    CHECK(sparse < 0.4);
    const VoxelGrid printed_grid = printed_voxels(g, direction);
    CHECK(printed_grid.count() < g.count());
    CHECK(printed_grid.count() > g.count() / 4);
}

TEST_CASE("printed materials: every record gives a usable material, measured values kept", "[fea]") {
    std::vector<std::string_view> ids;
    for (const MaterialRecord& r : material_records()) {
        INFO(r.id);
        CHECK(std::find(ids.begin(), ids.end(), r.id) == ids.end());
        ids.push_back(r.id);
        CHECK(find_material(r.id) == &r);
        CHECK(!r.source.empty());
        const OrthotropicMaterial m = as_printed(r);
        for (double v : {m.e1, m.e2, m.e3, m.g12, m.g13, m.g23, m.nu12, m.nu13, m.nu23, m.xt, m.xc, m.yt, m.yc, m.zt,
                         m.zc, m.s12, m.s13, m.s23})
            CHECK((std::isfinite(v) && v > 0.0));
        CHECK_NOTHROW(local_stiffness(m));
        // Across the layers is the weakest direction, and compression there
        // is not weaker than tension.
        CHECK(m.zt <= m.yt);
        CHECK(m.zc >= m.zt);
    }
    CHECK(find_material("no-such-material") == nullptr);

    const OrthotropicMaterial pla = as_printed(*find_material("bambu-pla-basic"));
    CHECK(pla.e1 == 2580.0);
    CHECK(pla.e2 == 2580.0);
    CHECK(pla.e3 == 2060.0);
    CHECK(pla.xt == 35.0);
    CHECK(pla.zt == 31.0);
    CHECK(pla.yt == 31.0); // 0.84 xt would be below the measured bond between layers
    CHECK_THAT(pla.s13, WithinRel(kShearBetweenLayers * 31.0, 1e-12));

    const OrthotropicMaterial song = as_printed(*find_material("song2017-pla"));
    CHECK(song.e2 == 4040.0);
    CHECK(song.e3 == 4040.0);
    CHECK(song.g12 == 1500.0);
    CHECK(song.g23 == 1470.0);
    CHECK(song.yt == 46.24);
    CHECK_THAT(song.zt, WithinRel(kBetweenLayers * 54.89, 1e-12));
    CHECK(song.xc == 98.40);
    CHECK_THAT(song.zc, WithinRel(kCompressionAcrossLayers * 98.40, 1e-12));

    const OrthotropicMaterial abs = as_printed(*find_material("cnckitchen-abs-open"));
    const OrthotropicMaterial cantrell = as_printed(*find_material("cantrell2017-abs"));
    CHECK(abs.e1 == cantrell.e1);
    CHECK(abs.g13 == 670.0);
    CHECK(abs.zt == 21.0);
    CHECK(cantrell.s12 == 28.8);
    CHECK(cantrell.s23 == 25.8);
}

TEST_CASE("multigrid: the same answer as Jacobi in a few iterations at any voxel size", "[fea]") {
    // The cantilever of the beam test at two voxel sizes.
    auto cantilever = [](double h, Preconditioner preconditioner) {
        const VoxelGrid g = block(int(40 / h), int(4 / h), int(4 / h), h);
        const Fixture clamp{face(0, 0.0)};
        SurfaceLoad load;
        load.box = face(0, 40.0);
        load.force[2] = -10.0;
        return solve_elasticity(g, Material{}, std::span(&clamp, 1), std::span(&load, 1),
                                {.tolerance = 1e-10, .preconditioner = preconditioner});
    };
    const ElasticityResult jacobi = cantilever(1.0, Preconditioner::Jacobi);
    const ElasticityResult coarse = cantilever(1.0, Preconditioner::Multigrid);
    const ElasticityResult fine = cantilever(0.5, Preconditioner::Multigrid);
    REQUIRE(jacobi.converged);
    REQUIRE(coarse.converged);
    REQUIRE(fine.converged);
    INFO("Jacobi " << jacobi.iterations << " iterations; multigrid " << coarse.iterations << " and " << fine.iterations);
    CHECK(jacobi.levels == 1);
    CHECK(coarse.levels == 2);
    CHECK(fine.levels == 3);
    for (std::size_t q = 0; q < jacobi.displacement.size(); ++q)
        CHECK_THAT(coarse.displacement[q], WithinAbs(jacobi.displacement[q], 1e-8));
    CHECK(coarse.iterations < jacobi.iterations / 10);
    // Halving the voxels doubles Jacobi's iterations but barely changes these.
    CHECK(fine.iterations <= coarse.iterations + 4);

    // Twice the same answer, to the last bit.
    const ElasticityResult again = cantilever(1.0, Preconditioner::Multigrid);
    CHECK(again.iterations == coarse.iterations);
    CHECK(again.displacement == coarse.displacement);
}

TEST_CASE("multigrid: odd sizes, flat parts, holes and parts that nothing holds", "[fea]") {
    auto compare = [](const VoxelGrid& g, std::span<const Fixture> fixtures, std::span<const SurfaceLoad> loads) {
        const ElasticityResult jacobi =
            solve_elasticity(g, Material{}, fixtures, loads, {.tolerance = 1e-11, .preconditioner = Preconditioner::Jacobi});
        const ElasticityResult multigrid = solve_elasticity(g, Material{}, fixtures, loads, {.tolerance = 1e-11});
        REQUIRE(jacobi.converged);
        REQUIRE(multigrid.converged);
        INFO("Jacobi " << jacobi.iterations << " iterations, multigrid " << multigrid.iterations << " on " << multigrid.levels << " levels");
        CHECK(multigrid.iterations < jacobi.iterations);
        double largest = 0.0;
        for (double u : jacobi.displacement)
            largest = std::max(largest, std::abs(u));
        for (std::size_t q = 0; q < jacobi.displacement.size(); ++q)
            CHECK_THAT(multigrid.displacement[q], WithinAbs(jacobi.displacement[q], 1e-7 * largest));
        return multigrid;
    };
    SECTION("a plate one voxel thick with odd sides, layers of two heights") {
        VoxelGrid g = block(33, 21, 1, 1.0);
        g.z = {0.0, 0.6};
        const Fixture clamp{face(0, 0.0)};
        SurfaceLoad load;
        load.box = face(0, 33.0);
        load.force[2] = -0.01;
        compare(g, std::span(&clamp, 1), std::span(&load, 1));
        // Its levels: the thickness stays one element.
        const VoxelOperator op(g, std::vector<Stiffness>{isotropic_stiffness(3500.0, 0.35)}, {}, {});
        std::vector<std::uint8_t> free(3 * op.nodes(), 1);
        const Multigrid mg(op, free, op.diagonal());
        REQUIRE(mg.levels() == 3);
        CHECK(mg.elements(0) == std::array<int, 3>{33, 21, 1});
        CHECK(mg.elements(1) == std::array<int, 3>{17, 11, 1});
        CHECK(mg.elements(2) == std::array<int, 3>{9, 6, 1});
    }
    SECTION("a lattice: columns and beams with holes between them") {
        VoxelGrid g = block(14, 14, 8, 0.5);
        for (int k = 0; k < g.nz; ++k)
            for (int j = 0; j < g.ny; ++j)
                for (int i = 0; i < g.nx; ++i)
                    g.solid[g.index(i, j, k)] = (i % 4 < 2) + (j % 4 < 2) + (k % 4 < 2) >= 2;
        Box bottom;
        bottom.max[2] = 1e-9;
        const Fixture fixed{bottom};
        SurfaceLoad load;
        load.box.min[2] = 4.0 - 1e-9;
        load.force[0] = 1.0;
        load.force[2] = -2.0;
        compare(g, std::span(&fixed, 1), std::span(&load, 1));
    }
    SECTION("a loose block that nothing holds") {
        // A gap splits a bar standing on its clamped base: the upper block
        // touches no fixture, so it takes no part and stays put.
        VoxelGrid g = block(4, 4, 9, 1.0);
        for (int j = 0; j < g.ny; ++j)
            for (int i = 0; i < g.nx; ++i)
                g.solid[g.index(i, j, 4)] = 0;
        Box bottom;
        bottom.max[2] = 1e-9;
        const Fixture clamp{bottom};
        SurfaceLoad load;
        load.box = face(2, 4.0);
        load.force[0] = 5.0;
        const ElasticityResult r = solve_elasticity(g, Material{}, std::span(&clamp, 1), std::span(&load, 1), {.tolerance = 1e-12});
        REQUIRE(r.converged);
        CHECK(r.loose_voxels == 4 * 4 * 4);
        CHECK(r.unapplied_loads == 0);
        VoxelGrid lower = g;
        for (std::size_t v = std::size_t(4) * 4 * 4; v < lower.solid.size(); ++v)
            lower.solid[v] = 0;
        const ElasticityResult reference =
            solve_elasticity(lower, Material{}, std::span(&clamp, 1), std::span(&load, 1), {.tolerance = 1e-12});
        CHECK(reference.loose_voxels == 0);
        const std::size_t below = std::size_t(5) * 5 * 5; // nodes up to the top of the lower part
        for (std::size_t q = 0; q < 3 * below; ++q)
            CHECK_THAT(r.displacement[q], WithinAbs(reference.displacement[q], 1e-12));
        for (std::size_t q = 3 * below; q < r.displacement.size(); ++q)
            CHECK(r.displacement[q] == 0.0);
        // A load on the loose block alone finds nothing to push.
        load.box = face(2, 9.0);
        const ElasticityResult lost = solve_elasticity(g, Material{}, std::span(&clamp, 1), std::span(&load, 1));
        CHECK(lost.unapplied_loads == 1);
    }
}
