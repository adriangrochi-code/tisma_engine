// Object labels (docs/spec/features/gcode_firmware.md, rule 4, tests FW1,
// FW3, FW6 and FW8).

#include "config/settings.hpp"
#include "gcode/check.hpp"
#include "gcode/writer.hpp"
#include "mesh/indexed_mesh.hpp"
#include "mesh/mesh.hpp"
#include "slice/pipeline.hpp"
#include "slice/placement.hpp"
#include "test_printer.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <sstream>
#include <string>
#include <vector>

using namespace tisma;
using Catch::Matchers::ContainsSubstring;

namespace {

TriangleMesh cube20() {
    TriangleMesh m;
    const Vec3f v[8] = {{90, 90, 0}, {110, 90, 0}, {110, 110, 0}, {90, 110, 0},
                        {90, 90, 10}, {110, 90, 10}, {110, 110, 10}, {90, 110, 10}};
    const auto quad = [&](int a, int b, int c, int d) {
        m.add_triangle(v[a], v[b], v[c]);
        m.add_triangle(v[a], v[c], v[d]);
    };
    quad(0, 3, 2, 1);
    quad(4, 5, 6, 7);
    quad(0, 1, 5, 4);
    quad(1, 2, 6, 5);
    quad(2, 3, 7, 6);
    quad(3, 0, 4, 7);
    return m;
}

Settings base(GcodeFlavor flavor, ObjectLabels labels) {
    Settings s = test_printer();
    s.flavor = flavor;
    s.object_labels = labels;
    s.bed = {{0, 0}, {200, 0}, {200, 200}, {0, 200}};
    s.layer_height = s.first_layer_height = 0.2;
    s.wall_count = 2;
    s.infill_density = 0.0;
    s.top_layers = s.bottom_layers = 0;
    s.skirt_loops = 0;
    return s;
}

// Two copies of the cube, as the spec's FW tests print them.
std::string gcode_of(const Settings& s, std::size_t copies = 2) {
    std::vector<MeshObject> objects;
    const TriangleMesh plate = place_copies(cube20(), copies, s, "cube20.stl", &objects);
    const SlicedModel model = slice_model(weld_exact(plate), s, nullptr, nullptr, nullptr, &objects);
    std::ostringstream out;
    write_gcode(out, model, s);
    const GcodeCheck check = check_gcode(out.str(), gcode_limits(model, s));
    INFO((check.problems.empty() ? std::string() : check.problems.front().message));
    CHECK(check.ok());
    return out.str();
}

std::size_t count(const std::string& text, const std::string& what) {
    std::size_t n = 0;
    for (std::size_t at = text.find(what); at != std::string::npos; at = text.find(what, at + 1))
        ++n;
    return n;
}

} // namespace

TEST_CASE("labels: Marlin's M486 defines every copy and wraps its extrusions (FW1)", "[labels]") {
    const std::string text = gcode_of(base(GcodeFlavor::Marlin, ObjectLabels::Firmware));
    CHECK_THAT(text, ContainsSubstring("M486 S0\nM486 Acube20.stl (Instance 1)\nM486 S-1\n"));
    CHECK_THAT(text, ContainsSubstring("M486 S1\nM486 Acube20.stl (Instance 2)\nM486 S-1\n"));
    // Each layer: both copies, each between M486 S<n> and M486 S-1.
    const std::size_t layers = count(text, ";LAYER_CHANGE");
    CHECK(count(text, "M486 S0\n") == layers + 1);
    CHECK(count(text, "M486 S1\n") == layers + 1);
    CHECK(count(text, "M486 S-1\n") == 2 * layers + 2);
    // The definitions come before the temperatures and the start G-code.
    CHECK(text.find("M486 S0") < text.find(";LAYER_CHANGE"));
}

TEST_CASE("labels: RepRapFirmware names a copy on one line", "[labels]") {
    const std::string text = gcode_of(base(GcodeFlavor::RepRapFirmware, ObjectLabels::Firmware));
    CHECK_THAT(text, ContainsSubstring("M486 S0 A\"cube20.stl (Instance 1)\"\n"));
    CHECK_THAT(text, ContainsSubstring("M486 S1 A\"cube20.stl (Instance 2)\"\n"));
}

TEST_CASE("labels: Klipper's EXCLUDE_OBJECT (FW3)", "[labels]") {
    const std::string text = gcode_of(base(GcodeFlavor::Klipper, ObjectLabels::Firmware));
    CHECK_THAT(text, ContainsSubstring("EXCLUDE_OBJECT_DEFINE NAME='cube20_stl_(Instance_1)' CENTER="));
    CHECK_THAT(text, ContainsSubstring(" POLYGON=[["));
    const std::size_t layers = count(text, ";LAYER_CHANGE");
    CHECK(count(text, "EXCLUDE_OBJECT_START NAME='cube20_stl_(Instance_2)'") == layers);
    CHECK(count(text, "EXCLUDE_OBJECT_END NAME='cube20_stl_(Instance_2)'") == layers);
}

TEST_CASE("labels: OctoPrint's comments (FW6); none on generic RepRap (FW8)", "[labels]") {
    const std::string octo = gcode_of(base(GcodeFlavor::Marlin, ObjectLabels::OctoPrint));
    CHECK_THAT(octo, ContainsSubstring("; printing object cube20.stl id:0 copy 0\n"));
    CHECK_THAT(octo, ContainsSubstring("; stop printing object cube20.stl id:0 copy 1\n"));
    CHECK(octo.find("M486") == std::string::npos);
    const std::string generic = gcode_of(base(GcodeFlavor::RepRap, ObjectLabels::Firmware));
    CHECK(generic.find("M486") == std::string::npos);
    CHECK(generic.find("EXCLUDE_OBJECT") == std::string::npos);
    CHECK(generic.find("printing object") == std::string::npos);
    // None asked: nothing.
    CHECK(gcode_of(base(GcodeFlavor::Marlin, ObjectLabels::None)).find("M486") == std::string::npos);
}

TEST_CASE("labels: one object at a time, each layer wrapped in its label", "[labels]") {
    Settings s = base(GcodeFlavor::Marlin, ObjectLabels::Firmware);
    s.sequential = true;
    const std::string text = gcode_of(s);
    const std::size_t layers = count(text, ";LAYER_CHANGE");
    CHECK(count(text, "M486 S0\n") + count(text, "M486 S1\n") == layers + 2);
}
