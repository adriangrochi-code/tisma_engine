// Own design; see PROVENANCE.md.

#include "ui/project_slice.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <utility>

namespace tisma::ui {

namespace {

// The mesh by a whole build transform (turn by rows, then offset); a mirror
// keeps its faces outward.
TriangleMesh transformed(const TriangleMesh& mesh, const std::array<double, 9>& t, const std::array<double, 3>& o) {
    const double det = t[0] * (t[4] * t[8] - t[5] * t[7]) - t[1] * (t[3] * t[8] - t[5] * t[6]) +
                       t[2] * (t[3] * t[7] - t[4] * t[6]);
    const auto at = [&](const Vec3f& p) {
        return Vec3f{float(t[0] * p.x + t[1] * p.y + t[2] * p.z + o[0]), float(t[3] * p.x + t[4] * p.y + t[5] * p.z + o[1]),
                     float(t[6] * p.x + t[7] * p.y + t[8] * p.z + o[2])};
    };
    TriangleMesh out;
    for (std::size_t k = 0; k < mesh.triangle_count(); ++k)
        if (det < 0.0)
            out.add_triangle(at(mesh.corner(k, 0)), at(mesh.corner(k, 2)), at(mesh.corner(k, 1)));
        else
            out.add_triangle(at(mesh.corner(k, 0)), at(mesh.corner(k, 1)), at(mesh.corner(k, 2)));
    return out;
}

} // namespace

std::vector<PlateObject> project_objects(std::vector<ProjectModel>& models) {
    std::vector<PlateObject> objects;
    objects.reserve(models.size());
    for (ProjectModel& m : models) {
        // The build transform as the plate's turn and scale (a rotation times
        // a positive scale per axis, formats.md rule 8): the mesh stays in the
        // part's own coordinates, with its conditions. Anything else (a
        // mirror, a shear) is baked into the mesh.
        const std::array<double, 9>& t = m.turn;
        std::array<double, 3> scale{};
        Rotation rotation{};
        bool plain = true;
        for (std::size_t j = 0; j < 3; ++j) {
            scale[j] = std::sqrt(t[j] * t[j] + t[3 + j] * t[3 + j] + t[6 + j] * t[6 + j]);
            plain = plain && scale[j] > 1e-9;
            for (std::size_t i = 0; i < 3 && plain; ++i)
                rotation[3 * i + j] = t[3 * i + j] / scale[j];
        }
        if (plain) {
            for (std::size_t a = 0; a < 3; ++a)
                for (std::size_t b = 0; b < 3; ++b) {
                    double d = 0.0;
                    for (std::size_t i = 0; i < 3; ++i)
                        d += rotation[3 * i + a] * rotation[3 * i + b];
                    plain = plain && std::abs(d - (a == b ? 1.0 : 0.0)) < 1e-6;
                }
            plain = plain && rotation[0] * (rotation[4] * rotation[8] - rotation[5] * rotation[7]) -
                                     rotation[1] * (rotation[3] * rotation[8] - rotation[5] * rotation[6]) +
                                     rotation[2] * (rotation[3] * rotation[7] - rotation[4] * rotation[6]) >
                                 0.0;
        }
        const bool identity = t == std::array<double, 9>{1, 0, 0, 0, 1, 0, 0, 0, 1};
        PlateObject o{m.name, identity || !plain ? transformed(m.mesh, t, m.offset) : m.mesh, {}, std::move(m.painting)};
        if (!identity && plain) {
            o.place.rotation = rotation;
            o.place.scale = scale;
        }
        o.extruder = m.extruder;
        o.wipe_into_infill = m.wipe_into_infill;
        o.wipe_into_object = m.wipe_into_object;
        o.settings = m.settings;
        for (const ProjectVolume& v : m.volumes)
            o.volumes.push_back({v.negative ? PlateVolume::Kind::Negative : PlateVolume::Kind::Modifier,
                                 v.lattice    ? PlateVolume::Shape::Lattice
                                 : v.cylinder ? PlateVolume::Shape::Cylinder
                                              : PlateVolume::Shape::Box,
                                 v.offset, v.size, v.settings, v.turn, v.lattice_cell, v.lattice_diameter});
        for (const ProjectRange& r : m.ranges)
            o.ranges.push_back({r.from, r.to, r.settings});
        // The conditions move with the mesh when it is baked; otherwise they
        // stay in the part's coordinates, as the mesh.
        o.engineering = std::move(m.engineering);
        if (identity || !plain)
            move_engineering(o.engineering, t, m.offset);
        const BoundingBox box = (identity || !plain ? o.mesh : transformed(m.mesh, t, m.offset)).bounding_box();
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
