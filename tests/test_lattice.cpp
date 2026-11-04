#include "mesh/lattice.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <map>
#include <numbers>
#include <utility>

using namespace tisma;
using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

TEST_CASE("lattice O6: one cell, 12 closed bars, none flatter than 45 degrees (structural_optimization.md, rule 7)",
          "[lattice]") {
    const std::vector<LatticeBar> bars = lattice_bars({0, 0, 0}, {5, 5, 5}, 5.0);
    REQUIRE(bars.size() == 12);
    for (const LatticeBar& b : bars) {
        const double dx = b.to[0] - b.from[0], dy = b.to[1] - b.from[1], dz = b.to[2] - b.from[2];
        const double length = std::sqrt(dx * dx + dy * dy + dz * dz);
        CHECK(std::abs(dz) / length >= std::sin(std::numbers::pi / 4.0) - 1e-12); // no horizontal bars
    }
    // Each prism is closed (every edge in two triangles) and faces out.
    for (const LatticeBar& b : bars) {
        const TriangleMesh m = lattice_mesh({b}, 1.0);
        REQUIRE(m.triangle_count() == 24);
        std::map<std::pair<std::array<float, 3>, std::array<float, 3>>, int> edges;
        for (std::uint32_t t = 0; t < m.triangle_count(); ++t)
            for (int k = 0; k < 3; ++k) {
                const Vec3f p = m.corner(t, k), q = m.corner(t, (k + 1) % 3);
                std::array<float, 3> a{p.x, p.y, p.z}, c{q.x, q.y, q.z};
                if (c < a)
                    std::swap(a, c);
                ++edges[{a, c}];
            }
        for (const auto& [edge, count] : edges)
            CHECK(count == 2);
        const double dx = b.to[0] - b.from[0], dy = b.to[1] - b.from[1], dz = b.to[2] - b.from[2];
        const double length = std::sqrt(dx * dx + dy * dy + dz * dz);
        CHECK_THAT(m.signed_volume(), WithinRel(1.5 * std::sqrt(3.0) * 0.25 * length, 1e-4)); // hexagon area (3√3/2) r²
    }
    // Neighbouring cells share their bars: 2 × 2 × 1 cells.
    const std::vector<LatticeBar> four = lattice_bars({0, 0, 0}, {10, 10, 5}, 5.0);
    // 9 vertical edges; XZ faces: 3 planes of y × 2 cells × 2 diagonals; the same for YZ.
    CHECK(four.size() == 9 + 12 + 12);
    // The density: the bars' length per cell is a (1 + 4 √2).
    CHECK_THAT(lattice_density(2.0, 10.0), WithinRel(std::numbers::pi * (1.0 + 4.0 * std::numbers::sqrt2) * 0.01, 1e-12));
}
