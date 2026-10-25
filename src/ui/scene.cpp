// Draw data of the 3D view. Own design; see PROVENANCE.md.

#include "ui/scene.hpp"

#include <algorithm>
#include <cmath>

namespace tisma::ui {
namespace {

constexpr float kLineLift = 0.02f; // mm: grid lines sit this far above the plate

// The bed polygon's box; a unit box at the origin when there is none.
void bed_box(const BedPolygon& bed, double& x0, double& y0, double& x1, double& y1) {
    if (bed.empty()) {
        x0 = y0 = x1 = y1 = 0.0;
        return;
    }
    x0 = x1 = bed[0][0];
    y0 = y1 = bed[0][1];
    for (const auto& p : bed) {
        x0 = std::min(x0, p[0]), x1 = std::max(x1, p[0]);
        y0 = std::min(y0, p[1]), y1 = std::max(y1, p[1]);
    }
}

// Where the line x = c (axis 0) or y = c (axis 1) is inside the polygon: the
// crossings with its edges along the other axis, sorted, in pairs.
std::vector<double> crossings(const BedPolygon& bed, int axis_index, double c) {
    std::vector<double> t;
    const auto axis = static_cast<std::size_t>(axis_index), other = 1 - axis;
    for (std::size_t i = 0; i < bed.size(); ++i) {
        const auto& a = bed[i];
        const auto& b = bed[(i + 1) % bed.size()];
        // Half-open in the axis, so a line through a vertex counts it once.
        if ((a[axis] <= c) != (b[axis] <= c)) {
            const double s = (c - a[axis]) / (b[axis] - a[axis]);
            t.push_back(a[other] + s * (b[other] - a[other]));
        }
    }
    std::sort(t.begin(), t.end());
    return t;
}

void add_line(std::vector<LineVertex>& out, float x0, float y0, float x1, float y1, std::uint32_t argb) {
    out.push_back({x0, y0, kLineLift, argb});
    out.push_back({x1, y1, kLineLift, argb});
}

} // namespace

Vec3 centre_on_bed(const BoundingBox& box, const BedPolygon& bed) {
    if (box.empty)
        return {};
    double x0, y0, x1, y1;
    bed_box(bed, x0, y0, x1, y1);
    const double cx = (x0 + x1) * 0.5, cy = (y0 + y1) * 0.5;
    return {float(cx - (double(box.min.x) + double(box.max.x)) * 0.5),
            float(cy - (double(box.min.y) + double(box.max.y)) * 0.5), -box.min.z};
}

TriangleMesh translated(const TriangleMesh& mesh, Vec3 offset) {
    TriangleMesh out;
    out.reserve(mesh.triangle_count());
    const auto moved = [&](const Vec3f& p) { return Vec3f{p.x + offset.x, p.y + offset.y, p.z + offset.z}; };
    for (std::size_t t = 0; t < mesh.triangle_count(); ++t)
        out.add_triangle(moved(mesh.corner(t, 0)), moved(mesh.corner(t, 1)), moved(mesh.corner(t, 2)));
    return out;
}

std::vector<float> mesh_vertices(const TriangleMesh& mesh, Vec3 offset) {
    std::vector<float> out;
    out.reserve(mesh.triangle_count() * 18);
    for (std::size_t t = 0; t < mesh.triangle_count(); ++t) {
        Vec3 p[3];
        for (int k = 0; k < 3; ++k) {
            const Vec3f& c = mesh.corner(t, k);
            p[k] = Vec3{c.x, c.y, c.z} + offset;
        }
        const Vec3 n = normalize(cross(p[1] - p[0], p[2] - p[0]));
        for (const Vec3& v : p)
            out.insert(out.end(), {v.x, v.y, v.z, n.x, n.y, n.z});
    }
    return out;
}

std::vector<LineVertex> bed_lines(const BedPolygon& bed, const BedLook& look) {
    std::vector<LineVertex> out;
    if (bed.size() < 3 || look.step <= 0.0)
        return out;
    double x0, y0, x1, y1;
    bed_box(bed, x0, y0, x1, y1);
    for (int axis = 0; axis < 2; ++axis) {
        const double lo = axis == 0 ? x0 : y0, hi = axis == 0 ? x1 : y1;
        // Lines at whole multiples of the step, so the grid lines up with the
        // machine's coordinates whatever the bed's origin.
        const auto first = static_cast<long long>(std::ceil(lo / look.step));
        const auto last = static_cast<long long>(std::floor(hi / look.step));
        for (long long i = first; i <= last; ++i) {
            const double c = double(i) * look.step;
            if (std::abs(c - lo) < 1e-9 || std::abs(c - hi) < 1e-9)
                continue; // on the box's edge: the outline is there
            const std::uint32_t argb = i % look.major_every == 0 ? look.major : look.minor;
            const std::vector<double> t = crossings(bed, axis, c);
            for (std::size_t k = 0; k + 1 < t.size(); k += 2) {
                if (axis == 0)
                    add_line(out, float(c), float(t[k]), float(c), float(t[k + 1]), argb);
                else
                    add_line(out, float(t[k]), float(c), float(t[k + 1]), float(c), argb);
            }
        }
    }
    for (std::size_t i = 0; i < bed.size(); ++i) {
        const auto& a = bed[i];
        const auto& b = bed[(i + 1) % bed.size()];
        add_line(out, float(a[0]), float(a[1]), float(b[0]), float(b[1]), look.outline);
    }
    return out;
}

std::vector<float> bed_triangles(const BedPolygon& bed) {
    std::vector<float> out;
    for (std::size_t i = 1; i + 1 < bed.size(); ++i)
        for (const auto* p : {&bed[0], &bed[i], &bed[i + 1]})
            out.insert(out.end(), {float((*p)[0]), float((*p)[1]), 0.0f});
    return out;
}

PreviewBuffers preview_buffers(const GcodePreview& preview, std::uint32_t travel_argb) {
    PreviewBuffers out;
    out.extrusions.reserve(preview.extrusions.size());
    out.layer_extrusions.reserve(preview.layers.size() + 1);
    out.layer_travels.reserve(preview.layers.size() + 1);
    bool first = true;
    for (const PreviewLayer& layer : preview.layers) {
        out.layer_extrusions.push_back(out.extrusions.size());
        out.layer_travels.push_back(out.travels.size());
        for (std::size_t i = layer.first; i < layer.first + layer.count; ++i) {
            const PreviewMove& m = preview.moves[i];
            if (m.kind == LineKind::Travel) {
                out.travels.push_back({m.x0, m.y0, m.z0, travel_argb});
                out.travels.push_back({m.x1, m.y1, m.z1, travel_argb});
                continue;
            }
            PreviewInstance p;
            p.x0 = m.x0, p.y0 = m.y0, p.z0 = m.z0;
            p.x1 = m.x1, p.y1 = m.y1, p.z1 = m.z1;
            p.width = m.width, p.height = m.height, p.feed = m.feed;
            p.kind_layer = static_cast<std::uint32_t>(m.kind) | (m.layer << 8);
            out.extrusions.push_back(p);
            out.min_feed = first ? m.feed : std::min(out.min_feed, m.feed);
            out.max_feed = first ? m.feed : std::max(out.max_feed, m.feed);
            out.min_width = first ? m.width : std::min(out.min_width, m.width);
            out.max_width = first ? m.width : std::max(out.max_width, m.width);
            out.min_height = first ? m.height : std::min(out.min_height, m.height);
            out.max_height = first ? m.height : std::max(out.max_height, m.height);
            first = false;
        }
    }
    out.layer_extrusions.push_back(out.extrusions.size());
    out.layer_travels.push_back(out.travels.size());
    return out;
}

std::uint32_t kind_colour(LineKind kind) {
    switch (kind) {
    case LineKind::Travel: return 0x4FA3FF;
    case LineKind::OuterWall: return 0xF28C28;
    case LineKind::InnerWall: return 0xF5CF47;
    case LineKind::OverhangWall: return 0x3A86FF;
    case LineKind::SparseInfill: return 0xC2263E;
    case LineKind::SolidInfill: return 0x9D40E9;
    case LineKind::TopSolidInfill: return 0xFF6F91;
    case LineKind::BridgeInfill: return 0x5BC0EB;
    case LineKind::GapFill: return 0xF2F2F4;
    case LineKind::Ironing: return 0x9AE6B4;
    case LineKind::SkirtBrim: return 0x2EC4B6;
    case LineKind::Support: return 0x43AA8B;
    case LineKind::SupportInterface: return 0x90BE6D;
    case LineKind::WipeTower: return 0xB2B2BC;
    case LineKind::Custom: return 0x808080;
    case LineKind::Other: return 0x6C6C78;
    }
    return 0x808080;
}

} // namespace tisma::ui
