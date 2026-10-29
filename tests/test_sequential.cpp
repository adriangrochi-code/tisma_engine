// Printing object by object (docs/spec/features/sequential.md, tests SQ1 to
// SQ4) and the copies on the bed (placement.md, rule 2).

#include "config/settings.hpp"
#include "gcode/check.hpp"
#include "gcode/writer.hpp"
#include "mesh/indexed_mesh.hpp"
#include "mesh/mesh.hpp"
#include "report/stability.hpp"
#include "slice/pipeline.hpp"
#include "slice/placement.hpp"
#include "slice/printable.hpp"
#include "test_printer.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <array>
#include <cmath>
#include <cstdlib>
#include <sstream>
#include <string>
#include <vector>

using namespace tisma;
using Catch::Matchers::ContainsSubstring;
using Catch::Matchers::WithinAbs;

namespace {

// A box from (x0, y0, 0) to (x1, y1, z1).
TriangleMesh box(float x0, float y0, float x1, float y1, float z1) {
    TriangleMesh m;
    const Vec3f v[8] = {{x0, y0, 0}, {x1, y0, 0}, {x1, y1, 0}, {x0, y1, 0},
                        {x0, y0, z1}, {x1, y0, z1}, {x1, y1, z1}, {x0, y1, z1}};
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

// The spec's base on a 200 mm bed: 0.2 layers, 2 walls, no infill, no skirt.
Settings base() {
    Settings s = test_printer();
    s.bed = {{0, 0}, {200, 0}, {200, 200}, {0, 200}};
    s.max_height = 200.0;
    s.layer_height = s.first_layer_height = 0.2;
    s.wall_count = 2;
    s.infill_density = 0.0;
    s.top_layers = s.bottom_layers = 0;
    s.skirt_loops = 0;
    s.brim_width = 0.0;
    s.variables["temperature"] = {"205"};
    s.variables["first_layer_temperature"] = {"200"};
    s.variables["bed_temperature"] = {"0"};
    return s;
}

struct Plate {
    TriangleMesh mesh;
    std::vector<MeshObject> objects;
};

Plate copies(const TriangleMesh& model, std::size_t n, const Settings& s) {
    Plate p;
    p.mesh = place_copies(model, n, s, "cube", &p.objects);
    return p;
}

SlicedModel sliced(const Plate& p, const Settings& s) {
    return slice_model(weld_exact(p.mesh), s, nullptr, nullptr, nullptr, &p.objects);
}

std::string gcode_of(const SlicedModel& model, const Settings& s) {
    std::ostringstream out;
    write_gcode(out, model, s);
    const GcodeCheck check = check_gcode(out.str(), gcode_limits(model, s));
    INFO((check.problems.empty() ? std::string()
                                 : "line " + std::to_string(check.problems.front().line) + ": " +
                                       check.problems.front().message));
    CHECK(check.ok());
    return out.str();
}

// The X range of each layer's extrusions.
std::vector<std::array<double, 2>> layer_x_ranges(const std::string& gcode) {
    std::vector<std::array<double, 2>> out;
    double x = 0.0, e = 0.0;
    std::istringstream in(gcode);
    for (std::string line; std::getline(in, line);) {
        if (line.starts_with(";LAYER_CHANGE")) {
            out.push_back({1e9, -1e9});
            continue;
        }
        const std::string code = line.substr(0, line.find(';'));
        if (code.starts_with("G92")) {
            e = 0.0;
            continue;
        }
        if (!code.starts_with("G1 "))
            continue;
        double nx = x, ne = e;
        bool has_x = false, has_e = false;
        std::istringstream words(code.substr(3));
        for (std::string w; words >> w;) {
            if (w[0] == 'X')
                nx = std::atof(w.c_str() + 1), has_x = true;
            else if (w[0] == 'E')
                ne = std::atof(w.c_str() + 1), has_e = true;
        }
        if (has_e && ne > e && !out.empty()) {
            out.back()[0] = std::min({out.back()[0], x, nx});
            out.back()[1] = std::max({out.back()[1], x, nx});
        }
        if (has_e)
            e = ne;
        if (has_x)
            x = nx;
    }
    return out;
}

std::vector<std::string> lines_of(const std::string& text) {
    std::vector<std::string> out;
    std::istringstream in(text);
    for (std::string line; std::getline(in, line);)
        out.push_back(line);
    return out;
}

} // namespace

TEST_CASE("sequential: each object whole before the next, in their order (SQ1)", "[sequential]") {
    Settings s = base();
    const TriangleMesh cube = box(0, 0, 20, 20, 10);
    for (const std::size_t n : {std::size_t(2), std::size_t(3)}) {
        CAPTURE(n);
        const Plate plate = copies(cube, n, s);
        REQUIRE(plate.objects.size() == n);
        // Layer by layer: 50 layers, every copy in each.
        s.sequential = false;
        const SlicedModel together = sliced(plate, s);
        CHECK(together.layers.size() == 50);
        CHECK_FALSE(together.sequential);
        const std::vector<std::array<double, 2>> mixed = layer_x_ranges(gcode_of(together, s));
        REQUIRE(mixed.size() == 50);
        // Object by object: 50 layers each, one copy at a time.
        s.sequential = true;
        const SlicedModel apart = sliced(copies(cube, n, s), s);
        REQUIRE(apart.layers.size() == 50 * n);
        REQUIRE(apart.objects.size() == n);
        const std::vector<std::array<double, 2>> ranges = layer_x_ranges(gcode_of(apart, s));
        REQUIRE(ranges.size() == 50 * n);
        for (std::size_t k = 0; k < n; ++k) {
            const SlicedObject& o = apart.objects[k];
            CHECK(o.first_layer == 50 * k);
            CHECK_THAT(o.height, WithinAbs(10.0, 1e-6));
            double lo = 1e9, hi = -1e9;
            for (const auto& q : o.outline)
                lo = std::min(lo, to_mm(q.x)), hi = std::max(hi, to_mm(q.x));
            for (std::size_t l = 50 * k; l < 50 * (k + 1); ++l) {
                CAPTURE(l);
                CHECK(ranges[l][0] >= lo - 1e-3);
                CHECK(ranges[l][1] <= hi + 1e-3);
            }
        }
        // Layer by layer, a layer holds more than one copy's width.
        CHECK(mixed[10][1] - mixed[10][0] > 30.0);
    }
}

TEST_CASE("sequential: copies arranged with room for the head (SQ2)", "[sequential]") {
    Settings s = base();
    const TriangleMesh cube = box(0, 0, 20, 20, 10);
    const auto gap = [&](const Settings& settings) {
        std::vector<MeshObject> objects;
        const TriangleMesh both = place_copies(cube, 2, settings, "cube", &objects);
        REQUIRE(objects.size() == 2);
        double lo[2] = {1e9, 1e9}, hi[2] = {-1e9, -1e9};
        for (std::size_t k = 0; k < 2; ++k)
            for (std::size_t t = objects[k].first; t < objects[k].first + objects[k].count; ++t)
                for (int i = 0; i < 3; ++i)
                    lo[k] = std::min(lo[k], double(both.corner(t, i).x)), hi[k] = std::max(hi[k], double(both.corner(t, i).x));
        return std::max(lo[1] - hi[0], lo[0] - hi[1]);
    };
    CHECK_THAT(gap(s), WithinAbs(6.0, 1e-4)); // placement.spacing, not printing one by one
    s.sequential = true;
    for (const double radius : {5.0, 20.0, 35.0}) {
        s.clearance_radius = radius;
        CHECK_THAT(gap(s), WithinAbs(std::max(6.0, radius), 1e-4));
    }
    // The set is centred on the bed (placement.md, rule 2).
    s.clearance_radius = 20.0;
    const BoundingBox b = place_copies(cube, 2, s, "cube", nullptr).bounding_box();
    CHECK_THAT((double(b.min.x) + double(b.max.x)) / 2.0, WithinAbs(100.0, 1e-4));
    CHECK_THAT((double(b.min.y) + double(b.max.y)) / 2.0, WithinAbs(100.0, 1e-4));
}

TEST_CASE("sequential: over to the next object (SQ3)", "[sequential]") {
    Settings s = base();
    s.sequential = true;
    s.gcode_between_objects = ";BETWEEN [current_object_idx]";
    const Plate plate = copies(box(0, 0, 20, 20, 10), 2, s);
    const SlicedModel model = sliced(plate, s);
    const std::vector<std::string> lines = lines_of(gcode_of(model, s));
    std::size_t at = 0;
    while (at < lines.size() && !lines[at].starts_with(";NEXT_OBJECT:1"))
        ++at;
    REQUIRE(at < lines.size());
    // Retracted, over at the top of the first copy (Z 10), the first layer's
    // temperatures without waiting, the user's G-code, the fan off.
    std::vector<std::string> next(lines.begin() + std::ptrdiff_t(at), lines.begin() + std::ptrdiff_t(std::min(lines.size(), at + 16)));
    std::string joined;
    for (const std::string& l : next)
        joined += l + "\n";
    INFO(joined);
    std::size_t m140 = 0, m104 = 0, between = 0, m107 = 0, change = 0;
    for (std::size_t k = 0; k < next.size(); ++k) {
        if (next[k] == "M140 S0")
            m140 = k;
        if (next[k] == "M104 S200")
            m104 = k;
        if (next[k] == ";BETWEEN 1")
            between = k;
        if (next[k] == "M107")
            m107 = k;
        if (next[k] == ";LAYER_CHANGE")
            change = k;
    }
    REQUIRE(m140 > 0);
    CHECK(m104 > m140);
    CHECK(between > m104);
    CHECK(m107 > between);
    CHECK(change > m107);
    CHECK_THAT(next[1], ContainsSubstring(" E")); // the retraction
    // The new object's first layer: over to its first point, then down to Z 0.2.
    std::size_t lower = change;
    while (lower < next.size() && next[lower] != "G1 Z0.200")
        ++lower;
    REQUIRE(lower < next.size());
    CHECK_THAT(next[lower - 1], ContainsSubstring("X"));
    // A G-code that sets temperatures itself: no others.
    s.gcode_between_objects = "M109 S[temperature]";
    const std::string text = gcode_of(sliced(plate, s), s);
    const std::size_t from = text.find(";NEXT_OBJECT:1");
    REQUIRE(from != std::string::npos);
    const std::string transition = text.substr(from, text.find(";LAYER_CHANGE", from) - from);
    CHECK_THAT(transition, ContainsSubstring("M109 S205"));
    CHECK(transition.find("M104") == std::string::npos);
    CHECK(transition.find("M140") == std::string::npos);
}

TEST_CASE("sequential: objects that the head or the X axis would hit are refused (SQ4, rule 2)", "[sequential]") {
    Settings s = base();
    s.sequential = true;
    s.clearance_height = 20.0;
    // Two towers of 25 mm: the first is taller than the X axis' clearance.
    const SlicedModel towers = sliced(copies(box(0, 0, 20, 20, 25), 2, s), s);
    CHECK_THAT(unprintable_reason(towers, s), ContainsSubstring("sequential.clearance_height"));
    // Lower ones print; the last may be taller.
    const SlicedModel low = sliced(copies(box(0, 0, 20, 20, 15), 2, s), s);
    CHECK(unprintable_reason(low, s).empty());
    // Placed by hand closer than the head's radius.
    Plate close;
    close.mesh = box(60, 90, 80, 110, 10);
    const TriangleMesh other = box(90, 90, 110, 110, 10);
    for (std::size_t t = 0; t < other.triangle_count(); ++t)
        close.mesh.add_triangle(other.corner(t, 0), other.corner(t, 1), other.corner(t, 2));
    close.objects = {{"a", 0, 12}, {"b", 12, 12}};
    CHECK_THAT(unprintable_reason(sliced(close, s), s), ContainsSubstring("sequential.clearance_radius"));
    s.clearance_radius = 10.0;
    CHECK(unprintable_reason(sliced(close, s), s).empty());
}

TEST_CASE("sequential: each object has its own first layers, skirt and stability (rules 1 and 5)", "[sequential]") {
    Settings s = base();
    s.sequential = true;
    s.skirt_loops = 1;
    s.fan_off_layers = 1;
    const Plate plate = copies(box(0, 0, 20, 20, 10), 2, s);
    const SlicedModel model = sliced(plate, s);
    REQUIRE(model.objects.size() == 2);
    CHECK_FALSE(model.objects[0].skirt.empty());
    CHECK_FALSE(model.objects[1].skirt.empty());
    // No part hangs in the air: the second object stands on the bed.
    for (const Warning& w : stability_warnings(model, s))
        CHECK(w.kind != "stability_air");
    const std::string text = gcode_of(model, s);
    // Two skirts, one at each object's first layer.
    std::size_t skirts = 0;
    for (std::size_t at = text.find(";TYPE:Skirt/Brim"); at != std::string::npos; at = text.find(";TYPE:Skirt/Brim", at + 1))
        ++skirts;
    CHECK(skirts == 2);
    // Vase mode: several objects only one after another.
    s.vase = true;
    s.skirt_loops = 0;
    const SlicedModel vases = sliced(plate, s);
    CHECK(unprintable_reason(vases, s).empty());
    gcode_of(vases, s);
    s.sequential = false;
    CHECK_THAT(unprintable_reason(sliced(plate, s), s), ContainsSubstring("vase mode"));
}

TEST_CASE("sequential: the settings read and write", "[sequential][settings]") {
    const Settings s = parse_settings(R"({"sequential": {"enabled": true, "clearance_radius": 35, "clearance_height": 18},
                                          "placement": {"spacing": 8},
                                          "gcode": {"between_objects": ";next [current_object_idx]"}})");
    CHECK(s.sequential);
    CHECK(s.clearance_radius == 35.0);
    CHECK(s.clearance_height == 18.0);
    CHECK(s.placement_spacing == 8.0);
    CHECK(s.gcode_between_objects == ";next [current_object_idx]");
    const Settings again = parse_settings(settings_json(s));
    CHECK(again.sequential);
    CHECK(again.clearance_radius == 35.0);
    CHECK(again.placement_spacing == 8.0);
    CHECK(again.gcode_between_objects == s.gcode_between_objects);
    CHECK_THROWS_AS(parse_settings(R"({"sequential": {"clearance_radius": -1}})"), SettingsError);
}
