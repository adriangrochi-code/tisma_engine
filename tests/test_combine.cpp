#include "infill/combine.hpp"
#include "infill/rectilinear.hpp"
#include "infill/stress_density.hpp"

#include "config/analysis_case.hpp"
#include "fea/analysis.hpp"
#include "gcode/check.hpp"
#include "gcode/writer.hpp"
#include "slice/pipeline.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <cstdint>
#include <map>
#include <numbers>
#include <sstream>
#include <string>

using namespace tisma;
using Catch::Matchers::WithinAbs;
using Runs = std::vector<std::array<std::size_t, 2>>;

namespace {

IndexedMesh cube(float side) {
    IndexedMesh m;
    m.vertices = {{0, 0, 0}, {side, 0, 0}, {side, side, 0}, {0, side, 0},
                  {0, 0, side}, {side, 0, side}, {side, side, side}, {0, side, side}};
    auto quad = [&](std::uint32_t a, std::uint32_t b, std::uint32_t c, std::uint32_t d) {
        m.triangles.push_back({a, b, c});
        m.triangles.push_back({a, c, d});
    };
    quad(0, 3, 2, 1);
    quad(4, 5, 6, 7);
    for (std::uint32_t i = 0; i < 4; ++i)
        quad(i, (i + 1) % 4, 4 + (i + 1) % 4, 4 + i);
    return m;
}

// The spec's base settings (infill_patterns.md): 0.4 nozzle, 0.2 layers, two
// walls, three solid layers up and down, rectilinear 20 %, no skirt or brim.
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
    s.skirt_loops = 0;
    s.relative_extrusion = true;
    s.min_layer_time = 0.0;
    return s;
}

// Per layer (by its ;Z:), the sparse infill's height and filament per mm.
struct Sparse {
    double height = 0.0, filament = 0.0, length = 0.0;
    int long_lines = 0; // across the part, not the joins along its edge
};

std::map<long, Sparse> sparse_by_layer(const std::string& gcode) {
    std::map<long, Sparse> out;
    double x = 0, y = 0, layer_z = 0, height = 0;
    std::string type;
    std::istringstream in(gcode);
    for (std::string line; std::getline(in, line);) {
        if (line.rfind(";Z:", 0) == 0)
            layer_z = std::stod(line.substr(3));
        else if (line.rfind(";HEIGHT:", 0) == 0)
            height = std::stod(line.substr(8));
        else if (line.rfind(";TYPE:", 0) == 0)
            type = line.substr(6);
        else if (line.rfind("G1 ", 0) == 0) {
            double nx = x, ny = y, e = 0;
            std::istringstream words(line.substr(3));
            for (std::string w; words >> w;) {
                if (w[0] == 'X')
                    nx = std::stod(w.substr(1));
                else if (w[0] == 'Y')
                    ny = std::stod(w.substr(1));
                else if (w[0] == 'E')
                    e = std::stod(w.substr(1));
            }
            const double length = std::hypot(nx - x, ny - y);
            if (type == "Internal infill" && e > 0 && length > 0) {
                Sparse& s = out[std::lround(layer_z * 1000.0)];
                s.height = height;
                s.filament += e;
                s.length += length;
                s.long_lines += length > 8.0;
            }
            x = nx, y = ny;
        }
    }
    return out;
}

double minutes(const std::string& gcode) {
    const std::size_t at = gcode.find("; estimated printing time = ");
    REQUIRE(at != std::string::npos);
    double total = 0.0, value = 0.0;
    for (std::size_t i = at + 28; i < gcode.size() && gcode[i] != '\n'; ++i) {
        const char c = gcode[i];
        if (c >= '0' && c <= '9')
            value = value * 10 + (c - '0');
        else if (c == 'd' || c == 'h' || c == 'm' || c == 's') {
            total += value * (c == 'd' ? 1440.0 : c == 'h' ? 60.0 : c == 'm' ? 1.0 : 1.0 / 60.0);
            value = 0.0;
        }
    }
    return total;
}

} // namespace

TEST_CASE("combined infill: the runs of layers", "[combine]") {
    std::vector<Layer> layers;
    for (int i = 0; i < 10; ++i)
        layers.push_back({0.2 * i, 0.2 * (i + 1)});
    Settings s = base();
    CHECK(combine_runs(layers, s).empty()); // 1: each layer its own
    s.infill_combine_layers = 2;
    CHECK(combine_runs(layers, s) == Runs{{1, 2}, {3, 4}, {5, 6}, {7, 8}});
    s.infill_combine_layers = 3; // 0.6 mm would pass the 0.4 nozzle (rule 19): two
    CHECK(combine_runs(layers, s) == Runs{{1, 2}, {3, 4}, {5, 6}, {7, 8}});
    s.nozzle_diameter = 0.8;
    CHECK(combine_runs(layers, s) == Runs{{1, 3}, {4, 6}, {7, 9}});
    s.infill_combine_max_height = {0.5, false}; // mm: two
    CHECK(combine_runs(layers, s) == Runs{{1, 2}, {3, 4}, {5, 6}, {7, 8}});
    s.infill_combine_layers = 1;
    s.infill_combine_auto = true; // as many as fit in 0.5 mm
    CHECK(combine_runs(layers, s) == Runs{{1, 2}, {3, 4}, {5, 6}, {7, 8}});
    s.infill_combine_max_height = {100.0, true}; // the 0.8 nozzle: four
    CHECK(combine_runs(layers, s) == Runs{{1, 4}, {5, 8}});
    CHECK_THAT(combine_max_height(s), WithinAbs(0.8, 1e-12));
}

TEST_CASE("combined infill: the spec's cube (P11), sliced to checked G-code", "[combine]") {
    const IndexedMesh m = cube(20.0f);
    const Settings one = base();
    Settings two = base();
    two.infill_combine_layers = 2;
    std::ostringstream a, b;
    const SlicedModel sa = slice_model(m, one);
    const SlicedModel sb = slice_model(m, two);
    write_gcode(a, sa, one);
    write_gcode(b, sb, two);
    for (const auto* g : {&a, &b}) {
        const GcodeCheck check = check_gcode(g->str(), gcode_limits(g == &a ? sa : sb, g == &a ? one : two));
        INFO((check.problems.empty() ? std::string() : check.problems.front().message));
        CHECK(check.ok());
    }
    const std::map<long, Sparse> each = sparse_by_layer(a.str());
    const std::map<long, Sparse> combined = sparse_by_layer(b.str());
    // Sparse infill on the even layers from layer 4 (Z 1.0, 1.4, ...), 0.4 mm
    // high; layer 3 (Z 0.8) has none.
    CHECK(combined.count(800) == 0);
    REQUIRE(combined.count(1000) == 1);
    CHECK(combined.count(1200) == 0);
    REQUIRE(combined.count(1400) == 1);
    for (const auto& [z, s] : combined) {
        CHECK((z / 200) % 2 == 1); // Z 1.0, 1.4, ...: layers 4, 6, ...
        CHECK_THAT(s.height, WithinAbs(0.4, 1e-9));
    }
    // The filament per mm of a 0.4 mm layer: twice that of a 0.2 mm one.
    const Sparse& thin = each.at(1000);
    const Sparse& thick = combined.at(1000);
    CHECK_THAT((thick.filament / thick.length) / (thin.filament / thin.length), WithinAbs(2.0, 0.03));
    // The same infill in half the passes: less time.
    CHECK(minutes(b.str()) < minutes(a.str()));
}

namespace {

// Length of the lines of a fill by direction (degrees 0 to 180, to 1 degree).
std::map<int, double> by_direction(const std::vector<Toolpath>& paths) {
    std::map<int, double> out;
    for (const Toolpath& t : paths)
        for (std::size_t i = 1; i < t.points.size(); ++i) {
            const double dx = t.points[i].p.x - t.points[i - 1].p.x, dy = t.points[i].p.y - t.points[i - 1].p.y;
            const double length = std::hypot(dx, dy);
            if (length < 0.3)
                continue;
            double a = std::atan2(dy, dx) * 180.0 / std::numbers::pi;
            a = std::fmod(a + 360.0, 180.0);
            out[int(std::lround(a)) % 180] += length;
        }
    return out;
}

double total(const std::vector<Toolpath>& paths) {
    double sum = 0.0;
    for (const auto& [a, l] : by_direction(paths))
        sum += l;
    return sum;
}

LayerSurfaces square_sparse(double side) {
    LayerSurfaces s;
    const auto u = [](double v) { return to_units(v); };
    s.sparse = {{{u(0), u(0)}, {u(side), u(0)}, {u(side), u(side)}, {u(0), u(side)}}};
    return s;
}

} // namespace

TEST_CASE("infill patterns: triangles and cubic, three directions 60 degrees apart", "[combine]") {
    FillParams p;
    p.width = 0.4;
    p.density = 0.2;
    p.angle = 45.0;
    const LayerSurfaces region = square_sparse(20.0);
    p.pattern = InfillPattern::Triangles;
    const std::vector<Toolpath> t1 = fill_layer(region, 5, p, 1.0).sparse;
    const std::vector<Toolpath> t2 = fill_layer(region, 6, p, 1.2).sparse;
    const std::map<int, double> d = by_direction(t1);
    const double all = total(t1);
    for (const int a : {45, 105, 165})
        CHECK_THAT(d.count(a) ? d.at(a) / all : 0.0, WithinAbs(1.0 / 3.0, 0.05)); // each a third
    // The same in every layer, about the density's length: 20 % of 400 mm2 at 0.4 mm.
    CHECK_THAT(total(t2), WithinAbs(all, 1e-6));
    CHECK_THAT(all, WithinAbs(0.2 * 400.0 / 0.4, 30.0));

    p.pattern = InfillPattern::Cubic;
    const std::vector<Toolpath> c1 = fill_layer(region, 5, p, 1.0).sparse;
    const std::vector<Toolpath> c2 = fill_layer(region, 6, p, 1.2).sparse;
    const std::map<int, double> dc = by_direction(c1);
    for (const int a : {45, 105, 165})
        CHECK(dc.count(a) == 1);
    // The planes move with the height: other lines a layer up, the same ones a
    // whole period (spacing x sqrt 2) up.
    REQUIRE(!c1.empty());
    REQUIRE(!c2.empty());
    CHECK(std::abs(c1.front().points.front().p.x - c2.front().points.front().p.x) +
              std::abs(c1.front().points.front().p.y - c2.front().points.front().p.y) > 1e-3);
    const double period = 3.0 * 0.4 / 0.2 * std::numbers::sqrt2;
    const std::vector<Toolpath> c3 = fill_layer(region, 5, p, 1.0 + period).sparse;
    CHECK_THAT(total(c3), WithinAbs(total(c1), 0.05 * total(c1)));
}

TEST_CASE("dense infill under the top surfaces (D1), sliced to checked G-code", "[combine]") {
    // A cube of 20 x 20 x 10: the infill at 10 %, the layer right under the top
    // skin at 40 %, only there.
    IndexedMesh m = cube(20.0f);
    for (Vec3f& v : m.vertices)
        v.z = v.z > 0.0f ? 10.0f : 0.0f;
    Settings s = base();
    s.infill_density = 10.0;
    s.infill_dense_under_top = true;
    const SlicedModel sliced = slice_model(m, s);
    std::ostringstream g;
    write_gcode(g, sliced, s);
    const GcodeCheck check = check_gcode(g.str(), gcode_limits(sliced, s));
    INFO((check.problems.empty() ? std::string() : check.problems.front().message));
    CHECK(check.ok());
    const std::map<long, Sparse> sparse = sparse_by_layer(g.str());
    // Layers 0 to 49, 0.2 mm each: 47 to 49 are the top skin, 46 the dense one.
    REQUIRE(sparse.count(9400) == 1); // layer 46
    REQUIRE(sparse.count(9000) == 1); // layer 44, the same direction
    CHECK(sparse.count(9600) == 0);
    // Four times the lines across (spacing 1 mm instead of 4).
    CHECK_THAT(double(sparse.at(9400).long_lines) / sparse.at(9000).long_lines, WithinAbs(4.0, 0.6));
    CHECK(sparse.at(5000).long_lines == sparse.at(9000).long_lines); // the others alike
}

TEST_CASE("the as-printed analysis sees the infill printed in thick layers", "[combine]") {
    // A combined infill prints at the top of its run but fills every layer of it.
    const IndexedMesh m = cube(10.0f);
    const AnalysisCase c = parse_analysis_case(
        R"({"model": "as_printed", "quality": "normal", "resolution_check": false,
            "fixtures": [{"face": "bottom"}], "loads": [{"face": "top", "force": [0, 0, -100]}]})");
    Settings one = base();
    Settings two = base();
    two.infill_combine_layers = 2;
    const AnalysisEstimate a = estimate_analysis(m, one, c);
    const AnalysisEstimate b = estimate_analysis(m, two, c);
    REQUIRE(a.printed_voxels > 0);
    CHECK_THAT(double(b.printed_voxels) / double(a.printed_voxels), WithinAbs(1.0, 0.05));
    // And layer heights by zone, whose thick zone holds the walls and the infill.
    Settings zones = base();
    zones.layer_height = 0.3;
    zones.detail_height = 0.1;
    const AnalysisEstimate z = estimate_analysis(m, zones, c);
    CHECK_THAT(double(z.printed_voxels) / double(z.part_voxels), WithinAbs(double(a.printed_voxels) / double(a.part_voxels), 0.08));
}

TEST_CASE("stress-guided infill: denser where the part works hardest", "[combine]") {
    // A bar held at one end and pushed down at the other: the bending
    // moment, and the stress, are highest at the held end.
    IndexedMesh bar = cube(10.0f);
    for (Vec3f& v : bar.vertices)
        v.x *= 6.0f;
    Settings s = base();
    s.infill_density = 10.0;
    s.infill_stress_density = 50.0;
    const AnalysisCase c = parse_analysis_case(
        R"({"model": "solid", "quality": "normal", "resolution_check": false,
            "fixtures": [{"face": "x_min"}], "loads": [{"face": "x_max", "force": [0, 0, -20]}]})");
    const StressField field = stress_field(run_analysis(bar, s, c));
    REQUIRE_FALSE(field.empty());
    // At the held end's top and bottom (the most stretched and squeezed), loaded; at the free end, not.
    const Polygons near = loaded_region(field, 9.5, 0.6);
    REQUIRE_FALSE(near.empty());
    double x_min = 1e9, x_max = -1e9;
    for (const Polygon& p : near)
        for (const auto& q : p)
            x_min = std::min(x_min, to_mm(q.x)), x_max = std::max(x_max, to_mm(q.x));
    CHECK(x_min < 5.0);
    CHECK(x_max < 30.0);
    CHECK(loaded_region(field, 50.0, 0.1).empty()); // above the part

    const std::vector<DensityBand> bands = density_bands(0.1, 0.5);
    REQUIRE(bands.size() == 3);
    CHECK_THAT(bands.back().density, WithinAbs(0.5, 1e-12));
    CHECK(density_bands(0.3, 0.2).empty()); // not above the plain density: off

    // Sliced: more infill per area in the held third than in the free third.
    const SlicedModel guided = slice_model(bar, s, nullptr, &field);
    const SlicedModel even = slice_model(bar, s);
    auto sparse_length = [](const SlicedModel& m, double from, double to) {
        double sum = 0.0;
        for (const LayerFill& f : m.fills)
            for (const Toolpath& t : f.sparse)
                for (std::size_t i = 1; i < t.points.size(); ++i) {
                    const double mx = 0.5 * (t.points[i].p.x + t.points[i - 1].p.x);
                    if (mx >= from && mx < to)
                        sum += std::hypot(t.points[i].p.x - t.points[i - 1].p.x, t.points[i].p.y - t.points[i - 1].p.y);
                }
        return sum;
    };
    CHECK(sparse_length(guided, 0, 20) > 1.5 * sparse_length(guided, 40, 60));
    CHECK_THAT(sparse_length(even, 0, 20) / sparse_length(even, 40, 60), WithinAbs(1.0, 0.15));
    std::ostringstream g;
    write_gcode(g, guided, s);
    CHECK(check_gcode(g.str(), gcode_limits(guided, s)).ok());
}

TEST_CASE("infill patterns: aligned rectilinear and zigzag (P1, P2)", "[combine]") {
    FillParams p;
    p.width = 0.4;
    p.density = 0.2;
    p.angle = 45.0;
    const LayerSurfaces region = square_sparse(20.0);
    // Zigzag is rectilinear.
    p.pattern = InfillPattern::Rectilinear;
    const std::vector<Toolpath> plain = fill_layer(region, 5, p, 1.0).sparse;
    p.pattern = InfillPattern::Zigzag;
    const std::vector<Toolpath> zigzag = fill_layer(region, 5, p, 1.0).sparse;
    REQUIRE(zigzag.size() == plain.size());
    for (std::size_t i = 0; i < plain.size(); ++i) {
        REQUIRE(zigzag[i].points.size() == plain[i].points.size());
        for (std::size_t k = 0; k < plain[i].points.size(); ++k) {
            CHECK(zigzag[i].points[k].p.x == plain[i].points[k].p.x);
            CHECK(zigzag[i].points[k].p.y == plain[i].points[k].p.y);
        }
    }
    // Aligned: every layer mostly at the angle, odd ones too.
    p.pattern = InfillPattern::AlignedRectilinear;
    for (const std::size_t layer : {std::size_t(5), std::size_t(6)}) {
        const std::vector<Toolpath> lines = fill_layer(region, layer, p, 0.2 * double(layer)).sparse;
        const std::map<int, double> d = by_direction(lines);
        CHECK(d.count(45) == 1);
        CHECK((d.count(45) ? d.at(45) : 0.0) > 0.75 * total(lines));
    }
}

TEST_CASE("infill patterns: stars, the triangles' lines with one family half a spacing over (P4)", "[combine]") {
    FillParams p;
    p.width = 0.4;
    p.density = 0.2;
    p.angle = 45.0;
    const LayerSurfaces region = square_sparse(20.0);
    p.pattern = InfillPattern::Stars;
    const std::vector<Toolpath> stars = fill_layer(region, 5, p, 1.0).sparse;
    const std::map<int, double> d = by_direction(stars);
    const double all = total(stars);
    for (const int a : {45, 105, 165})
        CHECK_THAT(d.count(a) ? d.at(a) / all : 0.0, WithinAbs(1.0 / 3.0, 0.06));
    // The family at 105 degrees lies half a spacing from the triangles'; the
    // others are the triangles'.
    const double spacing = 3.0 * 0.4 / 0.2;
    const auto offsets = [&](const std::vector<Toolpath>& lines, int angle) {
        std::vector<double> out;
        const double a = angle * std::numbers::pi / 180.0;
        for (const Toolpath& t : lines) {
            const auto& s = t.points.front().p;
            const auto& e = t.points.back().p;
            const double dir = std::fmod(std::atan2(e.y - s.y, e.x - s.x) * 180.0 / std::numbers::pi + 360.0, 180.0);
            if (std::abs(dir - angle) < 0.5)
                out.push_back(std::fmod(-std::sin(a) * s.x + std::cos(a) * s.y + 100.0 * spacing, spacing));
        }
        return out;
    };
    p.pattern = InfillPattern::Triangles;
    const std::vector<Toolpath> triangles = fill_layer(region, 5, p, 1.0).sparse;
    for (const int angle : {45, 105, 165}) {
        const std::vector<double> ts = offsets(triangles, angle), ss = offsets(stars, angle);
        REQUIRE(!ts.empty());
        REQUIRE(!ss.empty());
        const double want = angle == 105 ? 0.5 * spacing : 0.0;
        double gap = std::fmod(ss.front() - ts.front() + 2.0 * spacing, spacing);
        gap = std::min(gap, spacing - gap);
        CHECK_THAT(gap, WithinAbs(want, 1e-3));
    }
}

TEST_CASE("a solid layer every N layers, printed as a bridge (P12)", "[combine]") {
    const IndexedMesh m = cube(20.0f);
    Settings s = base();
    s.infill_solid_every = 5;
    const SlicedModel sliced = slice_model(m, s);
    std::ostringstream out;
    write_gcode(out, sliced, s);
    const GcodeCheck check = check_gcode(out.str(), gcode_limits(sliced, s));
    INFO((check.problems.empty() ? std::string() : check.problems.front().message));
    CHECK(check.ok());
    REQUIRE(sliced.fills.size() == 100);
    // Layers 5, 10 ... 95 under the top shell: bridge lines, no sparse infill.
    for (std::size_t l = 5; l < 97; l += 5) {
        INFO("layer " << l);
        CHECK(sliced.fills[l].sparse.empty());
        CHECK(!sliced.fills[l].bridge.empty());
    }
    CHECK(!sliced.fills[6].sparse.empty());
    CHECK(sliced.fills[6].bridge.empty());
    CHECK(out.str().find(";TYPE:Bridge infill") != std::string::npos);
}

TEST_CASE("the minimum shell thickness adds solid layers (P13)", "[combine]") {
    const IndexedMesh m = cube(20.0f);
    Settings s = base();
    s.top_layers = 1;
    s.bottom_layers = 1;
    s.top_min_thickness = 1.0;
    s.bottom_min_thickness = 0.6;
    const SlicedModel sliced = slice_model(m, s);
    REQUIRE(sliced.surfaces.size() == 100);
    // Layers 0 to 2 solid (0.6 mm), 95 to 99 too (1 mm).
    for (const std::size_t l : {0, 1, 2, 95, 96, 97, 98, 99}) {
        INFO("layer " << l);
        CHECK(sliced.surfaces[l].sparse.empty());
    }
    CHECK(!sliced.surfaces[3].sparse.empty());
    CHECK(!sliced.surfaces[94].sparse.empty());
    // Without the minimum, one layer each.
    s.top_min_thickness = 0.0;
    s.bottom_min_thickness = 0.0;
    const SlicedModel plain = slice_model(m, s);
    CHECK(!plain.surfaces[1].sparse.empty());
    CHECK(!plain.surfaces[98].sparse.empty());
}
