// Own design; see PROVENANCE.md.

#include "project/engineering_case.hpp"

#include "fea/verdict.hpp"
#include "report/analysis_json.hpp"

#include <nlohmann/json.hpp>

#include "fea/materials.hpp"
#include "fea/zone.hpp"
#include "project/engineering_values.hpp"

#include <algorithm>
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
            notes.push_back(name + ": displacement limits are checked after the solve (Structural tab)");
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
                                       const std::array<double, 3>& shift, std::vector<std::string>& notes,
                                       std::string_view filament_type, double layer_height) {
    AnalysisCase c;
    const std::string by_type = material_for_filament(filament_type);
    const auto known = [](const std::string& id) { return find_material(id) || family_of_material_id(id); };
    if (e.overrides.material && known(*e.overrides.material))
        c.material_id = *e.overrides.material;
    else if (!by_type.empty())
        c.material_id = by_type;
    if (e.overrides.material && !known(*e.overrides.material))
        notes.push_back("unknown material '" + *e.overrides.material + "': " + c.material_id + " is used");
    else if (!e.overrides.material && by_type.empty())
        notes.push_back("no material for the filament type '" + std::string(filament_type) + "': " + c.material_id +
                        " is used");
    // A family chosen to try (rule 12): the table's typical values.
    if (const MaterialFamily* family = family_of_material_id(c.material_id))
        c.material = family_material(*family);
    else
        c.material = as_printed(*find_material(c.material_id));
    // The working temperature (engineering_panel.md, rule 4): stiffness and
    // strength times f(T) of the material's family (structural_analysis.md,
    // method, point 5).
    if (const MaterialFamily* family = family_of(c.material_id); family && e.working_temperature > 23.0) {
        const double f = temperature_factor(*family, e.working_temperature);
        OrthotropicMaterial& m = c.material;
        for (double* v : {&m.e1, &m.e2, &m.e3, &m.g12, &m.g13, &m.g23, &m.xt, &m.xc, &m.yt, &m.yc, &m.zt, &m.zc, &m.s12,
                          &m.s13, &m.s23})
            *v *= f;
    }
    c.quality = e.structural.quality == "high" ? AnalysisQuality::High : AnalysisQuality::Normal;
    // Rápida (structural_analysis.md, «Resolución»: about 20 000 voxels): the
    // whole part solid, voxels of the size that gives that many over its
    // box, without the resolution check.
    if (e.structural.quality == "fast" && mesh.triangle_count() > 0) {
        const BoundingBox b = mesh.bounding_box();
        const double volume = std::max(double(b.max.x - b.min.x), 0.1) * std::max(double(b.max.y - b.min.y), 0.1) *
                              std::max(double(b.max.z - b.min.z), 0.1);
        // A row of voxels per layer: the voxels as wide as gives 20 000.
        const double layer = layer_height > 0.0 ? layer_height : 0.2;
        c.model = AnalysisModel::Solid;
        c.voxel = std::max(std::sqrt(volume / (layer * 20000.0)), layer);
        c.rows_per_layer = 1;
        c.resolution_check = false;
    }
    for (std::string& note : add_conditions(c, e, mesh, shift))
        notes.push_back(std::move(note));
    return c;
}

std::vector<LimitCheck> check_limits(const ProjectEngineering& e, const TriangleMesh& mesh, const AnalysisReport& report) {
    std::vector<LimitCheck> out;
    const VoxelGrid& g = report.grid;
    const std::vector<double>& u = report.result.elasticity.displacement;
    const std::size_t nodes = std::size_t(g.nx + 1) * std::size_t(g.ny + 1) * std::size_t(g.nz + 1);
    if (g.nx <= 0 || u.size() != 3 * nodes)
        return out;
    const BoundingBox box = mesh.bounding_box();
    const double largest = std::max({double(box.max.x - box.min.x), double(box.max.y - box.min.y), double(box.max.z - box.min.z)});
    double reach = std::max(g.hx, g.hy);
    for (int k = 0; k < g.nz; ++k)
        reach = std::max(reach, g.hz(k));
    for (const EngineeringCondition& condition : e.conditions) {
        if (condition.hidden || condition.kind != ConditionKind::Limit)
            continue;
        ZoneShape shape;
        for (const std::uint32_t t : condition.triangles)
            if (t < mesh.triangle_count())
                shape.triangles.push_back(triangle_of(mesh, t, {}));
        LimitCheck check;
        check.name = condition_name(condition);
        check.limit_mm = condition.limit_percent ? condition.limit / 100.0 * largest : condition.limit;
        if (!shape.triangles.empty()) {
            const SurfaceZone zone(shape, reach, reach);
            const std::array<double, 6>& b = zone.bounds();
            for (int k = 0; k <= g.nz; ++k)
                for (int j = 0; j <= g.ny; ++j)
                    for (int i = 0; i <= g.nx; ++i) {
                        const double x = g.x0 + i * g.hx, y = g.y0 + j * g.hy, z = g.z[std::size_t(k)];
                        if (x < b[0] || y < b[1] || z < b[2] || x > b[3] || y > b[4] || z > b[5])
                            continue;
                        // A node of the part: a corner of a solid voxel.
                        bool used = false;
                        for (int dk = -1; dk <= 0 && !used; ++dk)
                            for (int dj = -1; dj <= 0 && !used; ++dj)
                                for (int di = -1; di <= 0 && !used; ++di)
                                    used = g.at(i + di, j + dj, k + dk);
                        if (!used || !zone.near(x, y, z))
                            continue;
                        const std::size_t n = (std::size_t(k) * std::size_t(g.ny + 1) + std::size_t(j)) * std::size_t(g.nx + 1) + std::size_t(i);
                        check.displacement_mm = std::max(check.displacement_mm,
                                                         std::sqrt(u[3 * n] * u[3 * n] + u[3 * n + 1] * u[3 * n + 1] + u[3 * n + 2] * u[3 * n + 2]));
                    }
        }
        check.exceeded = check.displacement_mm > check.limit_mm;
        out.push_back(check);
    }
    return out;
}

std::string structural_report_json(const AnalysisReport& report, const AnalysisCase& c, const ProjectEngineering& e,
                                   const std::array<double, 12>& to_part, const TriangleMesh* mesh) {
    nlohmann::json j = nlohmann::json::parse(analysis_json(report, c, to_part));
    nlohmann::json limits = nlohmann::json::array();
    bool exceeded = false;
    if (mesh)
        for (const LimitCheck& l : check_limits(e, *mesh, report)) {
            limits.push_back({{"name", l.name}, {"limit_mm", l.limit_mm}, {"displacement_mm", l.displacement_mm}, {"exceeded", l.exceeded}});
            exceeded = exceeded || l.exceeded;
        }
    VerdictInput in;
    in.limit_exceeded = exceeded;
    in.family = family_of(c.material_id);
    in.required_safety = e.structural.safety_factor;
    in.working_temperature = e.working_temperature;
    in.min_safety = report.result.min_safety;
    const Verdict v = structural_verdict(in);
    nlohmann::json suggestions = nlohmann::json::array();
    if (v != Verdict::Holds && v != Verdict::NotApplicable)
        for (const MaterialSuggestion& s : suggest_materials(in))
            suggestions.push_back({{"family", std::string(s.family->name)},
                                   {"min_safety", std::isfinite(s.safety) ? nlohmann::json(s.safety) : nlohmann::json(nullptr)}});
    j["verdict"] = {{"name", verdict_name(v)},
                    {"valid", report.valid},
                    {"required_safety", in.required_safety},
                    {"working_temperature", in.working_temperature},
                    {"family", in.family ? std::string(in.family->name) : std::string()},
                    {"mass_g", in.family ? voxel_mass_grams(report.grid, in.family->density) : 0.0},
                    {"limits_checked", mesh != nullptr},
                    {"limits", limits},
                    {"suggestions", suggestions}};
    return j.dump();
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
