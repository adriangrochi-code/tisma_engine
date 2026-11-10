// Own design; see PROVENANCE.md.

#include "project/engineering_case.hpp"

#include "fea/verdict.hpp"
#include "report/analysis_json.hpp"

#include <nlohmann/json.hpp>

#include "fea/materials.hpp"
#include "fea/zone.hpp"
#include "mesh/indexed_mesh.hpp"
#include "mesh/lattice.hpp"
#include "project/engineering_values.hpp"

#include <algorithm>
#include <cmath>
#include <memory>
#include <numbers>

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
                                       std::string_view filament_type, double layer_height,
                                       const PrintConditions& print) {
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
    // How the lines stick together as this part prints (the owner's request,
    // 2026-11-03): the materials' constants are measured at their sources'
    // settings; the factors move them to the part's.
    c.adhesion = adhesion(family_of(c.material_id), print);
    apply_adhesion(c.material, c.adhesion);
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
    // Extrema (the owner's request, 2026-11-03; the cloud's «ultra»): 6
    // voxels per line width.
    c.quality = e.structural.quality == "high"                                     ? AnalysisQuality::High
                : e.structural.quality == "extreme" || e.structural.quality == "ultra" ? AnalysisQuality::Extreme
                                                                                     : AnalysisQuality::Normal;
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
        // The check costs an eighth more and gives the band its voxel error
        // (the owner's request, 2026-11-03: the most reliable result each
        // quality can give).
        c.resolution_check = true;
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

FailureMode weakest_mode(const AnalysisReport& report) {
    const std::vector<FailureMode>& mode = report.result.mode;
    return report.result.weakest < mode.size() ? mode[report.result.weakest] : FailureMode::None;
}

Verdict report_verdict(const AnalysisReport& report, const AnalysisCase& c, const ProjectEngineering& e,
                       const TriangleMesh& mesh) {
    VerdictInput in;
    for (const LimitCheck& l : check_limits(e, mesh, report))
        in.limit_exceeded = in.limit_exceeded || l.exceeded;
    in.family = family_of(c.material_id);
    in.required_safety = e.structural.safety_factor;
    in.working_temperature = e.working_temperature;
    in.min_safety = report.result.min_safety;
    return structural_verdict(in);
}

double infill_strength_exponent(InfillPattern pattern) {
    // structural_analysis.md, «Exponentes del relleno»; the patterns the table
    // does not name take the nearest kind's (lines as rectilinear, 3D cells
    // as cubic).
    switch (pattern) {
    case InfillPattern::Honeycomb: return 1.4;
    case InfillPattern::Cubic:
    case InfillPattern::AdaptiveCubic:
    case InfillPattern::SupportCubic:
    case InfillPattern::Octagram: return 1.4;
    case InfillPattern::Gyroid: return 1.5;
    case InfillPattern::Lightning: return 2.5;
    default: return 1.2;
    }
}

namespace {

TriangleMesh box_between(const Vec3f& lo, const Vec3f& hi) {
    const Vec3f v[8] = {{lo.x, lo.y, lo.z}, {hi.x, lo.y, lo.z}, {hi.x, hi.y, lo.z}, {lo.x, hi.y, lo.z},
                        {lo.x, lo.y, hi.z}, {hi.x, lo.y, hi.z}, {hi.x, hi.y, hi.z}, {lo.x, hi.y, hi.z}};
    static constexpr int kFaces[12][3] = {{0, 2, 1}, {0, 3, 2}, {4, 5, 6}, {4, 6, 7}, {0, 1, 5}, {0, 5, 4},
                                          {1, 2, 6}, {1, 6, 5}, {2, 3, 7}, {2, 7, 6}, {3, 0, 4}, {3, 4, 7}};
    TriangleMesh m;
    for (const auto& f : kFaces)
        m.add_triangle(v[f[0]], v[f[1]], v[f[2]]);
    return m;
}

} // namespace

namespace {

// The part cut in cells of 3 voxels a side and at least 3 mm (in Z by the
// planes' heights): structural_optimization.md, rules 3 and 4.
struct Cells {
    const VoxelGrid& g;
    int cx = 3, cy = 3, nx = 0, ny = 0, nz = 0;
    std::vector<int> z_cell; // each layer of voxels to its cell row
    std::vector<double> z_cut;
    explicit Cells(const VoxelGrid& grid) : g(grid) {
        cx = std::max(3, int(std::ceil(3.0 / g.hx)));
        cy = std::max(3, int(std::ceil(3.0 / g.hy)));
        z_cell.resize(std::size_t(g.nz));
        z_cut = {g.z.front()};
        for (int k = 0, count = 0; k < g.nz; ++k) {
            z_cell[std::size_t(k)] = int(z_cut.size()) - 1;
            ++count;
            if ((count >= 3 && g.z[std::size_t(k) + 1] - z_cut.back() >= 3.0 - 1e-9) || k + 1 == g.nz) {
                z_cut.push_back(g.z[std::size_t(k) + 1]);
                count = 0;
            }
        }
        nx = (g.nx + cx - 1) / cx;
        ny = (g.ny + cy - 1) / cy;
        nz = int(z_cut.size()) - 1;
    }
    std::size_t count() const { return std::size_t(nx) * std::size_t(ny) * std::size_t(nz); }
    std::size_t cell(int i, int j, int k) const { return (std::size_t(k) * std::size_t(ny) + std::size_t(j)) * std::size_t(nx) + std::size_t(i); }
    std::size_t of(int i, int j, int k) const { return cell(i / cx, j / cy, z_cell[std::size_t(k)]); }
    // Each run along X of cells in the same group (group_of: -1 for none) as
    // one box: emit(group, cells, lo, hi).
    template <class GroupOf, class Emit>
    void rows(GroupOf group_of, Emit emit) const {
        for (int k = 0; k < nz; ++k)
            for (int j = 0; j < ny; ++j)
                for (int i = 0; i < nx;) {
                    const int which = group_of(cell(i, j, k));
                    if (which < 0) {
                        ++i;
                        continue;
                    }
                    int end = i + 1;
                    while (end < nx && group_of(cell(end, j, k)) == which)
                        ++end;
                    const Vec3f lo{float(g.x0 + i * cx * g.hx), float(g.y0 + j * cy * g.hy), float(z_cut[std::size_t(k)])};
                    const Vec3f hi{float(g.x0 + std::min(end * cx, g.nx) * g.hx), float(g.y0 + std::min((j + 1) * cy, g.ny) * g.hy),
                                   float(z_cut[std::size_t(k) + 1])};
                    emit(which, std::size_t(end - i), lo, hi);
                    i = end;
                }
    }
};

} // namespace

InfillZones infill_zones(const AnalysisReport& report, double density, double asked, InfillPattern pattern, double scale) {
    InfillZones out;
    const VoxelGrid& g = report.grid;
    const std::vector<double>& safety = report.result.safety;
    if (g.solid.empty() || safety.size() != g.solid.size() || g.z.size() < 2)
        return out;
    const double n = infill_strength_exponent(pattern);
    const Cells cells(g);
    std::vector<double> want(cells.count(), -1.0); // -1: no voxel
    for (int k = 0; k < g.nz; ++k)
        for (int j = 0; j < g.ny; ++j)
            for (int i = 0; i < g.nx; ++i) {
                if (!g.at(i, j, k))
                    continue;
                const double s = safety[g.index(i, j, k)];
                const double u = std::isfinite(s) && s > 0.0 ? asked / s : 0.0;
                double& w = want[cells.of(i, j, k)];
                w = std::max(w, std::min(100.0, density * std::pow(u, 1.0 / n)));
            }
    // Three levels by thirds of the cells, each at its most.
    std::vector<double> sorted;
    for (double w : want)
        if (w >= 0.0)
            sorted.push_back(w);
    if (sorted.empty())
        return out;
    std::sort(sorted.begin(), sorted.end());
    const std::size_t third = sorted.size() / 3;
    const std::array<double, 2> cut{sorted[std::min(sorted.size() - 1, third)], sorted[std::min(sorted.size() - 1, 2 * third)]};
    std::array<double, 3> most{0.0, 0.0, 0.0};
    const auto level_of = [&](double w) { return w <= cut[0] ? 0 : w <= cut[1] ? 1 : 2; };
    for (double w : sorted)
        most[std::size_t(level_of(w))] = std::max(most[std::size_t(level_of(w))], w);
    for (std::size_t l = 0; l < 3; ++l)
        out.density[l] = std::min(100.0, std::round(most[l] * scale));
    // The boxes: a level's cells along X joined.
    cells.rows([&](std::size_t c) { return want[c] < 0.0 ? -1 : level_of(want[c]); },
               [&](int level, std::size_t count, const Vec3f& lo, const Vec3f& hi) {
                   out.cells[std::size_t(level)] += count;
                   out.modifiers.push_back({"zone " + std::to_string(level + 1), 0, box_between(lo, hi),
                                            "{\"infill\": {\"density\": " + std::to_string(int(out.density[std::size_t(level)])) + "}}"});
               });
    return out;
}

InfillRibs infill_ribs(const AnalysisReport& report) {
    InfillRibs out;
    const VoxelGrid& g = report.grid;
    const std::vector<double>& safety = report.result.safety;
    const auto& stress = report.result.layer_stress;
    if (g.solid.empty() || safety.size() != g.solid.size() || stress.size() != g.solid.size() || g.z.size() < 2)
        return out;
    // Each voxel: its principal stresses in the layer, the larger one's
    // direction θ and how much it works one way, a = (|σ1| - |σ2|) / (|σ1| + |σ2|);
    // each cell averages a · (cos 2θ, sin 2θ) weighed by the failure index.
    const Cells cells(g);
    struct Sum {
        double c = 0.0, s = 0.0, weight = 0.0, most = 0.0;
        bool any = false;
    };
    std::vector<Sum> sum(cells.count());
    double most = 0.0;
    for (int k = 0; k < g.nz; ++k)
        for (int j = 0; j < g.ny; ++j)
            for (int i = 0; i < g.nx; ++i) {
                if (!g.at(i, j, k))
                    continue;
                const std::size_t v = g.index(i, j, k);
                Sum& t = sum[cells.of(i, j, k)];
                t.any = true;
                const double f = std::isfinite(safety[v]) && safety[v] > 0.0 ? 1.0 / safety[v] : 0.0;
                if (f <= 0.0)
                    continue;
                const double sx = stress[v][0], sy = stress[v][1], txy = stress[v][2];
                const double mid = 0.5 * (sx + sy), r = std::hypot(0.5 * (sx - sy), txy);
                const double s1 = mid + r, s2 = mid - r;
                if (std::abs(s1) + std::abs(s2) <= 0.0)
                    continue;
                const double a = std::abs(std::abs(s1) - std::abs(s2)) / (std::abs(s1) + std::abs(s2));
                // The direction of s1 is at 2θ1 = atan2(2τ, σx - σy); s2's is 90° from it.
                double two_theta = std::atan2(2.0 * txy, sx - sy);
                if (std::abs(s2) > std::abs(s1))
                    two_theta += std::numbers::pi;
                t.c += f * a * std::cos(two_theta);
                t.s += f * a * std::sin(two_theta);
                t.weight += f;
                t.most = std::max(t.most, f);
                most = std::max(most, f);
            }
    if (most <= 0.0)
        return out;
    // The cells kept: at least 20 % of the largest failure index, and working
    // one way 0.5 or more on average; grouped by angle in steps of 15°.
    constexpr double kStep = 15.0;
    std::vector<int> group(cells.count(), -1);
    for (std::size_t c = 0; c < cells.count(); ++c) {
        const Sum& t = sum[c];
        if (!t.any || t.weight <= 0.0 || t.most < 0.2 * most || std::hypot(t.c, t.s) / t.weight < 0.5)
            continue;
        double degrees = 0.5 * std::atan2(t.s, t.c) * 180.0 / std::numbers::pi; // (-90, 90]
        int step = int(std::lround(degrees / kStep));
        step = ((step % 12) + 12) % 12; // 0 to 165 degrees
        group[c] = step;
        ++out.cells;
    }
    std::array<std::size_t, 12> used{};
    cells.rows([&](std::size_t c) { return group[c]; },
               [&](int step, std::size_t count, const Vec3f& lo, const Vec3f& hi) {
                   used[std::size_t(step)] += count;
                   const int degrees = int(step * kStep);
                   out.modifiers.push_back({"rib " + std::to_string(degrees), 0, box_between(lo, hi),
                                            "{\"infill\": {\"pattern\": \"aligned_rectilinear\", \"angle\": " + std::to_string(degrees) + "}}"});
               });
    for (int step = 0; step < 12; ++step)
        if (used[std::size_t(step)] > 0) {
            out.angles.push_back(step * kStep);
            out.group_cells.push_back(used[std::size_t(step)]);
        }
    return out;
}

Reinforcement reinforcement_zone(const AnalysisReport& report, const AnalysisCase& c, const Settings& settings, double radius) {
    Reinforcement out;
    const VoxelGrid& g = report.grid;
    if (g.solid.empty() || g.z.size() < 2)
        return out;
    if (radius <= 0.0) {
        double load = 0.0;
        for (const CaseLoad& l : c.loads)
            if (l.region.shape && l.region.shape->disc)
                load = std::max(load, l.region.shape->radius);
        radius = std::max(5.0, 2.0 * load);
    }
    out.radius = radius;
    // The places: each region's marked surface, else its box (a face of the
    // part: the grid's side).
    const double x1 = g.x0 + g.nx * g.hx, y1 = g.y0 + g.ny * g.hy, z0 = g.z.front(), z1 = g.z.back();
    struct Place {
        std::unique_ptr<SurfaceZone> zone;
        double lo[3], hi[3];
    };
    std::vector<Place> places;
    const auto add = [&](const Region& r) {
        Place p;
        for (int a = 0; a < 3; ++a)
            p.lo[a] = r.box.min[a], p.hi[a] = r.box.max[a];
        switch (r.face) {
        case PartFace::Bottom: p.hi[2] = z0; break;
        case PartFace::Top: p.lo[2] = z1; break;
        case PartFace::XMin: p.hi[0] = g.x0; break;
        case PartFace::XMax: p.lo[0] = x1; break;
        case PartFace::YMin: p.hi[1] = g.y0; break;
        case PartFace::YMax: p.lo[1] = y1; break;
        case PartFace::None: break;
        }
        if (r.shape)
            p.zone = std::make_unique<SurfaceZone>(*r.shape, radius, radius);
        places.push_back(std::move(p));
    };
    for (const CaseFixture& f : c.fixtures)
        add(f.region);
    for (const CaseLoad& l : c.loads)
        add(l.region);
    const auto near = [&](double x, double y, double z) {
        for (const Place& p : places) {
            if (p.zone) {
                if (p.zone->near(x, y, z))
                    return true;
                continue;
            }
            const double q[3] = {x, y, z};
            double d2 = 0.0;
            for (int a = 0; a < 3; ++a) {
                const double d = std::max({p.lo[a] - q[a], 0.0, q[a] - p.hi[a]});
                d2 += d * d;
            }
            if (d2 <= radius * radius)
                return true;
        }
        return false;
    };
    // Cells of 2 mm (at least a voxel) with some of the part.
    const int cx = std::max(1, int(std::lround(2.0 / g.hx))), cy = std::max(1, int(std::lround(2.0 / g.hy)));
    std::vector<int> z_cell(std::size_t(g.nz));
    std::vector<double> z_cut{z0};
    for (int k = 0; k < g.nz; ++k) {
        z_cell[std::size_t(k)] = int(z_cut.size()) - 1;
        if (g.z[std::size_t(k) + 1] - z_cut.back() >= 2.0 - 1e-9 || k + 1 == g.nz)
            z_cut.push_back(g.z[std::size_t(k) + 1]);
    }
    const int nx = (g.nx + cx - 1) / cx, ny = (g.ny + cy - 1) / cy, nz = int(z_cut.size()) - 1;
    std::vector<std::uint8_t> part(std::size_t(nx) * std::size_t(ny) * std::size_t(nz), 0);
    const auto cell = [&](int i, int j, int k) { return (std::size_t(k) * std::size_t(ny) + std::size_t(j)) * std::size_t(nx) + std::size_t(i); };
    for (int k = 0; k < g.nz; ++k)
        for (int j = 0; j < g.ny; ++j)
            for (int i = 0; i < g.nx; ++i)
                if (g.at(i, j, k))
                    part[cell(i / cx, j / cy, z_cell[std::size_t(k)])] = 1;
    std::vector<std::uint8_t> in(part.size(), 0);
    for (int k = 0; k < nz; ++k)
        for (int j = 0; j < ny; ++j)
            for (int i = 0; i < nx; ++i) {
                if (!part[cell(i, j, k)])
                    continue;
                const double x = g.x0 + (std::min((i + 1) * cx, g.nx) + i * cx) * 0.5 * g.hx;
                const double y = g.y0 + (std::min((j + 1) * cy, g.ny) + j * cy) * 0.5 * g.hy;
                const double z = 0.5 * (z_cut[std::size_t(k)] + z_cut[std::size_t(k) + 1]);
                if (near(x, y, z)) {
                    in[cell(i, j, k)] = 1;
                    ++out.cells;
                }
            }
    const std::string own = "{\"walls\": {\"count\": " + std::to_string(settings.wall_count + 2) +
                            "}, \"infill\": {\"density\": " + std::to_string(int(std::max(40.0, std::ceil(settings.infill_density)))) + "}}";
    for (int k = 0; k < nz; ++k)
        for (int j = 0; j < ny; ++j)
            for (int i = 0; i < nx;) {
                if (!in[cell(i, j, k)]) {
                    ++i;
                    continue;
                }
                int end = i + 1;
                while (end < nx && in[cell(end, j, k)])
                    ++end;
                const Vec3f lo{float(g.x0 + i * cx * g.hx), float(g.y0 + j * cy * g.hy), float(z_cut[std::size_t(k)])};
                const Vec3f hi{float(g.x0 + std::min(end * cx, g.nx) * g.hx), float(g.y0 + std::min((j + 1) * cy, g.ny) * g.hy),
                               float(z_cut[std::size_t(k) + 1])};
                out.modifiers.push_back({"reinforcement", 0, box_between(lo, hi), own});
                i = end;
            }
    return out;
}

MeshModifier lattice_modifier(const TriangleMesh& part, double cell, double diameter) {
    return {"lattice", 0, lattice_in(part, cell, diameter), "{\"infill\": {\"density\": 100}}"};
}

double lattice_cell_for(const TriangleMesh& part) {
    const BoundingBox b = part.bounding_box();
    const double smallest = std::min({double(b.max.x) - b.min.x, double(b.max.y) - b.min.y, double(b.max.z) - b.min.z});
    return std::clamp(smallest / 3.0, 3.0, 10.0);
}

LightestLattice lightest_lattice(const IndexedMesh& mesh, const TriangleMesh& placed, const Settings& settings,
                                 const AnalysisCase& c, const ProjectEngineering& e,
                                 const std::function<void(double)>& progress) {
    LightestLattice out;
    const auto finish = [&]() -> LightestLattice {
        if (progress)
            progress(1.0);
        return out;
    };
    if (c.model == AnalysisModel::Solid)
        return finish();
    constexpr double kThinnest = 0.9, kThickest = 3.0, kStep = 0.05;
    constexpr int kMostAnalyses = 8; // 3 mm, 0.9 mm, then halving to 0.05 mm
    out.cell = lattice_cell_for(placed);
    const MaterialFamily* family = family_of(c.material_id);
    const double grams_per_cm3 = family ? family->density : 1.0;
    Settings hollow = settings;
    hollow.infill_density = 0.0;
    const auto holds = [&](double diameter, LightestLattice& at) {
        const std::vector<MeshModifier> mods{lattice_modifier(placed, out.cell, diameter)};
        const int step = out.analyses++;
        const AnalysisReport r = run_analysis(
            mesh, hollow, c, nullptr,
            [&](std::string_view, double done) {
                if (progress)
                    progress(std::min(1.0, (double(step) + std::clamp(done, 0.0, 1.0)) / kMostAnalyses));
            },
            nullptr, &mods);
        at.min_safety = r.result.min_safety;
        at.mass_g = voxel_mass_grams(r.grid, grams_per_cm3);
        at.lattice = mods.front();
        at.diameter = diameter;
        return r.valid && report_verdict(r, c, e, placed) == Verdict::Holds;
    };
    LightestLattice best = out;
    if (!holds(kThickest, best)) {
        out.min_safety = best.min_safety;
        return finish();
    }
    LightestLattice trial = out;
    if (holds(kThinnest, trial)) {
        best = trial;
    } else {
        double fails = kThinnest, holds_at = kThickest;
        while (holds_at - fails > kStep + 1e-9) {
            const double mid = std::round(0.5 * (fails + holds_at) / 0.01) * 0.01;
            if (holds(mid, trial)) {
                holds_at = mid;
                best = trial;
            } else {
                fails = mid;
            }
        }
    }
    best.analyses = out.analyses;
    best.cell = out.cell;
    best.found = true;
    out = best;
    return finish();
}

GradedLattice graded_lattice(const IndexedMesh& mesh, const TriangleMesh& placed, const Settings& settings,
                             const AnalysisCase& c, const ProjectEngineering& e, const LightestLattice& uniform,
                             const std::function<void(double)>& progress) {
    GradedLattice out;
    const auto finish = [&]() -> GradedLattice {
        if (progress)
            progress(1.0);
        return out;
    };
    if (!uniform.found || c.model == AnalysisModel::Solid)
        return finish();
    constexpr double kThinnest = 0.9, kThickest = 3.0;
    constexpr int kHalvings = 6, kMostAnalyses = 1 + kHalvings + 1;
    const MaterialFamily* family = family_of(c.material_id);
    const double grams_per_cm3 = family ? family->density : 1.0;
    Settings hollow = settings;
    hollow.infill_density = 0.0;
    const auto analyse = [&](const std::vector<MeshModifier>& mods) {
        const int step = out.analyses++;
        return run_analysis(
            mesh, hollow, c, nullptr,
            [&](std::string_view, double done) {
                if (progress)
                    progress(std::min(1.0, (double(step) + std::clamp(done, 0.0, 1.0)) / kMostAnalyses));
            },
            nullptr, &mods);
    };
    // The uniform lattice's analysis: the utilization at each bar's middle.
    const BoundingBox b = placed.bounding_box();
    out.bars = bars_touching(lattice_bars({b.min.x, b.min.y, b.min.z}, {b.max.x, b.max.y, b.max.z}, uniform.cell), placed);
    if (out.bars.empty())
        return finish();
    const AnalysisReport base = analyse({uniform.lattice});
    if (!base.valid)
        return finish();
    const double asked = e.structural.safety_factor;
    const VoxelGrid& g = base.grid;
    // The worst utilization within half a cell of the bar's middle.
    std::vector<double> use(out.bars.size(), 0.0);
    const double reach = 0.5 * uniform.cell;
    for (std::size_t n = 0; n < out.bars.size(); ++n) {
        const LatticeBar& bar = out.bars[n];
        const double mx = 0.5 * (bar.from[0] + bar.to[0]), my = 0.5 * (bar.from[1] + bar.to[1]), mz = 0.5 * (bar.from[2] + bar.to[2]);
        const int i0 = std::max(0, int(std::floor((mx - reach - g.x0) / g.hx))), i1 = std::min(g.nx - 1, int(std::floor((mx + reach - g.x0) / g.hx)));
        const int j0 = std::max(0, int(std::floor((my - reach - g.y0) / g.hy))), j1 = std::min(g.ny - 1, int(std::floor((my + reach - g.y0) / g.hy)));
        for (int k = 0; k < g.nz; ++k) {
            const double zc = 0.5 * (g.z[std::size_t(k)] + g.z[std::size_t(k) + 1]);
            if (std::abs(zc - mz) > reach)
                continue;
            for (int j = j0; j <= j1; ++j)
                for (int i = i0; i <= i1; ++i)
                    if (g.at(i, j, k)) {
                        const double s = base.result.safety[g.index(i, j, k)];
                        if (std::isfinite(s) && s > 0.0)
                            use[n] = std::max(use[n], asked / s);
                    }
        }
    }
    const double most = *std::max_element(use.begin(), use.end());
    if (!(most > 0.0))
        return finish();
    // d = k sqrt(u / most): k = 3 is every bar at most 3 mm; halve k between
    // what fails and what holds, starting from the uniform diameter.
    const auto diameters_for = [&](double k) {
        std::vector<double> d(use.size());
        for (std::size_t n = 0; n < use.size(); ++n)
            d[n] = std::clamp(k * std::sqrt(use[n] / most), kThinnest, kThickest);
        return d;
    };
    const auto holds = [&](double k, GradedLattice& at) {
        at.diameters = diameters_for(k);
        const std::vector<MeshModifier> mods{{"lattice", 0, lattice_mesh(out.bars, at.diameters), "{\"infill\": {\"density\": 100}}"}};
        const AnalysisReport r = analyse(mods);
        at.min_safety = r.result.min_safety;
        at.mass_g = voxel_mass_grams(r.grid, grams_per_cm3);
        return r.valid && report_verdict(r, c, e, placed) == Verdict::Holds;
    };
    GradedLattice best, trial;
    double fails = kThinnest, holds_at = kThickest;
    bool any = false;
    for (int h = 0; h < kHalvings; ++h) {
        const double k = 0.5 * (fails + holds_at);
        if (holds(k, trial)) {
            holds_at = k;
            best = trial;
            any = true;
        } else {
            fails = k;
        }
    }
    if (!any)
        return finish();
    out.found = true;
    out.diameters = best.diameters;
    out.min_safety = best.min_safety;
    out.mass_g = best.mass_g;
    out.thinnest = *std::min_element(out.diameters.begin(), out.diameters.end());
    out.thickest = *std::max_element(out.diameters.begin(), out.diameters.end());
    out.lighter = out.mass_g < uniform.mass_g;
    return finish();
}

std::array<double, 9> layers_along(int axis) {
    switch (axis) {
    case 1: return {0, 0, -1, 0, 1, 0, 1, 0, 0}; // a quarter turn about Y: X up
    case 2: return {1, 0, 0, 0, 0, -1, 0, 1, 0}; // a quarter turn about X: Y up
    default: return {1, 0, 0, 0, 1, 0, 0, 0, 1};
    }
}

OrientationChoice recommended_orientation(const TriangleMesh& placed, const Settings& settings, const ProjectEngineering& e,
                                          std::string_view filament_type, const std::function<void(double)>& progress) {
    OrientationChoice out;
    for (int axis = 0; axis < 3; ++axis) {
        const std::array<double, 9> r = layers_along(axis);
        std::vector<Vec3f> corners = placed.corners();
        for (Vec3f& v : corners)
            v = Vec3f{float(r[0] * v.x + r[1] * v.y + r[2] * v.z), float(r[3] * v.x + r[4] * v.y + r[5] * v.z),
                      float(r[6] * v.x + r[7] * v.y + r[8] * v.z)};
        const TriangleMesh turned(std::move(corners));
        ProjectEngineering moved = e;
        turn_engineering(moved, r);
        std::vector<std::string> notes;
        AnalysisCase c = engineering_analysis_case(moved, turned, {}, notes, filament_type, settings.layer_height,
                                                   print_conditions(settings, moved));
        const IndexedMesh mesh = weld_exact(turned);
        fit_voxel_limit(c, mesh, settings);
        const AnalysisReport report = run_analysis(mesh, settings, c, nullptr, [&](std::string_view, double done) {
            if (progress)
                progress((double(axis) + std::clamp(done, 0.0, 1.0)) / 3.0);
        });
        ++out.analyses;
        out.safety[std::size_t(axis)] = report.valid && std::isfinite(report.result.min_safety) ? report.result.min_safety : 0.0;
    }
    for (int axis = 1; axis < 3; ++axis)
        if (out.safety[std::size_t(axis)] > out.safety[std::size_t(out.best)])
            out.best = axis;
    out.recommend = out.best != 0 && out.safety[std::size_t(out.best)] > 1.05 * out.safety[0];
    if (progress)
        progress(1.0);
    return out;
}

ReinforcedInfill reinforced_infill(const IndexedMesh& mesh, const TriangleMesh& placed, const Settings& settings,
                                   const AnalysisCase& c, const ProjectEngineering& e, int density,
                                   const std::function<void(double)>& progress) {
    ReinforcedInfill out;
    const auto finish = [&]() -> ReinforcedInfill {
        if (progress)
            progress(1.0);
        return out;
    };
    if (c.model == AnalysisModel::Solid)
        return finish();
    // The plain analysis (a tenth of the bar), the search (eight tenths),
    // the mass with the zone (the last tenth).
    const MaterialFamily* family = family_of(c.material_id);
    const double grams_per_cm3 = family ? family->density : 1.0;
    const auto part = [&](double from, double width) {
        return [&progress, from, width](std::string_view, double done) {
            if (progress)
                progress(from + width * std::clamp(done, 0.0, 1.0));
        };
    };
    Settings at = settings;
    at.infill_density = double(density);
    const AnalysisReport plain = run_analysis(mesh, at, c, nullptr, part(0.0, 0.1));
    ++out.analyses;
    if (!plain.valid)
        return finish();
    out.plain_mass_g = voxel_mass_grams(plain.grid, grams_per_cm3);
    out.zone = reinforcement_zone(plain, c, at);
    if (out.zone.modifiers.empty())
        return finish();
    out.best = lightest_infill(
        mesh, placed, settings, c, e,
        [&](double done) {
            if (progress)
                progress(0.1 + 0.8 * done);
        },
        &out.zone.modifiers);
    out.analyses += out.best.analyses;
    if (!out.best.found || out.best.solid)
        return finish();
    out.found = true;
    Settings best = settings;
    best.infill_density = double(out.best.density);
    const AnalysisReport r = run_analysis(mesh, best, c, nullptr, part(0.9, 0.1), nullptr, &out.zone.modifiers);
    ++out.analyses;
    out.mass_g = voxel_mass_grams(r.grid, grams_per_cm3);
    out.lighter = out.mass_g < out.plain_mass_g;
    return finish();
}

RibbedInfill ribbed_infill(const IndexedMesh& mesh, const TriangleMesh& placed, const Settings& settings,
                           const AnalysisCase& c, const ProjectEngineering& e, int density,
                           const std::function<void(double)>& progress) {
    RibbedInfill out;
    if (c.model == AnalysisModel::Solid || density <= 0) {
        if (progress)
            progress(1.0);
        return out;
    }
    // The analysis without ribs, then `density` with them, then halving
    // below it: at most 2 + 7 analyses.
    constexpr int kMostAnalyses = 9;
    const auto analyse = [&](int at, const std::vector<MeshModifier>* modifiers) {
        Settings s = settings;
        s.infill_density = double(at);
        const int step = out.analyses++;
        return run_analysis(mesh, s, c, nullptr,
                            [&](std::string_view, double done) {
                                if (progress)
                                    progress((double(step) + std::clamp(done, 0.0, 1.0)) / kMostAnalyses);
                            },
                            nullptr, modifiers);
    };
    const auto finish = [&]() -> RibbedInfill {
        if (progress)
            progress(1.0);
        return out;
    };
    const AnalysisReport plain = analyse(density, nullptr);
    if (!plain.valid)
        return finish();
    out.ribs = infill_ribs(plain);
    if (out.ribs.modifiers.empty())
        return finish();
    const auto holds = [&](int at, double& safety) {
        const AnalysisReport r = analyse(at, &out.ribs.modifiers);
        safety = r.result.min_safety;
        return r.valid && report_verdict(r, c, e, placed) == Verdict::Holds;
    };
    double safety = 0.0;
    if (!holds(density, safety))
        return finish();
    out.found = true;
    out.density = density;
    out.min_safety = safety;
    int fails = -1;
    while (out.density - fails > 1) {
        const int mid = (fails + out.density) / 2;
        if (holds(mid, safety)) {
            out.density = mid;
            out.min_safety = safety;
        } else {
            fails = mid;
        }
    }
    out.lighter = out.density < density;
    return finish();
}

ZonedInfill zoned_infill(const IndexedMesh& mesh, const TriangleMesh& placed, const Settings& settings,
                         const AnalysisCase& c, const ProjectEngineering& e, int density,
                         const std::function<void(double)>& progress) {
    ZonedInfill out;
    if (c.model == AnalysisModel::Solid)
        return out;
    // The uniform analysis, then scale 1, then halving: at most 7 analyses.
    constexpr int kMostAnalyses = 7, kHalvings = 5;
    const MaterialFamily* family = family_of(c.material_id);
    const double grams_per_cm3 = family ? family->density : 1.0;
    const auto analyse = [&](const Settings& s, const std::vector<MeshModifier>* modifiers) {
        const int step = out.analyses++;
        return run_analysis(mesh, s, c, nullptr,
                            [&](std::string_view, double done) {
                                if (progress)
                                    progress((double(step) + std::clamp(done, 0.0, 1.0)) / kMostAnalyses);
                            },
                            nullptr, modifiers);
    };
    Settings uniform = settings;
    uniform.infill_density = double(density);
    const AnalysisReport base = analyse(uniform, nullptr);
    if (!base.valid) {
        if (progress)
            progress(1.0);
        return out;
    }
    out.uniform_mass_g = voxel_mass_grams(base.grid, grams_per_cm3);
    const double asked = e.structural.safety_factor;
    // A try: the object at the lowest level, the denser levels as modifiers.
    const auto holds = [&](double scale, InfillZones& zones, double& mass, double& safety) {
        zones = infill_zones(base, double(density), asked, settings.infill_pattern, scale);
        if (zones.modifiers.empty())
            return false;
        const std::string lowest = "zone 1";
        std::vector<MeshModifier> denser;
        for (const MeshModifier& m : zones.modifiers)
            if (m.name != lowest)
                denser.push_back(m);
        zones.modifiers = denser;
        Settings s = settings;
        s.infill_density = zones.density[0];
        const AnalysisReport r = analyse(s, &zones.modifiers);
        mass = voxel_mass_grams(r.grid, grams_per_cm3);
        safety = r.result.min_safety;
        return r.valid && report_verdict(r, c, e, placed) == Verdict::Holds;
    };
    const auto keep = [&](const InfillZones& zones, double mass, double safety) {
        out.found = true;
        out.zones = zones;
        out.mass_g = mass;
        out.min_safety = safety;
    };
    InfillZones zones;
    double mass = 0.0, safety = 0.0;
    // At scale 1 every cell gets what brings it to the asked factor; the
    // scale that fills the lowest level to 100 % is the most that matters.
    double fails = 0.0, holds_at = 0.0;
    if (holds(1.0, zones, mass, safety)) {
        keep(zones, mass, safety);
        holds_at = 1.0;
    } else {
        fails = 1.0;
        holds_at = 100.0 / std::max(1.0, infill_zones(base, double(density), asked, settings.infill_pattern).density[0]);
    }
    for (int h = 0; h < kHalvings && holds_at > fails; ++h) {
        const double mid = 0.5 * (fails + holds_at);
        if (holds(mid, zones, mass, safety)) {
            keep(zones, mass, safety);
            holds_at = mid;
        } else {
            fails = mid;
        }
    }
    out.lighter = out.found && out.mass_g < out.uniform_mass_g;
    if (progress)
        progress(1.0);
    return out;
}

LightestInfill lightest_infill(const IndexedMesh& mesh, const TriangleMesh& placed, const Settings& settings,
                               const AnalysisCase& c, const ProjectEngineering& e,
                               const std::function<void(double)>& progress, const std::vector<MeshModifier>* modifiers) {
    LightestInfill out;
    if (c.model == AnalysisModel::Solid) {
        out.solid = true;
        return out;
    }
    // 100 %, then halving between the highest that fails and the lowest
    // that holds: at most 8 analyses.
    constexpr int kMostAnalyses = 8;
    const auto holds = [&](int density, double& safety) {
        Settings s = settings;
        s.infill_density = double(density);
        const int step = out.analyses++;
        const AnalysisReport r = run_analysis(
            mesh, s, c, nullptr,
            [&](std::string_view, double done) {
                if (progress)
                    progress((double(step) + std::clamp(done, 0.0, 1.0)) / kMostAnalyses);
            },
            nullptr, modifiers);
        safety = r.result.min_safety;
        return r.valid && report_verdict(r, c, e, placed) == Verdict::Holds;
    };
    double safety = 0.0;
    if (!holds(100, safety)) {
        out.min_safety = safety;
        if (progress)
            progress(1.0);
        return out;
    }
    out.found = true;
    out.min_safety = safety;
    int fails = -1, holds_at = 100;
    while (holds_at - fails > 1) {
        const int mid = (fails + holds_at) / 2;
        double s = 0.0;
        if (holds(mid, s)) {
            holds_at = mid;
            out.min_safety = s;
        } else {
            fails = mid;
        }
    }
    out.density = holds_at;
    if (progress)
        progress(1.0);
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
    // The adhesion the material was given and what its uncertainty makes of
    // the smallest safety factor (an approximation; the owner's request,
    // 2026-11-03).
    const FailureMode mode = weakest_mode(report);
    const double discretization = report.resolution.done ? std::abs(report.resolution.change) : 0.0;
    const SafetyBand band = safety_band(report.result.min_safety, mode, c.adhesion, discretization);
    const auto finite_or_null = [](double x) { return std::isfinite(x) ? nlohmann::json(x) : nlohmann::json(nullptr); };
    j["adhesion"] = {{"modelled", c.adhesion.modelled},
                     {"nozzle_temperature", c.adhesion.temperature},
                     {"reference_temperature", c.adhesion.reference_temperature},
                     {"z_strength_factor", c.adhesion.z_strength},
                     {"z_stiffness_factor", c.adhesion.z_stiffness},
                     {"across_strength_factor", c.adhesion.across_strength},
                     {"z_ratio", c.adhesion.z_ratio},
                     {"z_uncertainty", c.adhesion.z_uncertainty},
                     {"across_uncertainty", c.adhesion.across_uncertainty},
                     {"too_cold", c.adhesion.too_cold},
                     {"too_hot", c.adhesion.too_hot},
                     {"weakest_mode", failure_mode_name(mode)},
                     {"min_safety_low", finite_or_null(band.low)},
                     {"min_safety_high", finite_or_null(band.high)},
                     {"band_sigma", band.sigma},
                     {"discretization", discretization}};
    return j.dump();
}

PrintConditions print_conditions(const Settings& settings, const ProjectEngineering& e) {
    PrintConditions p;
    const EngineeringValues v = engineering_values(settings, e);
    if (v.extrusion_temperature.value > 0.0)
        p.nozzle_temperature = v.extrusion_temperature.value;
    if (v.layer_height.value > 0.0)
        p.layer_height = v.layer_height.value;
    if (v.line_width.value > 0.0)
        p.line_width = v.line_width.value;
    p.fan = std::clamp(settings.fan_max / 100.0, 0.0, 1.0);
    // A chamber only where the printer heats one.
    if (settings.max_chamber_temp > 0.0 && settings.chamber_temperature > 0.0)
        p.chamber = std::min(settings.chamber_temperature, settings.max_chamber_temp);
    return p;
}

void apply_engineering_overrides(Settings& settings, const ProjectEngineering& e, std::vector<std::string>& /*notes*/) {
    const EngineeringOverrides& o = e.overrides;
    if (o.layer_height && *o.layer_height > 0.0)
        settings.layer_height = *o.layer_height;
    if (o.first_layer_height && *o.first_layer_height > 0.0)
        settings.first_layer_height = *o.first_layer_height;
    if (o.line_width && *o.line_width > 0.0) {
        settings.wall_width = *o.line_width;
        settings.wall_width_auto = false;
    }
}

} // namespace tisma
