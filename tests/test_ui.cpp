#include "config/analysis_case.hpp"
#include "fea/analysis.hpp"
#include "gcode/writer.hpp"
#include "io/stl.hpp"
#include "ui/camera.hpp"
#include "ui/files.hpp"
#include "ui/icons.hpp"
#include "ui/png.hpp"
#include "ui/presets.hpp"
#include "ui/scene.hpp"
#include "ui/slicing.hpp"
#include "ui/text.hpp"
#include "test_temp.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <functional>
#include <fstream>
#include <numbers>
#include <string>

using namespace tisma;
using namespace tisma::ui;
using Catch::Matchers::ContainsSubstring;
using Catch::Matchers::WithinAbs;

namespace {

bool inside_clip(const Mat4& m, Vec3 p) {
    const Vec3 c = transform_point(m, p);
    return std::abs(c.x) <= 1.0f + 1e-5f && std::abs(c.y) <= 1.0f + 1e-5f && c.z >= -1.0f && c.z <= 1.0f;
}

std::uint32_t be32(const std::vector<std::uint8_t>& d, std::size_t at) {
    return std::uint32_t(d[at]) << 24 | std::uint32_t(d[at + 1]) << 16 | std::uint32_t(d[at + 2]) << 8 | d[at + 3];
}

// The printf conversions of a text, in order ("%s", "%.1f" -> 's', 'f').
std::string conversions(const char* text) {
    std::string out;
    for (const char* p = text; *p; ++p) {
        if (*p != '%')
            continue;
        ++p;
        if (*p == '%')
            continue;
        while (*p && !std::isalpha(static_cast<unsigned char>(*p)))
            ++p;
        if (!*p)
            break; // a '%' with no conversion after it: never past the end
        out += *p;
    }
    return out;
}

TriangleMesh read_cube() {
    return read_stl(std::filesystem::path(TISMA_TEST_DATA_DIR) / "cube_ascii.stl").mesh;
}

} // namespace

TEST_CASE("ui: the camera's axes and matrices", "[ui]") {
    OrbitCamera cam;
    cam.target = {100, 50, 10};
    cam.distance = 300;
    for (float yaw : {0.0f, 0.7f, -2.0f, 3.1f})
        for (float pitch : {-1.0f, 0.0f, 0.5f, std::numbers::pi_v<float> / 2}) {
            cam.yaw = yaw, cam.pitch = pitch;
            const Vec3 f = cam.forward(), r = cam.right(), u = cam.up();
            CHECK_THAT(length(f), WithinAbs(1.0, 1e-5));
            CHECK_THAT(length(r), WithinAbs(1.0, 1e-5));
            CHECK_THAT(length(u), WithinAbs(1.0, 1e-5));
            CHECK_THAT(dot(f, r), WithinAbs(0.0, 1e-5));
            CHECK_THAT(dot(f, u), WithinAbs(0.0, 1e-5));
            CHECK_THAT(length(cam.eye() - cam.target), WithinAbs(300.0, 1e-3));
            // The view puts the eye at the origin and the target straight ahead.
            const Vec3 t = transform_point(cam.view(), cam.target);
            CHECK_THAT(t.x, WithinAbs(0.0, 1e-3));
            CHECK_THAT(t.y, WithinAbs(0.0, 1e-3));
            CHECK_THAT(t.z, WithinAbs(-300.0, 1e-2));
            const Vec3 back = transform_point(rigid_inverse(cam.view()), Vec3{0, 0, -300});
            CHECK_THAT(back.x, WithinAbs(100.0, 1e-2));
            CHECK_THAT(back.z, WithinAbs(10.0, 1e-2));
            const Mat4 clip = cam.projection(1.5f) * cam.view();
            const Vec3 c = transform_point(clip, cam.target);
            CHECK_THAT(c.x, WithinAbs(0.0, 1e-4));
            CHECK_THAT(c.y, WithinAbs(0.0, 1e-4));
            CHECK(std::abs(c.z) < 1.0f);
        }

    // From the top, the back of the bed (+Y) is up on the screen; from the
    // front, +Z is; from the right, +Y goes right.
    cam.set_view(StandardView::Top);
    CHECK(transform_point(cam.projection(1.0f) * cam.view(), cam.target + Vec3{0, 10, 0}).y > 0.0f);
    CHECK(transform_point(cam.projection(1.0f) * cam.view(), cam.target + Vec3{10, 0, 0}).x > 0.0f);
    cam.set_view(StandardView::Front);
    CHECK(transform_point(cam.projection(1.0f) * cam.view(), cam.target + Vec3{0, 0, 10}).y > 0.0f);
    CHECK(cam.eye().y < cam.target.y);
    cam.set_view(StandardView::Right);
    CHECK(cam.eye().x > cam.target.x);
    CHECK(transform_point(cam.projection(1.0f) * cam.view(), cam.target + Vec3{0, 10, 0}).x > 0.0f);
    cam.set_view(StandardView::Back);
    CHECK(cam.eye().y > cam.target.y);
}

TEST_CASE("ui: the camera frames a box and moves with the mouse", "[ui]") {
    OrbitCamera cam;
    const Vec3 lo{-20, 30, 0}, hi{60, 50, 45};
    for (StandardView v : {StandardView::Iso, StandardView::Top, StandardView::Front, StandardView::Left})
        for (float aspect : {0.5f, 1.0f, 2.5f}) {
            cam.set_view(v);
            cam.fit(lo, hi, aspect);
            const Mat4 clip = cam.projection(aspect) * cam.view();
            for (int k = 0; k < 8; ++k)
                CHECK(inside_clip(clip, {k & 1 ? hi.x : lo.x, k & 2 ? hi.y : lo.y, k & 4 ? hi.z : lo.z}));
        }

    cam.set_view(StandardView::Iso);
    cam.orbit(0, 100000); // pitch stops at straight down
    CHECK_THAT(cam.pitch, WithinAbs(std::numbers::pi / 2, 1e-6));
    cam.orbit(0, -100000);
    CHECK_THAT(cam.pitch, WithinAbs(-89.0 * std::numbers::pi / 180.0, 1e-5));

    const float d = cam.distance;
    cam.zoom(1);
    CHECK_THAT(cam.distance, WithinAbs(d * 0.85, 1e-3));
    cam.zoom(-1);
    CHECK_THAT(cam.distance, WithinAbs(d, 1e-3));

    // Dragging right moves the scene right: the target goes left.
    const Vec3 before = cam.target;
    cam.pan(10, 0, 800);
    CHECK(dot(cam.target - before, cam.right()) < 0.0f);
}

TEST_CASE("ui: the orthographic view keeps sizes whatever the depth (preview.md, rule 10)", "[ui]") {
    OrbitCamera cam;
    cam.target = {0, 0, 0};
    cam.distance = 300.0f;
    cam.set_view(StandardView::Front);
    // A 10 mm bar at the target and 100 mm nearer: its size on the screen.
    const auto width_at = [&](float y) {
        const Mat4 vp = cam.projection(1.0f) * cam.view();
        return transform_point(vp, Vec3{5.0f, y, 0.0f}).x - transform_point(vp, Vec3{-5.0f, y, 0.0f}).x;
    };
    const float far = width_at(0.0f), near = width_at(-100.0f);
    CHECK(near / far > 1.3f); // perspective: nearer is bigger
    cam.orthographic = true;
    const float far_o = width_at(0.0f), near_o = width_at(-100.0f);
    CHECK(near_o / far_o < 1.01f);
    // The target keeps its size on the screen, so zoom and pan feel the same.
    CHECK_THAT(double(far_o), Catch::Matchers::WithinRel(double(far), 1e-3));
    CHECK(length(cam.eye() - cam.target) > 10.0f * cam.distance);
    // The bottom view looks up.
    cam.set_view(StandardView::Bottom);
    CHECK(cam.forward().z > 0.99f);
}

TEST_CASE("ui: a model goes to the middle of the bed, on it", "[ui]") {
    BoundingBox box;
    box.min = {10, -5, 2};
    box.max = {30, 5, 12};
    box.empty = false;
    const Vec3 o = centre_on_bed(box, {{0, 0}, {200, 0}, {200, 100}, {0, 100}});
    CHECK(o == Vec3{80, 50, -2});
    CHECK(centre_on_bed(box, {}) == Vec3{-20, 0, -2});

    const TriangleMesh cube = read_cube();
    const std::vector<float> v = mesh_vertices(cube, {1, 2, 3});
    REQUIRE(v.size() == cube.triangle_count() * 18);
    for (std::size_t i = 0; i < v.size(); i += 6)
        CHECK_THAT(std::sqrt(v[i + 3] * v[i + 3] + v[i + 4] * v[i + 4] + v[i + 5] * v[i + 5]), WithinAbs(1.0, 1e-5));
    CHECK(v[0] == cube.corner(0, 0).x + 1);
    CHECK(v[2] == cube.corner(0, 0).z + 3);
    const TriangleMesh moved = translated(cube, {1, 2, 3});
    CHECK(moved.bounding_box().min.y == cube.bounding_box().min.y + 2);
}

TEST_CASE("ui: the bed's grid follows its polygon", "[ui]") {
    BedLook look;
    const std::vector<LineVertex> rect = bed_lines({{0, 0}, {100, 0}, {100, 50}, {0, 50}}, look);
    // 9 lines across x and 4 across y (those on the edges are the outline's),
    // and the 4 edges, two vertices each.
    REQUIRE(rect.size() == (9 + 4 + 4) * 2);
    int majors = 0;
    for (std::size_t i = 0; i < 26; i += 2)
        majors += rect[i].argb == look.major;
    CHECK(majors == 1); // x = 50
    CHECK(rect.back().argb == look.outline);
    CHECK(bed_triangles({{0, 0}, {100, 0}, {100, 50}, {0, 50}}).size() == 18);

    // An L-shaped bed: the line y = 75 only crosses the left arm.
    const BedPolygon l = {{0, 0}, {100, 0}, {100, 50}, {50, 50}, {50, 100}, {0, 100}};
    for (std::size_t i = 0; i + 1 < bed_lines(l, look).size(); i += 2) {
        const LineVertex a = bed_lines(l, look)[i], b = bed_lines(l, look)[i + 1];
        if (a.argb == look.outline)
            continue;
        CHECK(std::max(a.x, b.x) <= 100.0f);
        if (a.y == b.y && a.y > 50.0f)
            CHECK(std::max(a.x, b.x) <= 50.0f);
    }
    CHECK(bed_lines({}, look).empty());
}

TEST_CASE("ui: preview buffers keep the layers and kinds", "[ui]") {
    const GcodePreview p = read_gcode_preview(";LAYER_CHANGE\n;Z:0.2\n;HEIGHT:0.2\nG1 Z0.2 F600\n"
                                              ";TYPE:External perimeter\n;WIDTH:0.45\nG1 X10 E1 F1200\nG1 X20 Y5 F6000\n"
                                              ";TYPE:Internal infill\n;WIDTH:0.5\nG1 X30 E2 F3000\n"
                                              ";LAYER_CHANGE\n;Z:0.4\n;HEIGHT:0.2\nG1 Z0.4\n"
                                              ";TYPE:Perimeter\nG1 X0 E3 F2400\n");
    const PreviewBuffers b = preview_buffers(p, 0xFF0000FF);
    REQUIRE(b.extrusions.size() == 3);
    CHECK(b.layer_extrusions == std::vector<std::size_t>{0, 2, 3});
    CHECK(b.layer_travels == std::vector<std::size_t>{0, 4, 6});
    CHECK(b.travels.front().argb == 0xFF0000FF);
    CHECK((b.extrusions[0].kind_layer & 0xFF) == std::uint32_t(LineKind::OuterWall));
    CHECK((b.extrusions[2].kind_layer >> 8) == 1);
    CHECK((b.extrusions[2].kind_layer & 0xFF) == std::uint32_t(LineKind::InnerWall));
    CHECK(b.min_feed == 20.0f);
    CHECK(b.max_feed == 50.0f);
    CHECK(b.min_width == 0.45f);
    CHECK(b.max_width == 0.5f);
    CHECK(b.min_height == 0.2f);
    CHECK(b.max_height == 0.2f);
    // A thick zone within a layer (docs/LOCAL_LAYERS.md): its own height.
    const PreviewBuffers zones = preview_buffers(read_gcode_preview(
        ";LAYER_CHANGE\n;Z:0.3\n;HEIGHT:0.1\nG1 Z0.3 F600\n;TYPE:Perimeter\n;WIDTH:0.45\nG1 X10 E1 F1200\n"
        ";HEIGHT:0.3\n;TYPE:Internal infill\nG1 X20 E2\n;HEIGHT:0.1\n"));
    CHECK(zones.min_height == 0.1f);
    CHECK(zones.max_height == 0.3f);
    for (std::size_t k = 0; k < kLineKinds; ++k)
        CHECK(kind_colour(LineKind(k)) <= 0xFFFFFF);
}

namespace {

// G-code of layers of square walls: per layer, the outer wall's square
// {x0, y0, side}, an inner wall 0.45 mm in and a line of infill across. An
// open wall leaves its last side out.
std::string squares_gcode(const std::vector<std::array<float, 3>>& layers, bool open = false) {
    std::string g;
    double e = 0.0;
    char line[128];
    const auto move = [&](float x, float y, bool extrude) {
        if (extrude)
            std::snprintf(line, sizeof line, "G1 X%.3f Y%.3f E%.4f F1800\n", double(x), double(y), e += 1.0);
        else
            std::snprintf(line, sizeof line, "G1 X%.3f Y%.3f F6000\n", double(x), double(y));
        g += line;
    };
    const auto square = [&](float x0, float y0, float side, int sides) {
        move(x0, y0, false);
        const float xs[] = {x0 + side, x0 + side, x0, x0}, ys[] = {y0, y0 + side, y0 + side, y0};
        for (int k = 0; k < sides; ++k)
            move(xs[k], ys[k], true);
    };
    for (std::size_t l = 0; l < layers.size(); ++l) {
        const auto [x0, y0, side] = layers[l];
        std::snprintf(line, sizeof line, ";LAYER_CHANGE\n;Z:%.1f\n;HEIGHT:0.2\nG1 Z%.1f F600\n;WIDTH:0.45\n",
                      0.2 * double(l + 1), 0.2 * double(l + 1));
        g += line;
        g += ";TYPE:Perimeter\n";
        square(x0 + 0.45f, y0 + 0.45f, side - 0.9f, 4);
        g += ";TYPE:External perimeter\n";
        square(x0, y0, side, open ? 3 : 4);
        g += ";TYPE:Internal infill\n";
        move(x0 + 1.0f, y0 + side / 2, false);
        move(x0 + side - 1.0f, y0 + side / 2, true);
    }
    return g;
}

// Whether every extrusion of `kind` in layer l is hidden (1), none is (0), or some are (-1).
int hidden_of(const PreviewBuffers& b, std::size_t l, LineKind kind) {
    int seen = 0, hidden = 0;
    for (std::size_t i = b.layer_extrusions[l]; i < b.layer_extrusions[l + 1]; ++i)
        if ((b.extrusions[i].kind_layer & 0xFFu) == std::uint32_t(kind))
            ++seen, hidden += b.hidden[i];
    return hidden == 0 ? 0 : hidden == seen ? 1 : -1;
}

} // namespace

TEST_CASE("ui: the preview's numeric views and their 11 steps (preview.md, rules 1 and 7, PV2, PV3)", "[ui]") {
    // PV2 and PV3: 11 steps between the least and the most.
    const std::vector<double> speed = legend_steps(0.0, 230.0, false);
    REQUIRE(speed.size() == 11);
    for (std::size_t i = 0; i < speed.size(); ++i)
        CHECK_THAT(speed[i], Catch::Matchers::WithinAbs(23.0 * double(i), 1e-9));
    const std::vector<double> width = legend_steps(0.38, 0.76, false);
    CHECK_THAT(width[1] - width[0], Catch::Matchers::WithinAbs(0.038, 1e-12));
    // In equal steps of the logarithm; one value alone, one step.
    const std::vector<double> log = legend_steps(std::log10(20.0), std::log10(62.0), true);
    CHECK_THAT(log.front(), Catch::Matchers::WithinRel(20.0, 1e-9));
    CHECK_THAT(log.back(), Catch::Matchers::WithinRel(62.0, 1e-9));
    CHECK_THAT(log[2] / log[1], Catch::Matchers::WithinRel(log[1] / log[0], 1e-9));
    CHECK(legend_steps(5.0, 5.0, false) == std::vector<double>{5.0});
    // The instances carry the view's number.
    GcodePreview p = read_gcode_preview(";LAYER_CHANGE\n;Z:0.2\n;HEIGHT:0.2\n;TYPE:Perimeter\n;WIDTH:0.5\n"
                                        "M106 S255\nM104 S210\nG1 Z0.2 F600\nG1 X10 Y0 E1\nT1\nG1 X20 E2 F1200\n");
    time_preview(p, Kinematics{});
    const PreviewBuffers fan = preview_buffers(p, 0xFF0000FF, PreviewValue::Fan);
    CHECK(fan.min_feed == 100.0f);
    const PreviewBuffers flow = preview_buffers(p, 0xFF0000FF, PreviewValue::Flow);
    CHECK_THAT(double(flow.min_feed), Catch::Matchers::WithinAbs(0.5 * 0.2 * 10.0, 1e-5));
    CHECK_THAT(double(flow.max_feed), Catch::Matchers::WithinAbs(0.5 * 0.2 * 20.0, 1e-5));
    const PreviewBuffers tool = preview_buffers(p, 0xFF0000FF, PreviewValue::Tool);
    CHECK(tool.min_feed == 0.0f);
    CHECK(tool.max_feed == 1.0f);
    // The reached speed is never above the asked one (here the move starts
    // at the corner's speed, its feed).
    const PreviewMove& first = p.moves[p.extrusions.front()];
    CHECK(preview_value(p, first, PreviewValue::ActualFeed) <= 10.0f + 1e-4f);
    CHECK(preview_value(p, first, PreviewValue::ActualFeed) > 9.0f);
    CHECK(preview_value(p, first, PreviewValue::Temperature) == 210.0f);
    CHECK_THAT(double(preview_value(p, first, PreviewValue::LayerTime)),
               Catch::Matchers::WithinAbs(p.layers[first.layer].seconds, 1e-5));
}

TEST_CASE("ui: the preview leaves out what the part hides when all of it shows", "[ui]") {
    // Six equal layers of a 20 mm square: inside the layers between the
    // first and the last, the inner wall and the infill are covered from
    // under, over and the sides.
    const PreviewBuffers equal = preview_buffers(read_gcode_preview(squares_gcode(std::vector<std::array<float, 3>>(6, {0, 0, 20}))));
    REQUIRE(equal.hidden.size() == equal.extrusions.size());
    for (std::size_t l = 0; l < 6; ++l) {
        INFO("layer " << l);
        const int inside = l == 0 || l == 5 ? 0 : 1;
        CHECK(hidden_of(equal, l, LineKind::InnerWall) == inside);
        CHECK(hidden_of(equal, l, LineKind::SparseInfill) == inside);
        CHECK(hidden_of(equal, l, LineKind::OuterWall) == 0);
    }

    // A step: three layers of 20 mm, then three of 10 mm in the middle. The
    // big part's last layer is under the air around the small one; the small
    // part's first layer stands back from the big one's wall, and the groove
    // between them shows its inner wall.
    std::vector<std::array<float, 3>> step(3, {0, 0, 20});
    step.insert(step.end(), 3, {5, 5, 10});
    const PreviewBuffers stepped = preview_buffers(read_gcode_preview(squares_gcode(step)));
    CHECK(hidden_of(stepped, 1, LineKind::InnerWall) == 1);
    CHECK(hidden_of(stepped, 2, LineKind::InnerWall) == 0);  // under the air
    CHECK(hidden_of(stepped, 2, LineKind::SparseInfill) == 0); // one line, its ends under the air
    CHECK(hidden_of(stepped, 3, LineKind::InnerWall) == 0);  // by the step
    CHECK(hidden_of(stepped, 4, LineKind::InnerWall) == 1);

    // Walls that do not close: nothing is known to be inside.
    const PreviewBuffers open =
        preview_buffers(read_gcode_preview(squares_gcode(std::vector<std::array<float, 3>>(6, {0, 0, 20}), true)));
    for (const std::uint8_t h : open.hidden)
        CHECK(h == 0);
}

TEST_CASE("ui: preview beads join where they go on into the next move", "[ui]") {
    // A square of four moves, a travel, then a line out and back (a 180 degree
    // turn) and a slight bend.
    const GcodePreview p = read_gcode_preview(
        ";LAYER_CHANGE\n;Z:0.2\n;HEIGHT:0.2\nG1 Z0.2 F600\n;TYPE:External perimeter\n;WIDTH:0.45\n"
        "G1 X10 E1 F1200\nG1 Y10 E2\nG1 X0 E3\nG1 Y0 E4\n"
        "G1 X20 Y20 F6000\n;TYPE:Internal infill\nG1 X30 E5\nG1 X20 E6\nG1 X10 Y21 E7\n");
    const PreviewBuffers b = preview_buffers(p);
    REQUIRE(b.extrusions.size() == 7);
    const auto joined = [](float x, float y) { return x * x + y * y > 0.25f; };
    const PreviewInstance* e = b.extrusions.data();
    // The square: free at its start and end (the seam), mitred at its three
    // corners, both moves on the same plane.
    CHECK_FALSE(joined(e[0].join0_x, e[0].join0_y));
    for (int k = 0; k < 3; ++k) {
        INFO("corner " << k);
        REQUIRE(joined(e[k].join1_x, e[k].join1_y));
        CHECK(e[k].join1_x == e[k + 1].join0_x);
        CHECK(e[k].join1_y == e[k + 1].join0_y);
        CHECK_THAT(std::hypot(e[k].join1_x, e[k].join1_y), Catch::Matchers::WithinAbs(1.0, 1e-6));
    }
    CHECK_THAT(e[0].join1_x, Catch::Matchers::WithinAbs(std::sqrt(0.5), 1e-6)); // halfway between +X and +Y
    CHECK_THAT(e[0].join1_y, Catch::Matchers::WithinAbs(std::sqrt(0.5), 1e-6));
    CHECK_FALSE(joined(e[3].join1_x, e[3].join1_y));
    // After the travel: free; the turn back is too sharp to join; the bend joins.
    CHECK_FALSE(joined(e[4].join0_x, e[4].join0_y));
    CHECK_FALSE(joined(e[4].join1_x, e[4].join1_y));
    CHECK_FALSE(joined(e[5].join0_x, e[5].join0_y));
    CHECK(joined(e[5].join1_x, e[5].join1_y));
    CHECK(joined(e[6].join0_x, e[6].join0_y));
}

TEST_CASE("ui: PNG files are valid", "[ui]") {
    const std::uint8_t check[] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
    CHECK(crc32(check, 9) == 0xCBF43926u); // the CRC-32 check value
    const char* wiki = "Wikipedia";
    CHECK(adler32(reinterpret_cast<const std::uint8_t*>(wiki), 9) == 0x11E60398u);

    // 300 x 300 needs several stored blocks of at most 65535 bytes.
    for (int size : {3, 300}) {
        std::vector<std::uint8_t> rgba(std::size_t(size) * std::size_t(size) * 4);
        for (std::size_t i = 0; i < rgba.size(); ++i)
            rgba[i] = std::uint8_t(i * 7 + 3);
        const std::vector<std::uint8_t> png = encode_png(size, size, rgba.data());
        REQUIRE(std::memcmp(png.data(), "\x89PNG\r\n\x1a\n", 8) == 0);
        // Walk the chunks, checking each CRC, and gather IDAT.
        std::vector<std::uint8_t> z;
        std::vector<std::string> types;
        for (std::size_t at = 8; at < png.size();) {
            const std::uint32_t n = be32(png, at);
            types.emplace_back(reinterpret_cast<const char*>(&png[at + 4]), 4);
            CHECK(crc32(&png[at + 4], n + 4) == be32(png, at + 8 + n));
            if (types.back() == "IHDR") {
                CHECK(be32(png, at + 8) == std::uint32_t(size));
                CHECK(png[at + 16] == 8);
                CHECK(png[at + 17] == 6);
            }
            if (types.back() == "IDAT")
                z.insert(z.end(), png.begin() + std::ptrdiff_t(at + 8), png.begin() + std::ptrdiff_t(at + 8 + n));
            at += 12 + n;
        }
        CHECK(types == std::vector<std::string>{"IHDR", "IDAT", "IEND"});
        // Undo the stored blocks and compare with the rows.
        REQUIRE(z.size() > 6);
        CHECK((z[0] * 256 + z[1]) % 31 == 0);
        std::vector<std::uint8_t> raw;
        std::size_t at = 2;
        bool last = false;
        while (!last) {
            last = z[at] & 1;
            const std::size_t len = z[at + 1] | std::size_t(z[at + 2]) << 8;
            const std::size_t nlen = z[at + 3] | std::size_t(z[at + 4]) << 8;
            CHECK((len ^ nlen) == 0xFFFF);
            raw.insert(raw.end(), z.begin() + std::ptrdiff_t(at + 5), z.begin() + std::ptrdiff_t(at + 5 + len));
            at += 5 + len;
        }
        CHECK(be32(z, at) == adler32(raw.data(), raw.size()));
        REQUIRE(raw.size() == std::size_t(size) * (1 + 4 * std::size_t(size)));
        for (int y = 0; y < size; ++y) {
            const std::size_t row = std::size_t(y) * (1 + 4 * std::size_t(size));
            CHECK(raw[row] == 0);
            CHECK(std::memcmp(&raw[row + 1], &rgba[std::size_t(y) * 4 * std::size_t(size)], 4 * std::size_t(size)) == 0);
        }
    }
}

TEST_CASE("ui: the shadow map's camera holds the box, nearest the light at -1", "[ui]") {
    const Vec3 lo{10, 20, 0}, hi{50, 40, 30};
    for (Vec3 towards : {normalize(Vec3{-0.45f, 0.45f, 0.8f}), Vec3{0, 0, 1}, normalize(Vec3{1, 0, 0.2f})}) {
        const Mat4 m = shadow_view_projection(towards, lo, hi);
        float zmin = 2, zmax = -2;
        for (int k = 0; k < 8; ++k) {
            const Vec3 p{(k & 1) ? hi.x : lo.x, (k & 2) ? hi.y : lo.y, (k & 4) ? hi.z : lo.z};
            const Vec3 q = transform_point(m, p);
            CHECK(std::fabs(q.x) <= 1.0001f);
            CHECK(std::fabs(q.y) <= 1.0001f);
            CHECK(std::fabs(q.z) <= 1.0001f);
            zmin = std::min(zmin, q.z), zmax = std::max(zmax, q.z);
        }
        CHECK_THAT(zmin, WithinAbs(-1.0, 1e-4));
        CHECK_THAT(zmax, WithinAbs(1.0, 1e-4));
        // A point further towards the light is nearer.
        const Vec3 c = (lo + hi) * 0.5f;
        CHECK(transform_point(m, c + towards * 5.0f).z < transform_point(m, c).z);
    }
}

TEST_CASE("ui: screenshots read back and compare", "[ui]") {
    // A 300 x 300 picture (several stored blocks) reads back as it was.
    const int size = 300;
    std::vector<std::uint8_t> rgba(std::size_t(size) * std::size_t(size) * 4);
    for (std::size_t i = 0; i < rgba.size(); ++i)
        rgba[i] = std::uint8_t(i * 13 + 5);
    std::vector<std::uint8_t> png = encode_png(size, size, rgba.data());
    int w = 0, h = 0;
    std::vector<std::uint8_t> back;
    REQUIRE(decode_png(png, w, h, back));
    CHECK(w == size);
    CHECK(h == size);
    CHECK(back == rgba);
    // A damaged one does not.
    png[png.size() / 2] ^= 0x40;
    CHECK_FALSE(decode_png(png, w, h, back));
    CHECK_FALSE(decode_png({}, w, h, back));

    // Two pixels changed, one by more than the tolerance; one more outside
    // the rectangle; alpha does not count.
    std::vector<std::uint8_t> other = rgba;
    const auto at = [&](int x, int y) { return (std::size_t(y) * std::size_t(size) + std::size_t(x)) * 4; };
    other[at(10, 20) + 1] = std::uint8_t(other[at(10, 20) + 1] + 5);
    other[at(50, 60) + 2] = std::uint8_t(other[at(50, 60) + 2] ^ 0x80);
    other[at(250, 250)] = std::uint8_t(other[at(250, 250)] + 90);
    other[at(30, 30) + 3] = std::uint8_t(other[at(30, 30) + 3] + 90);
    const PictureDifference d = compare_pictures(rgba.data(), other.data(), size, size, 0, 0, 100, 100, 24);
    CHECK(d.pixels == 100 * 100);
    CHECK(d.differing == 2);
    CHECK(d.over == 1);
    CHECK(d.largest == 128);
    CHECK(compare_pictures(rgba.data(), rgba.data(), size, size, -10, -10, 1000, 1000, 0).pixels ==
          std::size_t(size) * std::size_t(size));
}

TEST_CASE("ui: SVG icons rasterize and take the theme's colour", "[ui]") {
    const char* square = R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24">
        <path fill="#808080" d="M6,6H18V18H6Z"/><path fill="#00FF00" d="M0,0H3V3H0Z"/></svg>)";
    const Image red = rasterize_svg(square, 24, 0xFF0000);
    REQUIRE(red.width == 24);
    REQUIRE(red.rgba.size() == 24 * 24 * 4);
    const auto pixel = [&](int x, int y) { return &red.rgba[std::size_t(y * 24 + x) * 4]; };
    CHECK(pixel(12, 12)[0] == 255);
    CHECK(pixel(12, 12)[1] == 0);
    CHECK(pixel(12, 12)[3] == 255);
    CHECK(pixel(1, 1)[1] == 255); // other colours stay
    CHECK(pixel(20, 20)[3] == 0);
    CHECK(rasterize_svg("not an svg", 24).rgba.empty());
    CHECK(rasterize_svg(square, 0).rgba.empty());

    // Every Tisma icon draws something.
    const std::filesystem::path icons = std::filesystem::path(TISMA_ASSETS_DIR) / "brand" / "icons";
    int count = 0;
    for (const auto& entry : std::filesystem::directory_iterator(icons)) {
        std::ifstream in(entry.path(), std::ios::binary);
        const std::string svg((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        const Image image = rasterize_svg(svg, 48, 0xFFFFFF);
        REQUIRE(image.width == 48);
        std::size_t covered = 0;
        for (std::size_t i = 3; i < image.rgba.size(); i += 4)
            covered += image.rgba[i] > 128;
        CHECK(covered > 48);
        ++count;
    }
    CHECK(count == 21);
}

TEST_CASE("ui: every text has both languages with the same fields", "[ui]") {
    const Language before = language();
    for (int i = 0; i < int(T::Count); ++i) {
        set_language(Language::Spanish);
        const char* es = tr(T(i));
        set_language(Language::English);
        const char* en = tr(T(i));
        INFO("text " << i << ": " << es << " / " << en);
        CHECK(std::strlen(es) > 0);
        CHECK(std::strlen(en) > 0);
        CHECK(conversions(es) == conversions(en));
    }
    set_language(Language::Spanish);
    CHECK(std::string(tr(T::Slice)) == "Laminar");
    set_language(Language::English);
    CHECK(std::string(tr(T::Slice)) == "Slice");
    set_language(before);
}

TEST_CASE("ui: folders list folders first, then the files asked for", "[ui]") {
    const std::filesystem::path dir = test_temp_dir() / "tisma_ui_files";
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir / "Sub");
    std::filesystem::create_directories(dir / ".hidden");
    for (const char* name : {"b.gcode", "A.STL", "c.txt", ".x.stl"})
        std::ofstream(dir / name) << "x";
    std::ofstream(dir / from_utf8("módulo.stl")) << "xyz";

    std::vector<FolderEntry> e;
    REQUIRE(list_folder(dir, {".stl", ".gcode"}, e));
    REQUIRE(e.size() == 4);
    CHECK(e[0].name == "Sub");
    CHECK(e[0].folder);
    CHECK(e[1].name == "A.STL");
    CHECK(e[2].name == "b.gcode");
    CHECK(e[3].name == "módulo.stl");
    CHECK(e[3].size == 3);
    REQUIRE(list_folder(dir, {}, e));
    CHECK(e.size() == 5);
    CHECK_FALSE(list_folder(dir / "missing", {}, e));
    CHECK(lower_extension("model.STL") == ".stl");
    CHECK(lower_extension("noext") == "");
    CHECK(utf8(from_utf8("módulo ñ.stl")) == "módulo ñ.stl");
    CHECK_FALSE(folder_roots().empty());
    std::filesystem::remove_all(dir);
}

TEST_CASE("ui: a change to a G-code setting makes only the G-code again, the same bytes", "[ui]") {
    // Two posts and a plate over them: walls, solid and sparse infill, a
    // bridge, travels, slow short layers and the fan all show.
    const auto box = [](TriangleMesh& m, float x0, float y0, float z0, float x1, float y1, float z1) {
        const auto p = [&](int i, int j, int k) { return Vec3f{i ? x1 : x0, j ? y1 : y0, k ? z1 : z0}; };
        const int faces[12][3][3] = {
            {{0, 0, 0}, {0, 1, 0}, {1, 1, 0}}, {{0, 0, 0}, {1, 1, 0}, {1, 0, 0}}, {{0, 0, 1}, {1, 0, 1}, {1, 1, 1}},
            {{0, 0, 1}, {1, 1, 1}, {0, 1, 1}}, {{0, 0, 0}, {1, 0, 0}, {1, 0, 1}}, {{0, 0, 0}, {1, 0, 1}, {0, 0, 1}},
            {{0, 1, 0}, {0, 1, 1}, {1, 1, 1}}, {{0, 1, 0}, {1, 1, 1}, {1, 1, 0}}, {{0, 0, 0}, {0, 0, 1}, {0, 1, 1}},
            {{0, 0, 0}, {0, 1, 1}, {0, 1, 0}}, {{1, 0, 0}, {1, 1, 0}, {1, 1, 1}}, {{1, 0, 0}, {1, 1, 1}, {1, 0, 1}}};
        for (const auto& f : faces)
            m.add_triangle(p(f[0][0], f[0][1], f[0][2]), p(f[1][0], f[1][1], f[1][2]), p(f[2][0], f[2][1], f[2][2]));
    };
    TriangleMesh table;
    box(table, 0, 0, 0, 4, 6, 4);
    box(table, 12, 0, 0, 16, 6, 4);
    box(table, 0, 0, 4, 16, 6, 5.6f);
    Settings base = default_settings();
    base.gpu = GpuMode::Off;
    const TriangleMesh placed = translated(table, centre_on_bed(table.bounding_box(), base.bed));

    Slicer staged;
    const SliceResult first = staged.run(placed, base);
    REQUIRE(first.ok);
    CHECK_FALSE(first.staged);
    CHECK_THAT(first.gcode, ContainsSubstring(";TYPE:Bridge infill"));
    const SliceResult same = staged.run(placed, base);
    CHECK(same.staged);
    CHECK(same.model == first.model);
    CHECK(same.gcode == first.gcode);

    const std::vector<std::pair<const char*, std::function<void(Settings&)>>> changes = {
        {"speed.walls", [](Settings& s) { s.wall_speed *= 1.5; }},
        {"speed.outer_wall", [](Settings& s) { s.outer_wall_speed = Speed{50.0, true}; }},
        {"speed.infill", [](Settings& s) { s.infill_speed *= 1.5; }},
        {"speed.solid_infill", [](Settings& s) { s.solid_infill_speed *= 0.7; }},
        {"speed.top_solid_infill", [](Settings& s) { s.top_solid_infill_speed *= 0.7; }},
        {"speed.support", [](Settings& s) { s.support_speed *= 0.7; }},
        {"speed.travel", [](Settings& s) { s.travel_speed *= 0.5; }},
        {"speed.first_layer", [](Settings& s) { s.first_layer_speed *= 0.5; }},
        {"speed.max", [](Settings& s) { s.max_speed = 35.0; }},
        {"machine.corner_speed", [](Settings& s) { s.corner_speed = 4.0; }},
        {"machine.acceleration", [](Settings& s) { s.acceleration = 3000.0, s.travel_acceleration = 4000.0; }},
        {"machine.max_feed", [](Settings& s) { s.max_feed_x = s.max_feed_y = 150.0, s.max_feed_z = 4.0, s.max_feed_e = 30.0; }},
        {"retraction.length", [](Settings& s) { s.retract_length = 0.8; }},
        {"retraction.speed", [](Settings& s) { s.retract_speed = 45.0; }},
        {"retraction.lift", [](Settings& s) { s.retract_lift = 0.4; }},
        {"retraction.min_travel", [](Settings& s) { s.retract_min_travel = 0.5; }},
        {"cooling.min_layer_time", [](Settings& s) { s.min_layer_time = 15.0; }},
        {"cooling.min_speed", [](Settings& s) { s.min_print_speed = 15.0; }},
        {"cooling.keep_outer_speed", [](Settings& s) { s.keep_outer_speed = !s.keep_outer_speed; }},
        {"cooling.fan", [](Settings& s) { s.fan_min = 20.0, s.fan_max = 80.0, s.fan_below_time = 30.0; }},
        {"cooling.fan_always", [](Settings& s) { s.fan_always = !s.fan_always; }},
        {"cooling.fan_bridges", [](Settings& s) { s.fan_bridges = 60.0; }},
        {"cooling.fan_off_layers", [](Settings& s) { s.fan_off_layers = 1; }},
        {"machine.start_gcode", [](Settings& s) { s.start_gcode += "; staged\n"; }},
        {"machine.end_gcode", [](Settings& s) { s.end_gcode += "; staged\n"; }},
        {"temperature", [](Settings& s) { s.variables["temperature"] = {"215"}, s.variables["bed_temperature"] = {"55"}; }},
        {"extrusion.flow", [](Settings& s) { s.flow = 95.0; }},
        {"extrusion.pressure_advance", [](Settings& s) { s.pressure_advance = 0.05; }},
        {"extrusion.max_flow", [](Settings& s) { s.filament_max_flow = 2.0, s.machine_max_flow = 3.0; }},
        {"machine.max_temps", [](Settings& s) { s.max_nozzle_temp = 300.0, s.max_bed_temp = 110.0; }},
        {"extrusion.chamber_temperature", [](Settings& s) { s.chamber_temperature = 40.0, s.max_chamber_temp = 60.0; }},
        {"gcode.resolution", [](Settings& s) { s.gcode_resolution = 0.05; }},
        {"machine.relative_extrusion", [](Settings& s) { s.relative_extrusion = !s.relative_extrusion; }},
        {"machine.flavor", [](Settings& s) { s.flavor = GcodeFlavor::Klipper; }},
        {"walls.scarf", [](Settings& s) { s.scarf = ScarfMode::Everywhere, s.scarf_only_smooth = false, s.scarf_length = 5.0; }},
        {"walls.detect_overhangs", [](Settings& s) { s.detect_overhangs = !s.detect_overhangs; }},
    };
    for (const auto& [name, change] : changes) {
        INFO(name);
        Settings changed = base;
        change(changed);
        CHECK(without_gcode_settings(changed).wall_speed == without_gcode_settings(base).wall_speed);
        const SliceResult again = staged.run(placed, changed);
        Slicer fresh;
        const SliceResult whole = fresh.run(placed, changed);
        REQUIRE(again.ok == whole.ok);
        CHECK(again.staged);
        CHECK(again.gcode == whole.gcode);
        CHECK(again.check.ok() == whole.check.ok());
        CHECK(again.preview.estimated_seconds == whole.preview.estimated_seconds);
    }

    // A setting of the cut, another mesh or another painting: cut anew.
    Settings thinner = base;
    thinner.layer_height = 0.15;
    CHECK_FALSE(staged.run(placed, thinner).staged);
    CHECK_FALSE(staged.run(translated(placed, {1, 0, 0}), thinner).staged);
    Painting seam;
    seam.set_tree(PaintChannel::Seam, 0, {kPaintEnforce});
    CHECK_FALSE(staged.run(translated(placed, {1, 0, 0}), thinner, nullptr, &seam).staged);
    CHECK(staged.run(translated(placed, {1, 0, 0}), thinner, nullptr, &seam).staged);
}

TEST_CASE("ui: a resin printer slices into layer images and a preview of their outlines", "[ui][resin]") {
    Settings settings = default_settings();
    settings.machine_technology = MachineTechnology::Msla;
    settings.bed = {{0.0, 0.0}, {settings.display_width, 0.0}, {settings.display_width, settings.display_height},
                    {0.0, settings.display_height}};
    const TriangleMesh cube = read_cube();
    const TriangleMesh placed = translated(cube, centre_on_bed(cube.bounding_box(), settings.bed));
    Slicer slicer;
    const SliceResult r = slicer.run(placed, settings);
    REQUIRE(r.ok);
    REQUIRE(r.resin);
    CHECK_FALSE(r.model);
    CHECK(r.gcode.empty());
    // On supports over the pad by default: 2 + 5 mm under the part.
    CHECK_THAT(r.resin->layers.back().z, WithinAbs(7.0 + cube.bounding_box().max.z - cube.bounding_box().min.z, 1e-3));
    CHECK(r.preview.layers.size() == r.resin->layers.size());
    CHECK(r.preview.kind_moves[std::size_t(LineKind::Support)] > 0);
    CHECK(r.preview.estimated_seconds == r.resin->seconds);
    // What does not fit on the display is refused with the engine's reason.
    const SliceResult off = slicer.run(translated(cube, {-500, 0, 0}), settings);
    CHECK_FALSE(off.ok);
    CHECK_THAT(off.error, ContainsSubstring("display"));
}

TEST_CASE("ui: the slicing job gives checked G-code and its preview", "[ui]") {
    Settings settings = default_settings();
    settings.gpu = GpuMode::Off;
    CHECK(settings_json(settings).find("bed_temperature") != std::string::npos);
    CHECK(describe(parse_settings(settings_json(settings))) == describe(settings));

    const TriangleMesh cube = read_cube();
    const BoundingBox box = cube.bounding_box();
    const TriangleMesh placed = translated(cube, centre_on_bed(box, settings.bed));
    Slicer slicer;
    const SliceResult r = slicer.run(placed, settings);
    REQUIRE(r.ok);
    CHECK(r.error.empty());
    CHECK(r.check.ok());
    CHECK(r.gpu.empty());
    CHECK(r.preview.layers.size() == r.model->layers.size());
    CHECK(r.preview.estimated_seconds > 0.0);
    // The start G-code's variables are filled in.
    CHECK_THAT(r.gcode, ContainsSubstring("M104 S210"));
    CHECK_THAT(r.gcode, ContainsSubstring("M140 S60"));
    // The same G-code export_gcode would write.
    std::ostringstream again;
    write_gcode(again, *r.model, settings);
    CHECK(again.str() == r.gcode);

    // Off the bed: refused, with the engine's reason.
    const SliceResult off = slicer.run(translated(cube, {-500, 0, 0}), settings);
    CHECK_FALSE(off.ok);
    CHECK_THAT(off.error, ContainsSubstring("bed"));

    const VoxelEstimate v = estimate_voxels(placed, settings);
    REQUIRE(v.ok);
    REQUIRE(v.normal.ok);
    CHECK(v.normal.printed_voxels > 0);
    CHECK(v.normal.grid_voxels >= v.normal.part_voxels);
    CHECK(v.normal.part_voxels >= v.normal.printed_voxels);
    CHECK_FALSE(v.normal.over_limit);
    CHECK(v.high.grid_voxels > v.normal.grid_voxels);

    Background<int> job;
    CHECK_FALSE(job.take().has_value());
    job.start([] { return 42; });
    job.wait();
    const std::optional<int> got = job.take();
    REQUIRE(got.has_value());
    CHECK(*got == 42);
    CHECK_FALSE(job.running());
}

TEST_CASE("ui: the built-in profiles start from the interface's settings and read back", "[ui][profiles]") {
    const Settings defaults = default_settings();
    for (const ProfileKind kind : kProfileKinds) {
        const std::vector<Profile> profiles = builtin_profiles(kind);
        REQUIRE_FALSE(profiles.empty());
        INFO(std::string(profile_kind_name(kind)));
        // The first is what the interface starts with, so it shows unchanged.
        CHECK(same_profile(kind, profiles.front().settings, defaults));
        for (std::size_t i = 0; i < profiles.size(); ++i) {
            const Profile& p = profiles[i];
            INFO(p.name);
            CHECK(p.kind == kind);
            for (std::size_t j = 0; j < i; ++j)
                CHECK(profiles[j].name != p.name);
            const Profile back = parse_profile(profile_file_json(p));
            CHECK(same_profile(kind, back.settings, p.settings));
            if (i > 0)
                CHECK_FALSE(same_profile(kind, p.settings, defaults));
            // Each one slices, with the others' first profiles, to checked G-code.
            Settings mixed = with_profile(defaults, kind, p.settings);
            mixed.gpu = GpuMode::Off;
            const TriangleMesh cube = read_cube();
            Slicer slicer;
            const SliceResult r = slicer.run(translated(cube, centre_on_bed(cube.bounding_box(), mixed.bed)), mixed);
            REQUIRE(r.ok);
            CHECK(r.check.ok());
        }
    }
    const Profile petg = builtin_profiles(ProfileKind::Filament)[1];
    CHECK(petg.name == "PETG");
    CHECK(petg.settings.variables.at("temperature") == std::vector<std::string>{"240"});
}

TEST_CASE("ui: a background result is matched against what changed since it started", "[ui]") {
    Revision now;
    const Revision made = now;
    CHECK(fit(made, now) == Fit::Current);
    ++now.settings;
    CHECK(fit(made, now) == Fit::OutOfDate);
    ++now.model;
    CHECK(fit(made, now) == Fit::OtherModel);
    Revision other_model = made;
    ++other_model.model;
    CHECK(fit(made, other_model) == Fit::OtherModel);
}

TEST_CASE("ui: the slicer takes a load case for stress-guided infill", "[ui]") {
    // A bar of 60 x 10 x 10 on the bed, held at x = 100 and pushed down at x = 160.
    TriangleMesh bar;
    const Vec3f v[8] = {{100, 100, 0}, {160, 100, 0}, {160, 110, 0}, {100, 110, 0},
                        {100, 100, 10}, {160, 100, 10}, {160, 110, 10}, {100, 110, 10}};
    const int quads[6][4] = {{0, 3, 2, 1}, {4, 5, 6, 7}, {0, 1, 5, 4}, {1, 2, 6, 5}, {2, 3, 7, 6}, {3, 0, 4, 7}};
    for (const auto& q : quads) {
        bar.add_triangle(v[q[0]], v[q[1]], v[q[2]]);
        bar.add_triangle(v[q[0]], v[q[2]], v[q[3]]);
    }
    Settings s = default_settings();
    s.gpu = GpuMode::Off;
    s.infill_density = 10.0;
    s.infill_stress_density = 50.0;
    const AnalysisCase load = parse_analysis_case(
        R"({"model": "solid", "quality": "normal", "fixtures": [{"face": "x_min"}],
            "loads": [{"face": "x_max", "force": [0, 0, -20]}]})");
    auto held_over_free = [](const SliceResult& r) {
        double held = 0.0, free = 0.0;
        for (const LayerFill& f : r.model->fills)
            for (const Toolpath& t : f.sparse)
                for (std::size_t i = 1; i < t.points.size(); ++i) {
                    const double mx = 0.5 * (t.points[i].p.x + t.points[i - 1].p.x);
                    const double l = std::hypot(t.points[i].p.x - t.points[i - 1].p.x, t.points[i].p.y - t.points[i - 1].p.y);
                    if (mx < 120.0)
                        held += l;
                    else if (mx >= 140.0)
                        free += l;
                }
        return held / free;
    };
    Slicer slicer;
    const SliceResult guided = slicer.run(bar, s, &load);
    INFO(guided.error);
    REQUIRE(guided.ok);
    CHECK(guided.check.ok());
    CHECK(held_over_free(guided) > 1.5);
    // Without a case the infill stays even; with one that holds nothing, too, with a warning.
    const SliceResult even = slicer.run(bar, s);
    REQUIRE(even.ok);
    CHECK_FALSE(even.staged); // the guided cut does not serve a run without its case
    CHECK_THAT(held_over_free(even), Catch::Matchers::WithinAbs(1.0, 0.15));
    const AnalysisCase loose = parse_analysis_case(R"({"model": "solid", "loads": [{"face": "x_max", "force": [0, 0, -20]}]})");
    const SliceResult unheld = slicer.run(bar, s, &loose);
    REQUIRE(unheld.ok);
    CHECK_THAT(held_over_free(unheld), Catch::Matchers::WithinAbs(1.0, 0.15));
}

TEST_CASE("ui: the move slider draws the top layer up to a move (preview.md, rule 4)", "[ui]") {
    PreviewDrawOrder order;
    order.layer_shown = {0, 10, 25};
    order.layer_hidden = {25, 27, 30};
    // Layers 0 and 1, the top one cut after 4 shown and 1 hidden extrusions.
    const auto cut = preview_ranges(order, 0, 1, 0xFFFFFFFFu, 4, 1);
    CHECK(cut[0].first == 0);
    CHECK(cut[0].count == 14);
    CHECK(cut[1].first == 25);
    CHECK(cut[1].count == 3); // the hidden ones too: not all of the part is drawn
    // Whole: the hidden ones stay out.
    const auto whole = preview_ranges(order, 0, 1, 0xFFFFFFFFu);
    CHECK(whole[0].count == 25);
    CHECK(whole[1].count == 0);
    // A cut past the layer's end draws it all.
    CHECK(preview_ranges(order, 0, 1, 0xFFFFFFFFu, 100, 100)[0].count == 25);
}
