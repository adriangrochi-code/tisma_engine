#include "config/settings.hpp"
#include "gcode/writer.hpp"
#include "mesh/indexed_mesh.hpp"
#include "mesh/shapes.hpp"
#include "slice/pipeline.hpp"
#include "test_printer.hpp"

#include <catch2/catch_test_macros.hpp>

#include <sstream>
#include <string>
#include <vector>

using namespace tisma;

namespace {

// A hat: a stem of radius 5 up to z 4, a brim of radius 15 from 4 to 6.
IndexedMesh hat() {
    TriangleMesh m = make_cylinder(5.0, 4.0, 128);
    const TriangleMesh brim = make_cylinder(15.0, 2.0, 128);
    for (std::size_t t = 0; t < brim.triangle_count(); ++t) {
        Vec3f c[3];
        for (int k = 0; k < 3; ++k)
            c[k] = brim.corner(t, k), c[k].z += 4.0f;
        m.add_triangle(c[0], c[1], c[2]);
    }
    return weld_exact(m);
}

// The ;TYPE: lines of the layer that starts at z, in order, without repeats.
std::vector<std::string> types_at(const std::string& g, const std::string& z) {
    const std::size_t at = g.find(";Z:" + z + "\n");
    REQUIRE(at != std::string::npos);
    const std::size_t end = g.find(";LAYER_CHANGE", at);
    std::vector<std::string> out;
    std::istringstream in(g.substr(at, end - at));
    for (std::string line; std::getline(in, line);)
        if (line.rfind(";TYPE:", 0) == 0 && (out.empty() || out.back() != line.substr(6)))
            out.push_back(line.substr(6));
    return out;
}

bool is_wall(const std::string& t) { return t == "External perimeter" || t == "Perimeter"; }

} // namespace

TEST_CASE("walls.unsupported_last: a brim's loops over nothing go after its layer's fill (orca_2_5.md, rule 3, OR3)",
          "[walls][unsupported]") {
    Settings s = test_printer();
    s.skirt_loops = 0;
    s.gpu = GpuMode::Off;
    s.wall_count = 3;
    s.detect_overhangs = false;
    const IndexedMesh mesh = hat();
    const SlicedModel model = slice_model(mesh, s);
    std::ostringstream plain;
    write_gcode(plain, model, s);
    // Off: the walls first, as always.
    const std::vector<std::string> before = types_at(plain.str(), "4.200");
    INFO(plain.str().substr(plain.str().find(";Z:4.200\n"), 400));
    REQUIRE(!before.empty());
    CHECK(is_wall(before.front()));
    // On: the brim's loops hang over nothing and touch nothing held, so they
    // come after the layer's fill.
    s.unsupported_last = true;
    std::ostringstream last;
    write_gcode(last, slice_model(mesh, s), s);
    const std::vector<std::string> after = types_at(last.str(), "4.200");
    REQUIRE(!after.empty());
    CHECK_FALSE(is_wall(after.front()));
    CHECK(is_wall(after.back()));
    // The stem's layers, held all round, keep the usual order.
    CHECK(types_at(last.str(), "2.000") == types_at(plain.str(), "2.000"));
}

TEST_CASE("walls.unsupported_last: a loop next to a held wall stays before the fill", "[walls][unsupported]") {
    // A step: a 20 mm square up to z 2, then a 24 mm square on it. On the step's
    // first layer the outer loop hangs, the inner ones stand on the square:
    // the hanging one touches them and goes right after them, before the fill.
    TriangleMesh m = make_box(20.0, 20.0, 2.0);
    const TriangleMesh top = make_box(21.0, 21.0, 2.0);
    for (std::size_t t = 0; t < top.triangle_count(); ++t) {
        Vec3f c[3];
        for (int k = 0; k < 3; ++k)
            c[k] = top.corner(t, k), c[k].z += 2.0f;
        m.add_triangle(c[0], c[1], c[2]);
    }
    Settings s = test_printer();
    s.skirt_loops = 0;
    s.gpu = GpuMode::Off;
    s.wall_count = 3;
    s.detect_overhangs = false;
    s.unsupported_last = true;
    std::ostringstream out;
    write_gcode(out, slice_model(weld_exact(m), s), s);
    const std::vector<std::string> types = types_at(out.str(), "2.200");
    INFO(types.size());
    REQUIRE(types.size() >= 2);
    // Inner walls, then the outer one, then the fill: no wall after the fill.
    CHECK(is_wall(types.front()));
    CHECK(types[types.size() - 2] == "External perimeter");
    CHECK_FALSE(is_wall(types.back()));
}
