// Calibration parts and compensation. Own design; see PROVENANCE.md.

#include "calibration/calibration.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <numbers>

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
    else
        std::snprintf(text, sizeof text, "%.2f", v);
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
    // The angles of the hole's sides and of the block's corners, so the
    // block keeps its corners: each angle gives a point on the hole and the
    // point on the block's outline along the same ray.
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
        inner.push_back({r * c, r * s});
        const double reach = std::min(std::abs(c) > 1e-12 ? hx / std::abs(c) : 1e300, std::abs(s) > 1e-12 ? hy / std::abs(s) : 1e300);
        outer.push_back({reach * c, reach * s});
    }
    const auto p = [](const std::array<double, 2>& q, double z) { return P{float(q[0]), float(q[1]), float(z)}; };
    const std::size_t n = angles.size();
    for (std::size_t i = 0; i < n; ++i) {
        const std::size_t j = (i + 1) % n;
        quad(out.mesh, p(inner[i], t.z), p(outer[i], t.z), p(outer[j], t.z), p(inner[j], t.z)); // top
        quad(out.mesh, p(inner[j], 0), p(outer[j], 0), p(outer[i], 0), p(inner[i], 0));         // bottom
        quad(out.mesh, p(outer[i], 0), p(outer[j], 0), p(outer[j], t.z), p(outer[i], t.z));      // outside
        quad(out.mesh, p(inner[j], 0), p(inner[i], 0), p(inner[i], t.z), p(inner[j], t.z));      // the hole
    }
    return out;
}

CalibrationPrint temperature_tower(double start, double end, double step, double segment) {
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
    CalibrationPrint out;
    out.kind = "retraction";
    out.unit = "mm";
    const std::vector<double> values = tower_values(start, end, step);
    for (std::size_t i = 0; i < values.size(); ++i) {
        const bool last = i + 1 == values.size();
        add_segment(out.mesh, -19.0, -4.0, -11.0, 4.0, double(i) * segment, segment, last);
        add_segment(out.mesh, 11.0, -4.0, 19.0, 4.0, double(i) * segment, segment, last);
        add_label(out.mesh, value_text(values[i]), -18.4, double(i) * segment + 0.6, -4.0, true,
                  std::min(3.0, segment - kNeck - 1.0), 0.6, 0.5);
    }
    tower_changes(out, values, segment, [](HeightChange& c, double v) { c.retract_length = v; });
    return out;
}

CalibrationPrint flow_tower(double start, double end, double step, double segment) {
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

CalibrationPrint automatic_calibration(CalibrationKind kind, const Settings& s) {
    switch (kind) {
    case CalibrationKind::Dimensions:
        return dimension_test();
    case CalibrationKind::Temperature: {
        // Around the temperature in use, 20 C each way.
        double t = 210.0;
        if (const auto it = s.variables.find("temperature"); it != s.variables.end() && !it->second.empty())
            t = std::clamp(std::atof(it->second.front().c_str()), 150.0, 400.0);
        t = std::round(t / 5.0) * 5.0;
        return temperature_tower(t + 20.0, t - 20.0, 5.0);
    }
    case CalibrationKind::Retraction:
        // A short retraction means a direct drive extruder; a long one, a Bowden tube.
        return s.retract_length <= 1.5 ? retraction_tower(0.2, 2.0, 0.2) : retraction_tower(1.0, 6.0, 0.5);
    case CalibrationKind::Flow:
        return flow_tower(s.flow - 10.0, s.flow + 10.0, 2.5);
    case CalibrationKind::Speed:
        return speed_tower(100.0, 300.0, 25.0);
    }
    return {};
}

bool apply_calibration(CalibrationKind kind, double value, Settings& s) {
    switch (kind) {
    case CalibrationKind::Dimensions:
        return false;
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
