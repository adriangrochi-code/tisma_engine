#include "fea/elasticity.hpp"
#include "fea/hex8.hpp"
#include "fea/voxels.hpp"
#include "io/stl.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <filesystem>

using namespace tisma;
using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

namespace {

VoxelGrid block(int nx, int ny, int nz, double h) {
    VoxelGrid g;
    g.h = h, g.nx = nx, g.ny = ny, g.nz = nz;
    g.solid.assign(std::size_t(nx) * std::size_t(ny) * std::size_t(nz), 1);
    return g;
}

Box face(int axis, double at) {
    Box b;
    b.min[axis] = at - 1e-9;
    b.max[axis] = at + 1e-9;
    return b;
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
    const ElasticityResult r = solve_elasticity(g, m, fixtures, std::span(&load, 1), 1e-12);
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
    const ElasticityResult r = solve_elasticity(g, m, std::span(&clamp, 1), std::span(&load, 1), 1e-10);
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
