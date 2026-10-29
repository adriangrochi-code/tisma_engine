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

TEST_CASE("welding an empty mesh", "[mesh]") {
    const IndexedMesh m = weld_exact(TriangleMesh{});
    CHECK(m.vertices.empty());
    CHECK(m.triangles.empty());
}
