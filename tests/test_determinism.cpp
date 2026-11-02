// The same G-code for the same input, whatever the number of cores
// (docs/spec/features/orca_2_5.md, rule 19, test OR19).

#include "config/settings.hpp"
#include "gcode/writer.hpp"
#include "mesh/indexed_mesh.hpp"
#include "slice/pipeline.hpp"
#include "slice/placement.hpp"
#include "test_printer.hpp"

#include <catch2/catch_test_macros.hpp>

#include <oneapi/tbb/global_control.h>
#include <oneapi/tbb/task_arena.h>

#include <sstream>
#include <string>
#include <vector>

using namespace tisma;

namespace {

// A closed box from (x0, y0, z0) to (x1, y1, z1), appended to the mesh.
void add_box(IndexedMesh& m, float x0, float y0, float z0, float x1, float y1, float z1) {
    const auto base = std::uint32_t(m.vertices.size());
    for (int k = 0; k < 8; ++k)
        m.vertices.push_back({k & 1 ? x1 : x0, (k >> 1) & 1 ? y1 : y0, (k >> 2) & 1 ? z1 : z0});
    const std::uint32_t faces[6][4] = {{0, 2, 3, 1}, {4, 5, 7, 6}, {0, 1, 5, 4}, {2, 6, 7, 3}, {0, 4, 6, 2}, {1, 3, 7, 5}};
    for (const auto& f : faces) {
        m.triangles.push_back({base + f[0], base + f[1], base + f[2]});
        m.triangles.push_back({base + f[0], base + f[2], base + f[3]});
    }
}

// Sliced and written with at most `threads` threads.
std::string gcode_with(int threads, const IndexedMesh& mesh, const Settings& s,
                       const std::vector<MeshObject>* objects = nullptr) {
    tbb::global_control limit(tbb::global_control::max_allowed_parallelism, std::size_t(threads));
    tbb::task_arena arena(threads);
    std::string text;
    arena.execute([&] {
        std::ostringstream out;
        write_gcode(out, slice_model(mesh, s, nullptr, nullptr, nullptr, objects), s);
        text = out.str();
    });
    return text;
}

} // namespace

TEST_CASE("the same G-code on 1, 2 and 8 cores, and twice (OR19)", "[determinism]") {
    // A T: a column under a slab that hangs out on both sides, with tree
    // supports under the overhangs and lightning infill.
    IndexedMesh mesh;
    add_box(mesh, 95, 90, 0, 105, 110, 10);
    add_box(mesh, 80, 90, 10, 120, 110, 13);
    Settings s = test_printer();
    s.support = true;
    s.support_style = SupportStyle::Organic;
    s.infill_pattern = InfillPattern::Lightning;
    s.infill_density = 20.0;
    const std::string one = gcode_with(1, mesh, s);
    REQUIRE(one.size() > 10000);
    CHECK(one.find(";TYPE:Support") != std::string::npos);
    CHECK(gcode_with(2, mesh, s) == one);
    CHECK(gcode_with(8, mesh, s) == one);
    CHECK(gcode_with(8, mesh, s) == one);
}

TEST_CASE("the same G-code on 1, 2 and 8 cores with two objects, two extruders, fuzzy skin and arcs (OR19)",
          "[determinism]") {
    // Two Ts side by side, each its own object on its own extruder, with
    // labels, fuzzy skin on the outer walls, arcs over the overhangs and the
    // overhang speeds, tree supports and lightning infill.
    IndexedMesh mesh;
    add_box(mesh, 65, 90, 0, 75, 110, 10);
    add_box(mesh, 55, 90, 10, 85, 110, 13);
    add_box(mesh, 125, 90, 0, 135, 110, 10);
    add_box(mesh, 115, 90, 10, 145, 110, 13);
    std::vector<MeshObject> objects(2);
    objects[0].name = "left", objects[0].first = 0, objects[0].count = 24, objects[0].object = 0, objects[0].extruder = 1;
    objects[1].name = "right", objects[1].first = 24, objects[1].count = 24, objects[1].object = 1, objects[1].extruder = 2;
    Settings s = test_printer();
    s.extruders = 2;
    s.support = true;
    s.support_style = SupportStyle::Organic;
    s.infill_pattern = InfillPattern::Lightning;
    s.infill_density = 20.0;
    s.fuzzy_mode = FuzzyMode::External;
    s.arcs = true;
    s.flavor = GcodeFlavor::Marlin;
    s.object_labels = ObjectLabels::Firmware; // M486 in Marlin
    const std::string one = gcode_with(1, mesh, s, &objects);
    REQUIRE(one.size() > 10000);
    CHECK(one.find("\nT1") != std::string::npos);  // the second extruder prints
    CHECK(one.find("M486") != std::string::npos);   // the objects' labels
    CHECK(one.find(";TYPE:Support") != std::string::npos);
    CHECK(gcode_with(2, mesh, s, &objects) == one);
    CHECK(gcode_with(8, mesh, s, &objects) == one);
    CHECK(gcode_with(8, mesh, s, &objects) == one);
}
