// The plate: placing, turning, scaling and arranging the models. Own design;
// see PROVENANCE.md. The ray test is Möller and Trumbore's (1997, "Fast,
// minimum storage ray-triangle intersection").

#include "ui/plate.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>

namespace tisma::ui {

namespace {

constexpr double kPi = 3.14159265358979323846;

Rotation multiply(const Rotation& a, const Rotation& b) {
    Rotation m{};
    for (int r = 0; r < 3; ++r)
        for (int c = 0; c < 3; ++c)
            m[std::size_t(r * 3 + c)] = a[std::size_t(r * 3)] * b[std::size_t(c)] +
                                        a[std::size_t(r * 3 + 1)] * b[std::size_t(3 + c)] +
                                        a[std::size_t(r * 3 + 2)] * b[std::size_t(6 + c)];
    return m;
}

Rotation about_axis(int axis, double radians) {
    const double c = std::cos(radians), s = std::sin(radians);
    switch (axis) {
    case 0: return {1, 0, 0, 0, c, -s, 0, s, c};
    case 1: return {c, 0, s, 0, 1, 0, -s, 0, c};
    default: return {c, -s, 0, s, c, 0, 0, 0, 1};
    }
}

// Turns `angle` radians about the unit axis (Rodrigues).
Rotation about(double ax, double ay, double az, double angle) {
    const double c = std::cos(angle), s = std::sin(angle), t = 1.0 - c;
    return {t * ax * ax + c,      t * ax * ay - s * az, t * ax * az + s * ay,
            t * ax * ay + s * az, t * ay * ay + c,      t * ay * az - s * ax,
            t * ax * az - s * ay, t * ay * az + s * ax, t * az * az + c};
}

// The corners of the mesh scaled about the box centre and turned, before
// they are set on the bed.
std::vector<Vec3f> turned(const PlateObject& o) {
    const BoundingBox box = o.mesh.bounding_box();
    const double cx = 0.5 * (double(box.min.x) + double(box.max.x)), cy = 0.5 * (double(box.min.y) + double(box.max.y)),
                 cz = 0.5 * (double(box.min.z) + double(box.max.z));
    const Rotation& r = o.place.rotation;
    const auto& s = o.place.scale;
    std::vector<Vec3f> out;
    out.reserve(o.mesh.corners().size());
    for (const Vec3f& v : o.mesh.corners()) {
        const double x = (double(v.x) - cx) * s[0], y = (double(v.y) - cy) * s[1], z = (double(v.z) - cz) * s[2];
        out.push_back({float(r[0] * x + r[1] * y + r[2] * z), float(r[3] * x + r[4] * y + r[5] * z),
                       float(r[6] * x + r[7] * y + r[8] * z)});
    }
    return out;
}

} // namespace

Rotation rotated(const Rotation& r, int axis, double degrees) { return multiply(about_axis(axis, degrees * kPi / 180.0), r); }

Rotation laid_on(const Rotation& r, Vec3 normal) {
    const double nx = normal.x, ny = normal.y, nz = normal.z, length = std::sqrt(nx * nx + ny * ny + nz * nz);
    if (!(length > 0.0))
        return r;
    // The turn from the normal to straight down: about normal x (0, 0, -1).
    double ax = -ny / length, ay = nx / length;
    const double cosine = std::clamp(-nz / length, -1.0, 1.0), sine = std::sqrt(ax * ax + ay * ay);
    if (sine < 1e-9)
        return cosine > 0.0 ? r : multiply(about_axis(0, kPi), r); // already down, or straight up: half a turn
    ax /= sine, ay /= sine;
    return multiply(about(ax, ay, 0.0, std::atan2(sine, cosine)), r);
}

std::array<double, 3> euler_degrees(const Rotation& r) {
    const double sb = std::clamp(-r[6], -1.0, 1.0), b = std::asin(sb);
    double a, c;
    if (std::fabs(sb) > 1.0 - 1e-9) { // straight up or down: X and Z turn the same way
        a = 0.0;
        c = std::atan2(-r[1], r[4]);
    } else {
        a = std::atan2(r[7], r[8]);
        c = std::atan2(r[3], r[0]);
    }
    const auto degrees = [](double v) {
        const double d = v * 180.0 / kPi;
        return std::fabs(d) < 1e-9 ? 0.0 : d;
    };
    return {degrees(a), degrees(b), degrees(c)};
}

Rotation from_euler_degrees(const std::array<double, 3>& d) {
    return multiply(about_axis(2, d[2] * kPi / 180.0),
                    multiply(about_axis(1, d[1] * kPi / 180.0), about_axis(0, d[0] * kPi / 180.0)));
}

TriangleMesh placed_mesh(const PlateObject& object) {
    std::vector<Vec3f> p = turned(object);
    TriangleMesh out;
    if (p.empty())
        return out;
    float x0 = p[0].x, x1 = x0, y0 = p[0].y, y1 = y0, z0 = p[0].z;
    for (const Vec3f& v : p)
        x0 = std::min(x0, v.x), x1 = std::max(x1, v.x), y0 = std::min(y0, v.y), y1 = std::max(y1, v.y),
        z0 = std::min(z0, v.z);
    const double dx = object.place.x - 0.5 * (double(x0) + double(x1)), dy = object.place.y - 0.5 * (double(y0) + double(y1));
    out.reserve(p.size() / 3);
    const auto at = [&](const Vec3f& v) { return Vec3f{float(v.x + dx), float(v.y + dy), v.z - z0}; };
    for (std::size_t i = 0; i + 2 < p.size(); i += 3)
        out.add_triangle(at(p[i]), at(p[i + 1]), at(p[i + 2]));
    return out;
}

TriangleMesh plate_mesh(const std::vector<TriangleMesh>& placed) {
    std::size_t count = 0;
    for (const TriangleMesh& m : placed)
        count += m.triangle_count();
    TriangleMesh out;
    out.reserve(count);
    for (const TriangleMesh& m : placed)
        for (std::size_t t = 0; t < m.triangle_count(); ++t)
            out.add_triangle(m.corner(t, 0), m.corner(t, 1), m.corner(t, 2));
    return out;
}

std::array<double, 3> placed_size(const PlateObject& object) {
    const std::vector<Vec3f> p = turned(object);
    if (p.empty())
        return {0.0, 0.0, 0.0};
    Vec3f lo = p[0], hi = p[0];
    for (const Vec3f& v : p)
        lo = {std::min(lo.x, v.x), std::min(lo.y, v.y), std::min(lo.z, v.z)},
        hi = {std::max(hi.x, v.x), std::max(hi.y, v.y), std::max(hi.z, v.z)};
    return {double(hi.x - lo.x), double(hi.y - lo.y), double(hi.z - lo.z)};
}

std::vector<std::array<double, 2>> arrange(const std::vector<std::array<double, 2>>& sizes, std::array<double, 2> centre,
                                           double spacing) {
    const std::size_t n = sizes.size();
    std::vector<std::array<double, 2>> out(n, centre);
    if (n == 0)
        return out;
    // Deepest first, then widest; ties keep their order.
    std::vector<std::size_t> order(n);
    std::iota(order.begin(), order.end(), std::size_t(0));
    std::stable_sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) {
        return sizes[a][1] > sizes[b][1] || (sizes[a][1] == sizes[b][1] && sizes[a][0] > sizes[b][0]);
    });
    // A row holds about the square root of the count, as wide as that many average boxes.
    const std::size_t per_row = std::size_t(std::ceil(std::sqrt(double(n))));
    double average = 0.0, widest = 0.0;
    for (const auto& s : sizes)
        average += s[0] / double(n), widest = std::max(widest, s[0]);
    const double row_width = std::max(widest, double(per_row) * average + double(per_row - 1) * spacing) + 1e-9;
    // Rows from the back of the bed (+Y) to the front, each filled left to right.
    struct Row {
        std::vector<std::size_t> items;
        double width = 0.0, depth = 0.0;
    };
    std::vector<Row> rows;
    for (const std::size_t i : order) {
        if (rows.empty() || (!rows.back().items.empty() && rows.back().width + spacing + sizes[i][0] > row_width))
            rows.emplace_back();
        Row& row = rows.back();
        row.width += (row.items.empty() ? 0.0 : spacing) + sizes[i][0];
        row.depth = std::max(row.depth, sizes[i][1]);
        row.items.push_back(i);
    }
    double total_depth = 0.0, total_width = 0.0;
    for (const Row& r : rows)
        total_depth += r.depth, total_width = std::max(total_width, r.width);
    total_depth += spacing * double(rows.size() - 1);
    // The grid's box is centred, its rows starting from its left side.
    double y = centre[1] + 0.5 * total_depth;
    for (const Row& r : rows) {
        double x = centre[0] - 0.5 * total_width;
        for (const std::size_t i : r.items) {
            out[i] = {x + 0.5 * sizes[i][0], y - 0.5 * r.depth};
            x += sizes[i][0] + spacing;
        }
        y -= r.depth + spacing;
    }
    return out;
}

Vec3 triangle_normal(const TriangleMesh& mesh, std::size_t t) {
    const Vec3f& a = mesh.corner(t, 0);
    const Vec3f& b = mesh.corner(t, 1);
    const Vec3f& c = mesh.corner(t, 2);
    return normalize(cross(Vec3{b.x - a.x, b.y - a.y, b.z - a.z}, Vec3{c.x - a.x, c.y - a.y, c.z - a.z}));
}

std::optional<PlateHit> pick(const std::vector<TriangleMesh>& placed, Vec3 origin, Vec3 direction) {
    std::optional<PlateHit> best;
    const double ox = origin.x, oy = origin.y, oz = origin.z, dx = direction.x, dy = direction.y, dz = direction.z;
    for (std::size_t o = 0; o < placed.size(); ++o) {
        const TriangleMesh& m = placed[o];
        for (std::size_t t = 0; t < m.triangle_count(); ++t) {
            const Vec3f& a = m.corner(t, 0);
            const Vec3f& b = m.corner(t, 1);
            const Vec3f& c = m.corner(t, 2);
            const double e1x = double(b.x) - a.x, e1y = double(b.y) - a.y, e1z = double(b.z) - a.z;
            const double e2x = double(c.x) - a.x, e2y = double(c.y) - a.y, e2z = double(c.z) - a.z;
            const double px = dy * e2z - dz * e2y, py = dz * e2x - dx * e2z, pz = dx * e2y - dy * e2x;
            const double det = e1x * px + e1y * py + e1z * pz;
            if (std::fabs(det) < 1e-12)
                continue;
            const double inv = 1.0 / det;
            const double tx = ox - a.x, ty = oy - a.y, tz = oz - a.z;
            const double u = (tx * px + ty * py + tz * pz) * inv;
            if (u < 0.0 || u > 1.0)
                continue;
            const double qx = ty * e1z - tz * e1y, qy = tz * e1x - tx * e1z, qz = tx * e1y - ty * e1x;
            const double v = (dx * qx + dy * qy + dz * qz) * inv;
            if (v < 0.0 || u + v > 1.0)
                continue;
            const double distance = (e2x * qx + e2y * qy + e2z * qz) * inv;
            if (distance > 0.0 && (!best || distance < best->distance))
                best = PlateHit{o, t, distance};
        }
    }
    return best;
}

} // namespace tisma::ui
