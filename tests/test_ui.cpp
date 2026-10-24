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

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <cmath>
#include <cstring>
#include <filesystem>
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
        if (*p)
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
    for (std::size_t k = 0; k < kLineKinds; ++k)
        CHECK(kind_colour(LineKind(k)) <= 0xFFFFFF);
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
    const std::filesystem::path dir = std::filesystem::temp_directory_path() / "tisma_ui_files";
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
    CHECK(r.preview.layers.size() == r.model.layers.size());
    CHECK(r.preview.estimated_seconds > 0.0);
    // The start G-code's variables are filled in.
    CHECK_THAT(r.gcode, ContainsSubstring("M104 S210"));
    CHECK_THAT(r.gcode, ContainsSubstring("M140 S60"));
    // The same G-code export_gcode would write.
    std::ostringstream again;
    write_gcode(again, r.model, settings);
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
