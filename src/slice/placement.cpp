// Placing the model on the bed. Own design on docs/spec/features/
// placement.md; see PROVENANCE.md.

#include "slice/placement.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>

namespace tisma {

void place_on_bed(TriangleMesh& mesh, const Settings& s, TriangleMesh* follower) {
    // Without a bed, only a centre given by hand moves it (placement.md, PL15).
    if (!s.placement_arrange || (s.bed.empty() && !s.placement_center) || mesh.empty())
        return;
    std::array<double, 2> target{};
    if (s.placement_center) {
        target = *s.placement_center;
    } else {
        double lo[2] = {s.bed.front()[0], s.bed.front()[1]}, hi[2] = {lo[0], lo[1]};
        for (const auto& p : s.bed)
            for (int a = 0; a < 2; ++a)
                lo[a] = std::min(lo[a], p[std::size_t(a)]), hi[a] = std::max(hi[a], p[std::size_t(a)]);
        target = {0.5 * (lo[0] + hi[0]), 0.5 * (lo[1] + hi[1])};
    }
    const BoundingBox box = mesh.bounding_box();
    const double dx = target[0] - 0.5 * (double(box.min.x) + double(box.max.x));
    const double dy = target[1] - 0.5 * (double(box.min.y) + double(box.max.y));
    if (dx == 0.0 && dy == 0.0)
        return;
    mesh.translate(float(dx), float(dy), 0.0f);
    if (follower)
        follower->translate(float(dx), float(dy), 0.0f);
}

std::vector<std::array<double, 2>> arrange_boxes(const std::vector<std::array<double, 2>>& sizes, std::array<double, 2> centre,
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

std::array<double, 2> bed_centre(const Settings& s) {
    if (s.placement_center)
        return *s.placement_center;
    if (s.bed.empty())
        return {0.0, 0.0};
    double lo[2] = {s.bed.front()[0], s.bed.front()[1]}, hi[2] = {lo[0], lo[1]};
    for (const auto& p : s.bed)
        for (int a = 0; a < 2; ++a)
            lo[a] = std::min(lo[a], p[std::size_t(a)]), hi[a] = std::max(hi[a], p[std::size_t(a)]);
    return {0.5 * (lo[0] + hi[0]), 0.5 * (lo[1] + hi[1])};
}

TriangleMesh place_copies(const TriangleMesh& mesh, std::size_t copies, const Settings& s, const std::string& name,
                          std::vector<MeshObject>* objects) {
    TriangleMesh out;
    if (mesh.empty() || copies == 0)
        return out;
    const BoundingBox box = mesh.bounding_box();
    const std::array<double, 2> size{double(box.max.x) - double(box.min.x), double(box.max.y) - double(box.min.y)};
    const std::array<double, 2> own{0.5 * (double(box.min.x) + double(box.max.x)), 0.5 * (double(box.min.y) + double(box.max.y))};
    // Copies only move with placement.arrange and somewhere to centre them on.
    const bool move = s.placement_arrange && (!s.bed.empty() || s.placement_center);
    const double spacing = s.sequential ? std::max(s.placement_spacing, s.clearance_radius) : s.placement_spacing;
    const std::vector<std::array<double, 2>> at =
        arrange_boxes(std::vector<std::array<double, 2>>(copies, size), move ? bed_centre(s) : own, spacing);
    out.reserve(mesh.triangle_count() * copies);
    for (std::size_t k = 0; k < copies; ++k) {
        const float dx = float(at[k][0] - own[0]), dy = float(at[k][1] - own[1]);
        if (objects)
            objects->push_back({name, out.triangle_count(), mesh.triangle_count(), 0, k});
        for (std::size_t t = 0; t < mesh.triangle_count(); ++t) {
            Vec3f c[3];
            for (int i = 0; i < 3; ++i) {
                c[i] = mesh.corner(t, i);
                c[i].x += dx, c[i].y += dy;
            }
            out.add_triangle(c[0], c[1], c[2]);
        }
    }
    return out;
}

TriangleMesh place_objects(const std::vector<TriangleMesh>& meshes, const Settings& s, const std::string& name,
                           std::vector<MeshObject>* objects) {
    TriangleMesh out;
    std::vector<std::array<double, 2>> sizes, own;
    std::vector<float> floor;
    double lo[2] = {1e300, 1e300}, hi[2] = {-1e300, -1e300};
    for (const TriangleMesh& m : meshes) {
        const BoundingBox box = m.bounding_box();
        sizes.push_back({double(box.max.x) - box.min.x, double(box.max.y) - box.min.y});
        own.push_back({0.5 * (double(box.min.x) + box.max.x), 0.5 * (double(box.min.y) + box.max.y)});
        floor.push_back(box.min.z);
        lo[0] = std::min(lo[0], double(box.min.x)), lo[1] = std::min(lo[1], double(box.min.y));
        hi[0] = std::max(hi[0], double(box.max.x)), hi[1] = std::max(hi[1], double(box.max.y));
    }
    if (meshes.empty())
        return out;
    // Without a bed to centre them on, the set stays where the model was.
    const bool move = s.placement_arrange && (!s.bed.empty() || s.placement_center);
    const std::array<double, 2> centre = move ? bed_centre(s) : std::array<double, 2>{0.5 * (lo[0] + hi[0]), 0.5 * (lo[1] + hi[1])};
    const double spacing = s.sequential ? std::max(s.placement_spacing, s.clearance_radius) : s.placement_spacing;
    const std::vector<std::array<double, 2>> at = arrange_boxes(sizes, centre, spacing);
    for (std::size_t k = 0; k < meshes.size(); ++k) {
        if (objects)
            objects->push_back({name + " " + std::to_string(k + 1), out.triangle_count(), meshes[k].triangle_count(), k, 0});
        const float dx = float(at[k][0] - own[k][0]), dy = float(at[k][1] - own[k][1]), dz = -floor[k];
        for (std::size_t t = 0; t < meshes[k].triangle_count(); ++t) {
            Vec3f c[3];
            for (int i = 0; i < 3; ++i) {
                c[i] = meshes[k].corner(t, i);
                c[i].x += dx, c[i].y += dy, c[i].z += dz;
            }
            out.add_triangle(c[0], c[1], c[2]);
        }
    }
    return out;
}

} // namespace tisma
