#include "slice/variable_layers.hpp"

#include "gcode/check.hpp"
#include "gcode/writer.hpp"
#include "slice/pipeline.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <array>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <sstream>

using namespace tisma;
using Catch::Matchers::WithinAbs;

namespace {

void quad(IndexedMesh& m, std::uint32_t a, std::uint32_t b, std::uint32_t c, std::uint32_t d) {
    m.triangles.push_back({a, b, c});
    m.triangles.push_back({a, c, d});
}

// A prism on (x0, y0)-(x1, y1): h0 high at x0, its roof sloping up towards
// x1 by `degrees` (one closed solid: walls, a flat bottom and a sloping roof).
IndexedMesh wedge(float x0, float y0, float x1, float y1, float h0, float degrees) {
    IndexedMesh m;
    const float h1 = h0 + (x1 - x0) * float(std::tan(degrees * std::numbers::pi / 180.0));
    m.vertices = {{x0, y0, 0}, {x1, y0, 0}, {x1, y1, 0}, {x0, y1, 0}, {x0, y0, h0}, {x1, y0, h1}, {x1, y1, h1}, {x0, y1, h0}};
    quad(m, 0, 3, 2, 1);
    quad(m, 4, 5, 6, 7);
    for (std::uint32_t i = 0; i < 4; ++i)
        quad(m, i, (i + 1) % 4, 4 + (i + 1) % 4, 4 + i);
    return m;
}

// A dome on a column, as one closed mesh: a cylinder of radius r up to
// `column`, then a half sphere on it.
IndexedMesh dome_on_column(float r, float column, int segments = 48, int rings = 24) {
    IndexedMesh m;
    m.vertices.push_back({0, 0, 0}); // the bottom's centre
    std::vector<std::pair<float, float>> profile{{r, 0.0f}, {r, column}};
    for (int i = 1; i < rings; ++i) {
        const double a = 0.5 * std::numbers::pi * i / rings;
        profile.push_back({float(r * std::cos(a)), float(column + r * std::sin(a))});
    }
    for (const auto& [radius, z] : profile)
        for (int s = 0; s < segments; ++s) {
            const double a = 2.0 * std::numbers::pi * s / segments;
            m.vertices.push_back({float(radius * std::cos(a)), float(radius * std::sin(a)), z});
        }
    const auto top = std::uint32_t(m.vertices.size());
    m.vertices.push_back({0, 0, column + r});
    const auto at = [&](std::size_t ring, int s) { return std::uint32_t(1 + ring * std::size_t(segments) + std::size_t(s % segments)); };
    for (int s = 0; s < segments; ++s)
        m.triangles.push_back({0, at(0, s + 1), at(0, s)});
    for (std::size_t ring = 0; ring + 1 < profile.size(); ++ring)
        for (int s = 0; s < segments; ++s)
            quad(m, at(ring, s), at(ring, s + 1), at(ring + 1, s + 1), at(ring + 1, s));
    for (int s = 0; s < segments; ++s)
        m.triangles.push_back({at(profile.size() - 1, s), at(profile.size() - 1, s + 1), top});
    return m;
}

IndexedMesh box(float x, float y, float h) { return wedge(0, 0, x, y, h, 0.0f); }

bool has_boundary(const std::vector<Layer>& layers, double z) {
    for (const Layer& l : layers)
        if (std::abs(l.bottom - z) < 1e-6)
            return true;
    return false;
}

double extruded_volume(const std::string& gcode, double filament_diameter) {
    double e = 0.0;
    std::istringstream in(gcode);
    for (std::string line; std::getline(in, line);) {
        if (line.rfind("G1 ", 0) != 0)
            continue;
        if (const std::size_t at = line.find(" E"); at != std::string::npos)
            e += std::stod(line.substr(at + 2));
    }
    return e * std::numbers::pi * filament_diameter * filament_diameter / 4.0;
}

} // namespace

TEST_CASE("variable layers: the list of (z, height) steps", "[variable_layers]") {
    Settings s;
    s.layer_height = 0.2;
    s.first_layer_height = 0.2;
    const IndexedMesh m = box(10, 10, 5);

    SECTION("each height from its z up") {
        s.layer_heights = {{2.0, 0.1}, {3.0, 0.3}};
        const LayerStack stack = plan_layer_stack(m, 0.0, 5.0, s);
        REQUIRE(stack.layers.size() > 10);
        CHECK(stack.groups.empty());
        for (const Layer& l : stack.layers) {
            const double h = l.top - l.bottom;
            const double expected = l.bottom < 2.0 - 1e-9 ? 0.2 : l.bottom < 3.0 - 1e-9 ? 0.1 : 0.3;
            CHECK_THAT(h, WithinAbs(expected, 1e-9));
        }
        CHECK(has_boundary(stack.layers, 2.0));
        CHECK(has_boundary(stack.layers, 3.0));
        CHECK(stack.layers.back().slice_z() < 5.0);
        CHECK(listed_height(s, 1.0) == 0.2);
        CHECK(listed_height(s, 2.5) == 0.1);
        CHECK(listed_height(s, 9.0) == 0.3);
    }

    SECTION("a step between two layers starts a layer of its own") {
        s.layer_heights = {{2.05, 0.1}};
        const LayerStack stack = plan_layer_stack(m, 0.0, 5.0, s);
        CHECK(has_boundary(stack.layers, 2.0));
        CHECK(has_boundary(stack.layers, 2.05)); // 0.05 between: a layer that thin fits
        CHECK(has_boundary(stack.layers, 2.15));
    }

    SECTION("within the machine's heights and never above the nozzle") {
        s.layer_heights = {{1.0, 0.6}, {3.0, 0.01}};
        LayerStack stack = plan_layer_stack(m, 0.0, 5.0, s);
        for (const Layer& l : stack.layers) {
            const double h = l.top - l.bottom;
            if (l.bottom > 1.0 + 1e-9 && l.top < 3.0 - 1e-9)
                CHECK_THAT(h, WithinAbs(0.4, 1e-9)); // the nozzle's 0.4
            if (l.bottom > 3.0 + 1e-9)
                CHECK_THAT(h, WithinAbs(0.04, 1e-9)); // the thinnest known
        }
        s.max_layer_height = 0.3;
        s.min_layer_height = 0.08;
        stack = plan_layer_stack(m, 0.0, 5.0, s);
        for (const Layer& l : stack.layers) {
            const double h = l.top - l.bottom;
            if (l.bottom > 1.0 + 1e-9 && l.top < 2.9)
                CHECK_THAT(h, WithinAbs(0.3, 1e-9));
            if (l.bottom > 3.0 + 1e-9)
                CHECK_THAT(h, WithinAbs(0.08, 1e-9));
        }
        const HeightBounds b = height_bounds(s);
        CHECK(b.min == 0.08);
        CHECK(b.max == 0.3);
        s.max_layer_height = 0.8; // past the nozzle: the nozzle
        CHECK(height_bounds(s).max == 0.4);
    }

    SECTION("without a list or a cusp height, the uniform stack") {
        const LayerStack stack = plan_layer_stack(m, 0.0, 5.0, s);
        const std::vector<Layer> uniform = plan_layers(0.0, 5.0, {0.2, 0.2});
        REQUIRE(stack.layers.size() == uniform.size());
        for (std::size_t i = 0; i < uniform.size(); ++i)
            CHECK(stack.layers[i].top == uniform[i].top);
        s.layer_height = 0.3, s.detail_height = 0.1;
        const LayerStack zones = plan_layer_stack(m, 0.0, 5.0, s);
        REQUIRE(!zones.groups.empty());
        CHECK(zones.groups.front() == std::array<std::size_t, 2>{1, 3});
        for (const auto& g : zones.groups)
            CHECK(g[1] - g[0] == 2);
    }
}

TEST_CASE("variable layers: adaptive by slope", "[variable_layers]") {
    Settings s;
    s.layer_height = 0.3;
    s.first_layer_height = 0.2;
    s.cusp_height = 0.05;
    // One solid: straight walls up to Z 3, then a roof at 10 degrees up to Z 6.53.
    const IndexedMesh m = wedge(-10, -10, 10, 10, 3, 10);
    const double roof_top = 3.0 + 20.0 * std::tan(10.0 * std::numbers::pi / 180.0);
    const double on_roof = 0.05 / std::cos(10.0 * std::numbers::pi / 180.0); // what the roof allows

    SECTION("the cusp height bounds the layer on the slope, not on the walls") {
        const SlopeLimit limit = slope_limit(m, s.cusp_height, 0.4);
        CHECK(limit.allowed(1.0, 1.3) > 0.4);
        CHECK_THAT(limit.allowed(4.0, 4.1), WithinAbs(on_roof, 1e-6));
        CHECK_THAT(limit.tallest(2.9, 0.3), WithinAbs(0.1, 1e-9)); // up to where the roof starts
        CHECK(limit.flats.size() == 1); // the bottom

        const LayerStack stack = plan_layer_stack(m, 0.0, roof_top, s);
        CHECK(stack.groups.empty());
        CHECK(has_boundary(stack.layers, 3.0));
        for (const Layer& l : stack.layers) {
            const double h = l.top - l.bottom;
            if (l.top <= 2.9 + 1e-9 && l.bottom > 0.0)
                CHECK_THAT(h, WithinAbs(0.3, 1e-9));
            if (l.bottom >= 3.0 - 1e-9)
                CHECK(h <= on_roof + 1e-6);
        }
    }

    SECTION("a flat face ends a layer") {
        const IndexedMesh b = box(10, 10, 5.13f);
        const LayerStack stack = plan_layer_stack(b, 0.0, 5.13, s);
        CHECK_THAT(stack.layers.back().top, WithinAbs(double(5.13f), 1e-9));
        const IndexedMesh low = box(10, 10, 5.27f); // 0.07 past a layer: that one stretches to it
        const LayerStack stretched = plan_layer_stack(low, 0.0, double(5.27f), s);
        CHECK_THAT(stretched.layers.back().top, WithinAbs(double(5.27f), 1e-9));
        CHECK(stretched.layers.back().top - stretched.layers.back().bottom <= 0.4);
    }

    SECTION("with layer heights by zone, the slope picks the sub-layers") {
        s.detail_height = 0.1;
        const LayerStack stack = plan_layer_stack(m, 0.0, roof_top, s);
        REQUIRE(!stack.groups.empty());
        for (const auto& g : stack.groups) {
            CHECK(g[1] - g[0] == 2); // three of 0.1: as fine as the detail height lets
            CHECK(stack.layers[g[0]].bottom >= 2.9 - 1e-9);
        }
        for (const Layer& l : stack.layers)
            if (l.top <= 2.9 + 1e-9 && l.bottom > 0.0)
                CHECK_THAT(l.top - l.bottom, WithinAbs(0.3, 1e-9)); // the walls: whole thick layers
    }
}

TEST_CASE("variable layers: the skins count in thick layers", "[variable_layers]") {
    const std::vector<double> share{1.0, 0.5, 0.5, 0.5, 0.5, 1.0, 1.0};
    const std::vector<int> up = skin_counts(share, 2, +1);
    CHECK(up == std::vector<int>{4, 4, 3, 3, 2, 2, 1});
    const std::vector<int> down = skin_counts(share, 2, -1);
    CHECK(down == std::vector<int>{1, 2, 3, 3, 4, 4, 3});
    CHECK(skin_counts(share, 0, +1) == std::vector<int>(7, 0));
}

TEST_CASE("variable layers: one complex part sliced to checked G-code", "[variable_layers]") {
    const IndexedMesh m = dome_on_column(10.0f, 6.0f);
    Settings base;
    base.gpu = GpuMode::Off;
    base.relative_extrusion = true;
    base.layer_height = 0.3;
    base.first_layer_height = 0.3;

    std::ostringstream ref;
    const SlicedModel uniform = slice_model(m, base);
    write_gcode(ref, uniform, base);
    const double volume = extruded_volume(ref.str(), base.filament_diameter);

    Settings listed = base;
    listed.layer_heights = {{6.0, 0.15}};
    Settings adaptive = base;
    adaptive.cusp_height = 0.05;
    Settings both = adaptive;
    both.detail_height = 0.1;
    for (const Settings* s : {&listed, &adaptive, &both}) {
        const SlicedModel model = slice_model(m, *s);
        std::ostringstream out;
        write_gcode(out, model, *s);
        const GcodeCheck check = check_gcode(out.str(), gcode_limits(model, *s));
        INFO((check.problems.empty() ? std::string() : check.problems.front().message));
        CHECK(check.ok());
        CHECK(model.layers.size() > uniform.layers.size());
        // The same part: the material within a few percent.
        CHECK_THAT(extruded_volume(out.str(), s->filament_diameter) / volume, WithinAbs(1.0, 0.06));
        for (const Layer& l : model.layers)
            if (l.top < 5.9 && l.bottom > 0.0)
                CHECK_THAT(l.top - l.bottom, WithinAbs(0.3, 1e-9)); // the column stays thick
    }
    const SlicedModel zoned = slice_model(m, both);
    REQUIRE(!zoned.groups.empty());
    std::size_t thick = 0;
    for (const ThickLayer& t : zoned.thick)
        thick += !t.empty();
    CHECK(thick > 0); // the dome's middle prints thick, its slope fine
}
