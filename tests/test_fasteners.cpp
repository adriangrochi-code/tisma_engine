// Metric fasteners for printing (the owner's request, 2026-11-02).

#include "cad/fasteners.hpp"
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
#include <sstream>
#include <tuple>

using namespace tisma;
using namespace tisma::cad;
using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

namespace {

constexpr double pi = std::numbers::pi;

using Key = std::tuple<float, float, float>;
Key key(const Vec3f& v) {
    return {v.x, v.y, v.z};
}

// Every edge used once in each direction: closed, and every face turned the
// same way.
bool closed_and_oriented(const TriangleMesh& m) {
    std::map<std::pair<Key, Key>, int> edges;
    for (std::size_t t = 0; t < m.triangle_count(); ++t)
        for (int k = 0; k < 3; ++k)
            ++edges[{key(m.corner(t, k)), key(m.corner(t, (k + 1) % 3))}];
    for (const auto& [edge, count] : edges) {
        if (count != 1)
            return false;
        const auto back = edges.find({edge.second, edge.first});
        if (back == edges.end() || back->second != 1)
            return false;
    }
    return true;
}

FastenerSpec spec_of(FastenerKind kind, const char* size, double length = 20.0, double play = 0.2) {
    FastenerSpec s;
    s.kind = kind;
    s.size = size;
    s.length = length;
    s.clearance = play;
    return s;
}

// The phase of a point of a right-hand thread of pitch p.
double phase(const Vec3f& v, double p) {
    double a = std::atan2(double(v.y), double(v.x));
    if (a < 0)
        a += 2.0 * pi;
    double u = double(v.z) / p - a / (2.0 * pi);
    return u - std::floor(u);
}

} // namespace

TEST_CASE("fasteners: the ISO 68-1 profile and the size table", "[fasteners]") {
    // M8: crest at 4, root at 4 - 5H/8, the pitch line where groove and tooth
    // are as wide (d2 = d - 0.6495 p).
    const double d = 8.0, p = 1.25, H = std::sqrt(3.0) / 2.0 * p;
    CHECK_THAT(thread_profile_radius(d, p, 0.05), WithinAbs(4.0, 1e-12));
    CHECK_THAT(thread_profile_radius(d, p, 0.55), WithinAbs(4.0 - 5.0 * H / 8.0, 1e-12));
    CHECK_THAT(thread_profile_radius(d, p, 1.05), WithinAbs(4.0, 1e-12)); // periodic
    const double d2 = d - 0.649519 * p;
    int above = 0;
    constexpr int kSamples = 100000;
    for (int i = 0; i < kSamples; ++i)
        above += thread_profile_radius(d, p, (i + 0.5) / kSamples) >= d2 / 2.0;
    CHECK_THAT(double(above) / kSamples, WithinAbs(0.5, 1e-3));
    // The flanks at 60°: the radius drops 5H/8 over 5p/16 of the axis.
    const double slope = (thread_profile_radius(d, p, 0.2) - thread_profile_radius(d, p, 0.3)) / (0.1 * p);
    CHECK_THAT(slope, WithinAbs(std::tan(pi / 3.0), 1e-9));

    REQUIRE(metric_sizes().size() == 10);
    const MetricSize* m8 = metric_size("m8");
    REQUIRE(m8);
    CHECK(m8->pitch == 1.25);
    CHECK(m8->nut_width == 13.0);
    CHECK(m8->clearance_hole == 9.0);
    CHECK(metric_size("M7") == nullptr);
    CHECK(metric_size("M5")->clearance_hole == 5.5); // the spec's CD5
}

TEST_CASE("fasteners: every kind is a closed mesh facing out", "[fasteners]") {
    for (const FastenerKind kind : kFastenerKinds)
        for (const char* size : {"M3", "M8", "M24"}) {
            INFO(fastener_id(kind) << " " << size);
            const TriangleMesh m = make_fastener(spec_of(kind, size));
            CHECK(m.triangle_count() > 0);
            CHECK(closed_and_oriented(m));
            CHECK(m.signed_volume() > 0.0);
            CHECK(m.bounding_box().min.z == 0.0f);
        }
    FastenerSpec custom = spec_of(FastenerKind::HexNut, "custom");
    custom.diameter = 40.0;
    custom.pitch = 3.0;
    CHECK(closed_and_oriented(make_fastener(custom)));
}

TEST_CASE("fasteners: sizes, volumes and the play between bolt and nut", "[fasteners]") {
    const MetricSize& m8 = *metric_size("M8");
    const double play = 0.2;

    SECTION("a hex bolt stands on its head, its thread above") {
        const TriangleMesh bolt = make_fastener(spec_of(FastenerKind::HexBolt, "M8", 30.0, play));
        const BoundingBox box = bolt.bounding_box();
        CHECK_THAT(box.max.z, WithinAbs(m8.head_height + 30.0, 1e-4));
        // The head's corners at s / cos 30°.
        CHECK_THAT(box.max.x - box.min.x, WithinAbs(2.0 * m8.nut_width / std::sqrt(3.0), 1e-3));
        // Every vertex of the thread on the profile, thinner by the play.
        int on_thread = 0;
        for (const Vec3f& v : bolt.corners()) {
            if (v.z <= m8.head_height + 1e-4 || v.z > m8.head_height + 30.0 - 1.0)
                continue; // the head, and the chamfered tip
            const double r = std::hypot(double(v.x), double(v.y));
            CHECK_THAT(r, WithinAbs(thread_profile_radius(8.0, 1.25, phase(v, 1.25)) - play, 2e-4));
            ++on_thread;
        }
        CHECK(on_thread > 1000);
    }
    SECTION("a nut's thread is wider by the play: 2 x play between them") {
        const TriangleMesh nut = make_fastener(spec_of(FastenerKind::HexNut, "M8", 0.0, play));
        CHECK_THAT(nut.bounding_box().max.z, WithinAbs(m8.nut_height, 1e-4));
        int on_thread = 0;
        for (const Vec3f& v : nut.corners()) {
            const double r = std::hypot(double(v.x), double(v.y));
            if (r > 4.0 + play + 1e-3 || v.z < 1.0 || v.z > m8.nut_height - 1.0)
                continue; // the outside, and the countersinks
            CHECK_THAT(r, WithinAbs(thread_profile_radius(8.0, 1.25, phase(v, 1.25)) + play, 2e-4));
            ++on_thread;
        }
        CHECK(on_thread > 100);
        // Less than the plain hexagonal prism, more than it minus the major
        // cylinder.
        const double prism = std::sqrt(3.0) / 2.0 * m8.nut_width * m8.nut_width * m8.nut_height;
        CHECK(nut.signed_volume() < prism);
        CHECK(nut.signed_volume() > prism * 0.9 - pi * 4.2 * 4.2 * m8.nut_height);
    }
    SECTION("a threaded rod holds the profile's mean area") {
        const double length = 40.0;
        const TriangleMesh rod = make_fastener(spec_of(FastenerKind::ThreadedRod, "M8", length, 0.0));
        double mean = 0.0;
        constexpr int kSamples = 20000;
        for (int i = 0; i < kSamples; ++i) {
            const double r = thread_profile_radius(8.0, 1.25, (i + 0.5) / kSamples);
            mean += r * r / kSamples;
        }
        // The polygon around (72 sides) and the two chamfers take a little.
        const double expected = pi * mean * length * std::sin(2.0 * pi / 72) * 72 / (2.0 * pi);
        CHECK_THAT(rod.signed_volume(), WithinRel(expected, 0.02));
        CHECK(rod.signed_volume() < expected);
    }
    SECTION("a washer is its ring") {
        const TriangleMesh w = make_fastener(spec_of(FastenerKind::Washer, "M8"));
        const double polygon = 72.0 / 2.0 * std::sin(2.0 * pi / 72.0);
        CHECK_THAT(w.signed_volume(), WithinRel(polygon * (8.0 * 8.0 - 4.2 * 4.2) * 1.6, 1e-5));
    }
    SECTION("a socket bolt's key hole opens on the bed") {
        const TriangleMesh bolt = make_fastener(spec_of(FastenerKind::SocketBolt, "M8", 20.0, play));
        CHECK_THAT(bolt.bounding_box().max.z, WithinAbs(8.0 + 20.0, 1e-4));
        bool hole_on_bed = false;
        for (const Vec3f& v : bolt.corners())
            if (v.z == 0.0f && std::hypot(double(v.x), double(v.y)) < (m8.socket_key / 2.0 + play) * 1.2)
                hole_on_bed = true;
        CHECK(hole_on_bed);
    }
}

TEST_CASE("fasteners: the checks, the pitch and the saved spec", "[fasteners]") {
    FastenerSpec s = spec_of(FastenerKind::HexBolt, "M8");
    CHECK(check_fastener(s).empty());
    s.size = "M7";
    CHECK_FALSE(check_fastener(s).empty());
    s.size = "M8";
    s.length = 1.0; // under two pitches
    CHECK_FALSE(check_fastener(s).empty());
    CHECK_THROWS_AS(make_fastener(s), std::invalid_argument);
    s.length = 20.0;
    s.clearance = 2.5;
    CHECK_FALSE(check_fastener(s).empty());
    s.clearance = 0.15;
    s.pitch = 1.0; // the fine M8 x 1
    CHECK(fastener_dims(s).pitch == 1.0);
    s.kind = FastenerKind::ThreadedRod;
    const FastenerSpec back = parse_fastener(fastener_json(s));
    CHECK(back.kind == FastenerKind::ThreadedRod);
    CHECK(back.size == "M8");
    CHECK(back.pitch == 1.0);
    CHECK(back.length == 20.0);
    CHECK(back.clearance == 0.15);
    CHECK_THROWS(parse_fastener("{\"format\":\"tisma-fastener-1\",\"kind\":\"rivet\"}"));
    CHECK_THROWS(parse_fastener("not json"));
    FastenerSpec custom = spec_of(FastenerKind::ThreadedRod, "custom");
    custom.diameter = 1.0;
    CHECK_FALSE(check_fastener(custom).empty());
    custom.diameter = 30.0;
    CHECK(check_fastener(custom).empty());
    CHECK(fastener_dims(custom).pitch == 4.5);
}

TEST_CASE("fasteners: an M8 nut and bolt slice and pass the G-code check", "[fasteners]") {
    for (const FastenerKind kind : {FastenerKind::HexNut, FastenerKind::HexBolt}) {
        INFO(fastener_id(kind));
        TriangleMesh part = make_fastener(spec_of(kind, "M8", 12.0));
        part.translate(100.0f, 100.0f, 0.0f);
        const Settings s = test_printer();
        const SlicedModel model = slice_model(weld_exact(part), s);
        REQUIRE(!model.layers.empty());
        std::ostringstream out;
        write_gcode(out, model, s);
        const GcodeCheck check = check_gcode(out.str(), gcode_limits(model, s));
        INFO((check.problems.empty() ? std::string() : check.problems.front().message));
        CHECK(check.ok());
    }
}
