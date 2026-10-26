// Calibration parts and compensation. Own design; see PROVENANCE.md.

#include "calibration/calibration.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <numbers>
#include <utility>

namespace tisma {
namespace {

constexpr int kHoleSegments = 96;   // sides of the hole's polygon
constexpr double kNeck = 1.0;       // mm: the neck between tower segments
constexpr double kNeckInset = 1.5;  // mm: how much narrower the neck is on each side
constexpr std::size_t kMaxSegments = 40;

using P = Vec3f;

void quad(TriangleMesh& m, P a, P b, P c, P d) {
    m.add_triangle(a, b, c);
    m.add_triangle(a, c, d);
}

// A box with its faces wound outwards.
void add_box(TriangleMesh& m, double x0, double y0, double z0, double x1, double y1, double z1) {
    const auto p = [](double x, double y, double z) { return P{float(x), float(y), float(z)}; };
    quad(m, p(x0, y0, z0), p(x0, y1, z0), p(x1, y1, z0), p(x1, y0, z0)); // bottom
    quad(m, p(x0, y0, z1), p(x1, y0, z1), p(x1, y1, z1), p(x0, y1, z1)); // top
    quad(m, p(x0, y0, z0), p(x1, y0, z0), p(x1, y0, z1), p(x0, y0, z1)); // -Y
    quad(m, p(x1, y1, z0), p(x0, y1, z0), p(x0, y1, z1), p(x1, y1, z1)); // +Y
    quad(m, p(x0, y1, z0), p(x0, y0, z0), p(x0, y0, z1), p(x0, y1, z1)); // -X
    quad(m, p(x1, y0, z0), p(x1, y1, z0), p(x1, y1, z1), p(x1, y0, z1)); // +X
}

// A segment of a tower and the neck above it (none above the last).
void add_segment(TriangleMesh& m, double x0, double y0, double x1, double y1, double z0, double height, bool last) {
    const double top = last ? z0 + height : z0 + height - kNeck;
    add_box(m, x0, y0, z0, x1, y1, top);
    if (!last)
        add_box(m, x0 + kNeckInset, y0 + kNeckInset, top, x1 - kNeckInset, y1 - kNeckInset, z0 + height);
}

// Points in double precision while a part is built.
struct V {
    double x = 0, y = 0, z = 0;
    friend V operator+(V a, V b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
    friend V operator-(V a, V b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
    friend V operator*(V a, double s) { return {a.x * s, a.y * s, a.z * s}; }
};
double dot(V a, V b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
V cross(V a, V b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
P to_f(V v) { return P{float(v.x), float(v.y), float(v.z)}; }

// A planar (or nearly planar) face as a fan, wound so its normal points away
// from `inside`, a point inside the convex solid it bounds.
void add_face(TriangleMesh& m, const std::vector<V>& loop, V inside) {
    for (std::size_t i = 1; i + 1 < loop.size(); ++i) {
        V a = loop[0], b = loop[i], c = loop[i + 1];
        if (dot(cross(b - a, c - a), a - inside) < 0.0)
            std::swap(b, c);
        m.add_triangle(to_f(a), to_f(b), to_f(c));
    }
}

// The solid c +- a +- b +- h (a parallelepiped; a box when the half axes are
// orthogonal).
void add_solid(TriangleMesh& m, V c, V a, V b, V h) {
    const auto corner = [&](int i, int j, int k) { return c + a * double(i) + b * double(j) + h * double(k); };
    for (int s : {-1, 1}) {
        add_face(m, {corner(s, -1, -1), corner(s, 1, -1), corner(s, 1, 1), corner(s, -1, 1)}, c);
        add_face(m, {corner(-1, s, -1), corner(1, s, -1), corner(1, s, 1), corner(-1, s, 1)}, c);
        add_face(m, {corner(-1, -1, s), corner(1, -1, s), corner(1, 1, s), corner(-1, 1, s)}, c);
    }
}

// A prism: the polygon `section` (convex, in the plane of u and v) swept along
// w from 0 to length, from origin.
void add_prism(TriangleMesh& m, V origin, V u, V v, V w, const std::vector<std::array<double, 2>>& section,
               double length) {
    std::vector<V> near, far;
    V inside{};
    for (const auto& q : section) {
        near.push_back(origin + u * q[0] + v * q[1]);
        far.push_back(near.back() + w * length);
        inside = inside + near.back() + far.back();
    }
    inside = inside * (1.0 / double(2 * section.size()));
    add_face(m, near, inside);
    add_face(m, far, inside);
    for (std::size_t i = 0; i < section.size(); ++i) {
        const std::size_t j = (i + 1) % section.size();
        add_face(m, {near[i], near[j], far[j], far[i]}, inside);
    }
}

// The value of a segment as text: whole numbers without decimals.
std::string value_text(double v) {
    char text[32];
    if (std::abs(v - std::round(v)) < 1e-6)
        std::snprintf(text, sizeof text, "%.0f", v);
    else if (std::abs(v * 10.0 - std::round(v * 10.0)) < 1e-6)
        std::snprintf(text, sizeof text, "%.1f", v);
    else if (std::abs(v * 100.0 - std::round(v * 100.0)) < 1e-6)
        std::snprintf(text, sizeof text, "%.2f", v);
    else // steps of 0.001 (pressure advance): two decimals made neighbours look alike
        std::snprintf(text, sizeof text, "%.3f", v);
    return text;
}

// A tower's changes and segments for these values; set(change, value) writes
// the value into the change.
template <class Set>
void tower_changes(CalibrationPrint& out, const std::vector<double>& values, double segment, Set set) {
    for (std::size_t i = 0; i < values.size(); ++i) {
        HeightChange c;
        c.z = double(i) * segment;
        set(c, values[i]);
        out.changes.push_back(c);
        out.segments.push_back({double(i) * segment, double(i + 1) * segment, values[i]});
    }
}

// A block hx by hy each side of (cx, cy), from z0 to z1, with a round hole of
// radius r through it. The angles of the hole's sides and of the block's
// corners are merged, so the block keeps its corners: each angle gives a
// point on the hole and the point on the block's outline along the same ray.
void add_block_with_hole(TriangleMesh& m, double cx, double cy, double hx, double hy, double r, double z0, double z1) {
    std::vector<double> angles;
    for (int i = 0; i < kHoleSegments; ++i)
        angles.push_back(2.0 * std::numbers::pi * i / kHoleSegments);
    const double corner = std::atan2(hy, hx);
    for (double a : {corner, std::numbers::pi - corner, std::numbers::pi + corner, 2.0 * std::numbers::pi - corner})
        angles.push_back(a);
    std::sort(angles.begin(), angles.end());
    angles.erase(std::unique(angles.begin(), angles.end(), [](double a, double b) { return std::abs(a - b) < 1e-9; }),
                 angles.end());
    std::vector<std::array<double, 2>> inner, outer;
    for (double a : angles) {
        const double c = std::cos(a), s = std::sin(a);
        inner.push_back({cx + r * c, cy + r * s});
        const double reach = std::min(std::abs(c) > 1e-12 ? hx / std::abs(c) : 1e300, std::abs(s) > 1e-12 ? hy / std::abs(s) : 1e300);
        outer.push_back({cx + reach * c, cy + reach * s});
    }
    const auto p = [](const std::array<double, 2>& q, double z) { return P{float(q[0]), float(q[1]), float(z)}; };
    const std::size_t n = angles.size();
    for (std::size_t i = 0; i < n; ++i) {
        const std::size_t j = (i + 1) % n;
        quad(m, p(inner[i], z1), p(outer[i], z1), p(outer[j], z1), p(inner[j], z1)); // top
        quad(m, p(inner[j], z0), p(outer[j], z0), p(outer[i], z0), p(inner[i], z0)); // bottom
        quad(m, p(outer[i], z0), p(outer[j], z0), p(outer[j], z1), p(outer[i], z1)); // outside
        quad(m, p(inner[j], z0), p(inner[i], z0), p(inner[i], z1), p(inner[j], z1)); // the hole
    }
}

// An upright cylinder of radius r around (cx, cy), from z0 to z1.
void add_cylinder(TriangleMesh& m, double cx, double cy, double r, double z0, double z1) {
    std::vector<std::array<double, 2>> ring;
    for (int i = 0; i < kHoleSegments; ++i) {
        const double a = 2.0 * std::numbers::pi * i / kHoleSegments;
        ring.push_back({r * std::cos(a), r * std::sin(a)});
    }
    add_prism(m, V{cx, cy, z0}, V{1, 0, 0}, V{0, 1, 0}, V{0, 0, 1}, ring, z1 - z0);
}

// The retraction towers' two columns, 30 mm apart, with the values on the
// left one.
void retraction_columns(CalibrationPrint& out, const std::vector<double>& values, double segment) {
    for (std::size_t i = 0; i < values.size(); ++i) {
        const bool last = i + 1 == values.size();
        add_segment(out.mesh, -19.0, -4.0, -11.0, 4.0, double(i) * segment, segment, last);
        add_segment(out.mesh, 11.0, -4.0, 19.0, 4.0, double(i) * segment, segment, last);
        add_label(out.mesh, value_text(values[i]), -18.4, double(i) * segment + 0.6, -4.0, true,
                  std::min(3.0, segment - kNeck - 1.0), 0.6, 0.5);
    }
}

// A square tube from z0 to z1: outer half width h, walls t thick.
void add_tube(TriangleMesh& m, double h, double t, double z0, double z1) {
    add_box(m, -h, -h, z0, h, -h + t, z1);
    add_box(m, -h, h - t, z0, h, h, z1);
    add_box(m, -h, -h + t, z0, -h + t, h - t, z1);
    add_box(m, h - t, -h + t, z0, h, h - t, z1);
}

// The values each kind takes in the settings and in the changes by height
// (docs/CONFIG.md): a tower or a chosen value past them would make settings,
// profiles and projects that cannot be read back.
std::pair<double, double> calibration_range(CalibrationKind kind) {
    switch (kind) {
    case CalibrationKind::Temperature: return {100.0, 500.0};
    case CalibrationKind::Retraction: return {0.0, 50.0};
    case CalibrationKind::Flow: return {10.0, 300.0};
    case CalibrationKind::Speed: return {5.0, 1000.0};
    case CalibrationKind::RetractionSpeed: return {1.0, 500.0};
    case CalibrationKind::PressureAdvance: return {0.0, 2.0};
    case CalibrationKind::MaxFlow: return {0.5, 1000.0};
    case CalibrationKind::Acceleration: return {100.0, 100000.0};
    default: return {-1e9, 1e9};
    }
}

void within(CalibrationKind kind, double& start, double& end) {
    const auto [lo, hi] = calibration_range(kind);
    start = std::clamp(start, lo, hi);
    end = std::clamp(end, lo, hi);
}

} // namespace

std::vector<double> tower_values(double start, double end, double step) {
    std::vector<double> out;
    step = std::abs(step);
    if (!(step > 0.0) || !std::isfinite(start) || !std::isfinite(end))
        return {start};
    const double direction = end >= start ? 1.0 : -1.0;
    for (std::size_t i = 0; i < kMaxSegments; ++i) {
        const double v = start + direction * step * double(i);
        if ((v - end) * direction > step * 1e-6)
            break;
        out.push_back(v);
    }
    return out;
}

CalibrationPrint dimension_test(const DimensionTest& t) {
    CalibrationPrint out;
    out.kind = "dimensions";
    out.unit = "mm";
    const double hx = t.x / 2.0, hy = t.y / 2.0;
    const double r = std::clamp(t.hole / 2.0, 0.5, std::min(hx, hy) - 1.0);
    add_block_with_hole(out.mesh, 0.0, 0.0, hx, hy, r, 0.0, t.z);
    return out;
}

CalibrationPrint temperature_tower(double start, double end, double step, double segment) {
    within(CalibrationKind::Temperature, start, end);
    CalibrationPrint out;
    out.kind = "temperature";
    out.unit = "C";
    segment = std::max(segment, 10.0);
    const std::vector<double> values = tower_values(start, end, step);
    constexpr double kRidge = 4.0;                                   // mm: the ridge's height
    const double reach_low = kRidge * std::tan(30.0 * std::numbers::pi / 180.0);
    const double reach_high = kRidge * std::tan(72.0 * std::numbers::pi / 180.0);
    for (std::size_t i = 0; i < values.size(); ++i) {
        const double z0 = double(i) * segment;
        const bool last = i + 1 == values.size();
        const double top = last ? z0 + segment : z0 + segment - kNeck;
        add_segment(out.mesh, -12.0, -6.0, 12.0, 6.0, z0, segment, last);

        // The ridge, on the left: its overhang angle from the vertical grows
        // from 30 degrees at the front to 72 at the back.
        const V b0{-12.0, -6.0, top - kRidge}, b1{-12.0, 6.0, top - kRidge};
        const V i0{-12.0, -6.0, top}, i1{-12.0, 6.0, top};
        const V t0{-12.0 - reach_low, -6.0, top}, t1{-12.0 - reach_high, 6.0, top};
        const V inside = (b0 + b1 + i0 + i1 + t0 + t1) * (1.0 / 6.0);
        add_face(out.mesh, {b0, i0, t0}, inside);
        add_face(out.mesh, {b1, i1, t1}, inside);
        add_face(out.mesh, {i0, t0, t1, i1}, inside);
        add_face(out.mesh, {b0, t0, t1}, inside); // the overhang, a twisted face in two triangles
        add_face(out.mesh, {b0, t1, b1}, inside);
        add_face(out.mesh, {b0, b1, i1, i0}, inside);

        // The flat shelf, on the right, with nothing under it.
        add_box(out.mesh, 12.0, -4.0, top - 1.6, 20.0, 4.0, top);

        // The 45 degree bracket at the back and the pin standing on it.
        const double bracket = z0 + 0.5;
        add_prism(out.mesh, V{-3.0, 6.0, bracket}, V{0, 1, 0}, V{0, 0, 1}, V{1, 0, 0},
                  {{0.0, 0.0}, {4.0, 4.0}, {0.0, 4.0}}, 6.0);
        add_box(out.mesh, -1.2, 7.6, bracket + 4.0, 1.2, 10.0, top - 0.2);

        // The front: the temperature, a diamond and a ring in relief.
        add_label(out.mesh, value_text(values[i]), -11.0, z0 + 2.0, -6.0, true, 5.0, 0.8, 0.6);
        const double cz = z0 + 4.5;
        const auto stroke = [&](double x0, double y0, double x1, double y1) {
            const double len = std::hypot(x1 - x0, y1 - y0);
            const V d{(x1 - x0) / len, 0.0, (y1 - y0) / len};
            const V n{-d.z, 0.0, d.x};
            add_solid(out.mesh, V{(x0 + x1) / 2.0, -6.0 - 0.2, (y0 + y1) / 2.0}, d * (len / 2.0 + 0.4),
                      V{0.0, 0.4, 0.0}, n * 0.4);
        };
        stroke(2.0, cz, 4.2, cz + 2.2);
        stroke(4.2, cz + 2.2, 6.4, cz);
        stroke(6.4, cz, 4.2, cz - 2.2);
        stroke(4.2, cz - 2.2, 2.0, cz);
        for (int k = 0; k < 8; ++k) {
            const double a0 = 2.0 * std::numbers::pi * k / 8.0, a1 = 2.0 * std::numbers::pi * (k + 1) / 8.0;
            stroke(9.2 + 1.8 * std::cos(a0), cz + 1.8 * std::sin(a0), 9.2 + 1.8 * std::cos(a1), cz + 1.8 * std::sin(a1));
        }
    }
    tower_changes(out, values, segment, [](HeightChange& c, double v) { c.temperature = v; });
    return out;
}

CalibrationPrint retraction_tower(double start, double end, double step, double segment) {
    within(CalibrationKind::Retraction, start, end);
    CalibrationPrint out;
    out.kind = "retraction";
    out.unit = "mm";
    const std::vector<double> values = tower_values(start, end, step);
    retraction_columns(out, values, segment);
    tower_changes(out, values, segment, [](HeightChange& c, double v) { c.retract_length = v; });
    return out;
}

CalibrationPrint retraction_speed_tower(double start, double end, double step, double segment) {
    within(CalibrationKind::RetractionSpeed, start, end);
    CalibrationPrint out;
    out.kind = "retraction_speed";
    out.unit = "mm/s";
    const std::vector<double> values = tower_values(start, end, step);
    retraction_columns(out, values, segment);
    tower_changes(out, values, segment, [](HeightChange& c, double v) { c.retract_speed = v; });
    return out;
}

// The hollow square tower of the pressure advance, flow limit and
// acceleration tests: 36 mm wide, 1.6 mm walls, sharp corners, a shallow
// groove between segments, each segment's value on the front.
void tube_tower(CalibrationPrint& out, const std::vector<double>& values, double segment) {
    constexpr double kHalf = 18.0, kWall = 1.6, kGroove = 0.6, kGrooveDepth = 0.4;
    for (std::size_t i = 0; i < values.size(); ++i) {
        const double z0 = double(i) * segment;
        const bool last = i + 1 == values.size();
        // The groove at the top of each segment but the last: the outer
        // faces step in, the inner ones stay.
        const double top = last ? z0 + segment : z0 + segment - kGroove;
        add_tube(out.mesh, kHalf, kWall, z0, top);
        if (!last)
            add_tube(out.mesh, kHalf - kGrooveDepth, kWall - kGrooveDepth, top, z0 + segment);
        add_label(out.mesh, value_text(values[i]), -16.5, z0 + 0.6, -kHalf, true, std::min(3.0, segment - kGroove - 1.0),
                  0.6, 0.5);
    }
}

CalibrationPrint pressure_advance_tower(double start, double end, double step, double segment) {
    within(CalibrationKind::PressureAdvance, start, end);
    CalibrationPrint out;
    out.kind = "pressure_advance";
    out.unit = "";
    segment = std::max(segment, 3.0);
    const std::vector<double> values = tower_values(start, end, step);
    tube_tower(out, values, segment);
    tower_changes(out, values, segment, [](HeightChange& c, double v) { c.pressure_advance = v; });
    return out;
}

CalibrationPrint max_flow_tower(double start, double end, double step, double segment) {
    within(CalibrationKind::MaxFlow, start, end);
    CalibrationPrint out;
    out.kind = "max_flow";
    out.unit = "mm³/s";
    segment = std::max(segment, 3.0);
    const std::vector<double> values = tower_values(start, end, step);
    tube_tower(out, values, segment);
    tower_changes(out, values, segment, [](HeightChange& c, double v) { c.volumetric_flow = v; });
    return out;
}

CalibrationPrint acceleration_tower(double start, double end, double step, double segment) {
    within(CalibrationKind::Acceleration, start, end);
    CalibrationPrint out;
    out.kind = "acceleration";
    out.unit = "mm/s²";
    segment = std::max(segment, 3.0);
    const std::vector<double> values = tower_values(start, end, step);
    tube_tower(out, values, segment);
    tower_changes(out, values, segment, [](HeightChange& c, double v) { c.acceleration = v; });
    return out;
}

CalibrationPrint elephant_foot_test() {
    CalibrationPrint out;
    out.kind = "elephant_foot";
    out.unit = "mm";
    add_box(out.mesh, -10.0, -10.0, 0.0, 10.0, 10.0, 6.0);
    return out;
}

double elephant_foot_from(double bottom, double body, const Settings& current) {
    if (bottom <= 0.0 || body <= 0.0)
        return current.first_layer_shrink;
    return std::clamp(current.first_layer_shrink + (bottom - body) / 2.0, 0.0, 2.0);
}

CalibrationPrint first_layer_test(const Settings& settings) {
    CalibrationPrint out;
    out.kind = "first_layer";
    out.unit = "";
    // Where the squares go: around the middle of the bed's bounding box, the
    // outer ones 30 mm in from its edges (the skirt fits around them).
    double w = 200.0, d = 200.0;
    if (!settings.bed.empty()) {
        double x0 = 1e300, x1 = -1e300, y0 = 1e300, y1 = -1e300;
        for (const auto& q : settings.bed)
            x0 = std::min(x0, q[0]), x1 = std::max(x1, q[0]), y0 = std::min(y0, q[1]), y1 = std::max(y1, q[1]);
        w = x1 - x0, d = y1 - y0;
    }
    constexpr double kSide = 30.0, kInset = 30.0;
    const double ox = std::max(0.0, w / 2.0 - kInset - kSide / 2.0), oy = std::max(0.0, d / 2.0 - kInset - kSide / 2.0);
    const double h = settings.first_layer_height;
    for (const auto& [cx, cy] : std::vector<std::array<double, 2>>{{0, 0}, {-ox, -oy}, {ox, -oy}, {ox, oy}, {-ox, oy}}) {
        // Squares that would touch the middle one (a small bed) are left out.
        if ((cx != 0.0 || cy != 0.0) && std::max(std::abs(cx), std::abs(cy)) < kSide + 5.0)
            continue;
        add_box(out.mesh, cx - kSide / 2.0, cy - kSide / 2.0, 0.0, cx + kSide / 2.0, cy + kSide / 2.0, h);
    }
    return out;
}

CalibrationPrint fit_test(const std::vector<double>& values) {
    CalibrationPrint out;
    out.kind = "fit";
    out.unit = "mm";
    constexpr double kCell = 14.0, kDepth = 16.0, kHeight = 5.0, kPin = 4.0, kPinHeight = 8.0;
    const double x_first = -kCell * double(values.size()) / 2.0;
    for (std::size_t i = 0; i < values.size(); ++i) {
        const double c = std::clamp(values[i], 0.0, 2.0);
        const double cx = x_first + kCell * (double(i) + 0.5);
        add_block_with_hole(out.mesh, cx, 0.0, kCell / 2.0, kDepth / 2.0, kPin + c, 0.0, kHeight);
        add_cylinder(out.mesh, cx, 0.0, kPin, 0.0, kPinHeight);
        add_label(out.mesh, value_text(values[i]), cx - kCell / 2.0 + 1.0, 1.0, -kDepth / 2.0, true, 3.0, 0.6, 0.5);
        out.segments.push_back({0.0, kPinHeight, values[i]});
    }
    return out;
}

double volumetric_flow(const Settings& s, double speed_percent) {
    const double fastest = fastest_extrusion(s);
    return fastest * speed_percent / 100.0 * s.wall_width * s.layer_height * s.flow / 100.0;
}

std::string extruder_test_gcode(const Settings& s, double length) {
    double t = 210.0;
    if (const auto it = s.variables.find("temperature"); it != s.variables.end() && !it->second.empty())
        t = std::clamp(std::atof(it->second.front().c_str()), 150.0, 400.0);
    char text[1024];
    std::snprintf(text, sizeof text,
                  "; generated by Tisma engine %s: extruder steps test (docs/CALIBRATION.md)\n"
                  "; Mark the filament %.0f mm above the extruder's entry before running this.\n"
                  "M104 S%.0f ; heat the nozzle\n"
                  "M109 S%.0f ; and wait\n"
                  "M83 ; relative extrusion\n"
                  "G1 E%.1f F60 ; %.0f mm at 1 mm/s\n"
                  "M400 ; wait for the moves to end\n"
                  "M104 S0 ; nozzle off\n"
                  "M82\n",
                  TISMA_VERSION, length + 20.0, t, t, length, length);
    return text;
}

double extruder_steps_from(GcodeFlavor flavor, double current, double asked, double extruded) {
    if (!(current > 0.0) || !(asked > 0.0) || !(extruded > 0.0))
        return current;
    // Steps per mm grow when too little came out; Klipper's rotation distance
    // (mm per turn) shrinks.
    return flavor == GcodeFlavor::Klipper ? current * extruded / asked : current * asked / extruded;
}

std::string extruder_steps_text(GcodeFlavor flavor, double value) {
    char text[160];
    switch (flavor) {
    case GcodeFlavor::Marlin:
        std::snprintf(text, sizeof text, "M92 E%.2f\nM500", value);
        break;
    case GcodeFlavor::RepRap:
        std::snprintf(text, sizeof text, "M92 E%.2f", value);
        break;
    case GcodeFlavor::Klipper:
        std::snprintf(text, sizeof text, "rotation_distance: %.4f", value);
        break;
    }
    return text;
}

CalibrationPrint flow_tower(double start, double end, double step, double segment) {
    within(CalibrationKind::Flow, start, end);
    CalibrationPrint out;
    out.kind = "flow";
    out.unit = "%";
    const std::vector<double> values = tower_values(start, end, step);
    for (std::size_t i = 0; i < values.size(); ++i) {
        add_segment(out.mesh, -10.0, -10.0, 10.0, 10.0, double(i) * segment, segment, i + 1 == values.size());
        add_label(out.mesh, value_text(values[i]), -8.5, double(i) * segment + 0.6, -10.0, true,
                  std::min(3.0, segment - kNeck - 1.0), 0.6, 0.5);
    }
    tower_changes(out, values, segment, [](HeightChange& c, double v) { c.flow = v; });
    return out;
}

CalibrationPrint speed_tower(double start, double end, double step, double segment) {
    within(CalibrationKind::Speed, start, end);
    CalibrationPrint out;
    out.kind = "speed";
    out.unit = "%";
    const std::vector<double> values = tower_values(start, end, step);
    for (std::size_t i = 0; i < values.size(); ++i) {
        add_segment(out.mesh, -15.0, -15.0, 15.0, 15.0, double(i) * segment, segment, i + 1 == values.size());
        add_label(out.mesh, value_text(values[i]), -13.5, double(i) * segment + 0.6, -15.0, true,
                  std::min(3.0, segment - kNeck - 1.0), 0.6, 0.5);
    }
    tower_changes(out, values, segment, [](HeightChange& c, double v) { c.speed = v; });
    return out;
}

std::vector<Stroke> label_strokes(const std::string& text, double w, double h, double gap) {
    // Seven segments: a top, b upper right, c lower right, d bottom, e lower
    // left, f upper left, g middle.
    const Stroke seg[7] = {{0, h, w, h},         {w, h, w, h / 2}, {w, h / 2, w, 0}, {0, 0, w, 0},
                           {0, 0, 0, h / 2},     {0, h / 2, 0, h}, {0, h / 2, w, h / 2}};
    const char* digits[10] = {"abcdef", "bc", "abged", "abgcd", "fgbc", "afgcd", "afgedc", "abc", "abcdefg", "abcdfg"};
    std::vector<Stroke> out;
    double x = 0.0;
    for (char ch : text) {
        const char* parts = ch >= '0' && ch <= '9' ? digits[ch - '0'] : ch == '-' ? "g" : nullptr;
        if (parts) {
            for (const char* p = parts; *p; ++p) {
                const Stroke& s = seg[*p - 'a'];
                out.push_back({x + s.x0, s.y0, x + s.x1, s.y1});
            }
            x += w + gap;
        } else if (ch == '.') {
            out.push_back({x, 0.0, x, 0.0}); // a dot: a stroke of no length
            x += gap * 1.5;
        } else {
            x += w + gap; // anything else: a blank cell
        }
    }
    return out;
}

void add_label(TriangleMesh& mesh, const std::string& text, double x, double z, double face_y, bool towards_minus_y,
               double height, double stroke, double depth) {
    const double width = height * 0.55;
    const double out = towards_minus_y ? -1.0 : 1.0;
    // From 0.2 mm inside the face (so the relief is one solid with it) to depth out.
    const double y_in = face_y - out * 0.2, y_out = face_y + out * depth;
    for (const Stroke& s : label_strokes(text, width, height, stroke * 1.4)) {
        const double len = std::hypot(s.x1 - s.x0, s.y1 - s.y0);
        const V d = len > 1e-9 ? V{(s.x1 - s.x0) / len, 0.0, (s.y1 - s.y0) / len} : V{1.0, 0.0, 0.0};
        const V n{-d.z, 0.0, d.x};
        const V centre{x + (s.x0 + s.x1) / 2.0, (y_in + y_out) / 2.0, z + (s.y0 + s.y1) / 2.0};
        add_solid(mesh, centre, d * (len / 2.0 + stroke / 2.0), V{0.0, std::abs(y_out - y_in) / 2.0, 0.0},
                  n * (stroke / 2.0));
    }
}

const char* calibration_kind_name(CalibrationKind kind) {
    switch (kind) {
    case CalibrationKind::Dimensions: return "dimensions";
    case CalibrationKind::Temperature: return "temperature";
    case CalibrationKind::Retraction: return "retraction";
    case CalibrationKind::Flow: return "flow";
    case CalibrationKind::Speed: return "speed";
    case CalibrationKind::RetractionSpeed: return "retraction_speed";
    case CalibrationKind::PressureAdvance: return "pressure_advance";
    case CalibrationKind::FirstLayer: return "first_layer";
    case CalibrationKind::Fit: return "fit";
    case CalibrationKind::MaxFlow: return "max_flow";
    case CalibrationKind::Acceleration: return "acceleration";
    case CalibrationKind::ElephantFoot: return "elephant_foot";
    }
    return "dimensions";
}

std::optional<CalibrationKind> calibration_kind_from_name(std::string_view name) {
    for (int i = 0; i < kCalibrationKinds; ++i)
        if (name == calibration_kind_name(CalibrationKind(i)))
            return CalibrationKind(i);
    return std::nullopt;
}

bool is_tower(CalibrationKind kind) {
    return kind != CalibrationKind::Dimensions && kind != CalibrationKind::FirstLayer && kind != CalibrationKind::Fit &&
           kind != CalibrationKind::ElephantFoot;
}

CalibrationPrint calibration_tower(CalibrationKind kind, double start, double end, double step, double segment) {
    switch (kind) {
    case CalibrationKind::Temperature: return temperature_tower(start, end, step, segment);
    case CalibrationKind::Retraction: return retraction_tower(start, end, step, segment);
    case CalibrationKind::Flow: return flow_tower(start, end, step, segment);
    case CalibrationKind::Speed: return speed_tower(start, end, step, segment);
    case CalibrationKind::RetractionSpeed: return retraction_speed_tower(start, end, step, segment);
    case CalibrationKind::PressureAdvance: return pressure_advance_tower(start, end, step, segment);
    case CalibrationKind::MaxFlow: return max_flow_tower(start, end, step, segment);
    case CalibrationKind::Acceleration: return acceleration_tower(start, end, step, segment);
    default: return {};
    }
}

CalibrationPrint automatic_calibration(CalibrationKind kind, const Settings& s) {
    // A short retraction means a direct drive extruder; a long one, a Bowden tube.
    const bool direct = s.retract_length <= 1.5;
    switch (kind) {
    case CalibrationKind::Dimensions:
        return dimension_test();
    case CalibrationKind::RetractionSpeed:
        return retraction_speed_tower(20.0, 60.0, 5.0);
    case CalibrationKind::PressureAdvance:
        return direct ? pressure_advance_tower(0.0, 0.08, 0.01) : pressure_advance_tower(0.0, 0.8, 0.1);
    case CalibrationKind::FirstLayer:
        return first_layer_test(s);
    case CalibrationKind::Fit:
        return fit_test();
    case CalibrationKind::ElephantFoot:
        return elephant_foot_test();
    case CalibrationKind::MaxFlow: {
        // Around the known limit, half to one and a half times; else the
        // range of common hot ends.
        if (s.machine_max_flow <= 0.0)
            return max_flow_tower(4.0, 30.0, 2.0);
        const double step = std::max(0.5, std::round(s.machine_max_flow / 10.0 * 2.0) / 2.0);
        return max_flow_tower(std::max(0.5, std::round(s.machine_max_flow / 2.0)), std::round(s.machine_max_flow * 1.5), step);
    }
    case CalibrationKind::Acceleration: {
        // From 500 up to twice the acceleration in use (at least 5000), in about ten steps.
        const double end = std::max(5000.0, 2.0 * s.acceleration);
        const double step = std::max(500.0, std::round(end / 10.0 / 500.0) * 500.0);
        return acceleration_tower(500.0, end, step);
    }
    case CalibrationKind::Temperature: {
        // Around the temperature in use, 20 C each way.
        double t = 210.0;
        if (const auto it = s.variables.find("temperature"); it != s.variables.end() && !it->second.empty())
            t = std::clamp(std::atof(it->second.front().c_str()), 150.0, 400.0);
        t = std::round(t / 5.0) * 5.0;
        // Never past what the printer reaches.
        const double top = s.max_nozzle_temp > 0.0 ? std::min(t + 20.0, std::floor(s.max_nozzle_temp / 5.0) * 5.0) : t + 20.0;
        return temperature_tower(top, std::min(t - 20.0, top - 5.0), 5.0);
    }
    case CalibrationKind::Retraction:
        return direct ? retraction_tower(0.2, 2.0, 0.2) : retraction_tower(1.0, 6.0, 0.5);
    case CalibrationKind::Flow:
        return flow_tower(s.flow - 10.0, s.flow + 10.0, 2.5);
    case CalibrationKind::Speed:
        return speed_tower(100.0, 300.0, 25.0);
    }
    return {};
}

bool apply_calibration(CalibrationKind kind, double value, Settings& s) {
    const auto [lo, hi] = calibration_range(kind);
    value = std::clamp(value, lo, hi);
    switch (kind) {
    case CalibrationKind::Dimensions:
    case CalibrationKind::FirstLayer:
    case CalibrationKind::Fit:
    case CalibrationKind::ElephantFoot:
        return false;
    case CalibrationKind::MaxFlow:
        s.machine_max_flow = value;
        break;
    case CalibrationKind::Acceleration:
        s.acceleration = value;
        s.travel_acceleration = std::max(s.travel_acceleration, value);
        break;
    case CalibrationKind::RetractionSpeed:
        s.retract_speed = value;
        break;
    case CalibrationKind::PressureAdvance:
        s.pressure_advance = value;
        break;
    case CalibrationKind::Temperature:
        s.variables["temperature"] = {std::to_string(std::lround(value))};
        break;
    case CalibrationKind::Retraction:
        s.retract_length = value;
        break;
    case CalibrationKind::Flow:
        s.flow = value;
        break;
    case CalibrationKind::Speed: {
        const double f = value / 100.0;
        for (double* v : {&s.wall_speed, &s.infill_speed, &s.solid_infill_speed, &s.top_solid_infill_speed,
                          &s.support_speed, &s.max_speed})
            *v *= f;
        if (!s.outer_wall_speed.percent)
            s.outer_wall_speed.value *= f;
        break;
    }
    }
    s.changes.clear();
    return true;
}

Compensation compensation_from(const DimensionTest& nominal, const Measurements& measured, const Settings& current) {
    Compensation out{current.scale_x, current.scale_y, current.scale_z, current.hole_offset};
    const auto scaled = [](double scale, double design, double got) {
        return got > 0.0 && design > 0.0 ? std::clamp(scale * design / got, 50.0, 200.0) : scale;
    };
    out.scale_x = scaled(current.scale_x, nominal.x, measured.x);
    out.scale_y = scaled(current.scale_y, nominal.y, measured.y);
    out.scale_z = scaled(current.scale_z, nominal.z, measured.z);
    if (measured.hole > 0.0 && nominal.hole > 0.0) {
        // The hole will grow with the new X and Y scales; what remains is the
        // holes' own error, on the diameter: half of it on the radius.
        const double grow = (out.scale_x / current.scale_x + out.scale_y / current.scale_y) / 2.0;
        const double expected = measured.hole * grow;
        out.hole_offset = std::clamp(current.hole_offset + (nominal.hole - expected) / 2.0, -5.0, 5.0);
    }
    return out;
}

} // namespace tisma
