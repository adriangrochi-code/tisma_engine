#include "config/settings.hpp"
#include "gcode/check.hpp"
#include "gcode/writer.hpp"
#include "mesh/indexed_mesh.hpp"
#include "slice/pipeline.hpp"
#include "test_printer.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <map>
#include <numbers>
#include <set>
#include <sstream>
#include <string>

using namespace tisma;
using Catch::Matchers::WithinAbs;

namespace {

// A cylinder of radius r around (100, 100) from z 0 to h, n sides.
IndexedMesh cylinder(double r, double h, int n) {
    IndexedMesh m;
    for (int ring = 0; ring < 2; ++ring)
        for (int i = 0; i < n; ++i) {
            const double a = 2.0 * std::numbers::pi * i / n;
            m.vertices.push_back({float(100.0 + r * std::cos(a)), float(100.0 + r * std::sin(a)), float(ring * h)});
        }
    m.vertices.push_back({100.0f, 100.0f, 0.0f});
    m.vertices.push_back({100.0f, 100.0f, float(h)});
    const std::uint32_t bottom = std::uint32_t(2 * n), top = bottom + 1;
    for (int i = 0; i < n; ++i) {
        const std::uint32_t a = std::uint32_t(i), b = std::uint32_t((i + 1) % n);
        m.triangles.push_back({a, b, b + std::uint32_t(n)});
        m.triangles.push_back({a, b + std::uint32_t(n), a + std::uint32_t(n)});
        m.triangles.push_back({bottom, b, a});
        m.triangles.push_back({top, a + std::uint32_t(n), b + std::uint32_t(n)});
    }
    return m;
}

IndexedMesh box(double x, double y, double z) {
    IndexedMesh m;
    for (int k = 0; k < 8; ++k)
        m.vertices.push_back({float(100 - x / 2 + x * (k & 1)), float(100 - y / 2 + y * ((k >> 1) & 1)), float(z * ((k >> 2) & 1))});
    const std::uint32_t faces[6][4] = {{0, 2, 3, 1}, {4, 5, 7, 6}, {0, 1, 5, 4}, {2, 6, 7, 3}, {0, 4, 6, 2}, {1, 3, 7, 5}};
    for (const auto& f : faces) {
        m.triangles.push_back({f[0], f[1], f[2]});
        m.triangles.push_back({f[0], f[2], f[3]});
    }
    return m;
}

// Per layer of the G-code: its types, travels, the extrusions' Z range, the
// path laid, the filament per mm and the first and last point extruded.
struct LayerInfo {
    std::set<std::string> types;
    int travels = 0, retractions = 0;
    double z_min = 1e9, z_max = -1e9, length = 0.0, filament = 0.0;
    double min_per_mm = 1e9, max_per_mm = 0.0;
    double first_x = 0, first_y = 0, last_x = 0, last_y = 0;
    bool any = false;
};
std::map<int, LayerInfo> layers_of(const std::string& gcode) {
    std::map<int, LayerInfo> out;
    std::istringstream in(gcode);
    std::string line, type;
    int layer = -1;
    double x = 0, y = 0, z = 0, e = 0;
    while (std::getline(in, line)) {
        if (line.rfind(";LAYER_CHANGE", 0) == 0) {
            ++layer;
            continue;
        }
        if (line.rfind(";TYPE:", 0) == 0) {
            type = line.substr(6);
            continue;
        }
        if (line.rfind("G92", 0) == 0) {
            if (const auto k = line.find('E'); k != std::string::npos)
                e = std::stod(line.substr(k + 1));
            continue;
        }
        if (line.rfind("G1", 0) != 0)
            continue;
        double nx = x, ny = y, nz = z, ne = e;
        bool moves = false, extrudes = false;
        std::istringstream words(line.substr(2));
        std::string w;
        while (words >> w) {
            if (w[0] == ';')
                break;
            const double v = std::stod(w.substr(1));
            if (w[0] == 'X')
                nx = v, moves = true;
            else if (w[0] == 'Y')
                ny = v, moves = true;
            else if (w[0] == 'Z')
                nz = v;
            else if (w[0] == 'E')
                ne = v, extrudes = true;
        }
        LayerInfo& info = out[layer];
        const double d = std::hypot(nx - x, ny - y);
        if (moves && extrudes && ne > e) {
            info.types.insert(type);
            if (!info.any)
                info.first_x = x, info.first_y = y, info.any = true;
            info.last_x = nx, info.last_y = ny;
            info.z_min = std::min({info.z_min, z, nz});
            info.z_max = std::max({info.z_max, z, nz});
            info.length += d;
            info.filament += ne - e;
            if (d > 0.5) {
                info.min_per_mm = std::min(info.min_per_mm, (ne - e) / d);
                info.max_per_mm = std::max(info.max_per_mm, (ne - e) / d);
            }
        } else if (moves && !extrudes) {
            ++info.travels;
        } else if (!moves && extrudes && ne < e) {
            ++info.retractions;
        }
        x = nx, y = ny, z = nz, e = ne;
    }
    return out;
}

std::string vase_gcode(const IndexedMesh& m, Settings s) {
    const SlicedModel sliced = slice_model(m, s);
    std::ostringstream out;
    write_gcode(out, sliced, s);
    const GcodeCheck check = check_gcode(out.str(), gcode_limits(sliced, s));
    INFO((check.problems.empty() ? std::string() : check.problems.front().message));
    CHECK(check.ok());
    return out.str();
}

Settings vase_settings() {
    Settings s = test_printer();
    s.vase = true;
    s.skirt_loops = 0;
    return s;
}

} // namespace

TEST_CASE("vase mode: one wall, flat bottom layers, the last of them the top (VA1)", "[vase]") {
    Settings s = vase_settings();
    s.wall_count = 3;
    s.infill_density = 20.0;
    s.top_layers = 3;
    s.bottom_layers = 3;
    const auto layers = layers_of(vase_gcode(cylinder(10.0, 5.0, 64), s));
    REQUIRE(layers.size() == 25);
    CHECK(layers.at(0).types == std::set<std::string>{"External perimeter", "Solid infill"});
    CHECK(layers.at(1).types == std::set<std::string>{"External perimeter", "Solid infill"});
    CHECK(layers.at(2).types == std::set<std::string>{"External perimeter", "Top solid infill"});
    for (int l = 3; l < 25; ++l) {
        CAPTURE(l);
        CHECK(layers.at(l).types == std::set<std::string>{"External perimeter"});
    }
}

TEST_CASE("vase mode: each turn rises a layer from where the one below ended, flow unchanged (VA2)", "[vase]") {
    Settings s = vase_settings();
    s.bottom_layers = 3;
    const auto layers = layers_of(vase_gcode(cylinder(10.0, 5.0, 64), s));
    for (int l = 3; l < 25; ++l) {
        CAPTURE(l);
        const LayerInfo& info = layers.at(l);
        const double top = 0.2 * (l + 1);
        CHECK_THAT(info.z_min, WithinAbs(top - 0.2, 1e-3));
        CHECK_THAT(info.z_max, WithinAbs(top, 1e-3));
        // A full turn round the wall, starting and ending at the same place.
        CHECK(info.length > 2.0 * std::numbers::pi * 9.7);
        CHECK_THAT(info.first_x, WithinAbs(info.last_x, 1e-3));
        CHECK_THAT(info.first_y, WithinAbs(info.last_y, 1e-3));
        CHECK(info.retractions == 0);
        // Flow of a whole layer all the way, first and last turn too.
        CHECK(info.max_per_mm < info.min_per_mm * 1.02);
        if (l > 3) {
            CHECK(info.travels == 0); // it goes on from the turn below
            CHECK_THAT(info.first_x, WithinAbs(layers.at(l - 1).last_x, 1e-3));
            CHECK_THAT(info.first_y, WithinAbs(layers.at(l - 1).last_y, 1e-3));
            CHECK_THAT(info.min_per_mm, WithinAbs(layers.at(3).min_per_mm, 1e-3));
        }
    }
}

TEST_CASE("vase mode: without bottom layers, the first layer is a flat turn (VA3)", "[vase]") {
    Settings s = vase_settings();
    s.bottom_layers = 0;
    auto layers = layers_of(vase_gcode(box(20.0, 20.0, 10.0), s));
    CHECK(layers.at(0).types == std::set<std::string>{"External perimeter"});
    CHECK_THAT(layers.at(0).z_min, WithinAbs(0.2, 1e-3));
    CHECK_THAT(layers.at(0).z_max, WithinAbs(0.2, 1e-3));
    CHECK_THAT(layers.at(1).z_min, WithinAbs(0.2, 1e-3));
    CHECK_THAT(layers.at(1).z_max, WithinAbs(0.4, 1e-3));
    s.bottom_layers = 2;
    layers = layers_of(vase_gcode(box(20.0, 20.0, 10.0), s));
    CHECK(layers.at(0).types.contains("Solid infill"));
    CHECK(layers.at(1).types.contains("Top solid infill"));
    CHECK_THAT(layers.at(1).z_max, WithinAbs(0.4, 1e-3));
    CHECK_THAT(layers.at(2).z_min, WithinAbs(0.4, 1e-3));
    CHECK_THAT(layers.at(2).z_max, WithinAbs(0.6, 1e-3));
}

TEST_CASE("vase mode: not with supports (VA4)", "[vase]") {
    Settings s = vase_settings();
    s.support = true;
    CHECK_THROWS_AS(check_settings(s), SettingsError);
    s.support = false;
    CHECK_NOTHROW(check_settings(s));
    // Read and written back.
    CHECK(parse_settings(settings_json(s)).vase);
    CHECK_FALSE(parse_settings("{}").vase);
}
