// The plate: placing, turning, scaling and arranging the models. Own design;
// see PROVENANCE.md. The ray test is Möller and Trumbore's (1997, "Fast,
// minimum storage ray-triangle intersection").

#include "ui/plate.hpp"

#include "slice/placement.hpp"

#include <nlohmann/json.hpp>
#include <oneapi/tbb/blocked_range.h>
#include <oneapi/tbb/parallel_for.h>
#include <oneapi/tbb/parallel_reduce.h>

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

// A box that grows by points, joined across threads.
struct Box3 {
    Vec3f lo{std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max()};
    Vec3f hi{std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest()};
    void add(const Vec3f& v) {
        lo = {std::min(lo.x, v.x), std::min(lo.y, v.y), std::min(lo.z, v.z)};
        hi = {std::max(hi.x, v.x), std::max(hi.y, v.y), std::max(hi.z, v.z)};
    }
    Box3 joined(const Box3& b) const {
        Box3 out = *this;
        out.add(b.lo);
        out.add(b.hi);
        return out;
    }
};

// Blocks of corners for the threads: a model of millions of triangles
// (the owner's large-model item, 2026-10-28).
constexpr std::size_t kCornerBlock = std::size_t(1) << 15;

// The box of `n` points, point(i) each, over the threads.
template <class Point>
Box3 box_of(std::size_t n, Point&& point) {
    return tbb::parallel_reduce(
        tbb::blocked_range<std::size_t>(0, n, kCornerBlock), Box3{},
        [&](const tbb::blocked_range<std::size_t>& range, Box3 box) {
            for (std::size_t i = range.begin(); i != range.end(); ++i)
                box.add(point(i));
            return box;
        },
        [](const Box3& a, const Box3& b) { return a.joined(b); });
}

// The turn of a mesh's corners: scaled about its box centre and turned,
// before they are set on the bed.
struct Turn {
    double cx, cy, cz;
    const Rotation& r;
    const std::array<double, 3>& s;
    Vec3f operator()(const Vec3f& v) const {
        const double x = (double(v.x) - cx) * s[0], y = (double(v.y) - cy) * s[1], z = (double(v.z) - cz) * s[2];
        return {float(r[0] * x + r[1] * y + r[2] * z), float(r[3] * x + r[4] * y + r[5] * z),
                float(r[6] * x + r[7] * y + r[8] * z)};
    }
};

Turn turn_of(const PlateObject& o) {
    const std::vector<Vec3f>& c = o.mesh.corners();
    const Box3 box = box_of(c.size(), [&](std::size_t i) { return c[i]; });
    return {0.5 * (double(box.lo.x) + double(box.hi.x)), 0.5 * (double(box.lo.y) + double(box.hi.y)),
            0.5 * (double(box.lo.z) + double(box.hi.z)), o.place.rotation, o.place.scale};
}

// The turned corners of the mesh, over the threads.
std::vector<Vec3f> turned(const PlateObject& o) {
    const std::vector<Vec3f>& c = o.mesh.corners();
    std::vector<Vec3f> out(c.size());
    if (c.empty())
        return out;
    const Turn turn = turn_of(o);
    tbb::parallel_for(tbb::blocked_range<std::size_t>(0, c.size(), kCornerBlock),
                      [&](const tbb::blocked_range<std::size_t>& range) {
                          for (std::size_t i = range.begin(); i != range.end(); ++i)
                              out[i] = turn(c[i]);
                      });
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
    if (p.empty())
        return {};
    const Box3 box = box_of(p.size(), [&](std::size_t i) { return p[i]; });
    const double dx = object.place.x - 0.5 * (double(box.lo.x) + double(box.hi.x)),
                 dy = object.place.y - 0.5 * (double(box.lo.y) + double(box.hi.y));
    const float z0 = box.lo.z;
    // Set down where it goes, in place: the turned corners become the mesh.
    tbb::parallel_for(tbb::blocked_range<std::size_t>(0, p.size(), kCornerBlock),
                      [&](const tbb::blocked_range<std::size_t>& range) {
                          for (std::size_t i = range.begin(); i != range.end(); ++i)
                              p[i] = Vec3f{float(p[i].x + dx), float(p[i].y + dy), p[i].z - z0};
                      });
    return TriangleMesh(std::move(p));
}

TriangleMesh plate_mesh(const std::vector<TriangleMesh>& placed) {
    std::size_t count = 0;
    for (const TriangleMesh& m : placed)
        count += m.corners().size();
    std::vector<Vec3f> corners;
    corners.reserve(count);
    for (const TriangleMesh& m : placed) // whole blocks of corners, not triangle by triangle
        corners.insert(corners.end(), m.corners().begin(), m.corners().end());
    return TriangleMesh(std::move(corners));
}

Painting plate_painting(const std::vector<PlateObject>& objects) {
    Painting out;
    std::size_t triangles = 0;
    for (const PlateObject& o : objects) {
        out.append(o.painting, triangles);
        triangles += o.mesh.triangle_count();
    }
    return out;
}

PaintMesh paint_mesh(const std::vector<PlateObject>& objects, const std::vector<TriangleMesh>& placed,
                     PaintChannel channel) {
    // The pieces of each mark apart first, then one after the other.
    std::vector<std::vector<float>> by_mark(kPaintMaxMark + 1);
    for (std::size_t i = 0; i < objects.size() && i < placed.size(); ++i) {
        const Painting& p = objects[i].painting;
        const TriangleMesh& m = placed[i];
        const std::size_t count = std::min(p.size(channel), m.triangle_count());
        for (std::size_t t = 0; t < count; ++t) {
            if (p.tree(channel, t).empty())
                continue;
            const Vec3 n = triangle_normal(m, t);
            for_each_painted(p, channel, t, m.corner(t, 0), m.corner(t, 1), m.corner(t, 2),
                             [&](const Vec3f& a, const Vec3f& b, const Vec3f& c, std::uint8_t mark) {
                                 std::vector<float>& v = by_mark[std::min<std::size_t>(mark, kPaintMaxMark)];
                                 for (const Vec3f* q : {&a, &b, &c})
                                     v.insert(v.end(), {q->x, q->y, q->z, n.x, n.y, n.z});
                             });
        }
    }
    PaintMesh out;
    for (std::size_t mark = 1; mark < by_mark.size(); ++mark) {
        if (by_mark[mark].empty())
            continue;
        out.groups.push_back({std::uint8_t(mark), out.vertices.size() / 6, by_mark[mark].size() / 6});
        out.vertices.insert(out.vertices.end(), by_mark[mark].begin(), by_mark[mark].end());
    }
    return out;
}

std::array<double, 3> placed_size(const PlateObject& object) {
    if (object.mesh.empty())
        return {0.0, 0.0, 0.0};
    // The turned corners' box, without keeping them.
    const Turn turn = turn_of(object);
    const std::vector<Vec3f>& c = object.mesh.corners();
    const Box3 box = box_of(c.size(), [&](std::size_t i) { return turn(c[i]); });
    return {double(box.hi.x - box.lo.x), double(box.hi.y - box.lo.y), double(box.hi.z - box.lo.z)};
}

std::vector<std::array<double, 2>> arrange(const std::vector<std::array<double, 2>>& sizes, std::array<double, 2> centre,
                                           double spacing) {
    return arrange_boxes(sizes, centre, spacing);
}

Vec3 triangle_normal(const TriangleMesh& mesh, std::size_t t) {
    const Vec3f& a = mesh.corner(t, 0);
    const Vec3f& b = mesh.corner(t, 1);
    const Vec3f& c = mesh.corner(t, 2);
    return normalize(cross(Vec3{b.x - a.x, b.y - a.y, b.z - a.z}, Vec3{c.x - a.x, c.y - a.y, c.z - a.z}));
}

namespace {

// How far along the ray it meets triangle t, or -1.
double ray_triangle(const TriangleMesh& m, std::size_t t, Vec3 origin, Vec3 direction) {
    const double ox = origin.x, oy = origin.y, oz = origin.z, dx = direction.x, dy = direction.y, dz = direction.z;
    const Vec3f& a = m.corner(t, 0);
    const Vec3f& b = m.corner(t, 1);
    const Vec3f& c = m.corner(t, 2);
    const double e1x = double(b.x) - a.x, e1y = double(b.y) - a.y, e1z = double(b.z) - a.z;
    const double e2x = double(c.x) - a.x, e2y = double(c.y) - a.y, e2z = double(c.z) - a.z;
    const double px = dy * e2z - dz * e2y, py = dz * e2x - dx * e2z, pz = dx * e2y - dy * e2x;
    const double det = e1x * px + e1y * py + e1z * pz;
    if (std::fabs(det) < 1e-12)
        return -1.0;
    const double inv = 1.0 / det;
    const double tx = ox - a.x, ty = oy - a.y, tz = oz - a.z;
    const double u = (tx * px + ty * py + tz * pz) * inv;
    if (u < 0.0 || u > 1.0)
        return -1.0;
    const double qx = ty * e1z - tz * e1y, qy = tz * e1x - tx * e1z, qz = tx * e1y - ty * e1x;
    const double v = (dx * qx + dy * qy + dz * qz) * inv;
    if (v < 0.0 || u + v > 1.0)
        return -1.0;
    const double distance = (e2x * qx + e2y * qy + e2z * qz) * inv;
    return distance > 0.0 ? distance : -1.0;
}

// Where the ray enters the box (0 if it starts inside), or -1: the slab
// method (Kay and Kajiya 1986).
double ray_box(Vec3f lo, Vec3f hi, Vec3 origin, Vec3 direction) {
    double near = 0.0, far = std::numeric_limits<double>::max();
    const double o[3] = {origin.x, origin.y, origin.z}, d[3] = {direction.x, direction.y, direction.z};
    const double l[3] = {lo.x, lo.y, lo.z}, h[3] = {hi.x, hi.y, hi.z};
    for (int k = 0; k < 3; ++k) {
        if (std::fabs(d[k]) < 1e-30) {
            if (o[k] < l[k] || o[k] > h[k])
                return -1.0;
            continue;
        }
        double t0 = (l[k] - o[k]) / d[k], t1 = (h[k] - o[k]) / d[k];
        if (t0 > t1)
            std::swap(t0, t1);
        near = std::max(near, t0), far = std::min(far, t1);
        if (near > far)
            return -1.0;
    }
    return near;
}

constexpr std::uint32_t kLeafTriangles = 8;

} // namespace

std::optional<PlateHit> pick(const std::vector<TriangleMesh>& placed, Vec3 origin, Vec3 direction) {
    std::optional<PlateHit> best;
    for (std::size_t o = 0; o < placed.size(); ++o)
        for (std::size_t t = 0; t < placed[o].triangle_count(); ++t)
            if (const double d = ray_triangle(placed[o], t, origin, direction); d > 0.0 && (!best || d < best->distance))
                best = PlateHit{o, t, d};
    return best;
}

TriangleIndex::TriangleIndex(const TriangleMesh& mesh) {
    const std::size_t n = mesh.triangle_count();
    if (n == 0)
        return;
    std::vector<Vec3f> lo(n), hi(n), centre(n);
    for (std::size_t t = 0; t < n; ++t) {
        const Vec3f& a = mesh.corner(t, 0);
        const Vec3f& b = mesh.corner(t, 1);
        const Vec3f& c = mesh.corner(t, 2);
        lo[t] = {std::min({a.x, b.x, c.x}), std::min({a.y, b.y, c.y}), std::min({a.z, b.z, c.z})};
        hi[t] = {std::max({a.x, b.x, c.x}), std::max({a.y, b.y, c.y}), std::max({a.z, b.z, c.z})};
        centre[t] = {0.5f * (lo[t].x + hi[t].x), 0.5f * (lo[t].y + hi[t].y), 0.5f * (lo[t].z + hi[t].z)};
    }
    order_.resize(n);
    std::iota(order_.begin(), order_.end(), std::uint32_t(0));
    nodes_.reserve(2 * (n / kLeafTriangles + 1));
    struct Task {
        std::uint32_t node, first, count;
    };
    std::vector<Task> stack{{0, 0, std::uint32_t(n)}};
    nodes_.emplace_back();
    while (!stack.empty()) {
        const Task task = stack.back();
        stack.pop_back();
        Node& node = nodes_[task.node];
        node.lo = lo[order_[task.first]], node.hi = hi[order_[task.first]];
        Vec3f clo = centre[order_[task.first]], chi = clo;
        for (std::uint32_t i = task.first; i < task.first + task.count; ++i) {
            const std::uint32_t t = order_[i];
            node.lo = {std::min(node.lo.x, lo[t].x), std::min(node.lo.y, lo[t].y), std::min(node.lo.z, lo[t].z)};
            node.hi = {std::max(node.hi.x, hi[t].x), std::max(node.hi.y, hi[t].y), std::max(node.hi.z, hi[t].z)};
            clo = {std::min(clo.x, centre[t].x), std::min(clo.y, centre[t].y), std::min(clo.z, centre[t].z)};
            chi = {std::max(chi.x, centre[t].x), std::max(chi.y, centre[t].y), std::max(chi.z, centre[t].z)};
        }
        const float ex = chi.x - clo.x, ey = chi.y - clo.y, ez = chi.z - clo.z;
        if (task.count <= kLeafTriangles || std::max({ex, ey, ez}) <= 0.0f) {
            node.first = task.first, node.count = task.count;
            continue;
        }
        const int axis = ex >= ey && ex >= ez ? 0 : ey >= ez ? 1 : 2;
        const auto key = [&](std::uint32_t t) { return axis == 0 ? centre[t].x : axis == 1 ? centre[t].y : centre[t].z; };
        const std::uint32_t half = task.count / 2;
        std::nth_element(order_.begin() + task.first, order_.begin() + task.first + half,
                         order_.begin() + task.first + task.count,
                         [&](std::uint32_t a, std::uint32_t b) { return key(a) < key(b); });
        const auto left = std::uint32_t(nodes_.size());
        nodes_.emplace_back();
        const auto right = std::uint32_t(nodes_.size());
        nodes_.emplace_back();
        nodes_[task.node].first = left;
        nodes_[task.node].second = right;
        stack.push_back({right, task.first + half, task.count - half});
        stack.push_back({left, task.first, half});
    }
}

std::optional<std::pair<std::size_t, double>> TriangleIndex::hit(const TriangleMesh& mesh, Vec3 origin,
                                                                 Vec3 direction) const {
    std::optional<std::pair<std::size_t, double>> best;
    if (nodes_.empty())
        return best;
    std::vector<std::uint32_t> stack{0};
    while (!stack.empty()) {
        const Node& node = nodes_[stack.back()];
        stack.pop_back();
        const double enter = ray_box(node.lo, node.hi, origin, direction);
        if (enter < 0.0 || (best && enter > best->second))
            continue;
        if (node.count > 0) {
            for (std::uint32_t i = node.first; i < node.first + node.count; ++i)
                if (const double d = ray_triangle(mesh, order_[i], origin, direction);
                    d > 0.0 && (!best || d < best->second || (d == best->second && order_[i] < best->first)))
                    best = std::pair<std::size_t, double>{order_[i], d};
            continue;
        }
        stack.push_back(node.second);
        stack.push_back(node.first);
    }
    return best;
}

std::vector<std::uint32_t> TriangleIndex::near(Vec3f lo, Vec3f hi) const {
    std::vector<std::uint32_t> out;
    if (nodes_.empty())
        return out;
    std::vector<std::uint32_t> stack{0};
    while (!stack.empty()) {
        const Node& node = nodes_[stack.back()];
        stack.pop_back();
        if (node.lo.x > hi.x || node.hi.x < lo.x || node.lo.y > hi.y || node.hi.y < lo.y || node.lo.z > hi.z ||
            node.hi.z < lo.z)
            continue;
        if (node.count > 0) {
            out.insert(out.end(), order_.begin() + node.first, order_.begin() + node.first + node.count);
            continue;
        }
        stack.push_back(node.second);
        stack.push_back(node.first);
    }
    return out;
}

std::optional<PlateHit> pick(const std::vector<TriangleMesh>& placed, const std::vector<TriangleIndex>& indexes,
                             Vec3 origin, Vec3 direction) {
    std::optional<PlateHit> best;
    for (std::size_t o = 0; o < placed.size(); ++o) {
        std::optional<std::pair<std::size_t, double>> h;
        if (o < indexes.size() && !indexes[o].empty()) {
            h = indexes[o].hit(placed[o], origin, direction);
        } else {
            const auto one = pick({placed[o]}, origin, direction);
            if (one)
                h = std::pair<std::size_t, double>{one->triangle, one->distance};
        }
        if (h && (!best || h->second < best->distance))
            best = PlateHit{o, h->first, h->second};
    }
    return best;
}

namespace {

nlohmann::ordered_json own_json(const std::string& own) {
    if (own.empty())
        return nlohmann::ordered_json::object();
    nlohmann::ordered_json j = nlohmann::ordered_json::parse(own);
    return j.is_object() ? j : nlohmann::ordered_json::object();
}

std::pair<std::string, std::string> split_key(std::string_view key) {
    const std::size_t dot = key.find('.');
    if (dot == std::string_view::npos)
        return {std::string(key), {}};
    return {std::string(key.substr(0, dot)), std::string(key.substr(dot + 1))};
}

} // namespace

std::vector<std::string> own_keys(const std::string& own) {
    std::vector<std::string> out;
    const nlohmann::ordered_json j = own_json(own);
    for (const auto& [group, keys] : j.items())
        if (keys.is_object())
            for (const auto& [key, value] : keys.items())
                out.push_back(group + "." + key);
    return out;
}

std::vector<ObjectRow> object_rows(const PlateObject& object) {
    std::vector<ObjectRow> out;
    std::vector<std::string> groups;
    for (const std::string& key : own_keys(object.settings)) {
        const std::string group = key.substr(0, key.find('.'));
        if (std::find(groups.begin(), groups.end(), group) == groups.end())
            groups.push_back(group);
    }
    const auto rank = [](const std::string& g) { return g == "walls" ? 0 : g == "infill" ? 1 : g == "surfaces" ? 2 : 3; };
    std::stable_sort(groups.begin(), groups.end(), [&](const std::string& a, const std::string& b) { return rank(a) < rank(b); });
    for (std::string& g : groups)
        out.push_back({ObjectRow::Kind::Settings, std::move(g), 0});
    for (std::size_t k = 0; k < object.volumes.size(); ++k)
        out.push_back({ObjectRow::Kind::Volume, {}, k});
    for (std::size_t k = 0; k < object.ranges.size(); ++k)
        out.push_back({ObjectRow::Kind::Range, {}, k});
    return out;
}

std::string with_own_key(const std::string& own, const Settings& profile, std::string_view key) {
    nlohmann::ordered_json j = own_json(own);
    const auto [group, name] = split_key(key);
    const nlohmann::ordered_json all = nlohmann::ordered_json::parse(settings_json(profile));
    if (!all.contains(group) || !all.at(group).contains(name))
        return own;
    j[group][name] = all.at(group).at(name);
    return j.dump();
}

std::string without_own_key(const std::string& own, std::string_view key) {
    nlohmann::ordered_json j = own_json(own);
    const auto [group, name] = split_key(key);
    if (j.contains(group)) {
        j[group].erase(name);
        if (j[group].empty())
            j.erase(group);
    }
    return j.empty() ? std::string() : j.dump();
}

std::string own_values(const std::string& own, const Settings& edited) {
    nlohmann::ordered_json j = own_json(own);
    const nlohmann::ordered_json all = nlohmann::ordered_json::parse(settings_json(edited));
    for (auto& [group, keys] : j.items())
        for (auto& [key, value] : keys.items())
            if (all.contains(group) && all.at(group).contains(key))
                value = all.at(group).at(key);
    return j.empty() ? std::string() : j.dump();
}

std::vector<std::string> own_setting_keys(const Settings& profile) {
    std::vector<std::string> out;
    const nlohmann::ordered_json all = nlohmann::ordered_json::parse(settings_json(profile));
    for (const char* group : {"walls", "infill", "surfaces"})
        if (all.contains(group))
            for (const auto& [key, value] : all.at(group).items())
                out.push_back(std::string(group) + "." + key);
    return out;
}

Affine placing(const PlateObject& object, const TriangleMesh& placed) {
    Affine out;
    if (object.mesh.triangle_count() == 0 || placed.triangle_count() != object.mesh.triangle_count())
        return out;
    // Scaled and turned, and moved as the first corner was (placed_mesh scales
    // about the box's centre: the move takes that in).
    const Rotation& r = object.place.rotation;
    const auto& s = object.place.scale;
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
            out.a[std::size_t(3 * i + j)] = r[std::size_t(3 * i + j)] * s[std::size_t(j)];
    const Vec3f first = object.mesh.corner(0, 0), there = placed.corner(0, 0);
    const double m[3] = {double(first.x), double(first.y), double(first.z)};
    const double there_d[3] = {double(there.x), double(there.y), double(there.z)};
    for (int i = 0; i < 3; ++i) {
        double turned = 0.0;
        for (int j = 0; j < 3; ++j)
            turned += out.a[std::size_t(3 * i + j)] * m[j];
        out.b[std::size_t(i)] = there_d[i] - turned;
    }
    return out;
}

TriangleMesh volume_mesh(const PlateVolume& v, const PlateObject& object, const TriangleMesh& placed) {
    TriangleMesh m;
    if (object.mesh.triangle_count() == 0 || placed.triangle_count() != object.mesh.triangle_count())
        return m;
    // In the model's frame, over its box as its file has it, turned about its
    // centre; then set down with the model.
    const BoundingBox box = object.mesh.bounding_box();
    const double h = std::max(v.size[2], 0.01);
    const double cx = 0.5 * (double(box.min.x) + box.max.x) + v.offset[0];
    const double cy = 0.5 * (double(box.min.y) + box.max.y) + v.offset[1];
    const double cz = double(box.min.z) + v.offset[2] + h / 2.0;
    const double rx = std::max(v.size[0], 0.01) / 2.0, ry = std::max(v.size[1], 0.01) / 2.0;
    const Affine to_bed = placing(object, placed);
    const Rotation& u = v.turn;
    const auto p = [&](double x, double y, double z) { // x, y, z from the volume's centre, along its axes
        const double q[3] = {cx + u[0] * x + u[1] * y + u[2] * z, cy + u[3] * x + u[4] * y + u[5] * z,
                             cz + u[6] * x + u[7] * y + u[8] * z};
        const auto& a = to_bed.a;
        return Vec3f{float(a[0] * q[0] + a[1] * q[1] + a[2] * q[2] + to_bed.b[0]),
                     float(a[3] * q[0] + a[4] * q[1] + a[5] * q[2] + to_bed.b[1]),
                     float(a[6] * q[0] + a[7] * q[1] + a[8] * q[2] + to_bed.b[2])};
    };
    const double z0 = -h / 2.0, z1 = h / 2.0;
    if (v.shape == PlateVolume::Shape::Box) {
        const Vec3f c[8] = {p(-rx, -ry, z0), p(rx, -ry, z0), p(rx, ry, z0), p(-rx, ry, z0),
                            p(-rx, -ry, z1), p(rx, -ry, z1), p(rx, ry, z1), p(-rx, ry, z1)};
        static constexpr int f[12][3] = {{0, 2, 1}, {0, 3, 2}, {4, 5, 6}, {4, 6, 7}, {0, 1, 5}, {0, 5, 4},
                                         {1, 2, 6}, {1, 6, 5}, {2, 3, 7}, {2, 7, 6}, {3, 0, 4}, {3, 4, 7}};
        for (const auto& t : f)
            m.add_triangle(c[t[0]], c[t[1]], c[t[2]]);
        return m;
    }
    constexpr int sides = 48;
    const Vec3f bottom = p(0.0, 0.0, z0), top = p(0.0, 0.0, z1);
    for (int i = 0; i < sides; ++i) {
        const double a0 = 2.0 * 3.14159265358979323846 * i / sides, a1 = 2.0 * 3.14159265358979323846 * (i + 1) / sides;
        const Vec3f b0 = p(rx * std::cos(a0), ry * std::sin(a0), z0), b1 = p(rx * std::cos(a1), ry * std::sin(a1), z0);
        const Vec3f t0 = p(rx * std::cos(a0), ry * std::sin(a0), z1), t1 = p(rx * std::cos(a1), ry * std::sin(a1), z1);
        m.add_triangle(bottom, b1, b0);
        m.add_triangle(top, t0, t1);
        m.add_triangle(b0, b1, t1);
        m.add_triangle(b0, t1, t0);
    }
    return m;
}

PlateVolume carried_volume(const PlateVolume& v, const BoundingBox& from, const Affine& map, const BoundingBox& to) {
    PlateVolume out = v;
    const auto& a = map.a;
    const auto apply = [&](const std::array<double, 3>& q) {
        return std::array<double, 3>{a[0] * q[0] + a[1] * q[1] + a[2] * q[2], a[3] * q[0] + a[4] * q[1] + a[5] * q[2],
                                     a[6] * q[0] + a[7] * q[1] + a[8] * q[2]};
    };
    const double h = std::max(v.size[2], 0.01);
    std::array<double, 3> centre = apply({0.5 * (double(from.min.x) + from.max.x) + v.offset[0],
                                          0.5 * (double(from.min.y) + from.max.y) + v.offset[1],
                                          double(from.min.z) + v.offset[2] + h / 2.0});
    for (std::size_t k = 0; k < 3; ++k)
        centre[k] += map.b[k];
    // Its axes, mapped: each one's stretch scales its size.
    std::array<std::array<double, 3>, 3> axis;
    for (std::size_t k = 0; k < 3; ++k) {
        axis[k] = apply({v.turn[k], v.turn[3 + k], v.turn[6 + k]});
        const double length = std::sqrt(axis[k][0] * axis[k][0] + axis[k][1] * axis[k][1] + axis[k][2] * axis[k][2]);
        if (length > 0.0) {
            out.size[k] = v.size[k] * length;
            for (double& c : axis[k])
                c /= length;
        }
    }
    // A proper turn again after a mirror: one axis turns back. Which one does
    // not change the solid (a box and a cylinder are symmetric about their
    // middle planes); the one furthest from its own direction, so an upright
    // volume stays upright.
    const auto det = [](const std::array<std::array<double, 3>, 3>& m) {
        return m[0][0] * (m[1][1] * m[2][2] - m[1][2] * m[2][1]) - m[1][0] * (m[0][1] * m[2][2] - m[0][2] * m[2][1]) +
               m[2][0] * (m[0][1] * m[1][2] - m[0][2] * m[1][1]);
    };
    if (det(axis) < 0.0) {
        std::size_t flip = 0;
        for (std::size_t k = 1; k < 3; ++k)
            if (axis[k][k] < axis[flip][flip])
                flip = k;
        for (double& c : axis[flip])
            c = -c;
    }
    // Square again where an uneven scale sheared it (Gram-Schmidt).
    const auto dot = [](const std::array<double, 3>& p, const std::array<double, 3>& q) {
        return p[0] * q[0] + p[1] * q[1] + p[2] * q[2];
    };
    const auto normal = [](std::array<double, 3>& p) {
        const double l = std::sqrt(p[0] * p[0] + p[1] * p[1] + p[2] * p[2]);
        if (l > 0.0)
            for (double& c : p)
                c /= l;
    };
    for (std::size_t k = 1; k < 3; ++k) {
        for (std::size_t j = 0; j < k; ++j) {
            const double d = dot(axis[k], axis[j]);
            for (std::size_t c = 0; c < 3; ++c)
                axis[k][c] -= d * axis[j][c];
        }
        normal(axis[k]);
    }
    for (std::size_t k = 0; k < 3; ++k)
        for (std::size_t r = 0; r < 3; ++r)
            out.turn[3 * r + k] = std::abs(axis[k][r]) < 1e-12 ? 0.0 : axis[k][r];
    const double h2 = std::max(out.size[2], 0.01);
    out.offset = {centre[0] - 0.5 * (double(to.min.x) + to.max.x), centre[1] - 0.5 * (double(to.min.y) + to.max.y),
                  centre[2] - h2 / 2.0 - double(to.min.z)};
    return out;
}

std::array<double, 3> model_size(const PlateObject& object) {
    if (object.mesh.triangle_count() == 0)
        return {0.0, 0.0, 0.0};
    const BoundingBox box = object.mesh.bounding_box();
    return {double(box.max.x) - box.min.x, double(box.max.y) - box.min.y, double(box.max.z) - box.min.z};
}

} // namespace tisma::ui
