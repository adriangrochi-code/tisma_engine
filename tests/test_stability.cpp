#include "config/settings.hpp"
#include "mesh/indexed_mesh.hpp"
#include "report/stability.hpp"
#include "slice/pipeline.hpp"
#include "test_printer.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <array>
#include <cmath>
#include <numbers>
#include <string>
#include <vector>

using namespace tisma;
using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

namespace {

// A prism over a counter-clockwise outline, from z0 to z1, its top moved
// `lean` mm in +X (a leaning bar), facing out.
void add_prism(IndexedMesh& m, const std::vector<std::array<float, 2>>& outline, float z0, float z1, float lean = 0.0f) {
    const auto base = std::uint32_t(m.vertices.size());
    const auto k = std::uint32_t(outline.size());
    for (const auto& p : outline)
        m.vertices.push_back({p[0], p[1], z0});
    for (const auto& p : outline)
        m.vertices.push_back({p[0] + lean, p[1], z1});
    const std::uint32_t bottom_centre = std::uint32_t(m.vertices.size());
    float cx = 0.0f, cy = 0.0f;
    for (const auto& p : outline)
        cx += p[0], cy += p[1];
    cx /= float(k), cy /= float(k);
    m.vertices.push_back({cx, cy, z0});
    m.vertices.push_back({cx + lean, cy, z1});
    for (std::uint32_t i = 0; i < k; ++i) {
        const std::uint32_t j = (i + 1) % k;
        m.triangles.push_back({base + i, base + j, base + k + j});
        m.triangles.push_back({base + i, base + k + j, base + k + i});
        m.triangles.push_back({bottom_centre, base + j, base + i});
        m.triangles.push_back({bottom_centre + 1, base + k + i, base + k + j});
    }
}

void add_box(IndexedMesh& m, float x0, float y0, float x1, float y1, float z0, float z1, float lean = 0.0f) {
    add_prism(m, {{x0, y0}, {x1, y0}, {x1, y1}, {x0, y1}}, z0, z1, lean);
}

std::vector<Warning> review(const IndexedMesh& m, const Settings& s) { return stability_warnings(slice_model(m, s), s); }

// Copies: the review given is often a temporary.
std::vector<Warning> of_kind(const std::vector<Warning>& all, const std::string& kind) {
    std::vector<Warning> out;
    for (const Warning& w : all)
        if (w.kind == kind)
            out.push_back(w);
    return out;
}

Settings plain() {
    Settings s = test_printer();
    s.skirt_loops = 0;
    return s;
}

} // namespace

TEST_CASE("stability: ST1, a part that starts in the air, critical and first", "[stability]") {
    IndexedMesh m;
    add_box(m, 0, 0, 10, 10, 0, 20);
    add_box(m, 20, 0, 25, 5, 10, 15);
    const std::vector<Warning> all = review(m, plain());
    const auto air = of_kind(all, "stability_air");
    REQUIRE(air.size() == 1);
    CHECK(air.front().severity == "critical");
    CHECK_THAT(air.front().z, WithinAbs(10.2, 1e-6));
    CHECK(all.front().kind == "stability_air"); // the most serious first
    // With supports, the review is not made (print_review.md, rule 6).
    Settings supported = plain();
    supported.support = true;
    CHECK(review(m, supported).empty());
}

TEST_CASE("stability: ST2, a bar leaning at 45 degrees tips off its footing at Z 10.4", "[stability]") {
    IndexedMesh m;
    add_box(m, 0, 0, 10, 10, 0, 30, 30.0f);
    const auto tipping = of_kind(review(m, plain()), "stability_overturn");
    REQUIRE(tipping.size() == 1);
    CHECK(tipping.front().severity == "warning");
    CHECK_THAT(tipping.front().z, WithinAbs(10.4, 1e-6));
}

TEST_CASE("stability: ST3 to ST5 and ST8, columns that bend under the nozzle", "[stability]") {
    IndexedMesh pillar;
    add_box(pillar, 0, 0, 4, 4, 0, 40);
    Settings s = plain();
    // ST4: not measured, the flexibility as information: L^3 / 166 400 mm/N at Z 40.
    const auto flexible = of_kind(review(pillar, s), "stability_flexibility");
    REQUIRE(flexible.size() == 1);
    CHECK(flexible.front().severity == "information");
    CHECK_THAT(flexible.front().z, WithinAbs(40.0, 1e-6));
    CHECK_THAT(flexible.front().value, WithinRel(40.0 * 40.0 * 40.0 / 166400.0, 0.01));
    CHECK(of_kind(review(pillar, s), "stability_bending").empty());
    // ST3: with a push of 1 N, 0.1 mm at L = 25.53: a warning from Z 25.6.
    s.review_nozzle_force = 1.0;
    const auto bending = of_kind(review(pillar, s), "stability_bending");
    REQUIRE(bending.size() == 1);
    CHECK_THAT(bending.front().z, WithinAbs(25.6, 1e-6));
    // ST5: a 20 mm cube does not bend, nor is listed without the push (R1).
    IndexedMesh cube;
    add_box(cube, 0, 0, 20, 20, 0, 20);
    CHECK(of_kind(review(cube, s), "stability_bending").empty());
    CHECK(review(cube, plain()).empty());
    // ST8: two pillars on a plate: their base is the plate (Z 2), from Z 27.6.
    IndexedMesh pair;
    add_box(pair, 0, 0, 30, 10, 0, 2);
    add_box(pair, 3, 3, 7, 7, 2, 40);
    add_box(pair, 23, 3, 27, 7, 2, 40);
    const auto pair_bending = of_kind(review(pair, s), "stability_bending");
    REQUIRE(pair_bending.size() == 1);
    CHECK_THAT(pair_bending.front().z, WithinAbs(27.6, 1e-6));
    // QA8: a 1 x 1 x 80 mm needle is reported (as information without the push).
    IndexedMesh needle;
    add_box(needle, 0, 0, 1, 1, 0, 80);
    const auto needle_flex = of_kind(review(needle, plain()), "stability_flexibility");
    REQUIRE_FALSE(needle_flex.empty());
    CHECK(needle_flex.front().value > 500.0); // mm per newton
}

TEST_CASE("stability: ST7, a thin cylinder peels off the bed from Z 6.4", "[stability]") {
    std::vector<std::array<float, 2>> circle;
    for (int k = 0; k < 96; ++k) {
        const double a = 2.0 * std::numbers::pi * k / 96.0;
        circle.push_back({float(2.0 * std::cos(a)), float(2.0 * std::sin(a))});
    }
    IndexedMesh m;
    add_prism(m, circle, 0, 30);
    Settings s = plain();
    s.review_nozzle_force = 1.0;
    CHECK(of_kind(review(m, s), "stability_peel").empty()); // without the hold measured
    s.review_bed_adhesion = 1.0;
    const auto peel = of_kind(review(m, s), "stability_peel");
    REQUIRE(peel.size() == 1);
    CHECK_THAT(peel.front().z, WithinAbs(6.4, 1e-6));
}
