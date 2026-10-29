// The ray tracing scene of the 3D view (CPU reference of the GPU shader).
// Own design from the cited papers; see PROVENANCE.md.

#include "ui/raytrace.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace tisma::ui {
namespace {

constexpr int kBins = 16;
constexpr std::size_t kStack = 64;   // the GPU shader's traversal stack has the same size
constexpr float kTiny = 1e-12f;      // a direction component this small counts as this, to keep 1/d finite

struct Bounds {
    Vec3 lo{std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity(),
            std::numeric_limits<float>::infinity()};
    Vec3 hi{-std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity(),
            -std::numeric_limits<float>::infinity()};
    void add(Vec3 p) {
        lo = {std::min(lo.x, p.x), std::min(lo.y, p.y), std::min(lo.z, p.z)};
        hi = {std::max(hi.x, p.x), std::max(hi.y, p.y), std::max(hi.z, p.z)};
    }
    void add(const Bounds& b) {
        add(b.lo);
        add(b.hi);
    }
    bool valid() const { return lo.x <= hi.x; }
    float area() const {
        if (!valid())
            return 0.0f;
        const Vec3 d = hi - lo;
        return 2.0f * (d.x * d.y + d.y * d.z + d.z * d.x);
    }
};

float axis(Vec3 v, int k) { return k == 0 ? v.x : k == 1 ? v.y : v.z; }

// A bead's own axes: along the move, across it (horizontal) and up.
struct BeadAxes {
    Vec3 dir, side, up;
    float length = 0.0f;
};
BeadAxes bead_axes(Vec3 p0, Vec3 p1) {
    BeadAxes a;
    const Vec3 along = p1 - p0;
    a.length = length(along);
    a.dir = a.length > 0.0f ? along * (1.0f / a.length) : Vec3{1, 0, 0};
    const Vec3 side = cross(a.dir, Vec3{0, 0, 1});
    a.side = dot(side, side) > 1e-8f ? normalize(side) : Vec3{1, 0, 0};
    a.up = cross(a.side, a.dir);
    return a;
}

// How far a bead reaches past the end of its move: half a width at a free
// end, as far as the mitre's outer corner at a joined one.
float bead_reach(Vec3 dir, float width, float angle) {
    if (!(angle < 0.5f * kFreeEnd))
        return 0.5f * width;
    const float c = std::clamp(dir.x * std::cos(angle) + dir.y * std::sin(angle), 1e-3f, 1.0f);
    return 0.5f * width * std::sqrt(std::max(1.0f - c * c, 0.0f)) / c;
}

Bounds bounds_of(const RayPrimitive& p) {
    Bounds b;
    if (p.bead) {
        // The box around the prism and its mitres, a little larger.
        const BeadAxes a = bead_axes(p.p0, p.p1);
        const float w = p.p2.x, h = p.p2.y;
        const float r0 = bead_reach(a.dir, w, p.p2.z) * 1.01f + 1e-4f, r1 = bead_reach(a.dir, w, p.p3.x) * 1.01f + 1e-4f;
        for (const float s : {-0.5f * w, 0.5f * w})
            for (const float u : {-h, 0.0f})
                for (const Vec3 end : {p.p0 - a.dir * r0, p.p1 + a.dir * r1})
                    b.add(end + a.side * s + a.up * u);
    } else if (p.box) {
        const Vec3 r{std::abs(p.p1.x) + std::abs(p.p2.x) + std::abs(p.p3.x),
                     std::abs(p.p1.y) + std::abs(p.p2.y) + std::abs(p.p3.y),
                     std::abs(p.p1.z) + std::abs(p.p2.z) + std::abs(p.p3.z)};
        b.add(p.p0 - r);
        b.add(p.p0 + r);
    } else {
        b.add(p.p0);
        b.add(p.p1);
        b.add(p.p2);
    }
    return b;
}

Vec3 centroid(const RayPrimitive& p) {
    return p.bead ? (p.p0 + p.p1) * 0.5f : p.box ? p.p0 : (p.p0 + p.p1 + p.p2) * (1.0f / 3.0f);
}

float safe(float d) { return std::abs(d) < kTiny ? (d < 0.0f ? -kTiny : kTiny) : d; }

void put(std::vector<float>& out, Vec3 v, float w) { out.insert(out.end(), {v.x, v.y, v.z, w}); }

void pad(std::vector<float>& texels) {
    const std::size_t row = std::size_t(RayScene::kTextureWidth) * 4;
    texels.resize((texels.size() + row - 1) / row * row, 0.0f);
}

} // namespace

float intersect_triangle(const Ray& ray, Vec3 a, Vec3 e1, Vec3 e2) {
    const Vec3 p = cross(ray.dir, e2);
    const float det = dot(e1, p);
    if (std::abs(det) < 1e-12f)
        return -1.0f;
    const float inv = 1.0f / det;
    const Vec3 s = ray.origin - a;
    const float u = dot(s, p) * inv;
    if (u < 0.0f || u > 1.0f)
        return -1.0f;
    const Vec3 q = cross(s, e1);
    const float v = dot(ray.dir, q) * inv;
    if (v < 0.0f || u + v > 1.0f)
        return -1.0f;
    const float t = dot(e2, q) * inv;
    return t > 0.0f ? t : -1.0f;
}

float intersect_box(const Ray& ray, Vec3 centre, Vec3 half_x, Vec3 half_y, Vec3 half_z) {
    const Vec3 d = centre - ray.origin;
    float near = -std::numeric_limits<float>::infinity(), far = std::numeric_limits<float>::infinity();
    for (const Vec3& h : {half_x, half_y, half_z}) {
        const float l = length(h);
        if (l <= 0.0f)
            return -1.0f;
        const Vec3 a = h * (1.0f / l);
        const float e = dot(a, d), f = dot(a, ray.dir);
        if (std::abs(f) > 1e-12f) {
            float t1 = (e + l) / f, t2 = (e - l) / f;
            if (t1 > t2)
                std::swap(t1, t2);
            near = std::max(near, t1);
            far = std::min(far, t2);
            if (near > far || far < 0.0f)
                return -1.0f;
        } else if (-e - l > 0.0f || -e + l < 0.0f) {
            return -1.0f; // parallel to these faces and outside them
        }
    }
    return near > 0.0f ? near : -1.0f; // from inside: no hit
}

float intersect_bead(const Ray& ray, Vec3 p0, Vec3 p1, float width, float height, float start, float end) {
    const BeadAxes a = bead_axes(p0, p1);
    if (!(a.length > 0.0f) || !(width > 0.0f) || !(height > 0.0f))
        return -1.0f;
    // Everything from p0, for precision far from the origin; each plane keeps
    // dot(q, n) <= k, with q the point from p0.
    const Vec3 o = ray.origin - p0;
    float near = -std::numeric_limits<float>::infinity(), far = std::numeric_limits<float>::infinity();
    const auto clip = [&](Vec3 n, float k) {
        const float denom = dot(ray.dir, n), num = k - dot(o, n);
        if (std::abs(denom) < 1e-12f)
            return num >= 0.0f;
        const float t = num / denom;
        if (denom > 0.0f)
            far = std::min(far, t);
        else
            near = std::max(near, t);
        return near <= far;
    };
    // The rhombus around its centre, half a height under the path.
    const Vec3 c = a.up * (-0.5f * height);
    for (const float s : {-1.0f, 1.0f})
        for (const float u : {-1.0f, 1.0f}) {
            const Vec3 n = a.side * (s * 2.0f / width) + a.up * (u * 2.0f / height);
            if (!clip(n, dot(c, n) + 1.0f))
                return -1.0f;
        }
    // The two ends: the mitre plane through the end point, or half a width past it.
    if (start < 0.5f * kFreeEnd) {
        const Vec3 n{std::cos(start), std::sin(start), 0.0f};
        if (!clip(n * -1.0f, 0.0f))
            return -1.0f;
    } else if (!clip(a.dir * -1.0f, 0.5f * width)) {
        return -1.0f;
    }
    const Vec3 to_end = p1 - p0;
    if (end < 0.5f * kFreeEnd) {
        const Vec3 n{std::cos(end), std::sin(end), 0.0f};
        if (!clip(n, dot(to_end, n)))
            return -1.0f;
    } else if (!clip(a.dir, dot(to_end, a.dir) + 0.5f * width)) {
        return -1.0f;
    }
    if (far < 0.0f)
        return -1.0f;
    return near > 0.0f ? near : -1.0f; // from inside: no hit
}

void RayScene::build(std::vector<RayPrimitive> primitives) {
    primitives_ = std::move(primitives);
    if (primitives_.size() > kMaxPrimitives)
        primitives_.resize(kMaxPrimitives);
    nodes_.clear();
    depth_ = 0;
    if (primitives_.empty())
        return;
    const std::size_t n = primitives_.size();
    std::vector<Bounds> boxes(n);
    std::vector<Vec3> centres(n);
    for (std::size_t i = 0; i < n; ++i) {
        boxes[i] = bounds_of(primitives_[i]);
        centres[i] = centroid(primitives_[i]);
    }
    // The primitives are sorted through this order; it is applied at the end.
    std::vector<std::uint32_t> order(n);
    for (std::size_t i = 0; i < n; ++i)
        order[i] = std::uint32_t(i);

    struct Task {
        std::uint32_t node;
        std::size_t begin, end, depth;
    };
    nodes_.reserve(2 * n / kLeafSize + 2);
    nodes_.push_back({});
    std::vector<Task> tasks = {{0, 0, n, 1}};
    while (!tasks.empty()) {
        const Task task = tasks.back();
        tasks.pop_back();
        depth_ = std::max(depth_, task.depth);
        Bounds all, cent;
        std::uint32_t first_layer = 0xFFFFFFFFu, last_layer = 0;
        for (std::size_t i = task.begin; i < task.end; ++i) {
            all.add(boxes[order[i]]);
            cent.add(centres[order[i]]);
            first_layer = std::min(first_layer, primitives_[order[i]].layer);
            last_layer = std::max(last_layer, primitives_[order[i]].layer);
        }
        Node& node = nodes_[task.node];
        node.lo = all.lo, node.hi = all.hi;
        node.first_layer = first_layer, node.last_layer = last_layer;
        const std::size_t count = task.end - task.begin;
        if (count <= kLeafSize) {
            node.first = std::uint32_t(task.begin);
            node.count = std::uint32_t(count);
            continue;
        }

        // The cheapest split between bins, over the three axes.
        int best_axis = -1, best_bin = 0;
        float best_cost = std::numeric_limits<float>::infinity();
        for (int k = 0; k < 3; ++k) {
            const float lo = axis(cent.lo, k), extent = axis(cent.hi, k) - lo;
            if (extent <= 0.0f)
                continue;
            std::array<Bounds, kBins> bin_bounds{};
            std::array<std::size_t, kBins> bin_count{};
            for (std::size_t i = task.begin; i < task.end; ++i) {
                const int b = std::min(int((axis(centres[order[i]], k) - lo) / extent * kBins), kBins - 1);
                bin_bounds[std::size_t(b)].add(boxes[order[i]]);
                ++bin_count[std::size_t(b)];
            }
            std::array<float, kBins> left_cost{};
            Bounds left;
            std::size_t left_n = 0;
            for (int b = 0; b < kBins - 1; ++b) {
                left.add(bin_bounds[std::size_t(b)]);
                left_n += bin_count[std::size_t(b)];
                left_cost[std::size_t(b)] = left.area() * float(left_n);
            }
            Bounds right;
            std::size_t right_n = 0;
            for (int b = kBins - 1; b > 0; --b) {
                right.add(bin_bounds[std::size_t(b)]);
                right_n += bin_count[std::size_t(b)];
                const float cost = left_cost[std::size_t(b - 1)] + right.area() * float(right_n);
                if (right_n > 0 && right_n < count && cost < best_cost)
                    best_cost = cost, best_axis = k, best_bin = b;
            }
        }
        std::size_t middle = task.begin + count / 2;
        if (best_axis >= 0) {
            const float lo = axis(cent.lo, best_axis), extent = axis(cent.hi, best_axis) - lo;
            const auto split = std::partition(order.begin() + std::ptrdiff_t(task.begin),
                                              order.begin() + std::ptrdiff_t(task.end), [&](std::uint32_t i) {
                                                  const int b = std::min(
                                                      int((axis(centres[i], best_axis) - lo) / extent * kBins), kBins - 1);
                                                  return b < best_bin;
                                              });
            middle = std::size_t(split - order.begin());
        }
        if (middle == task.begin || middle == task.end) {
            // All centres in one place: halve by count, so the leaves stay small.
            middle = task.begin + count / 2;
        }
        const auto children = std::uint32_t(nodes_.size());
        nodes_[task.node].first = children;
        nodes_[task.node].count = 0;
        nodes_.push_back({});
        nodes_.push_back({});
        tasks.push_back({children, task.begin, middle, task.depth + 1});
        tasks.push_back({children + 1, middle, task.end, task.depth + 1});
    }
    std::vector<RayPrimitive> sorted(n);
    for (std::size_t i = 0; i < n; ++i)
        sorted[i] = primitives_[order[i]];
    primitives_ = std::move(sorted);
}

template <class Visit>
void RayScene::traverse(const Ray& ray, const RayFilter& filter, Visit visit) const {
    if (nodes_.empty())
        return;
    const Vec3 inv{1.0f / safe(ray.dir.x), 1.0f / safe(ray.dir.y), 1.0f / safe(ray.dir.z)};
    std::array<std::uint32_t, kStack> stack{};
    std::size_t sp = 0;
    std::uint32_t at = 0;
    float tmax = ray.tmax;
    for (;;) {
        const Node& node = nodes_[at];
        const Vec3 t0{(node.lo.x - ray.origin.x) * inv.x, (node.lo.y - ray.origin.y) * inv.y,
                      (node.lo.z - ray.origin.z) * inv.z};
        const Vec3 t1{(node.hi.x - ray.origin.x) * inv.x, (node.hi.y - ray.origin.y) * inv.y,
                      (node.hi.z - ray.origin.z) * inv.z};
        const float near = std::max({std::min(t0.x, t1.x), std::min(t0.y, t1.y), std::min(t0.z, t1.z)});
        const float far = std::min({std::max(t0.x, t1.x), std::max(t0.y, t1.y), std::max(t0.z, t1.z)});
        const bool layers = node.last_layer >= filter.first_layer && node.first_layer <= filter.last_layer;
        if (layers && near <= far && far >= 0.0f && near <= tmax) {
            if (node.count > 0) {
                for (std::uint32_t i = node.first; i < node.first + node.count; ++i) {
                    const RayPrimitive& p = primitives_[i];
                    if (p.layer < filter.first_layer || p.layer > filter.last_layer)
                        continue;
                    if (p.kind != kAlwaysVisible && !((filter.kind_mask >> p.kind) & 1u))
                        continue;
                    const float t = p.bead  ? intersect_bead(ray, p.p0, p.p1, p.p2.x, p.p2.y, p.p2.z, p.p3.x)
                                    : p.box ? intersect_box(ray, p.p0, p.p1, p.p2, p.p3)
                                            : intersect_triangle(ray, p.p0, p.p1 - p.p0, p.p2 - p.p0);
                    if (t >= 0.0f && t < tmax && visit(t, tmax))
                        return;
                }
            } else if (sp < kStack) {
                stack[sp++] = node.first + 1;
                at = node.first;
                continue;
            }
        }
        if (sp == 0)
            return;
        at = stack[--sp];
    }
}

bool RayScene::occluded(const Ray& ray, const RayFilter& filter) const {
    bool hit = false;
    traverse(ray, filter, [&](float, float&) { return hit = true; });
    return hit;
}

float RayScene::closest(const Ray& ray, const RayFilter& filter) const {
    float best = -1.0f;
    traverse(ray, filter, [&](float t, float& tmax) {
        best = t;
        tmax = t; // later hits must be nearer
        return false;
    });
    return best;
}

std::vector<float> RayScene::node_texels() const {
    std::vector<float> out;
    out.reserve(nodes_.size() * kNodeTexels * 4);
    for (const Node& n : nodes_) {
        put(out, n.lo, float(n.first));
        put(out, n.hi, float(n.count));
        put(out, {float(n.first_layer), float(n.last_layer), 0.0f}, 0.0f);
    }
    pad(out);
    return out;
}

std::vector<float> RayScene::primitive_texels() const {
    std::vector<float> out;
    out.reserve(primitives_.size() * kPrimitiveTexels * 4);
    for (const RayPrimitive& p : primitives_) {
        const bool solid = p.box || p.bead;
        put(out, p.p0, float(p.layer));
        put(out, solid ? p.p1 : p.p1 - p.p0, p.bead ? 2.0f : p.box ? 1.0f : 0.0f);
        put(out, solid ? p.p2 : p.p2 - p.p0, float(p.kind));
        put(out, p.p3, 0.0f);
    }
    pad(out);
    return out;
}

std::vector<RayPrimitive> model_primitives(const TriangleMesh& mesh, Vec3 offset) {
    std::vector<RayPrimitive> out;
    out.reserve(mesh.triangle_count());
    for (std::size_t t = 0; t < mesh.triangle_count(); ++t) {
        RayPrimitive p;
        Vec3* corner[3] = {&p.p0, &p.p1, &p.p2};
        for (int k = 0; k < 3; ++k) {
            const Vec3f& c = mesh.corner(t, k);
            *corner[k] = Vec3{c.x, c.y, c.z} + offset;
        }
        out.push_back(p);
    }
    return out;
}

std::vector<RayPrimitive> plate_primitives(const BedPolygon& bed) {
    std::vector<RayPrimitive> out;
    const std::vector<float> fan = bed_triangles(bed);
    for (std::size_t i = 0; i + 9 <= fan.size(); i += 9) {
        RayPrimitive p;
        p.p0 = {fan[i], fan[i + 1], fan[i + 2]};
        p.p1 = {fan[i + 3], fan[i + 4], fan[i + 5]};
        p.p2 = {fan[i + 6], fan[i + 7], fan[i + 8]};
        out.push_back(p);
    }
    return out;
}

std::vector<RayPrimitive> preview_primitives(const PreviewBuffers& preview) {
    std::vector<RayPrimitive> out;
    out.reserve(preview.extrusions.size());
    const auto angle = [](float x, float y) { return x * x + y * y < 0.25f ? kFreeEnd : std::atan2(y, x); };
    for (const PreviewInstance& e : preview.extrusions) {
        const Vec3 a{e.x0, e.y0, e.z0}, b{e.x1, e.y1, e.z1};
        if (length(b - a) <= 0.0f || e.width <= 0.0f || e.height <= 0.0f)
            continue;
        RayPrimitive p;
        p.bead = true;
        p.p0 = a;
        p.p1 = b;
        p.p2 = {e.width, e.height, angle(e.join0_x, e.join0_y)};
        p.p3 = {angle(e.join1_x, e.join1_y), 0.0f, 0.0f};
        p.layer = e.kind_layer >> 8;
        p.kind = e.kind_layer & 0xFFu;
        out.push_back(p);
    }
    return out;
}

} // namespace tisma::ui
