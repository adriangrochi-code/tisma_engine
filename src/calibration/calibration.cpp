// Calibration parts and compensation. Own design; see PROVENANCE.md.

#include "calibration/calibration.hpp"

#include <algorithm>
#include <array>
#include <cmath>
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

// A 45 degree overhang leaning out along +X from x = xa: a triangular prism
// from y0 to y1, with no width at z0 and `reach` mm wide at z1.
void add_overhang(TriangleMesh& m, double xa, double reach, double y0, double y1, double z0, double z1) {
    const auto p = [](double x, double y, double z) { return P{float(x), float(y), float(z)}; };
    const double xb = xa + reach;
    m.add_triangle(p(xa, y0, z0), p(xb, y0, z1), p(xa, y0, z1));          // -Y
    m.add_triangle(p(xa, y1, z0), p(xa, y1, z1), p(xb, y1, z1));          // +Y
    quad(m, p(xa, y0, z1), p(xb, y0, z1), p(xb, y1, z1), p(xa, y1, z1)); // top
    quad(m, p(xa, y0, z0), p(xa, y1, z0), p(xb, y1, z1), p(xb, y0, z1)); // the overhang
    quad(m, p(xa, y1, z0), p(xa, y0, z0), p(xa, y0, z1), p(xa, y1, z1)); // against the block
}

// A segment of a tower and the neck above it (none above the last).
void add_segment(TriangleMesh& m, double x0, double y0, double x1, double y1, double z0, double height, bool last) {
    const double top = last ? z0 + height : z0 + height - kNeck;
    add_box(m, x0, y0, z0, x1, y1, top);
    if (!last)
        add_box(m, x0 + kNeckInset, y0 + kNeckInset, top, x1 - kNeckInset, y1 - kNeckInset, z0 + height);
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
    const std::vector<double> values = tower_values(start, end, step);
    const double reach = std::max(segment - kNeck - 0.5, 1.0);
    for (std::size_t i = 0; i < values.size(); ++i) {
        const double z0 = double(i) * segment;
        const bool last = i + 1 == values.size();
        add_segment(out.mesh, -12.0, -6.0, 12.0, 6.0, z0, segment, last);
        add_overhang(out.mesh, 12.0, reach, -6.0, 6.0, z0 + (last ? segment : segment - kNeck) - reach, z0 + (last ? segment : segment - kNeck));
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
    }
    tower_changes(out, values, segment, [](HeightChange& c, double v) { c.retract_length = v; });
    return out;
}

CalibrationPrint flow_tower(double start, double end, double step, double segment) {
    CalibrationPrint out;
    out.kind = "flow";
    out.unit = "%";
    const std::vector<double> values = tower_values(start, end, step);
    for (std::size_t i = 0; i < values.size(); ++i)
        add_segment(out.mesh, -10.0, -10.0, 10.0, 10.0, double(i) * segment, segment, i + 1 == values.size());
    tower_changes(out, values, segment, [](HeightChange& c, double v) { c.flow = v; });
    return out;
}

CalibrationPrint speed_tower(double start, double end, double step, double segment) {
    CalibrationPrint out;
    out.kind = "speed";
    out.unit = "%";
    const std::vector<double> values = tower_values(start, end, step);
    for (std::size_t i = 0; i < values.size(); ++i)
        add_segment(out.mesh, -15.0, -15.0, 15.0, 15.0, double(i) * segment, segment, i + 1 == values.size());
    tower_changes(out, values, segment, [](HeightChange& c, double v) { c.speed = v; });
    return out;
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
