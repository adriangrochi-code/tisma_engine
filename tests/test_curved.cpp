// Curved layers (docs/spec/features/nonplanar.md; tests NP1 to NP4).

#include "config/settings.hpp"
#include "gcode/check.hpp"
#include "gcode/writer.hpp"
#include "mesh/indexed_mesh.hpp"
#include "mesh/mesh.hpp"
#include "slice/curved.hpp"
#include "slice/pipeline.hpp"
#include "test_printer.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <algorithm>
#include <map>
#include <cmath>
#include <numbers>
#include <sstream>
#include <string>
#include <vector>

using namespace tisma;
using Catch::Matchers::ContainsSubstring;
using Catch::Matchers::WithinAbs;

namespace {

void add_box(TriangleMesh& m, float x0, float y0, float z0, float x1, float y1, float z1) {
    const Vec3f v[8] = {{x0, y0, z0}, {x1, y0, z0}, {x1, y1, z0}, {x0, y1, z0},
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
}

// The spec's block: 40 x 40 x 12 mm centred on X 100, Y 100.
IndexedMesh block() {
    TriangleMesh m;
    add_box(m, 80, 80, 0, 120, 120, 12);
    return weld_exact(m);
}

Settings base(CurvedPattern pattern = CurvedPattern::Egg) {
    Settings s = test_printer();
    s.flavor = GcodeFlavor::Marlin;
    s.relative_extrusion = true;
    s.bed = {{0, 0}, {250, 0}, {250, 210}, {0, 210}};
    s.layer_height = s.first_layer_height = 0.2;
    s.skirt_loops = 0;
    s.curved_mode = CurvedMode::Wave;
    s.curved_pattern = pattern;
    return s;
}

std::string gcode_of(const SlicedModel& model, const Settings& s) {
    std::ostringstream out;
    write_gcode(out, model, s);
    const GcodeCheck check = check_gcode(out.str(), gcode_limits(model, s));
    INFO((check.problems.empty() ? std::string()
                                 : std::to_string(check.problems.front().line) + ": " + check.problems.front().message));
    CHECK(check.ok());
    return out.str();
}

// The extruded moves of the layer at flat Z `z`: (x, y, z) of each end,
// the filament and the line type.
struct Move {
    double x0, y0, z0, x1, y1, z1, e;
    std::string type;
};

std::vector<Move> layer_moves(const std::string& text, const std::string& z) {
    std::vector<Move> out;
    std::istringstream in(text);
    std::string line, type;
    double x = 0, y = 0, zz = 0;
    bool inside = false;
    while (std::getline(in, line)) {
        if (line.rfind(";Z:", 0) == 0)
            inside = line.substr(3) == z;
        if (line.rfind(";TYPE:", 0) == 0)
            type = line.substr(6);
        if (line.rfind("G1 ", 0) != 0)
            continue;
        double nx = x, ny = y, nz = zz, e = 0.0;
        std::istringstream words(line.substr(3));
        for (std::string w; words >> w;) {
            if (w[0] == ';')
                break;
            const double v = std::atof(w.c_str() + 1);
            if (w[0] == 'X')
                nx = v;
            else if (w[0] == 'Y')
                ny = v;
            else if (w[0] == 'Z')
                nz = v;
            else if (w[0] == 'E')
                e = v;
        }
        if (inside && e > 0.0 && (nx != x || ny != y))
            out.push_back({x, y, zz, nx, ny, nz, e, type});
        x = nx, y = ny, zz = nz;
    }
    return out;
}

int layer_count(const std::string& text) {
    int n = 0;
    for (std::size_t at = text.find(";LAYER_CHANGE"); at != std::string::npos; at = text.find(";LAYER_CHANGE", at + 1))
        ++n;
    return n;
}

} // namespace

TEST_CASE("curved layers: the egg crate wave, fitted to the head (NP1)", "[curved]") {
    const Settings s = base();
    const std::optional<CurvedField> f = curved_field(s, block());
    REQUIRE(f);
    // λ = π · 3 / tan 20° = 25.894 mm; the amplitude as J ≤ 1.4 in the ramp lets it (0.952).
    CHECK_THAT(2.0 * std::numbers::pi / f->k, WithinAbs(25.894, 0.001));
    CHECK_THAT(f->amplitude, WithinAbs(0.952, 0.004));
    const SlicedModel model = slice_model(block(), s);
    const std::string text = gcode_of(model, s);
    Settings flat = s;
    flat.curved_mode = CurvedMode::Disabled;
    CHECK(layer_count(gcode_of(slice_model(block(), flat), flat)) == 60);
    CHECK(std::abs(layer_count(text) - 65) <= 1); // Tisma Slicer: 65
    // The first three layers flat.
    for (const char* z : {"0.200", "0.400", "0.600"}) {
        const std::vector<Move> moves = layer_moves(text, z);
        REQUIRE_FALSE(moves.empty());
        for (const Move& m : moves)
            CHECK_THAT(m.z1, WithinAbs(std::atof(z), 1e-6));
    }
    // Layer 41 (Z 8.2): every point on Z = 8.2 + a sin(k(X - 100)) sin(k(Y - 100)).
    const std::vector<Move> moves = layer_moves(text, "8.200");
    REQUIRE(moves.size() > 200);
    double worst = 0.0;
    for (const Move& m : moves)
        worst = std::max(worst, std::abs(m.z1 - (8.2 + f->amplitude * std::sin(f->k * (m.x1 - 100.0)) *
                                                          std::sin(f->k * (m.y1 - 100.0)))));
    CHECK(worst < 0.0015); // Tisma Slicer: 0.0022 at most
}

TEST_CASE("curved layers: ridges along X (NP2)", "[curved]") {
    const Settings s = base(CurvedPattern::Ridges);
    const std::optional<CurvedField> f = curved_field(s, block());
    REQUIRE(f);
    CHECK_THAT(f->amplitude, WithinAbs(0.952, 0.004));
    const std::string text = gcode_of(slice_model(block(), s), s);
    const std::vector<Move> moves = layer_moves(text, "8.200");
    REQUIRE(moves.size() > 200);
    double worst = 0.0;
    for (const Move& m : moves)
        worst = std::max(worst, std::abs(m.z1 - (8.2 + f->amplitude * std::sin(f->k * (m.x1 - 100.0)))));
    CHECK(worst < 0.0015); // Tisma Slicer: 0.0008
}

TEST_CASE("curved layers: the material by the local thickness J (NP3)", "[curved]") {
    Settings s = base();
    s.infill_density = 20.0;
    const std::optional<CurvedField> f = curved_field(s, block());
    REQUIRE(f);
    const std::string text = gcode_of(slice_model(block(), s), s);
    // Layer 11 (Z 2.2, in the ramp): filament per mm over the flat amount
    // by J, on the infill.
    const double area = std::numbers::pi * 0.25 * s.filament_diameter * s.filament_diameter;
    std::vector<double> ratio;
    double j_lo = 10.0, j_hi = 0.0;
    for (const Move& m : layer_moves(text, "2.200")) {
        if (m.type != "Internal infill")
            continue;
        const double length = std::hypot(m.x1 - m.x0, m.y1 - m.y0);
        if (length < 0.2)
            continue;
        const double j = f->thickness(0.5 * (m.x0 + m.x1), 0.5 * (m.y0 + m.y1), 0.5 * (m.z0 + m.z1));
        j_lo = std::min(j_lo, j), j_hi = std::max(j_hi, j);
        ratio.push_back(m.e / length / (s.wall_width * s.layer_height / area * j));
    }
    REQUIRE(ratio.size() > 100);
    std::sort(ratio.begin(), ratio.end());
    CHECK_THAT(ratio[ratio.size() / 2], WithinAbs(1.0, 0.01));
    CHECK(j_lo < 0.95); // Tisma Slicer: J from 0.82 to 1.36
    CHECK(j_hi > 1.05);
}

TEST_CASE("curved layers: need relative E, and only what is done (NP4)", "[curved]") {
    Settings s = base();
    s.relative_extrusion = false;
    CHECK_THROWS_WITH(check_settings(s), ContainsSubstring("relative E"));
    s.relative_extrusion = true;
    CHECK_NOTHROW(check_settings(s));
    s.curved_mode = CurvedMode::Stress;
    CHECK_THROWS_AS(check_settings(s), SettingsError);
    s.curved_mode = CurvedMode::Wave;
    s.zaa = true;
    CHECK_THROWS_AS(check_settings(s), SettingsError);
    // Settings read and written.
    const Settings back = parse_settings(R"({"machine": {"relative_extrusion": true},
        "curved": {"mode": "wave", "pattern": "twisted", "amplitude": 0.4, "fit_head": false, "flat_top": true},
        "head": {"free_angle_x": 30, "clearance_height": 4, "profile": "2:45;5:30"}})");
    CHECK(back.curved_pattern == CurvedPattern::Twisted);
    CHECK(back.curved_amplitude == 0.4);
    CHECK_FALSE(back.curved_fit_head);
    CHECK(back.head_free_angle_x == 30.0);
    CHECK(back.head_profile == std::vector<std::array<double, 2>>{{2.0, 45.0}, {5.0, 30.0}});
    const Settings again = parse_settings(settings_json(back));
    CHECK(again.curved_mode == CurvedMode::Wave);
    CHECK(again.head_profile == back.head_profile);
    CHECK(again.curved_flat_top);
}

TEST_CASE("curved layers: a cone, the twisted ridges, and what the head does not allow", "[curved]") {
    Settings s = base();
    s.curved_mode = CurvedMode::Conical;
    s.curved_fit_head = false;
    s.curved_cone_angle = 3.0; // steeper does not fit J on this block (10 degrees: J up to 7.8)
    s.curved_ramp_height = 8.0;
    const std::optional<CurvedField> f = curved_field(s, block());
    REQUIRE(f);
    CHECK_THAT(f->cone, WithinAbs(std::tan(3.0 * std::numbers::pi / 180.0), 1e-9));
    const std::string cone = gcode_of(slice_model(block(), s), s);
    const std::vector<Move> moves = layer_moves(cone, "11.000");
    REQUIRE_FALSE(moves.empty());
    for (const Move& m : moves) { // rises outwards
        const double r = std::hypot(m.x1 - 100.0, m.y1 - 100.0);
        CHECK_THAT(m.z1, WithinAbs(11.0 + f->cone * (std::sqrt(r * r + 4.0) - 2.0), 0.0015));
    }
    // Too steep for the head without fitting it: an error, not a crash.
    s.curved_cone_angle = 40.0;
    CHECK_THROWS_AS(curved_field(s, block()), CurvedError);
    // Twisted ridges slice and check.
    Settings t = base(CurvedPattern::Twisted);
    gcode_of(slice_model(block(), t), t);
    // A wave too deep for the head's clearance, not fitted: an error.
    Settings deep = base();
    deep.curved_fit_head = false;
    deep.curved_amplitude = 2.0;
    deep.curved_wavelength = 60.0;
    deep.curved_ramp_height = 12.0;
    CHECK_THROWS_WITH(curved_field(deep, block()), ContainsSubstring("clearance"));
}

TEST_CASE("curved layers: the mesh split to bend, still closed", "[curved]") {
    std::vector<std::uint32_t> origin;
    const IndexedMesh m = refined_mesh(block(), 1.0, 0.6, &origin);
    REQUIRE(origin.size() == m.triangles.size());
    CHECK(m.triangles.size() > 1000);
    // Each edge in two triangles, once each way.
    std::map<std::pair<std::uint32_t, std::uint32_t>, int> edges;
    for (const auto& t : m.triangles)
        for (int e = 0; e < 3; ++e)
            ++edges[{t[std::size_t(e)], t[std::size_t((e + 1) % 3)]}];
    for (const auto& [e, n] : edges) {
        CHECK(n == 1);
        CHECK(edges.count({e.second, e.first}) == 1);
    }
    // Children follow their parent's order.
    CHECK(std::is_sorted(origin.begin(), origin.end()));
}

namespace {

// The spec's block with a roof: 40 x 40 mm, its top rising along X from
// `low` to `high` (SU1 to SU5).
IndexedMesh roof_block(float low, float high) {
    TriangleMesh m;
    const Vec3f v[8] = {{80, 80, 0}, {120, 80, 0}, {120, 120, 0}, {80, 120, 0},
                        {80, 80, low}, {120, 80, high}, {120, 120, high}, {80, 120, low}};
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
    return weld_exact(m);
}

Settings surface_base() {
    Settings s = base();
    s.curved_mode = CurvedMode::Surface;
    s.infill_density = 20.0;
    s.wall_count = 2;
    s.top_layers = s.bottom_layers = 3;
    return s;
}

// The top face's error (SU1): in 1 x 1 mm cells, 2 mm in from the edges,
// the highest extruded Z less the model's height there; its standard
// deviation.
double top_error(const std::string& text, float low, float high) {
    std::map<std::pair<int, int>, double> highest;
    std::istringstream in(text);
    double x = 0, y = 0, z = 0;
    for (std::string line; std::getline(in, line);) {
        if (line.rfind("G1 ", 0) != 0)
            continue;
        double nx = x, ny = y, nz = z, e = 0.0;
        std::istringstream words(line.substr(3));
        for (std::string w; words >> w;) {
            if (w[0] == ';')
                break;
            const double v = std::atof(w.c_str() + 1);
            if (w[0] == 'X')
                nx = v;
            else if (w[0] == 'Y')
                ny = v;
            else if (w[0] == 'Z')
                nz = v;
            else if (w[0] == 'E')
                e = v;
        }
        // Along the whole extrusion, a point every 0.25 mm.
        const int pieces = e > 0.0 ? std::max(1, int(std::ceil(std::hypot(nx - x, ny - y) / 0.25))) : 0;
        for (int k = 1; k <= pieces; ++k) {
            const double px = x + (nx - x) * k / pieces, py = y + (ny - y) * k / pieces, pz = z + (nz - z) * k / pieces;
            if (px > 82 && px < 118 && py > 82 && py < 118) {
                auto [it, fresh] = highest.try_emplace({int(std::floor(px)), int(std::floor(py))}, pz);
                it->second = std::max(it->second, pz);
            }
        }
        x = nx, y = ny, z = nz;
    }
    double sum = 0, squares = 0;
    for (const auto& [cell, top] : highest) {
        const double model = low + (high - low) * (cell.first + 0.5 - 80.0) / 40.0;
        sum += top - model, squares += (top - model) * (top - model);
    }
    const double n = double(highest.size()), mean = sum / n;
    return std::sqrt(std::max(0.0, squares / n - mean * mean));
}

// A layer's slope along X: the least squares line through its extruded points.
double slope_x(const std::vector<Move>& moves) {
    double sx = 0, sz = 0, sxx = 0, sxz = 0, n = 0;
    for (const Move& m : moves)
        sx += m.x1, sz += m.z1, sxx += m.x1 * m.x1, sxz += m.x1 * m.z1, ++n;
    return (n * sxz - sx * sz) / (n * sxx - sx * sx);
}

} // namespace

TEST_CASE("curved layers: top faces, a flat top stays flat (CO1)", "[curved][surface]") {
    const Settings s = surface_base();
    CHECK_FALSE(curved_field(s, block()).has_value());
    CHECK(layer_count(gcode_of(slice_model(block(), s), s)) == 60);
}

TEST_CASE("curved layers: top faces follow a sloping roof (SU1 to SU3, SU5)", "[curved][surface]") {
    Settings s = surface_base();
    Settings flat = s;
    flat.curved_mode = CurvedMode::Disabled;
    const std::string plain = gcode_of(slice_model(roof_block(20, 24), flat), flat);
    const std::string bent = gcode_of(slice_model(roof_block(20, 24), s), s);
    const double before = top_error(plain, 20, 24), after = top_error(bent, 20, 24);
    WARN("top face error: flat " << before << " mm, curved " << after << " mm");
    CHECK(after < 0.5 * before); // Tisma Slicer: 0.057 to 0.010
    // The layers' slope grows evenly with the height, up to the roof's 0.1 (SU3).
    double last = 0.0;
    for (const char* z : {"2.000", "10.000", "15.000", "18.000"}) {
        const double k = slope_x(layer_moves(bent, z));
        CHECK(k > last);
        last = k;
    }
    CHECK_THAT(slope_x(layer_moves(bent, "21.000")), WithinAbs(0.1, 0.012));
    CHECK(std::abs(layer_count(bent) - 110) <= 2); // flat: 120
    // The first layers flat (SU5: Tisma Slicer bends the first one too; the engine does not).
    for (const Move& m : layer_moves(bent, "0.200"))
        CHECK_THAT(m.z1, WithinAbs(0.2, 1e-6));
}

TEST_CASE("curved layers: a low part bends little (SU4)", "[curved][surface]") {
    const Settings s = surface_base();
    const std::optional<CurvedField> f = curved_field(s, roof_block(5, 9));
    REQUIRE(f);
    const std::string bent = gcode_of(slice_model(roof_block(5, 9), s), s);
    const double top = slope_x(layer_moves(bent, "6.000"));
    CHECK(top > 0.02);
    CHECK(top < 0.1); // Tisma Slicer: 0.087
}

namespace {

// The spec's mushroom (nonplanar_modes.md): a foot of radius 8 and `foot`
// high, a hat whose underside opens at `beta` degrees from the vertical out
// to radius 20, a 2 mm rim and a flat top; 128 sides, centred on X 100, Y 100.
IndexedMesh mushroom(double beta, double foot = 25.0) {
    TriangleMesh m;
    const int sides = 128;
    const double rise = 12.0 / std::tan(beta * std::numbers::pi / 180.0);
    const double rings[4][2] = {{8.0, 0.0}, {8.0, foot}, {20.0, foot + rise}, {20.0, foot + rise + 2.0}};
    const auto at = [&](int ring, int k) {
        const double a = 2.0 * std::numbers::pi * k / sides;
        return Vec3f{float(100.0 + rings[ring][0] * std::cos(a)), float(100.0 + rings[ring][0] * std::sin(a)),
                     float(rings[ring][1])};
    };
    const Vec3f bottom{100, 100, 0}, top{100, 100, float(rings[3][1])};
    for (int k = 0; k < sides; ++k) {
        const int n = (k + 1) % sides;
        m.add_triangle(bottom, at(0, n), at(0, k));
        for (int r = 0; r < 3; ++r) {
            m.add_triangle(at(r, k), at(r, n), at(r + 1, n));
            m.add_triangle(at(r, k), at(r + 1, n), at(r + 1, k));
        }
        m.add_triangle(at(3, k), at(3, n), top);
    }
    return weld_exact(m);
}

Settings overhang_base(CurvedMode mode, bool head25) {
    Settings s = base();
    s.curved_mode = mode;
    s.infill_density = 20.0;
    s.wall_count = 2;
    s.top_layers = s.bottom_layers = 3;
    s.support = true;
    if (head25) {
        s.head_free_angle_x = s.head_free_angle_y = 25.0;
        s.head_clearance_height = 7.0;
        s.head_clearance_radius = 15.0;
    }
    return s;
}

// The layers' slope along the radius under the hat: the least squares
// line of z against r through a layer's extruded points between r 9 and 19.
double radial_slope(const std::vector<Move>& moves) {
    double sr = 0, sz = 0, srr = 0, srz = 0, n = 0;
    for (const Move& m : moves) {
        const double r = std::hypot(m.x1 - 100.0, m.y1 - 100.0);
        if (r < 9.0 || r > 19.0)
            continue;
        sr += r, sz += m.z1, srr += r * r, srz += r * m.z1, ++n;
    }
    return n < 3 ? 0.0 : (n * srz - sr * sz) / (n * srr - sr * sr);
}

double support_length(const std::string& text) {
    double total = 0.0, x = 0.0, y = 0.0;
    std::string type;
    std::istringstream in(text);
    for (std::string line; std::getline(in, line);) {
        if (line.rfind(";TYPE:", 0) == 0)
            type = line.substr(6);
        if (line.rfind("G1 ", 0) != 0)
            continue;
        double nx = x, ny = y, e = 0.0;
        std::istringstream words(line.substr(3));
        for (std::string w; words >> w;) {
            if (w[0] == ';')
                break;
            const double v = std::atof(w.c_str() + 1);
            if (w[0] == 'X')
                nx = v;
            else if (w[0] == 'Y')
                ny = v;
            else if (w[0] == 'E')
                e = v;
        }
        if (e > 0.0 && type.find("upport") != std::string::npos)
            total += std::hypot(nx - x, ny - y);
        x = nx, y = ny;
    }
    return total;
}

} // namespace

TEST_CASE("curved layers: a part without overhangs stays flat (CO1)", "[curved][overhangs]") {
    for (const CurvedMode mode : {CurvedMode::Conical, CurvedMode::Overhangs}) {
        const Settings s = overhang_base(mode, true);
        CHECK_FALSE(curved_field(s, block()).has_value());
    }
}

TEST_CASE("curved layers: the cone from a mushroom's overhang (CO2, CO4)", "[curved][overhangs]") {
    // The 25 degree head: the slope each overhang asks, the steepest at tan 25 (CO4).
    for (const auto& [beta, slope] : {std::pair{48.0, 0.291}, {52.0, 0.410}, {55.0, 0.4663}}) {
        INFO(beta);
        const std::optional<CurvedField> f = curved_field(overhang_base(CurvedMode::Conical, true), mushroom(beta));
        REQUIRE(f);
        // Drop per mm along the radius at the top, between r 9 and 19.
        const double z = f->base + f->height;
        const double drop = (f->offset(119.0, 100.0, z) - f->offset(109.0, 100.0, z)) / 10.0;
        CHECK_THAT(-drop, WithinAbs(slope, 0.01));
    }
    CHECK_FALSE(curved_field(overhang_base(CurvedMode::Conical, true), mushroom(60.0)).has_value()); // left to the support
    // The default head: the clearance (3 mm at 15) bounds the slope at 0.2;
    // 48 degrees then comes off the support, 55 does not (CO2).
    const std::optional<CurvedField> f = curved_field(overhang_base(CurvedMode::Conical, false), mushroom(48.0));
    REQUIRE(f);
    const double z = f->base + f->height;
    CHECK_THAT(-(f->offset(119.0, 100.0, z) - f->offset(109.0, 100.0, z)) / 10.0, WithinAbs(0.2, 0.012));
    CHECK_FALSE(curved_field(overhang_base(CurvedMode::Conical, false), mushroom(55.0)).has_value());
    // Sliced: the layers go down outwards under the hat, and the support goes.
    Settings s = overhang_base(CurvedMode::Conical, true);
    Settings flat = s;
    flat.curved_mode = CurvedMode::Disabled;
    const std::string bent = gcode_of(slice_model(mushroom(52.0), s), s);
    const std::string plain = gcode_of(slice_model(mushroom(52.0), flat), flat);
    CHECK(support_length(plain) > 500.0);
    CHECK(support_length(bent) < 0.1 * support_length(plain));
}

TEST_CASE("curved layers: overhangs bend only around their section (VO2, VO4, VO5)", "[curved][overhangs]") {
    // The 25 degree head: 0.29, 0.41, and 0.42 (90 % of tan 25) for 55; 60 flat (VO4).
    for (const auto& [beta, slope] : {std::pair{48.0, 0.291}, {52.0, 0.410}, {55.0, 0.42}}) {
        INFO(beta);
        const std::optional<CurvedField> f = curved_field(overhang_base(CurvedMode::Overhangs, true), mushroom(beta));
        REQUIRE(f);
        const double z = f->base + f->height;
        CHECK_THAT(-(f->offset(119.0, 100.0, z) - f->offset(110.0, 100.0, z)) / 9.0, WithinAbs(slope, 0.01));
        // Flat inside the foot's section (VO5: up to r 7).
        CHECK_THAT(f->offset(106.0, 100.0, z), WithinAbs(0.0, 1e-6));
    }
    CHECK_FALSE(curved_field(overhang_base(CurvedMode::Overhangs, true), mushroom(60.0)).has_value());
    // The default head: 0.18 for 48 degrees (90 % of the clearance's 0.2); 52 flat (VO2).
    const std::optional<CurvedField> f = curved_field(overhang_base(CurvedMode::Overhangs, false), mushroom(48.0));
    REQUIRE(f);
    const double z = f->base + f->height;
    CHECK_THAT(-(f->offset(119.0, 100.0, z) - f->offset(110.0, 100.0, z)) / 9.0, WithinAbs(0.18, 0.01));
    CHECK_FALSE(curved_field(overhang_base(CurvedMode::Overhangs, false), mushroom(52.0)).has_value());
    // Sliced: a layer under the hat is a straight slope out from the foot.
    Settings s = overhang_base(CurvedMode::Overhangs, true);
    const std::string bent = gcode_of(slice_model(mushroom(55.0), s), s);
    const double k = radial_slope(layer_moves(bent, "32.200"));
    CHECK_THAT(k, WithinAbs(-0.42, 0.03)); // Tisma Slicer: 0.417
}
