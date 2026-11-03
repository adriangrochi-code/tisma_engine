// Own design; see PROVENANCE.md.

#include "ui/project_slice.hpp"

#include <algorithm>
#include <string>
#include <utility>

namespace tisma::ui {

namespace {

TriangleMesh moved(const TriangleMesh& mesh, const std::array<double, 3>& offset) {
    if (offset == std::array<double, 3>{0.0, 0.0, 0.0})
        return mesh;
    const Vec3f by{float(offset[0]), float(offset[1]), float(offset[2])};
    TriangleMesh out;
    for (std::size_t t = 0; t < mesh.triangle_count(); ++t) {
        const Vec3f a = mesh.corner(t, 0), b = mesh.corner(t, 1), c = mesh.corner(t, 2);
        out.add_triangle({a.x + by.x, a.y + by.y, a.z + by.z}, {b.x + by.x, b.y + by.y, b.z + by.z},
                         {c.x + by.x, c.y + by.y, c.z + by.z});
    }
    return out;
}

} // namespace

std::vector<PlateObject> project_objects(std::vector<ProjectModel>& models) {
    std::vector<PlateObject> objects;
    objects.reserve(models.size());
    for (ProjectModel& m : models) {
        PlateObject o{m.name, moved(m.mesh, m.offset), {}, std::move(m.painting)};
        o.extruder = m.extruder;
        o.wipe_into_infill = m.wipe_into_infill;
        o.wipe_into_object = m.wipe_into_object;
        o.settings = m.settings;
        for (const ProjectVolume& v : m.volumes)
            o.volumes.push_back({v.negative ? PlateVolume::Kind::Negative : PlateVolume::Kind::Modifier,
                                 v.cylinder ? PlateVolume::Shape::Cylinder : PlateVolume::Shape::Box, v.offset, v.size,
                                 v.settings, v.turn});
        for (const ProjectRange& r : m.ranges)
            o.ranges.push_back({r.from, r.to, r.settings});
        // The conditions move with the mesh, by its offset.
        o.engineering = std::move(m.engineering);
        move_engineering(o.engineering, {1, 0, 0, 0, 1, 0, 0, 0, 1}, m.offset);
        const BoundingBox box = o.mesh.bounding_box();
        o.place.x = 0.5 * (double(box.min.x) + double(box.max.x));
        o.place.y = 0.5 * (double(box.min.y) + double(box.max.y));
        objects.push_back(std::move(o));
    }
    return objects;
}

PlateSlice plate_slice(const std::vector<PlateObject>& objects, const std::vector<TriangleMesh>& placed,
                       const Settings& settings) {
    PlateSlice out;
    out.painting = plate_painting(objects);
    // Each model's triangles in the plate's mesh, as plate_mesh joins them.
    for (std::size_t i = 0, first = 0; i < objects.size() && i < placed.size(); ++i) {
        out.objects.push_back({objects[i].name, first, placed[i].triangle_count(), i, 0, objects[i].extruder,
                               objects[i].wipe_into_infill, objects[i].wipe_into_object, objects[i].settings});
        for (const PlateRange& r : objects[i].ranges)
            out.objects.back().ranges.push_back({r.from, r.to, r.settings});
        first += placed[i].triangle_count();
    }
    // Its modifiers and negative volumes, where the objects stand (model_tools.md, rules 2 and 3).
    for (std::size_t i = 0; i < objects.size() && i < placed.size(); ++i) {
        for (std::size_t k = 0; k < objects[i].volumes.size(); ++k) {
            const PlateVolume& v = objects[i].volumes[k];
            TriangleMesh shape = volume_mesh(v, objects[i], placed[i]);
            if (v.kind == PlateVolume::Kind::Negative) {
                for (std::size_t t = 0; t < shape.triangle_count(); ++t)
                    out.volumes.negative.add_triangle(shape.corner(t, 0), shape.corner(t, 1), shape.corner(t, 2));
            } else {
                out.volumes.modifiers.push_back(
                    {objects[i].name + " modifier " + std::to_string(k + 1), i, std::move(shape), v.settings});
            }
        }
    }
    // The objects go to the slicer when there are several, or when each may
    // print with its own extruder or a mixed colour.
    out.by_object = out.objects.size() > 1 || settings.extruders > 1 || !out.volumes.empty() ||
                    std::any_of(objects.begin(), objects.end(),
                                [](const PlateObject& o) { return !o.settings.empty() || !o.ranges.empty(); });
    return out;
}

} // namespace tisma::ui
