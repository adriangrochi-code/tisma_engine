// Own design; see PROVENANCE.md.

#include "project/engineering_case.hpp"

#include "fea/materials.hpp"
#include "fea/zone.hpp"

#include <cmath>
#include <memory>

namespace tisma {

namespace {

Triangle3 triangle_of(const TriangleMesh& mesh, std::size_t t, const std::array<double, 3>& shift) {
    Triangle3 out;
    for (int k = 0; k < 3; ++k) {
        const Vec3f c = mesh.corner(t, k);
        out[std::size_t(k)] = {double(c.x) + shift[0], double(c.y) + shift[1], double(c.z) + shift[2]};
    }
    return out;
}

// The area-weighted outward normal of triangles (zero for none).
std::array<double, 3> mean_normal(const std::vector<Triangle3>& triangles) {
    std::array<double, 3> n{};
    for (const Triangle3& t : triangles) {
        const double u[3] = {t[1][0] - t[0][0], t[1][1] - t[0][1], t[1][2] - t[0][2]};
        const double v[3] = {t[2][0] - t[0][0], t[2][1] - t[0][1], t[2][2] - t[0][2]};
        n[0] += u[1] * v[2] - u[2] * v[1];
        n[1] += u[2] * v[0] - u[0] * v[2];
        n[2] += u[0] * v[1] - u[1] * v[0];
    }
    return n;
}

std::array<double, 3> unit(std::array<double, 3> v) {
    const double len = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    if (len > 0.0)
        for (double& x : v)
            x /= len;
    return v;
}

} // namespace

std::string condition_name(const EngineeringCondition& condition) {
    return std::string(condition_kind_name(condition.kind)) + " " + std::to_string(condition.number);
}

std::vector<std::string> add_conditions(AnalysisCase& c, const ProjectEngineering& e, const TriangleMesh& mesh,
                                        const std::array<double, 3>& shift) {
    std::vector<std::string> notes;
    std::vector<Triangle3> all;
    all.reserve(mesh.triangle_count());
    for (std::size_t t = 0; t < mesh.triangle_count(); ++t)
        all.push_back(triangle_of(mesh, t, shift));
    for (const EngineeringCondition& condition : e.conditions) {
        if (condition.hidden)
            continue;
        const std::string name = condition_name(condition);
        if (condition.kind == ConditionKind::Limit) {
            notes.push_back(name + ": displacement limits are not checked yet");
            continue;
        }
        auto shape = std::make_shared<ZoneShape>();
        if (condition.is_point()) {
            shape->disc = true;
            shape->centre = {condition.point[0] + shift[0], condition.point[1] + shift[1], condition.point[2] + shift[2]};
            shape->radius = condition.radius;
            shape->triangles = triangles_near(all, shape->centre, condition.radius);
        } else {
            for (const std::uint32_t t : condition.triangles)
                if (t < all.size())
                    shape->triangles.push_back(all[t]);
        }
        if (shape->triangles.empty()) {
            notes.push_back(name + ": marks no surface of the part");
            continue;
        }
        Region region;
        region.shape = shape;
        if (!condition.is_force()) {
            CaseFixture fixture;
            fixture.name = name;
            fixture.region = region;
            c.fixtures.push_back(std::move(fixture));
            continue;
        }
        std::array<double, 3> direction{};
        switch (condition.direction) {
        case ForceDirection::Normal: { // into the part: against the surface's outward normal
            const std::array<double, 3> n = condition.is_point() && !shape->triangles.empty()
                                                ? std::array<double, 3>{condition.normal[0], condition.normal[1],
                                                                        condition.normal[2]}
                                                : mean_normal(shape->triangles);
            direction = unit({-n[0], -n[1], -n[2]});
            break;
        }
        case ForceDirection::Axis:
            if (const int a = std::abs(condition.axis); a >= 1 && a <= 3)
                direction[std::size_t(a - 1)] = condition.axis < 0 ? -1.0 : 1.0;
            break;
        case ForceDirection::Vector:
            direction = unit(condition.vector);
            break;
        }
        CaseLoad load;
        load.name = name;
        load.region = region;
        for (std::size_t a = 0; a < 3; ++a)
            load.force[a] = condition.newtons * direction[a];
        c.loads.push_back(std::move(load));
    }
    return notes;
}

AnalysisCase engineering_analysis_case(const ProjectEngineering& e, const TriangleMesh& mesh,
                                       const std::array<double, 3>& shift, std::vector<std::string>& notes) {
    AnalysisCase c;
    if (e.overrides.material) {
        if (find_material(*e.overrides.material))
            c.material_id = *e.overrides.material;
        else
            notes.push_back("unknown material '" + *e.overrides.material + "': " + c.material_id + " is used");
    }
    c.material = as_printed(*find_material(c.material_id));
    c.quality = e.structural.quality == "high" ? AnalysisQuality::High : AnalysisQuality::Normal;
    for (std::string& note : add_conditions(c, e, mesh, shift))
        notes.push_back(std::move(note));
    return c;
}

void apply_engineering_overrides(Settings& settings, const ProjectEngineering& e, std::vector<std::string>& notes) {
    const EngineeringOverrides& o = e.overrides;
    if (o.layer_height && *o.layer_height > 0.0)
        settings.layer_height = *o.layer_height;
    if (o.first_layer_height && *o.first_layer_height > 0.0)
        settings.first_layer_height = *o.first_layer_height;
    if (o.line_width && *o.line_width > 0.0) {
        settings.wall_width = *o.line_width;
        settings.wall_width_auto = false;
    }
    if (o.extrusion_temperature)
        notes.push_back("the extrusion temperature changed by hand does not change the material yet");
}

} // namespace tisma
