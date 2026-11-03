#include "aero/aero.hpp"

#include "aero/lbm.hpp"
#include "fea/voxels.hpp"
#include "mesh/indexed_mesh.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <optional>

namespace tisma::aero {

namespace {

using V = std::array<double, 3>;

V sub(const V& a, const V& b) { return {a[0] - b[0], a[1] - b[1], a[2] - b[2]}; }
V cross(const V& a, const V& b) { return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]}; }
double dot(const V& a, const V& b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
double norm(const V& a) { return std::sqrt(dot(a, a)); }
V scaled(const V& a, double s) { return {a[0] * s, a[1] * s, a[2] * s}; }

// The lattice's velocity: a Mach number low enough for the lattice.
constexpr double kLatticeVelocity = 0.05;

} // namespace

const char* quality_name(AeroQuality q) {
    switch (q) {
    case AeroQuality::Friction: return "friction";
    case AeroQuality::Fast: return "fast";
    case AeroQuality::Normal: return "normal";
    case AeroQuality::High: return "high";
    case AeroQuality::Extreme: return "extreme";
    }
    return "normal";
}

bool quality_named(const std::string& name, AeroQuality& q) {
    for (const AeroQuality k : {AeroQuality::Friction, AeroQuality::Fast, AeroQuality::Normal, AeroQuality::High, AeroQuality::Extreme})
        if (name == quality_name(k)) {
            q = k;
            return true;
        }
    return false;
}

int quality_cells(AeroQuality q) {
    switch (q) {
    case AeroQuality::Friction: return 0;
    case AeroQuality::Fast: return 16;
    case AeroQuality::Normal: return 24;
    case AeroQuality::High: return 32;
    case AeroQuality::Extreme: return 48;
    }
    return 24;
}

double face_roughness(double nz, double h) {
    const double a = std::abs(nz);
    const double rounding = 0.05 * h;
    // Walls (no staircase) and flat tops and bottoms: the bead's rounding.
    if (a < 0.02 || a > 0.98)
        return rounding;
    return std::max(rounding, h * a / 4.0);
}

double friction_coefficient(double x0, double x1, double speed, double ks, double nu) {
    x0 = std::max(0.0, x0) / 1000.0; // m
    x1 = std::max(x0 + 1e-9, x1 / 1000.0);
    const double mid = 0.5 * (x0 + x1);
    const double re_x = speed * mid / nu;
    const double ks_m = ks / 1000.0;
    const bool rough = ks_m > 0.0 && speed * ks_m / nu > 300.0;
    const double span = x1 - x0;
    // Means over [x0, x1] of the power laws (the leading edge's square root
    // integrated exactly, not sampled).
    const double laminar = 0.664 * std::pow(speed / nu, -0.5) * 2.0 * (std::sqrt(x1) - std::sqrt(x0)) / span;
    const double smooth = 0.0592 * std::pow(speed / nu, -0.2) * (std::pow(x1, 0.8) - std::pow(x0, 0.8)) / (0.8 * span);
    if (rough) {
        const double l = 2.87 + 1.58 * std::log10(std::max(mid / ks_m, 1.0));
        return std::max(smooth, std::pow(l, -2.5));
    }
    return re_x > 5e5 ? smooth : laminar;
}

AeroResult analyze(const AeroInput& in, const std::function<bool(double)>& progress) {
    const auto start = std::chrono::steady_clock::now();
    AeroResult r;
    const std::size_t tris = in.mesh.triangle_count();
    if (tris == 0) {
        r.error = "the part has no triangles";
        return r;
    }
    if (!(in.speed > 0.0) || in.speed > 100.0) {
        r.error = "the speed must be more than 0 and up to 100 m/s";
        return r;
    }
    V d = in.direction;
    const double dl = norm(d);
    if (!(dl > 0.0) || !std::isfinite(dl)) {
        r.error = "the air's direction has no length";
        return r;
    }
    d = scaled(d, 1.0 / dl);
    // A frame with the air along +x: z' as near the print's Z as it can be.
    const V up = std::abs(d[2]) < 0.9 ? V{0, 0, 1} : V{0, 1, 0};
    V ez = sub(up, scaled(d, dot(up, d)));
    ez = scaled(ez, 1.0 / norm(ez));
    const V ey = cross(ez, d);
    const auto turn = [&](const Vec3f& p) {
        const V v{p.x, p.y, p.z};
        return V{dot(v, d), dot(v, ey), dot(v, ez)};
    };

    // The part turned, its faces' normals, areas and the bounding box.
    TriangleMesh turned;
    std::vector<V> normal(tris), print_normal(tris);
    std::vector<double> area(tris);
    V lo{1e300, 1e300, 1e300}, hi{-1e300, -1e300, -1e300};
    for (std::size_t t = 0; t < tris; ++t) {
        const Vec3f a = in.mesh.corner(t, 0), b = in.mesh.corner(t, 1), c = in.mesh.corner(t, 2);
        const V n = cross(sub(V{b.x, b.y, b.z}, V{a.x, a.y, a.z}), sub(V{c.x, c.y, c.z}, V{a.x, a.y, a.z}));
        const double l = norm(n);
        area[t] = 0.5 * l;
        print_normal[t] = l > 0 ? scaled(n, 1.0 / l) : V{0, 0, 0};
        normal[t] = {dot(print_normal[t], d), dot(print_normal[t], ey), dot(print_normal[t], ez)};
        const V p[3] = {turn(a), turn(b), turn(c)};
        for (const V& q : p)
            for (int k = 0; k < 3; ++k)
                lo[k] = std::min(lo[k], q[k]), hi[k] = std::max(hi[k], q[k]);
        turned.add_triangle(Vec3f{float(p[0][0]), float(p[0][1]), float(p[0][2])}, Vec3f{float(p[1][0]), float(p[1][1]), float(p[1][2])},
                            Vec3f{float(p[2][0]), float(p[2][1]), float(p[2][2])});
    }
    const double size_y = hi[1] - lo[1], size_z = hi[2] - lo[2];
    r.frontal_size_mm = std::max(size_y, size_z);
    if (!(r.frontal_size_mm > 0.0)) {
        r.error = "the part has no size across the air";
        return r;
    }
    // The frontal area: the shadow on the y'z' plane, on a fine raster.
    {
        const int res = 400;
        const double px = r.frontal_size_mm / res;
        const int w = int(std::ceil(size_y / px)) + 1, h = int(std::ceil(size_z / px)) + 1;
        std::vector<std::uint8_t> shadow(std::size_t(w) * std::size_t(h), 0);
        for (std::size_t t = 0; t < tris; ++t) {
            const Vec3f a = turned.corner(t, 0), b = turned.corner(t, 1), c = turned.corner(t, 2);
            const double ay = a.y - lo[1], az = a.z - lo[2], by = b.y - lo[1], bz = b.z - lo[2], cy = c.y - lo[1], cz = c.z - lo[2];
            const double den = (by - ay) * (cz - az) - (cy - ay) * (bz - az);
            if (std::abs(den) < 1e-12)
                continue;
            const int i0 = std::max(0, int(std::min({ay, by, cy}) / px)), i1 = std::min(w - 1, int(std::max({ay, by, cy}) / px) + 1);
            const int j0 = std::max(0, int(std::min({az, bz, cz}) / px)), j1 = std::min(h - 1, int(std::max({az, bz, cz}) / px) + 1);
            for (int j = j0; j <= j1; ++j)
                for (int i = i0; i <= i1; ++i) {
                    const double y = (i + 0.5) * px, z = (j + 0.5) * px;
                    const double u = ((y - ay) * (cz - az) - (cy - ay) * (z - az)) / den;
                    const double v = ((by - ay) * (z - az) - (y - ay) * (bz - az)) / den;
                    if (u >= 0 && v >= 0 && u + v <= 1)
                        shadow[std::size_t(j) * std::size_t(w) + std::size_t(i)] = 1;
                }
        }
        std::size_t covered = 0;
        for (const std::uint8_t s : shadow)
            covered += s;
        r.frontal_area_mm2 = double(covered) * px * px;
    }
    const double nu = in.viscosity;
    r.reynolds = in.speed * r.frontal_size_mm / 1000.0 / nu;
    const double q_pa = 0.5 * in.density * in.speed * in.speed;

    // Roughness per face (in print coordinates: the layers are along Z).
    r.ra_um.assign(tris, 0.0f);
    r.cp.assign(tris, 0.0f);
    double ra_area = 0.0, total_area = 0.0;
    for (std::size_t t = 0; t < tris; ++t) {
        const double ra = face_roughness(print_normal[t][2], in.layer_height);
        r.ra_um[t] = float(ra * 1000.0);
        ra_area += ra * area[t], total_area += area[t];
    }
    r.ra_mean_um = total_area > 0 ? ra_area / total_area * 1000.0 : 0.0;

    // Friction: each face in pieces of about 2 mm, its coefficient by the
    // distance from the leading edge; faces looking downstream are in the
    // separated wake and add none.
    double friction = 0.0, friction_smooth = 0.0; // mm2 of q
    for (std::size_t t = 0; t < tris; ++t) {
        const double nx = normal[t][0];
        if (nx > 0.5 || area[t] <= 0.0)
            continue;
        const double along = 1.0 - nx * nx; // the shear's part along the air
        const double ks = 4.0 * r.ra_um[t] / 1000.0;
        const V a{turned.corner(t, 0).x, turned.corner(t, 0).y, turned.corner(t, 0).z};
        const V b{turned.corner(t, 1).x, turned.corner(t, 1).y, turned.corner(t, 1).z};
        const V c{turned.corner(t, 2).x, turned.corner(t, 2).y, turned.corner(t, 2).z};
        const double longest = std::max({norm(sub(b, a)), norm(sub(c, b)), norm(sub(a, c))});
        const int n = std::clamp(int(std::ceil(longest / 2.0)), 1, 64);
        const double piece = area[t] / double(n * n);
        // The n² small triangles of a regular split, by their corners' x.
        const auto at = [&](int i, int j) { // barycentric grid point
            const double u = double(i) / n, v = double(j) / n;
            return a[0] + u * (b[0] - a[0]) + v * (c[0] - a[0]);
        };
        for (int i = 0; i < n; ++i)
            for (int j = 0; i + j < n; ++j) {
                // Upright and, but for the last in each row, upside down.
                for (int flip = 0; flip < (i + j + 1 < n ? 2 : 1); ++flip) {
                    const double x1 = flip == 0 ? at(i, j) : at(i + 1, j + 1);
                    const double x2 = at(i + 1, j), x3 = at(i, j + 1);
                    const double xa = std::min({x1, x2, x3}) - lo[0], xb = std::max({x1, x2, x3}) - lo[0];
                    friction += friction_coefficient(xa, xb, in.speed, ks, nu) * piece * along;
                    friction_smooth += friction_coefficient(xa, xb, in.speed, 0.0, nu) * piece * along;
                }
            }
    }
    r.cd_friction = friction / r.frontal_area_mm2;
    r.roughness_share = friction > 0 ? std::max(0.0, (friction - friction_smooth) / friction) : 0.0;

    // Pressure: the lattice, unless only the friction is asked for.
    const int cells = in.cells > 0 ? in.cells : quality_cells(in.quality);
    double pressure_x = 0.0, pressure_y = 0.0, pressure_z = 0.0;
    if (cells > 0) {
        const double dx = r.frontal_size_mm / cells;
        const double L = r.frontal_size_mm;
        const V origin{lo[0] - 1.5 * L, lo[1] - 1.5 * L, lo[2] - 1.5 * L};
        LbmOptions o;
        o.nx = int(std::ceil((hi[0] - lo[0] + 4.5 * L) / dx));
        o.ny = int(std::ceil((size_y + 3.0 * L) / dx));
        o.nz = int(std::ceil((size_z + 3.0 * L) / dx));
        r.cells_x = o.nx, r.cells_y = o.ny, r.cells_z = o.nz;
        // The part in voxels of the lattice's size, placed in the lattice.
        const VoxelGrid grid = voxelize(weld_exact(turned), dx);
        std::vector<std::uint8_t> solid(std::size_t(o.nx) * std::size_t(o.ny) * std::size_t(o.nz), 0);
        for (int k = 0; k < grid.nz; ++k)
            for (int j = 0; j < grid.ny; ++j)
                for (int i = 0; i < grid.nx; ++i) {
                    if (!grid.solid[grid.index(i, j, k)])
                        continue;
                    const double cx = grid.x0 + (i + 0.5) * grid.hx, cy = grid.y0 + (j + 0.5) * grid.hy,
                                 cz = 0.5 * (grid.z[std::size_t(k)] + grid.z[std::size_t(k) + 1]);
                    const int x = int(std::floor((cx - origin[0]) / dx)), y = int(std::floor((cy - origin[1]) / dx)),
                              z = int(std::floor((cz - origin[2]) / dx));
                    if (x >= 1 && y >= 0 && z >= 0 && x < o.nx - 1 && y < o.ny && z < o.nz)
                        solid[(std::size_t(z) * std::size_t(o.ny) + std::size_t(y)) * std::size_t(o.nx) + std::size_t(x)] = 1;
                }
        r.reynolds_simulated = std::min(r.reynolds, 40.0 * cells);
        o.velocity = kLatticeVelocity;
        o.viscosity = kLatticeVelocity * cells / r.reynolds_simulated;
        o.steps = long(std::ceil(std::max(1.0, in.flow_throughs) * o.nx / kLatticeVelocity));
        o.average_from = o.steps * 2 / 3;
        r.steps = o.steps;
        const LbmResult flow = run_lbm(o, solid, progress);
        r.stable = flow.stable;
        if (!flow.stable) {
            r.error = progress ? "the simulation was stopped or did not stay stable" : "the simulation did not stay stable";
            return r;
        }
        r.simulated = true;
        // The pressure on each face: the mean density just outside it,
        // against the free stream's.
        const double q_lat = 0.5 * kLatticeVelocity * kLatticeVelocity;
        const auto density_at = [&](const V& p) -> std::optional<double> {
            const int x = int(std::floor((p[0] - origin[0]) / dx)), y = int(std::floor((p[1] - origin[1]) / dx)),
                      z = int(std::floor((p[2] - origin[2]) / dx));
            if (x < 0 || y < 0 || z < 0 || x >= o.nx || y >= o.ny || z >= o.nz)
                return std::nullopt;
            const std::size_t c = (std::size_t(z) * std::size_t(o.ny) + std::size_t(y)) * std::size_t(o.nx) + std::size_t(x);
            if (solid[c])
                return std::nullopt;
            return double(flow.mean_density[c]);
        };
        for (std::size_t t = 0; t < tris; ++t) {
            if (area[t] <= 0.0)
                continue;
            const V a{turned.corner(t, 0).x, turned.corner(t, 0).y, turned.corner(t, 0).z};
            const V b{turned.corner(t, 1).x, turned.corner(t, 1).y, turned.corner(t, 1).z};
            const V c{turned.corner(t, 2).x, turned.corner(t, 2).y, turned.corner(t, 2).z};
            const double longest = std::max({norm(sub(b, a)), norm(sub(c, b)), norm(sub(a, c))});
            const int n = std::clamp(int(std::ceil(longest / dx)), 1, 32);
            double sum = 0.0;
            int count = 0;
            for (int i = 0; i < n; ++i)
                for (int j = 0; i + j < n; ++j) {
                    const double u = (i + 1.0 / 3.0) / n, v = (j + 1.0 / 3.0) / n;
                    V p{a[0] + u * (b[0] - a[0]) + v * (c[0] - a[0]), a[1] + u * (b[1] - a[1]) + v * (c[1] - a[1]),
                        a[2] + u * (b[2] - a[2]) + v * (c[2] - a[2])};
                    for (double step = 0.75; step <= 3.0; step += 0.75) {
                        if (const auto rho = density_at({p[0] + normal[t][0] * step * dx, p[1] + normal[t][1] * step * dx,
                                                         p[2] + normal[t][2] * step * dx})) {
                            sum += ((*rho - flow.inlet_density) / 3.0) / q_lat;
                            ++count;
                            break;
                        }
                    }
                }
            const double cp = count > 0 ? sum / count : 0.0;
            r.cp[t] = float(cp);
            pressure_x += -cp * normal[t][0] * area[t];
            pressure_y += -cp * normal[t][1] * area[t];
            pressure_z += -cp * normal[t][2] * area[t];
        }
        r.cd_pressure = pressure_x / r.frontal_area_mm2;

        // Streamlines: seeds on a grid across the part's front, a little
        // wider than it, followed through the mean velocity (Heun's steps
        // of half a cell) until they leave the domain or meet the part.
        const auto cell_of = [&](int x, int y, int z) {
            return (std::size_t(z) * std::size_t(o.ny) + std::size_t(y)) * std::size_t(o.nx) + std::size_t(x);
        };
        // Trilinear, in lattice coordinates (cell centres at i + 0.5).
        const auto sample = [&](double gx, double gy, double gz, double out[4]) {
            const double fx = gx - 0.5, fy = gy - 0.5, fz = gz - 0.5;
            const int x0 = int(std::floor(fx)), y0 = int(std::floor(fy)), z0 = int(std::floor(fz));
            const double tx = fx - x0, ty = fy - y0, tz = fz - z0;
            for (int k = 0; k < 4; ++k)
                out[k] = 0.0;
            double weight = 0.0;
            for (int dz = 0; dz < 2; ++dz)
                for (int dy = 0; dy < 2; ++dy)
                    for (int dxx = 0; dxx < 2; ++dxx) {
                        const int x = std::clamp(x0 + dxx, 0, o.nx - 1), y = std::clamp(y0 + dy, 0, o.ny - 1), z = std::clamp(z0 + dz, 0, o.nz - 1);
                        const std::size_t c = cell_of(x, y, z);
                        if (solid[c])
                            continue;
                        const double w = (dxx ? tx : 1 - tx) * (dy ? ty : 1 - ty) * (dz ? tz : 1 - tz);
                        out[0] += w * flow.mean_velocity[3 * c];
                        out[1] += w * flow.mean_velocity[3 * c + 1];
                        out[2] += w * flow.mean_velocity[3 * c + 2];
                        out[3] += w * ((flow.mean_density[c] - flow.inlet_density) / 3.0) / q_lat;
                        weight += w;
                    }
            if (weight < 1e-9)
                return false;
            for (int k = 0; k < 4; ++k)
                out[k] /= weight;
            return true;
        };
        const auto solid_at = [&](double gx, double gy, double gz) {
            const int x = int(gx), y = int(gy), z = int(gz);
            return x >= 0 && y >= 0 && z >= 0 && x < o.nx && y < o.ny && z < o.nz && solid[cell_of(x, y, z)] != 0;
        };
        constexpr int kSeeds = 11;
        const double ylo = (lo[1] - origin[1]) / dx - 0.25 * size_y / dx, yhi = (hi[1] - origin[1]) / dx + 0.25 * size_y / dx;
        const double zlo = (lo[2] - origin[2]) / dx - 0.25 * size_z / dx, zhi = (hi[2] - origin[2]) / dx + 0.25 * size_z / dx;
        for (int sj = 0; sj < kSeeds; ++sj)
            for (int sk = 0; sk < kSeeds; ++sk) {
                double p[3] = {3.0, ylo + (yhi - ylo) * (sj + 0.5) / kSeeds, zlo + (zhi - zlo) * (sk + 0.5) / kSeeds};
                if (size_z <= 1e-9)
                    p[2] = 0.5 * (zlo + zhi);
                AeroResult::Streamline line;
                const int max_steps = 4 * (o.nx + o.ny + o.nz);
                for (int step = 0; step < max_steps; ++step) {
                    double a[4], b[4];
                    if (!sample(p[0], p[1], p[2], a))
                        break;
                    const double speed_a = std::sqrt(a[0] * a[0] + a[1] * a[1] + a[2] * a[2]);
                    if (speed_a < 1e-5)
                        break;
                    const double h = 0.5 / speed_a; // half a cell along the flow
                    if (step % 2 == 0) {
                        // In the input's coordinates: back from the turned frame.
                        const double tx = origin[0] + p[0] * dx, ty = origin[1] + p[1] * dx, tz = origin[2] + p[2] * dx;
                        line.points.push_back({float(d[0] * tx + ey[0] * ty + ez[0] * tz), float(d[1] * tx + ey[1] * ty + ez[1] * tz),
                                               float(d[2] * tx + ey[2] * ty + ez[2] * tz)});
                        line.cp.push_back(float(a[3]));
                    }
                    const double q[3] = {p[0] + h * a[0], p[1] + h * a[1], p[2] + h * a[2]};
                    if (!sample(q[0], q[1], q[2], b))
                        break;
                    for (int k = 0; k < 3; ++k)
                        p[k] += 0.5 * h * (a[k] + b[k]);
                    if (p[0] < 0 || p[1] < 0 || p[2] < 0 || p[0] >= o.nx - 1 || p[1] >= o.ny || p[2] >= o.nz || solid_at(p[0], p[1], p[2]))
                        break;
                }
                if (line.points.size() >= 2)
                    r.streamlines.push_back(std::move(line));
            }
    } else {
        r.reynolds_simulated = 0.0;
    }
    r.cd = r.cd_pressure + r.cd_friction;
    r.drag_n = r.cd * q_pa * r.frontal_area_mm2 * 1e-6;
    r.side_force_n = std::hypot(pressure_y, pressure_z) * q_pa * 1e-6;
    // What a symmetric part gives across the air is rounding noise: 0 below
    // a millionth of the drag.
    if (r.side_force_n < 1e-6 * std::abs(r.drag_n))
        r.side_force_n = 0.0;
    r.ok = true;
    r.seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    if (progress)
        progress(1.0);
    return r;
}

std::string result_json(const AeroResult& r, const AeroInput& in) {
    const auto finite = [](double x) { return std::isfinite(x) ? nlohmann::json(x) : nlohmann::json(nullptr); };
    nlohmann::json cp = nlohmann::json::array(), ra = nlohmann::json::array();
    for (const float v : r.cp)
        cp.push_back(std::round(double(v) * 1000.0) / 1000.0);
    for (const float v : r.ra_um)
        ra.push_back(std::round(double(v) * 10.0) / 10.0);
    nlohmann::json j{{"format", "tisma-aero-result"},
                     {"version", 1},
                     {"inputs",
                      {{"speed", in.speed},
                       {"direction", in.direction},
                       {"quality", quality_name(in.quality)},
                       {"layer_height", in.layer_height}}},
                     {"drag_n", finite(r.drag_n)},
                     {"cd", finite(r.cd)},
                     {"cd_pressure", finite(r.cd_pressure)},
                     {"cd_friction", finite(r.cd_friction)},
                     {"side_force_n", finite(r.side_force_n)},
                     {"frontal_area_mm2", finite(r.frontal_area_mm2)},
                     {"reynolds", finite(r.reynolds)},
                     {"reynolds_simulated", finite(r.reynolds_simulated)},
                     {"simulated", r.simulated},
                     {"ra_mean_um", finite(r.ra_mean_um)},
                     {"roughness_share", finite(r.roughness_share)},
                     {"cells", {r.cells_x, r.cells_y, r.cells_z}},
                     {"steps", r.steps},
                     {"cp", cp},
                     {"ra_um", ra}};
    nlohmann::json lines = nlohmann::json::array();
    for (const AeroResult::Streamline& s : r.streamlines) {
        nlohmann::json pts = nlohmann::json::array(), cps = nlohmann::json::array();
        for (const auto& p : s.points)
            for (const float v : p)
                pts.push_back(std::round(double(v) * 100.0) / 100.0);
        for (const float v : s.cp)
            cps.push_back(std::round(double(v) * 1000.0) / 1000.0);
        lines.push_back({{"points", pts}, {"cp", cps}});
    }
    j["streamlines"] = lines;
    return j.dump();
}

} // namespace tisma::aero
