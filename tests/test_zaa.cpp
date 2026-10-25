#include "slice/zaa.hpp"

#include "gcode/check.hpp"
#include "gcode/preview.hpp"
#include "gcode/writer.hpp"
#include "slice/pipeline.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <cstdint>
#include <numbers>
#include <sstream>
#include <string>

using namespace tisma;
using Catch::Matchers::WithinAbs;

namespace {

void quad(IndexedMesh& m, std::uint32_t a, std::uint32_t b, std::uint32_t c, std::uint32_t d) {
    m.triangles.push_back({a, b, c});
    m.triangles.push_back({a, c, d});
}

// The roofs of the spec's tests (docs/spec/features/z_antialiasing.md): a
// prism on X 90..110, Y 90..110, 3 mm high at X 90, its roof rising towards
// +X by `degrees`, so the surface is at 3 + (X - 90) tan(degrees).
IndexedMesh roof(double degrees) {
    IndexedMesh m;
    const float h1 = float(3.0 + 20.0 * std::tan(degrees * std::numbers::pi / 180.0));
    m.vertices = {{90, 90, 0}, {110, 90, 0}, {110, 110, 0}, {90, 110, 0},
                  {90, 90, 3}, {110, 90, h1}, {110, 110, h1}, {90, 110, 3}};
    quad(m, 0, 3, 2, 1);
    quad(m, 4, 5, 6, 7);
    for (std::uint32_t i = 0; i < 4; ++i)
        quad(m, i, (i + 1) % 4, 4 + (i + 1) % 4, 4 + i);
    return m;
}

double surface_at(double x, double degrees) { return 3.0 + (x - 90.0) * std::tan(degrees * std::numbers::pi / 180.0); }

// The base settings of the spec's tests: 0.4 nozzle, 0.2 layers, two walls,
// three solid layers, rectilinear 20 %, no skirt or brim, absolute E.
Settings base() {
    Settings s;
    s.gpu = GpuMode::Off;
    s.nozzle_diameter = 0.4;
    s.layer_height = 0.2;
    s.first_layer_height = 0.2;
    s.wall_count = 2;
    s.top_layers = 3;
    s.bottom_layers = 3;
    s.infill_density = 20.0;
    s.infill_pattern = InfillPattern::Rectilinear;
    s.skirt_loops = 0;
    s.brim_width = 0.0;
    s.relative_extrusion = false;
    s.min_layer_time = 0.0;
    return s;
}

// Every extrusion of the G-code: its type, where it ends, its layer and its
// filament per millimetre over what a whole layer's bead would take.
struct Extrusion {
    std::string type;
    double x0, y0, z0, x, y, z, layer_z, height, share;
};

std::vector<Extrusion> extrusions(const std::string& gcode, double filament_diameter) {
    std::vector<Extrusion> out;
    const double area = std::numbers::pi * filament_diameter * filament_diameter / 4.0;
    double x = 0, y = 0, z = 0, e = 0, layer_z = 0, height = 0, width = 0;
    std::string type;
    std::istringstream in(gcode);
    for (std::string line; std::getline(in, line);) {
        if (line.rfind(";Z:", 0) == 0)
            layer_z = std::stod(line.substr(3));
        else if (line.rfind(";HEIGHT:", 0) == 0)
            height = std::stod(line.substr(8));
        else if (line.rfind(";WIDTH:", 0) == 0)
            width = std::stod(line.substr(7));
        else if (line.rfind(";TYPE:", 0) == 0)
            type = line.substr(6);
        else if (line.rfind("G1 ", 0) == 0 || line.rfind("G92 ", 0) == 0) {
            double nx = x, ny = y, nz = z, ne = e;
            std::istringstream words(line.substr(line.find(' ') + 1));
            for (std::string w; words >> w;) {
                const double v = std::stod(w.substr(1));
                switch (w[0]) {
                case 'X': nx = v; break;
                case 'Y': ny = v; break;
                case 'Z': nz = v; break;
                case 'E': ne = v; break;
                default: break;
                }
            }
            const double length = std::hypot(nx - x, ny - y);
            if (line[1] == '1' && ne > e + 1e-9 && length > 1e-9)
                out.push_back({type, x, y, z, nx, ny, nz, layer_z, height,
                               (ne - e) * area / (length * width * height)});
            x = nx, y = ny, z = nz, e = ne;
        }
    }
    return out;
}

} // namespace

TEST_CASE("Z anti-aliasing: the surface over a point", "[zaa]") {
    const SurfaceIndex index(roof(5.0));
    CHECK_THAT(index.surface(100, 100, 0, 10, 4), WithinAbs(surface_at(100, 5.0), 1e-5));
    CHECK_THAT(index.surface(90.5, 109.5, 0, 10, 4), WithinAbs(surface_at(90.5, 5.0), 1e-5));
    CHECK(std::isnan(index.surface(100, 100, 0, 3.5, 4)));  // the roof is above the range
    CHECK(std::isnan(index.surface(120, 100, 0, 10, 4)));   // off the part
    CHECK(std::isnan(index.surface(100, 100, -1, 0.5, 0))); // the bottom faces down
}

TEST_CASE("Z anti-aliasing: a path follows the surface within its layer", "[zaa]") {
    const SurfaceIndex index(roof(5.0));
    ZaaParams params; // the spec's 25 %: never more than 0.15 mm below a 0.2 mm layer's top
    // Along Y at X 91.78, in the layer from 3.0 to 3.2: the spec's example, the
    // surface at 3.156.
    Toolpath along_y;
    along_y.points = {{{91.78, 95}, 0.45}, {{91.78, 105}, 0.45}};
    const Toolpath a = antialias_path(along_y, index, 3.0, 3.2, params);
    REQUIRE(a.points.size() > 2); // split where it leaves the top
    for (const ToolpathPoint& p : a.points)
        CHECK_THAT(3.2 + p.dz, WithinAbs(surface_at(91.78, 5.0), 1e-5));

    // Along X, up the slope: down to the floor at the low end, the surface in
    // between, the top where the surface is above it.
    Toolpath along_x;
    along_x.points = {{{90.2, 100}, 0.45}, {{96, 100}, 0.45}};
    const Toolpath b = antialias_path(along_x, index, 3.0, 3.2, params);
    for (const ToolpathPoint& p : b.points) {
        const double s = surface_at(p.p.x, 5.0);
        CHECK_THAT(3.2 + p.dz, WithinAbs(s >= 3.2 ? 3.2 : std::max(s, 3.05), 1e-5));
    }
    CHECK(b.points.front().dz < -0.149);
    CHECK(b.points.back().dz == 0.0);

    // Raised (Song et al. 2017): up to the surface above the top, half a layer at most.
    params.raise = true;
    const Toolpath c = antialias_path(along_x, index, 3.0, 3.2, params);
    for (const ToolpathPoint& p : c.points) {
        const double s = surface_at(p.p.x, 5.0);
        const double expected = s > 3.3 ? 3.2 : std::max(s, 3.05);
        CHECK_THAT(3.2 + p.dz, WithinAbs(expected, 1e-5));
    }

    // A path where the surface is nowhere in the layer stays as it was.
    Toolpath low;
    low.points = {{{100, 95}, 0.45}, {{100, 105}, 0.45}};
    const Toolpath d = antialias_path(low, index, 1.0, 1.2, params);
    REQUIRE(d.points.size() == 2);
    CHECK(d.points[0].dz == 0.0);
    CHECK(d.points[1].dz == 0.0);
}

TEST_CASE("Z anti-aliasing: the spec's roofs, sliced to checked G-code", "[zaa]") {
    for (const double degrees : {5.0, 10.0, 15.0}) {
        INFO("roof at " << degrees << " degrees");
        const IndexedMesh m = roof(degrees);
        Settings on = base();
        on.zaa = true;
        const SlicedModel sliced = slice_model(m, on);
        CHECK(sliced.zaa_points > 0);
        std::ostringstream gcode;
        write_gcode(gcode, sliced, on);
        const GcodeCheck check = check_gcode(gcode.str(), gcode_limits(sliced, on));
        INFO((check.problems.empty() ? std::string() : check.problems.front().message));
        CHECK(check.ok());

        // Z1: where the surface falls within the layer, the visible extrusions
        // go to max(surface, top - 0.15), with the material of what is left.
        std::size_t lowered = 0, kept = 0;
        for (const Extrusion& x : extrusions(gcode.str(), on.filament_diameter)) {
            const double bottom = x.layer_z - x.height;
            const bool visible = x.type == "External perimeter" || x.type == "Perimeter" ||
                                 x.type == "Top solid infill" || x.type == "Gap fill";
            const double s = surface_at(x.x, degrees);
            if (!visible) {
                CHECK_THAT(x.z, WithinAbs(x.layer_z, 1e-9)); // the infill keeps the layer's Z
                continue;
            }
            if (x.x < 90.0 || x.x > 110.0 || s <= bottom || s >= x.layer_z) {
                kept += std::abs(x.z - x.layer_z) < 1e-9;
                continue;
            }
            ++lowered;
            const double expected = std::max(s, x.layer_z - 0.75 * x.height);
            CHECK_THAT(x.z, WithinAbs(expected, 6e-4));
            // The material: what is left under the bead's two ends, over a whole layer.
            CHECK_THAT(x.share, WithinAbs(((x.z0 + x.z) / 2.0 - bottom) / x.height, 0.03));
        }
        CHECK(lowered > 50);
        CHECK(kept > 50);

        // The preview draws a lowered bead as thick as what is left under it.
        const GcodePreview preview = read_gcode_preview(gcode.str());
        std::size_t thinner = 0;
        for (const std::uint32_t i : preview.extrusions) {
            const PreviewMove& mv = preview.moves[i];
            const PreviewLayer& layer = preview.layers[mv.layer];
            if (mv.z0 < layer.z - 1e-3 && mv.z1 < layer.z - 1e-3) {
                const double left = (double(mv.z0) + double(mv.z1)) / 2.0 - (layer.z - layer.height);
                CHECK_THAT(double(mv.height), WithinAbs(left, 1e-3));
                ++thinner;
            }
        }
        CHECK(thinner > 50);
    }

    // Z2: without Z anti-aliasing no extrusion leaves its layer's Z.
    const Settings off = base();
    const SlicedModel flat = slice_model(roof(5.0), off);
    CHECK(flat.zaa_points == 0);
    std::ostringstream gcode;
    write_gcode(gcode, flat, off);
    for (const Extrusion& x : extrusions(gcode.str(), off.filament_diameter))
        CHECK_THAT(x.z, WithinAbs(x.layer_z, 1e-9));
}

TEST_CASE("Z anti-aliasing: raised, with layer heights by zone and by slope", "[zaa]") {
    const IndexedMesh m = roof(8.0);
    Settings s = base();
    s.zaa = true;
    s.zaa_raise = true;
    s.layer_height = 0.3;
    s.detail_height = 0.1;
    s.cusp_height = 0.05;
    s.relative_extrusion = true;
    const SlicedModel sliced = slice_model(m, s);
    REQUIRE(sliced.zaa_points > 0);
    std::ostringstream gcode;
    write_gcode(gcode, sliced, s);
    const GcodeCheck check = check_gcode(gcode.str(), gcode_limits(sliced, s));
    INFO((check.problems.empty() ? std::string() : check.problems.front().message));
    CHECK(check.ok());
    bool above = false;
    for (const Extrusion& x : extrusions(gcode.str(), s.filament_diameter)) {
        CHECK(x.z <= x.layer_z + 0.5 * x.height + 6e-4);
        CHECK(x.z >= x.layer_z - 0.75 * x.height - 6e-4);
        above = above || x.z > x.layer_z + 1e-6;
    }
    CHECK(above);

}

TEST_CASE("Z anti-aliasing: lower beads first and the nozzle never beside a higher one", "[zaa]") {
    // Song et al. 2017, Section 4: a bead laid lower than a neighbour laid
    // before it, within the nozzle's reach, gets the nozzle's flat end over
    // that neighbour. With raised and lowered paths, walls and fills alike.
    const IndexedMesh m = roof(8.0);
    Settings s = base();
    s.zaa = true;
    s.zaa_raise = true;
    const SlicedModel sliced = slice_model(m, s);
    std::ostringstream gcode;
    write_gcode(gcode, sliced, s);
    struct Laid {
        double x, y, z;
    };
    std::vector<Laid> layer;
    double under = 0.0, total = 0.0, layer_z = -1.0;
    for (const Extrusion& x : extrusions(gcode.str(), s.filament_diameter)) {
        if (x.layer_z != layer_z)
            layer.clear(), layer_z = x.layer_z;
        const double length = std::hypot(x.x - x.x0, x.y - x.y0);
        total += length;
        for (const Laid& q : layer) {
            const double d2 = (q.x - x.x) * (q.x - x.x) + (q.y - x.y) * (q.y - x.y);
            if (q.z > x.z + 0.01 && d2 < 0.7 * 0.7 && d2 > 0.2 * 0.2) {
                under += length;
                break;
            }
        }
        layer.push_back({x.x, x.y, x.z});
    }
    REQUIRE(total > 0.0);
    // 7 % laid in the order of the kinds, 1.0 % with the height graph: what
    // is left are paths whose order with a neighbour turns along them, which
    // the article splits where it turns (docs/ZAA.md).
    CHECK(under / total < 0.015);
}
