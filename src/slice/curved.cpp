// Curved layers (docs/spec/features/nonplanar.md, rules 1 to 8). The
// shapes, the ramp, the fitting to the head and J are the spec's; the
// conforming mesh refinement, the bounds' sampling and the G-code transform
// are own design. See PROVENANCE.md.

#include "slice/curved.hpp"

#include "gcode/dialect.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <map>
#include <numbers>
#include <sstream>

namespace tisma {

namespace {

constexpr double kTipRadius = 2.0; // mm: the cone's rounded tip (rule 1)
constexpr double kMinJ = 0.6, kMaxJ = 1.4;

double smooth(double t) {
    t = std::clamp(t, 0.0, 1.0);
    return t * t * (3.0 - 2.0 * t);
}

double radians(double degrees) { return degrees * std::numbers::pi / 180.0; }

} // namespace

double CurvedField::shape(double x, double y, double z) const {
    switch (mode) {
    case CurvedMode::Conical:
        return cone * (std::sqrt(x * x + y * y + kTipRadius * kTipRadius) - kTipRadius);
    case CurvedMode::Surface: {
        if (grid.empty())
            return 0.0;
        // Bilinear between the nodes, level past the grid's edges.
        const double gx = std::clamp((x + cx - grid_x) / cell, 0.0, double(nx - 1));
        const double gy = std::clamp((y + cy - grid_y) / cell, 0.0, double(ny - 1));
        const int i = std::min(int(gx), nx - 2), j = std::min(int(gy), ny - 2);
        const double u = gx - i, v = gy - j;
        const auto at = [&](int a, int b) { return double(grid[std::size_t(b) * std::size_t(nx) + std::size_t(a)]); };
        return scale * ((1 - u) * (1 - v) * at(i, j) + u * (1 - v) * at(i + 1, j) + (1 - u) * v * at(i, j + 1) +
                        u * v * at(i + 1, j + 1));
    }
    case CurvedMode::Wave:
        if (pattern == CurvedPattern::Egg)
            return amplitude * std::sin(k * x) * std::sin(k * y);
        {
            const double a = pattern == CurvedPattern::Twisted ? angle + twist * z : angle;
            return amplitude * std::sin(k * (x * std::cos(a) + y * std::sin(a)));
        }
    default:
        return 0.0;
    }
}

double CurvedField::ramp(double z) const {
    // Top faces: evenly over the whole height up to them (rule 15, SU3).
    if (mode == CurvedMode::Surface)
        return full > flat_below ? std::clamp((z - flat_below) / (full - flat_below), 0.0, 1.0) : 1.0;
    double r = smooth((z - flat_below) / ramp_height);
    if (flat_top)
        r *= 1.0 - smooth((z - (height - ramp_height)) / ramp_height);
    return r;
}

double CurvedField::offset(double x, double y, double z) const {
    const double h = z - base;
    if (h <= flat_below)
        return 0.0;
    return shape(x - cx, y - cy, h) * ramp(h);
}

double CurvedField::offset_dz(double x, double y, double z) const {
    const double e = 1e-4;
    return (offset(x, y, z + e) - offset(x, y, z - e)) / (2.0 * e);
}

std::array<double, 2> CurvedField::slope(double x, double y, double z) const {
    const double e = 1e-4;
    return {(offset(x + e, y, z) - offset(x - e, y, z)) / (2.0 * e), (offset(x, y + e, z) - offset(x, y - e, z)) / (2.0 * e)};
}

double CurvedField::thickness(double x, double y, double z) const {
    if (flat_at(z))
        return 1.0; // under the ramp: flat, whatever the ramp's kink
    return 1.0 / (1.0 - offset_dz(x, y, z));
}

double CurvedField::real_z(double x, double y, double flat) const {
    if (flat_at(flat))
        return flat;
    // z - D(z) = flat: D changes less than z does (J > 0), so this converges.
    double z = flat + offset(x, y, flat);
    for (int i = 0; i < 60; ++i) {
        const double next = flat + offset(x, y, z);
        if (std::abs(next - z) < 1e-9)
            return next;
        z = next;
    }
    return z;
}

double CurvedField::most() const {
    if (mode == CurvedMode::Wave)
        return amplitude;
    if (mode == CurvedMode::Conical)
        return std::abs(cone) * (std::sqrt(radius * radius + kTipRadius * kTipRadius) - kTipRadius);
    if (mode == CurvedMode::Surface) {
        double most = 0.0;
        for (const float h : grid)
            most = std::max(most, std::abs(double(h)));
        return most * scale;
    }
    return 0.0;
}

namespace {

// The part's box.
struct Box {
    double x0, y0, z0, x1, y1, z1;
};

Box box_of(const IndexedMesh& mesh) {
    Box b{1e300, 1e300, 1e300, -1e300, -1e300, -1e300};
    for (const Vec3f& v : mesh.vertices) {
        b.x0 = std::min(b.x0, double(v.x)), b.y0 = std::min(b.y0, double(v.y)), b.z0 = std::min(b.z0, double(v.z));
        b.x1 = std::max(b.x1, double(v.x)), b.y1 = std::max(b.y1, double(v.y)), b.z1 = std::max(b.z1, double(v.z));
    }
    return b;
}

// What the field does over the part's box, sampled: the thickest and
// thinnest J, the steepest slope as the head takes it (1 is all it allows),
// how far D goes up and down, and its steepest slope's size.
struct Bounds {
    double j_min = 1.0, j_max = 1.0;
    double head = 0.0;
    double d_min = 0.0, d_max = 0.0;
    double steepest = 0.0;
};

Bounds bounds_of(const CurvedField& f, const Box& b, double tan_x, double tan_y) {
    Bounds out;
    const int nxy = 48;
    // Heights: through the whole part, and closer in the ramps.
    std::vector<double> zs;
    for (int i = 0; i <= 64; ++i)
        zs.push_back(b.z0 + (b.z1 - b.z0) * i / 64.0);
    for (int i = 0; i <= 32; ++i) {
        zs.push_back(b.z0 + f.flat_below + f.ramp_height * i / 32.0);
        if (f.flat_top)
            zs.push_back(b.z0 + f.height - f.ramp_height * i / 32.0);
    }
    for (const double z : zs) {
        if (z < b.z0 || z > b.z1)
            continue;
        for (int i = 0; i <= nxy; ++i)
            for (int j = 0; j <= nxy; ++j) {
                const double x = b.x0 + (b.x1 - b.x0) * i / nxy, y = b.y0 + (b.y1 - b.y0) * j / nxy;
                const double jj = f.thickness(x, y, z);
                out.j_min = std::min(out.j_min, jj), out.j_max = std::max(out.j_max, jj);
                const double d = f.offset(x, y, z);
                out.d_min = std::min(out.d_min, d), out.d_max = std::max(out.d_max, d);
                const auto [gx, gy] = f.slope(x, y, z);
                out.head = std::max(out.head, std::sqrt((gx / tan_x) * (gx / tan_x) + (gy / tan_y) * (gy / tan_y)));
                out.steepest = std::max(out.steepest, std::hypot(gx, gy));
            }
    }
    return out;
}

// How much the printed part rises next to the nozzle, against what the head
// lets it (rule 6): over 1 is a crash. With head.profile, each measured
// height's free angle bounds the rise within the distance it reaches.
double clearance_use(const Bounds& b, const Settings& s) {
    const double range = b.d_max - b.d_min;
    double use = std::min(range, b.steepest * s.head_clearance_radius) / s.head_clearance_height;
    for (const auto& [h, angle] : s.head_profile) {
        const double reach = h / std::tan(radians(angle));
        use = std::max(use, std::min(range, b.steepest * reach) / h);
    }
    return use;
}

// Top faces (nonplanar_modes.md, rules 14 and 15): every face looking up
// asks the layers under it for its own slope (the steepest at the head's
// free slope), by its area and by how flat it lies; h on a grid over the
// part is the least squares fit of those slopes, edge by edge between the
// nodes, with a little weight everywhere so it stays smooth where nothing
// asks. False when no face slopes.
bool fit_surface(CurvedField& f, const IndexedMesh& mesh, const Box& b, double tan_x, double tan_y) {
    const double dx = b.x1 - b.x0, dy = b.y1 - b.y0;
    f.cell = std::max({1.0, dx / 63.0, dy / 63.0});
    f.nx = int(std::ceil(dx / f.cell)) + 2, f.ny = int(std::ceil(dy / f.cell)) + 2;
    f.grid_x = b.x0 - 0.5 * f.cell, f.grid_y = b.y0 - 0.5 * f.cell;
    const int cx = f.nx - 1, cy = f.ny - 1; // cells
    std::vector<double> weight(std::size_t(cx * cy), 0.0), sx(weight.size(), 0.0), sy(weight.size(), 0.0);
    double lowest_slope = 1e300; // where the sloping faces start
    for (const auto& tri : mesh.triangles) {
        const Vec3f& a = mesh.vertices[tri[0]];
        const Vec3f& p = mesh.vertices[tri[1]];
        const Vec3f& q = mesh.vertices[tri[2]];
        const double ux = double(p.x) - a.x, uy = double(p.y) - a.y, uz = double(p.z) - a.z;
        const double vx = double(q.x) - a.x, vy = double(q.y) - a.y, vz = double(q.z) - a.z;
        const double nx = uy * vz - uz * vy, ny = uz * vx - ux * vz, nz = ux * vy - uy * vx;
        const double length = std::sqrt(nx * nx + ny * ny + nz * nz);
        if (length <= 0.0 || nz / length < 0.05)
            continue; // not looking up
        const double area = 0.5 * length, up = nz / length;
        double tx = -nx / nz, ty = -ny / nz; // the face's slope
        const double over = std::sqrt((tx / tan_x) * (tx / tan_x) + (ty / tan_y) * (ty / tan_y));
        if (over > 1.0)
            tx /= over, ty /= over;
        if (std::hypot(tx, ty) > 0.005)
            lowest_slope = std::min({lowest_slope, double(a.z), double(p.z), double(q.z)});
        // Spread over the cells it covers, a sample every half cell.
        const double longest = std::max({std::hypot(ux, uy), std::hypot(vx, vy), std::hypot(vx - ux, vy - uy)});
        const int n = std::max(1, int(std::ceil(longest / (0.5 * f.cell))));
        const double share = area * up * up / (double(n + 1) * double(n + 2) / 2.0);
        for (int i = 0; i <= n; ++i)
            for (int j = 0; i + j <= n; ++j) {
                const double x = a.x + (ux * i + vx * j) / n, y = a.y + (uy * i + vy * j) / n;
                const int ci = std::clamp(int((x - f.grid_x) / f.cell), 0, cx - 1);
                const int cj = std::clamp(int((y - f.grid_y) / f.cell), 0, cy - 1);
                const std::size_t c = std::size_t(cj * cx + ci);
                weight[c] += share, sx[c] += share * tx, sy[c] += share * ty;
            }
    }
    if (lowest_slope > 1e299)
        return false;
    // Each edge between two nodes: the difference its cells ask for.
    double heaviest = 0.0;
    for (const double w : weight)
        heaviest = std::max(heaviest, w);
    const double soft = 1e-3 * heaviest, anchor = 1e-6 * heaviest;
    struct Edge {
        std::size_t a, b;
        double w, d;
    };
    std::vector<Edge> edges;
    const auto node = [&](int i, int j) { return std::size_t(j * f.nx + i); };
    const auto cell_at = [&](int i, int j) { return std::size_t(j * cx + i); };
    for (int j = 0; j < f.ny; ++j)
        for (int i = 0; i < f.nx; ++i)
            for (int along = 0; along < 2; ++along) {
                if ((along == 0 && i + 1 >= f.nx) || (along == 1 && j + 1 >= f.ny))
                    continue;
                double w = 0.0, s = 0.0;
                int cells = 0;
                for (int side = -1; side <= 0; ++side) { // the cells each side of the edge
                    const int ci = along == 0 ? i : i + side, cj = along == 0 ? j + side : j;
                    if (ci < 0 || cj < 0 || ci >= cx || cj >= cy)
                        continue;
                    const std::size_t c = cell_at(ci, cj);
                    w += weight[c], s += along == 0 ? sx[c] : sy[c];
                    ++cells;
                }
                if (cells == 0)
                    continue;
                const double slope = w > 0.0 ? s / w : 0.0;
                edges.push_back({node(i, j), along == 0 ? node(i + 1, j) : node(i, j + 1), w / cells + soft,
                                 (w / cells) / (w / cells + soft) * slope * f.cell});
            }
    // Conjugate gradients on the normal equations.
    const std::size_t n = std::size_t(f.nx * f.ny);
    const auto apply = [&](const std::vector<double>& h, std::vector<double>& out) {
        for (std::size_t k = 0; k < n; ++k)
            out[k] = anchor * h[k];
        for (const Edge& e : edges) {
            const double r = e.w * (h[e.b] - h[e.a]);
            out[e.b] += r, out[e.a] -= r;
        }
    };
    std::vector<double> rhs(n, 0.0), h(n, 0.0), r(n), p(n), ap(n);
    for (const Edge& e : edges)
        rhs[e.b] += e.w * e.d, rhs[e.a] -= e.w * e.d;
    r = rhs, p = r;
    double rr = 0.0;
    for (const double v : r)
        rr += v * v;
    const double stop = 1e-20 * std::max(rr, 1e-300);
    for (int it = 0; it < 2000 && rr > stop; ++it) {
        apply(p, ap);
        double pap = 0.0;
        for (std::size_t k = 0; k < n; ++k)
            pap += p[k] * ap[k];
        if (pap <= 0.0)
            break;
        const double alpha = rr / pap;
        double next = 0.0;
        for (std::size_t k = 0; k < n; ++k) {
            h[k] += alpha * p[k];
            r[k] -= alpha * ap[k];
            next += r[k] * r[k];
        }
        for (std::size_t k = 0; k < n; ++k)
            p[k] = r[k] + next / rr * p[k];
        rr = next;
    }
    // Level on average, so the layers bend both ways about where they were.
    double mean = 0.0;
    for (const double v : h)
        mean += v;
    mean /= double(n);
    f.grid.resize(n);
    for (std::size_t k = 0; k < n; ++k)
        f.grid[k] = float(h[k] - mean);
    f.full = std::max(lowest_slope - b.z0, f.flat_below + 1.0);
    f.scale = 1.0;
    return true;
}

} // namespace

std::optional<CurvedField> curved_field(const Settings& s, const IndexedMesh& mesh) {
    if (s.curved_mode == CurvedMode::Disabled || mesh.vertices.empty())
        return std::nullopt;
    const Box b = box_of(mesh);
    CurvedField f;
    f.mode = s.curved_mode;
    f.pattern = s.curved_pattern;
    f.cx = 0.5 * (b.x0 + b.x1), f.cy = 0.5 * (b.y0 + b.y1);
    f.base = b.z0;
    f.height = b.z1 - b.z0;
    // How far from the centre the nozzle goes: the bed's farthest corner (a
    // travel out to the skirt follows the cone too), or well past the part.
    f.radius = std::hypot(0.5 * (b.x1 - b.x0), 0.5 * (b.y1 - b.y0)) + 50.0;
    for (const auto& [bx, by] : s.bed)
        f.radius = std::max(f.radius, std::hypot(bx - f.cx, by - f.cy));
    f.angle = radians(s.curved_angle);
    f.twist = radians(s.curved_twist);
    f.flat_below = s.curved_flat_below;
    f.ramp_height = s.curved_ramp_height;
    f.flat_top = s.curved_flat_top;
    // The head's free slopes along X and Y (rule 3).
    const double tan_x = std::tan(radians(s.head_free_angle_x > 0.0 ? s.head_free_angle_x : s.curved_max_slope));
    const double tan_y = std::tan(radians(s.head_free_angle_y > 0.0 ? s.head_free_angle_y : s.curved_max_slope));
    const double tan_min = std::min(tan_x, tan_y);
    if (f.mode == CurvedMode::Wave) {
        if (s.curved_fit_head) {
            const double wavelength = std::clamp(std::numbers::pi * s.head_clearance_height / tan_min, 4.0, 100.0);
            f.k = 2.0 * std::numbers::pi / wavelength;
            // Ridges climb along their direction; the egg and the twisted
            // ridges along X and Y alike.
            double allowed = tan_min;
            if (f.pattern == CurvedPattern::Ridges) {
                const double c = std::cos(f.angle) / tan_x, sn = std::sin(f.angle) / tan_y;
                allowed = 1.0 / std::sqrt(c * c + sn * sn);
            }
            f.amplitude = std::min(allowed / f.k, 0.5 * s.head_clearance_height);
        } else {
            f.k = 2.0 * std::numbers::pi / s.curved_wavelength;
            f.amplitude = s.curved_amplitude;
        }
    } else if (f.mode == CurvedMode::Surface) {
        if (!fit_surface(f, mesh, b, tan_x, tan_y))
            return std::nullopt; // nothing slopes: flat layers (CO1)
    } else {
        f.cone = std::tan(radians(s.curved_cone_angle));
    }
    const auto fits = [&](const CurvedField& g) {
        const Bounds bb = bounds_of(g, b, tan_x, tan_y);
        return bb.j_min >= kMinJ - 1e-9 && bb.j_max <= kMaxJ + 1e-9 && bb.head <= 1.0 + 1e-9 &&
               clearance_use(bb, s) <= 1.0 + 1e-9;
    };
    if (fits(f))
        return f;
    if (!s.curved_fit_head && f.mode != CurvedMode::Surface) {
        const Bounds bb = bounds_of(f, b, tan_x, tan_y);
        char text[300];
        if (bb.j_min < kMinJ - 1e-9 || bb.j_max > kMaxJ + 1e-9)
            std::snprintf(text, sizeof text,
                          "curved.mode: the layers would be %.2f to %.2f times as thick as sliced (0.6 to 1.4 at most); "
                          "lower curved.amplitude or curved.cone_angle, or raise curved.ramp_height",
                          bb.j_min, bb.j_max);
        else if (bb.head > 1.0 + 1e-9)
            std::snprintf(text, sizeof text,
                          "curved.mode: the layers are steeper than the head lets the nozzle climb (head.free_angle_x, "
                          "head.free_angle_y or curved.max_slope)");
        else
            std::snprintf(text, sizeof text,
                          "curved.mode: the printed part would rise %.2f mm next to the nozzle, more than "
                          "head.clearance_height (%.2f mm)",
                          std::min(bb.d_max - bb.d_min, bb.steepest * s.head_clearance_radius), s.head_clearance_height);
        throw CurvedError(text);
    }
    // As large as fits, by halves (rule 3).
    double lo = 0.0, hi = 1.0;
    const CurvedField full = f;
    for (int i = 0; i < 40; ++i) {
        const double mid = 0.5 * (lo + hi);
        CurvedField g = full;
        g.amplitude *= mid, g.cone *= mid, g.scale *= mid;
        (fits(g) ? lo : hi) = mid;
    }
    f.amplitude = full.amplitude * lo, f.cone = full.cone * lo, f.scale = full.scale * lo;
    if (f.mode == CurvedMode::Surface && f.most() < 1e-3)
        return std::nullopt; // too low a part to bend (rule 17)
    return f;
}

IndexedMesh refined_mesh(const IndexedMesh& mesh, double longest, double flat_below, std::vector<std::uint32_t>* origin) {
    IndexedMesh m = mesh;
    std::vector<std::uint32_t> from(m.triangles.size());
    for (std::size_t t = 0; t < from.size(); ++t)
        from[t] = std::uint32_t(t);
    float floor_z = m.vertices.empty() ? 0.0f : m.vertices.front().z;
    for (const Vec3f& v : m.vertices)
        floor_z = std::min(floor_z, v.z);
    const double flat_top_z = double(floor_z) + flat_below;
    const double limit2 = longest * longest;
    for (int pass = 0; pass < 24; ++pass) {
        std::map<std::pair<std::uint32_t, std::uint32_t>, std::uint32_t> mid;
        const auto split = [&](std::uint32_t a, std::uint32_t b) -> bool {
            const Vec3f& p = m.vertices[a];
            const Vec3f& q = m.vertices[b];
            if (double(p.z) <= flat_top_z && double(q.z) <= flat_top_z)
                return false; // flat there: nothing moves
            const double dx = double(q.x) - p.x, dy = double(q.y) - p.y, dz = double(q.z) - p.z;
            return dx * dx + dy * dy + dz * dz > limit2;
        };
        const auto key = [](std::uint32_t a, std::uint32_t b) { return a < b ? std::pair{a, b} : std::pair{b, a}; };
        for (const auto& t : m.triangles)
            for (int e = 0; e < 3; ++e) {
                const std::uint32_t a = t[std::size_t(e)], b = t[std::size_t((e + 1) % 3)];
                if (split(a, b) && !mid.count(key(a, b))) {
                    const Vec3f& p = m.vertices[a];
                    const Vec3f& q = m.vertices[b];
                    mid[key(a, b)] = std::uint32_t(m.vertices.size());
                    m.vertices.push_back({0.5f * (p.x + q.x), 0.5f * (p.y + q.y), 0.5f * (p.z + q.z)});
                }
            }
        if (mid.empty())
            break;
        std::vector<std::array<std::uint32_t, 3>> next;
        std::vector<std::uint32_t> next_from;
        next.reserve(m.triangles.size() * 2);
        for (std::size_t i = 0; i < m.triangles.size(); ++i) {
            std::array<std::uint32_t, 3> t = m.triangles[i];
            const auto add = [&](std::uint32_t a, std::uint32_t b, std::uint32_t c) {
                next.push_back({a, b, c});
                next_from.push_back(from[i]);
            };
            std::array<int, 3> m3{};
            int count = 0;
            for (int e = 0; e < 3; ++e) {
                const auto it = mid.find(key(t[std::size_t(e)], t[std::size_t((e + 1) % 3)]));
                m3[std::size_t(e)] = it == mid.end() ? -1 : int(it->second);
                count += it != mid.end();
            }
            if (count == 0) {
                add(t[0], t[1], t[2]);
                continue;
            }
            if (count == 3) {
                const auto ab = std::uint32_t(m3[0]), bc = std::uint32_t(m3[1]), ca = std::uint32_t(m3[2]);
                add(t[0], ab, ca), add(ab, t[1], bc), add(ca, bc, t[2]), add(ab, bc, ca);
                continue;
            }
            // Turn the corners so that edge a-b is split (one) or c-a is the one left (two).
            for (int turn = 0; turn < 3; ++turn) {
                const bool ok = count == 1 ? m3[0] >= 0 : m3[2] < 0;
                if (ok)
                    break;
                std::rotate(t.begin(), t.begin() + 1, t.end());
                std::rotate(m3.begin(), m3.begin() + 1, m3.end());
            }
            if (count == 1) {
                const auto ab = std::uint32_t(m3[0]);
                add(t[0], ab, t[2]), add(ab, t[1], t[2]);
            } else {
                const auto ab = std::uint32_t(m3[0]), bc = std::uint32_t(m3[1]);
                add(ab, t[1], bc), add(t[0], ab, bc), add(t[0], bc, t[2]);
            }
        }
        m.triangles = std::move(next);
        from = std::move(next_from);
    }
    if (origin)
        *origin = std::move(from);
    return m;
}

void flatten_mesh(IndexedMesh& mesh, const CurvedField& field) {
    for (Vec3f& v : mesh.vertices)
        v.z = float(double(v.z) - field.offset(v.x, v.y, v.z));
}

namespace {

std::string number(double v, int decimals) {
    char text[48];
    if (std::abs(v) < 0.5 * std::pow(10.0, -decimals))
        v = 0.0;
    std::snprintf(text, sizeof text, "%.*f", decimals, v);
    return text;
}

} // namespace

std::string curve_gcode(std::string_view text, const CurvedField& field, const Settings& s) {
    std::string out;
    out.reserve(text.size() + text.size() / 2);
    const double z_offset = s.z_offset;
    const double piece = std::max(s.curved_segment_length, 0.05);
    const double z_most = s.max_feed_z > 0.0 ? s.max_feed_z * 60.0 : 0.0; // mm/min
    const char axis = dialect_of(s.flavor).e_axis; // E, or A on some (0: none)
    double x = 0.0, y = 0.0, flat = 0.0; // where the nozzle is, flat
    bool known_x = false, known_y = false, known_z = false;
    double feed = 0.0;      // the flat G-code's feed, mm/min
    double written = -1.0;  // the feed last written
    double z_out = 1e300;   // the Z last written
    std::size_t at = 0;
    while (at < text.size()) {
        std::size_t end = text.find('\n', at);
        if (end == std::string_view::npos)
            end = text.size();
        const std::string_view line = text.substr(at, end - at);
        at = end + 1;
        const bool move = line.size() > 2 && line[0] == 'G' && (line[1] == '1' || line[1] == '0') && line[2] == ' ';
        if (!move) {
            out.append(line).push_back('\n');
            continue;
        }
        // The words, and what follows them (a comment).
        const std::size_t semicolon = line.find(';');
        const std::string_view words = line.substr(3, semicolon == std::string_view::npos ? std::string_view::npos : semicolon - 3);
        const std::string_view note = semicolon == std::string_view::npos ? std::string_view() : line.substr(semicolon);
        std::optional<double> nx, ny, nz, ne, nf;
        bool other = false;
        std::istringstream in{std::string(words)};
        for (std::string w; in >> w;) {
            const double v = std::atof(w.c_str() + 1);
            switch (w[0]) {
            case 'X': nx = v; break;
            case 'Y': ny = v; break;
            case 'Z': nz = v; break;
            case 'F': nf = v; break;
            default:
                if (axis && w[0] == axis)
                    ne = v;
                else
                    other = true;
            }
        }
        if (nf)
            feed = *nf;
        const double x1 = nx.value_or(x), y1 = ny.value_or(y), flat1 = nz ? *nz - z_offset : flat;
        const bool was_known = known_x && known_y && known_z;
        known_x = known_x || nx.has_value(), known_y = known_y || ny.has_value(), known_z = known_z || nz.has_value();
        const bool flat_move = field.flat_at(flat) && field.flat_at(flat1);
        if (other || !was_known || !(known_x && known_y && known_z) || flat_move || (!nx && !ny && !nz)) {
            // As it is, the feed it runs at restored when a curved piece changed it.
            out.append(line.substr(0, semicolon == std::string_view::npos ? line.size() : semicolon));
            if (!nf && written >= 0.0 && std::lround(written) != std::lround(feed) && (nx || ny || nz || ne))
                out += " F" + std::to_string(std::lround(feed));
            if (nf || (nx || ny || nz || ne))
                written = feed;
            if (!note.empty())
                out.append(" ").append(note);
            out.push_back('\n');
            if (nz)
                z_out = *nz;
            x = x1, y = y1, flat = flat1;
            continue;
        }
        // On a curved layer: in pieces, each on the layer's surface; the
        // pieces that make a straight run, with about the same J, join again
        // (within gcode.resolution).
        const double length = std::hypot(x1 - x, y1 - y);
        const int n = std::max(1, int(std::ceil(length / piece - 1e-9)));
        const bool extrudes = ne && *ne > 0.0;
        struct Point {
            double x, y, z;
        };
        std::vector<Point> points{{x, y, field.real_z(x, y, flat)}};
        std::vector<double> js;
        for (int i = 1; i <= n; ++i) {
            const double t = double(i) / n;
            const double qx = x + (x1 - x) * t, qy = y + (y1 - y) * t;
            points.push_back({qx, qy, field.real_z(qx, qy, flat + (flat1 - flat) * t)});
            const Point& p = points[points.size() - 2];
            const Point& q = points.back();
            js.push_back(field.thickness(0.5 * (p.x + q.x), 0.5 * (p.y + q.y), 0.5 * (p.z + q.z)));
        }
        const double tolerance = std::max(s.gcode_resolution, 1e-4);
        const auto off_chord = [&](const Point& a, const Point& b, const Point& c) { // c from the segment a-b
            const double ux = b.x - a.x, uy = b.y - a.y, uz = b.z - a.z, vx = c.x - a.x, vy = c.y - a.y, vz = c.z - a.z;
            const double uu = ux * ux + uy * uy + uz * uz;
            const double w = uu > 0.0 ? std::clamp((ux * vx + uy * vy + uz * vz) / uu, 0.0, 1.0) : 0.0;
            return std::sqrt((vx - w * ux) * (vx - w * ux) + (vy - w * uy) * (vy - w * uy) + (vz - w * uz) * (vz - w * uz));
        };
        bool noted = false;
        for (std::size_t i = 0; i < js.size();) {
            // The longest straight run from point i.
            std::size_t j = i + 1;
            double j_lo = js[i], j_hi = js[i];
            while (j < js.size()) {
                const double lo = std::min(j_lo, js[j]), hi = std::max(j_hi, js[j]);
                if (hi > lo * 1.005)
                    break;
                bool straight = true;
                for (std::size_t m = i + 1; m <= j && straight; ++m)
                    straight = off_chord(points[i], points[j + 1], points[m]) <= tolerance;
                if (!straight)
                    break;
                j_lo = lo, j_hi = hi, ++j;
            }
            double share = 0.0; // of the flat move, by J
            for (std::size_t m = i; m < j; ++m)
                share += js[m] / n;
            const Point& p = points[i];
            const Point& q = points[j];
            const double jj = share * n / double(j - i); // the run's mean J
            double f = feed;
            // The same flow where the layer is thicker (rule 4, preserve);
            // where it is thinner the feed stays: no faster than planned.
            if (extrudes && s.curved_flow != CurvedFlow::Off)
                f /= std::max(jj, 1.0);
            const double dz = std::abs(q.z - p.z), dxy = std::hypot(q.x - p.x, q.y - p.y);
            if (z_most > 0.0 && dz > 1e-9)
                f = std::min(f, z_most * std::sqrt(dxy * dxy + dz * dz) / dz); // Z within its speed (rule 5)
            std::string g = "G1";
            if (nx || ny)
                g += " X" + number(q.x, 3) + " Y" + number(q.y, 3);
            const std::string z_text = number(q.z + z_offset, 3);
            if (z_text != number(z_out, 3)) {
                g += " Z" + z_text;
                z_out = std::atof(z_text.c_str());
            }
            if (ne)
                g += std::string(" ") + axis + number(extrudes ? *ne * share : *ne * double(j - i) / n, 5);
            // A new feed only when it changes by more than half a percent.
            if (written <= 0.0 || std::abs(f - written) > 0.005 * written) {
                g += " F" + std::to_string(std::max(1L, std::lround(f)));
                written = double(std::max(1L, std::lround(f)));
            }
            i = j;
            if (g == "G1")
                continue;
            out += g;
            if (!noted && !note.empty())
                out.append(" ").append(note);
            noted = true;
            out.push_back('\n');
        }
        x = x1, y = y1, flat = flat1;
    }
    return out;
}

} // namespace tisma
