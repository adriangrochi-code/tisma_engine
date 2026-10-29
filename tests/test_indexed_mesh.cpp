#include "io/stl.hpp"
#include "mesh/indexed_mesh.hpp"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>

using namespace tisma;

TEST_CASE("welding the STL cube gives 8 shared vertices", "[mesh]") {
    const StlFile stl = read_stl(std::filesystem::path(TISMA_TEST_DATA_DIR) / "cube_ascii.stl");
    const IndexedMesh m = weld_exact(stl.mesh);
    CHECK(m.vertices.size() == 8);
    REQUIRE(m.triangle_count() == 12);
    // Vertices are numbered in order of first appearance.
    CHECK(m.triangles[0] == std::array<std::uint32_t, 3>{0, 1, 2});
    // Triangles keep their order and winding.
    for (std::size_t t = 0; t < 12; ++t)
        for (int k = 0; k < 3; ++k)
            CHECK(m.vertices[m.triangles[t][std::size_t(k)]] == stl.mesh.corner(t, k));
}

TEST_CASE("welding merges -0 with 0 and keeps distinct points apart", "[mesh]") {
    TriangleMesh soup;
    soup.add_triangle({0.0f, 0, 0}, {1, 0, 0}, {0, 1, 0});
    soup.add_triangle({-0.0f, 0, 0}, {0, 1, 0}, {0, 0, 1});
    soup.add_triangle({1e-7f, 0, 0}, {1, 0, 0}, {0, 0, 1});
    const IndexedMesh m = weld_exact(soup);
    CHECK(m.vertices.size() == 5); // origin, x, y, z and the point 1e-7 away from the origin
    CHECK(m.triangles[0][0] == m.triangles[1][0]);
    CHECK(m.triangles[0][0] != m.triangles[2][0]);
}

TEST_CASE("welding on all cores gives the serial weld's mesh", "[mesh]") {
    // A grid of triangles whose corners repeat up to six times, in a
    // scrambled order, with -0 among them: the same vertices, numbered alike.
    TriangleMesh soup;
    const int n = 120;
    std::uint32_t mix = 12345;
    auto at = [](int i, int j) { return Vec3f{float(i) * 0.5f, float(j) * 0.25f, i == 0 ? -0.0f : float(i + j) * 0.1f}; };
    for (int k = 0; k < n * n; ++k) {
        mix = mix * 1103515245u + 12345u;
        const int i = int(mix % std::uint32_t(n)), j = int((mix >> 12) % std::uint32_t(n));
        soup.add_triangle(at(i, j), at(i + 1, j), at(i, j + 1));
        soup.add_triangle(at(i + 1, j), at(i + 1, j + 1), at(i, j + 1));
    }
    const IndexedMesh serial = weld_exact(soup); // below the size for all cores
    const IndexedMesh parallel = weld_exact_parallel(soup);
    CHECK(parallel.vertices == serial.vertices);
    CHECK(parallel.triangles == serial.triangles);
    CHECK(weld_exact_parallel(TriangleMesh{}).vertices.empty());
}

TEST_CASE("welding an empty mesh", "[mesh]") {
    const IndexedMesh m = weld_exact(TriangleMesh{});
    CHECK(m.vertices.empty());
    CHECK(m.triangles.empty());
}
