// The support on hard parts, in the three styles, checked on the G-code
// (tests/support_check.hpp): no overhang past the angle without support under
// it, no island in the air, no support standing on nothing (the owner's
// request through QA, 2026-11-16).

#include "config/settings.hpp"
#include "gcode/preview.hpp"
#include "gcode/writer.hpp"
#include "io/stl.hpp"
#include "mesh/indexed_mesh.hpp"
#include "mesh/shapes.hpp"
#include "slice/pipeline.hpp"
#include "support_check.hpp"
#include "test_printer.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <numbers>
#include <sstream>
#include <string>
#include <vector>

using namespace tisma;

namespace {

constexpr double kPi = std::numbers::pi;

void append(TriangleMesh& to, const TriangleMesh& from) {
    for (std::size_t t = 0; t < from.triangle_count(); ++t)
        to.add_triangle(from.corner(t, 0), from.corner(t, 1), from.corner(t, 2));
}

// Every corner through f.
TriangleMesh mapped(const TriangleMesh& m, const std::function<Vec3f(Vec3f)>& f) {
    TriangleMesh out;
    for (std::size_t t = 0; t < m.triangle_count(); ++t)
        out.add_triangle(f(m.corner(t, 0)), f(m.corner(t, 1)), f(m.corner(t, 2)));
    return out;
}

// A box from (x0, y0, z0) to (x1, y1, z1).
TriangleMesh box(double x0, double y0, double z0, double x1, double y1, double z1) {
    TriangleMesh b = make_box(x1 - x0, y1 - y0, z1 - z0);
    b.translate(float(0.5 * (x0 + x1)), float(0.5 * (y0 + y1)), float(z0));
    return b;
}

TriangleMesh rotated_x(const TriangleMesh& m, double degrees) {
    const double a = degrees * kPi / 180.0, c = std::cos(a), s = std::sin(a);
    return mapped(m, [&](Vec3f p) { return Vec3f{p.x, float(c * p.y - s * p.z), float(s * p.y + c * p.z)}; });
}

TriangleMesh rotated_y(const TriangleMesh& m, double degrees) {
    const double a = degrees * kPi / 180.0, c = std::cos(a), s = std::sin(a);
    return mapped(m, [&](Vec3f p) { return Vec3f{float(c * p.x + s * p.z), p.y, float(-s * p.x + c * p.z)}; });
}

TriangleMesh rotated_z(const TriangleMesh& m, double degrees) {
    const double a = degrees * kPi / 180.0, c = std::cos(a), s = std::sin(a);
    return mapped(m, [&](Vec3f p) { return Vec3f{float(c * p.x - s * p.y), float(s * p.x + c * p.y), p.z}; });
}

TriangleMesh scaled(const TriangleMesh& m, double k) {
    return mapped(m, [&](Vec3f p) { return Vec3f{float(k * p.x), float(k * p.y), float(k * p.z)}; });
}

// On the bed, in the middle of the test printer's plate.
TriangleMesh placed(const TriangleMesh& m) {
    float lo[3] = {1e9f, 1e9f, 1e9f}, hi[3] = {-1e9f, -1e9f, -1e9f};
    for (std::size_t t = 0; t < m.triangle_count(); ++t)
        for (int k = 0; k < 3; ++k) {
            const Vec3f p = m.corner(t, k);
            lo[0] = std::min(lo[0], p.x), lo[1] = std::min(lo[1], p.y), lo[2] = std::min(lo[2], p.z);
            hi[0] = std::max(hi[0], p.x), hi[1] = std::max(hi[1], p.y), hi[2] = std::max(hi[2], p.z);
        }
    TriangleMesh out = m;
    out.translate(100.0f - 0.5f * (lo[0] + hi[0]), 100.0f - 0.5f * (lo[1] + hi[1]), -lo[2]);
    return out;
}

// QA's parts (2026-11-16).
TriangleMesh tee() { // a column of 10 x 10 x 15 with a bar of 40 x 10 x 4 on top
    TriangleMesh m = box(-5, -5, 0, 5, 5, 15);
    append(m, box(-20, -5, 15, 20, 5, 19));
    return m;
}

// A column of 6 x 10 x 10 and an arm of 20 x 10 x 2 leaving its top at
// `degrees` from the vertical (its underside hangs that much).
TriangleMesh ramp(double degrees) {
    TriangleMesh m = box(-3, -5, 0, 3, 5, 10);
    // The arm along +X from the column's top edge, turned down by the angle
    // from the horizontal (90 - degrees).
    TriangleMesh arm = box(0, -5, -2, 20, 5, 0);
    arm = rotated_y(arm, -(90.0 - degrees));
    arm.translate(3.0f, 0.0f, 10.0f);
    append(m, arm);
    return m;
}

TriangleMesh island() { // a 10 mm cube and a block of 6 x 6 x 3 in the air, 15 mm away
    TriangleMesh m = box(-5, -5, 0, 5, 5, 10);
    append(m, box(17, -3, 10.5, 23, 3, 13.5));
    return m;
}

TriangleMesh bridge(double span) { // two pillars of 5 x 10 x 15, a lid of 2 mm
    TriangleMesh m = box(0, 0, 0, 5, 10, 15);
    append(m, box(5 + span, 0, 0, 10 + span, 10, 15));
    append(m, box(0, 0, 15, 10 + span, 10, 17));
    return m;
}

TriangleMesh hole() { // a lying tube: a horizontal hole and a round underside
    return rotated_x(make_tube(10.0, 5.0, 20.0, 96), 90.0);
}

// Not manifold: two boxes that cross (no union), a shelf that shares an
// edge with the column, and a triangle missing.
TriangleMesh broken() {
    TriangleMesh m = box(-5, -5, 0, 5, 5, 12);
    append(m, box(-2, -2, 6, 12, 2, 9)); // crosses the column, hangs past it
    append(m, box(5, 3, 10, 15, 8, 12)); // touches the column along an edge only
    std::vector<Vec3f> corners;
    for (std::size_t t = 1; t < m.triangle_count(); ++t)
        for (int k = 0; k < 3; ++k)
            corners.push_back(m.corner(t, k));
    return TriangleMesh(std::move(corners));
}

struct Case {
    std::string name;
    TriangleMesh mesh;
    bool needs_support = true;
};

std::vector<Case> cases() {
    std::vector<Case> out;
    out.push_back({"tee", tee()});
    for (double a : {40.0, 50.0, 60.0, 75.0})
        out.push_back({"ramp " + std::to_string(int(a)), ramp(a), a > 45.0});
    out.push_back({"island", island()});
    out.push_back({"bridge 40 mm", bridge(40.0), false}); // a bridge: no support (supports_skirt.md, rule 3)
    out.push_back({"hole", hole()});
    out.push_back({"tee turned 30 about X", rotated_x(tee(), 30.0)});
    out.push_back({"ramp 60 turned 35 about Z", rotated_z(ramp(60.0), 35.0)});
    out.push_back({"tee at 0.5", scaled(tee(), 0.5)});
    out.push_back({"tee at 3", scaled(tee(), 3.0)});
    out.push_back({"sphere, many triangles", make_sphere(15.0, 256, 512)});
    out.push_back({"not manifold", broken()});
    if (const char* path = std::getenv("TISMA_BENCHY_STL"))
        out.push_back({"Benchy", read_stl(path).mesh});
    return out;
}

support_check::Report run(const TriangleMesh& mesh, const Settings& s, const std::string& name = {}) {
    std::ostringstream out;
    write_gcode(out, slice_model(weld_exact(placed(mesh)), s), s);
    // TISMA_SWEEP_DUMP=<dir>: each case's G-code there, to look at.
    if (const char* dir = std::getenv("TISMA_SWEEP_DUMP"); dir && !name.empty()) {
        std::string file = std::string(dir) + "/" + name + ".gcode";
        std::replace(file.begin() + long(std::string(dir).size()) + 1, file.end(), ' ', '_');
        if (FILE* f = std::fopen(file.c_str(), "wb")) {
            const std::string text = out.str();
            std::fwrite(text.data(), 1, text.size(), f);
            std::fclose(f);
        }
    }
    support_check::Options o;
    o.z_gap = s.support_z_gap;
    o.xy_gap = s.support_xy_gap.of(s.outer_wall_width > 0.0 ? s.outer_wall_width : 1.125 * s.nozzle_diameter);
    return support_check::check(read_gcode_preview(out.str()), o);
}

const char* style_name(SupportStyle s) {
    return s == SupportStyle::Grid ? "grid" : s == SupportStyle::Organic ? "organic" : "snug";
}

} // namespace

TEST_CASE("support check: a lone overhang is caught, and support under it clears it", "[support-check]") {
    Settings s = test_printer();
    const support_check::Report bare = run(tee(), s);
    CHECK(bare.unsupported_area > 50.0);
    s.support = true;
    const support_check::Report held = run(tee(), s);
    INFO("unsupported " << support_check::describe(held.worst_unsupported));
    CHECK(held.support_moves > 0);
    CHECK(held.unsupported_area < 0.01 * bare.unsupported_area);
}

TEST_CASE("support sweep: hard parts in the three styles", "[.support-sweep]") {
    for (const Case& c : cases())
        for (SupportStyle style : {SupportStyle::Grid, SupportStyle::Organic, SupportStyle::Snug}) {
            // TISMA_SWEEP_ONLY=<text>: only the cases whose name and style hold it.
            if (const char* only = std::getenv("TISMA_SWEEP_ONLY");
                only && (c.name + " " + style_name(style)).find(only) == std::string::npos)
                continue;
            Settings s = test_printer();
            s.support = true;
            s.support_style = style;
            const support_check::Report r = run(c.mesh, s, c.name + " " + style_name(style));
            std::printf("%-28s %-8s part %9.1f unsupported %7.2f (worst %s) support %8.1f floating %6.2f (worst %s)\n",
                        c.name.c_str(), style_name(style), r.part_area, r.unsupported_area,
                        support_check::describe(r.worst_unsupported).c_str(), r.support_area, r.floating_area,
                        support_check::describe(r.worst_floating).c_str());
            std::printf("%-28s %-8s fused side %.2f top %.2f bottom %.2f (worst %s), on interface %.1f base %.1f\n", c.name.c_str(),
                        style_name(style), r.fused_side, r.fused_top, r.fused_bottom, support_check::describe(r.worst_fused).c_str(),
                        r.on_interface, r.on_base);
            INFO(c.name << ", " << style_name(style));
            INFO("unsupported: " << support_check::describe(r.worst_unsupported));
            INFO("floating: " << support_check::describe(r.worst_floating));
            if (c.needs_support)
                CHECK(r.support_moves > 0);
            CHECK(r.worst_unsupported.area < 1.0);
            CHECK(r.worst_floating.area < 1.0);
            // Quality (point 2): the gaps kept, so it comes off; the part on interface.
            INFO("fused: " << support_check::describe(r.worst_fused));
            CHECK(r.fused_side < 1.0);
            CHECK(r.fused_top < 1.0);
            CHECK(r.fused_bottom < 1.0);
            CHECK(r.on_base <= 0.02 * r.on_interface + 1.0);
        }
}


TEST_CASE("walls: the stem of a T keeps its wall from the inner corner up", "[support-check]") {
    // A bar along X and a stem on it, as a layer of QA's T turned 30 degrees
    // cuts them: the variable walls left the stem's face bare from the inner
    // corner (2.2 mm with a 3 mm bar, 9.5 mm with 1.64).
    for (double bar : {1.64, 3.0}) {
        Settings s = test_printer();
        TriangleMesh m = box(80, 92.51, 0, 120, 92.51 + bar, 2);
        append(m, box(95, 92.51 + bar, 0, 105, 104.06, 2));
        const SlicedModel sliced = slice_model(weld_exact(m), s);
        for (std::size_t l = 1; l < sliced.walls.size(); ++l) {
            // Along the stem's left face, the outer bead's line, every 0.25 mm.
            for (double y = 92.51 + bar + 0.6; y < 103.5; y += 0.25) {
                double best = 1e9;
                for (const Toolpath& t : sliced.walls[l].toolpaths)
                    for (std::size_t i = 0; i + 1 < t.points.size() + (t.closed ? 1 : 0); ++i) {
                        const Point2d a = t.points[i].p, b = t.points[(i + 1) % t.points.size()].p;
                        const double dx = b.x - a.x, dy = b.y - a.y, len2 = dx * dx + dy * dy;
                        const double u = len2 > 0 ? std::clamp(((95.225 - a.x) * dx + (y - a.y) * dy) / len2, 0.0, 1.0) : 0.0;
                        best = std::min(best, std::hypot(a.x + u * dx - 95.225, a.y + u * dy - y));
                    }
                INFO("bar " << bar << ", layer " << l << ", y " << y);
                REQUIRE(best < 0.1);
            }
        }
    }
}
