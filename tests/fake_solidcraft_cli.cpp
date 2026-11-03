// A stand-in for the bundled solidcraft-cli (docs/spec/features/
// cad_design.md, rule 1), for the tests: it answers `--version`, `commands`
// and `run <script> [--save FILE] [--out FILE] [--quiet]` the way the real
// tool's documented command line does, builds the D1 primitives with the
// engine's own shapes, and writes the 3MF with the engine's writer. With
// TISMA_FAKE_SOLIDCRAFT_HANG set it never answers; with
// TISMA_FAKE_SOLIDCRAFT_BROKEN it fails at once (rule 22 bis); with
// TISMA_FAKE_SOLIDCRAFT_REFUSE=NAME it refuses to make the body NAME, as the
// real tool refuses an operation it cannot do (rule 8).

#include "io/model.hpp"
#include "mesh/mesh.hpp"
#include "mesh/shapes.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <numbers>
#include <cstdlib>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

using json = nlohmann::json;

namespace {

constexpr const char* kCommands[] = {"solid.box",       "solid.cylinder",  "solid.sphere",     "solid.cone",
                                     "solid.torus",     "solid.extrude",   "solid.revolve",    "sketch.create",
                                     "sketch.line",     "sketch.rectangle.two_point", "sketch.circle.center",
                                     "sketch.arc.center_point", "sketch.finish", "sketch.constraint.coincident",
                                     "sketch.constraint.midpoint", "sketch.dimension",
                                     "file.export",     "file.export_3mf", "file.save_as",     "inspect.measure",
                                     "engine.commands", "solid.fillet",    "solid.chamfer",    "solid.shell",
                                     "solid.hole",      "solid.mirror",    "solid.pattern.circular",
                                     "solid.pattern.rectangular", "solid.combine", "solid.thread"};

// The sketch being drawn: its plane and its closed loops as polygons (a chain
// closed, a rectangle, a circle as 64 sides), in sketch coordinates.
struct Loop {
    std::vector<std::array<double, 2>> points;
};
struct SketchState {
    std::string plane = "XY";
    std::vector<Loop> loops;
    std::vector<Loop> open; // open lines and arcs, joined into loops at the end
    int constraints = 0, dimensions = 0;
};

// Open pieces meeting end to end (within 1e-6 mm) closed into loops.
void join_open(SketchState& s) {
    const auto near = [](const std::array<double, 2>& a, const std::array<double, 2>& b) {
        return std::hypot(a[0] - b[0], a[1] - b[1]) <= 1e-6;
    };
    std::vector<bool> used(s.open.size(), false);
    for (std::size_t i = 0; i < s.open.size(); ++i) {
        if (used[i])
            continue;
        std::vector<std::size_t> taken{i};
        used[i] = true;
        Loop loop = s.open[i];
        for (;;) {
            if (loop.points.size() > 2 && near(loop.points.back(), loop.points.front())) {
                loop.points.pop_back();
                s.loops.push_back(std::move(loop));
                break;
            }
            bool found = false;
            for (std::size_t j = 0; j < s.open.size() && !found; ++j) {
                if (used[j])
                    continue;
                auto add = s.open[j].points;
                if (near(add.back(), loop.points.back()))
                    std::reverse(add.begin(), add.end());
                if (!near(add.front(), loop.points.back()))
                    continue;
                used[j] = found = true;
                taken.push_back(j);
                loop.points.insert(loop.points.end(), add.begin() + 1, add.end());
            }
            if (!found) {
                for (const std::size_t t : taken)
                    used[t] = t == i;
                break;
            }
        }
    }
}

// Sketch (x, y, z along the normal) to world, as SolidCraft lays the planes.
tisma::Vec3f to_world(const std::string& plane, double x, double y, double z) {
    if (plane == "XZ")
        return {float(x), float(z), float(-y)};
    if (plane == "YZ")
        return {float(z), float(x), float(y)};
    return {float(x), float(y), float(z)};
}

double loop_area(const Loop& l) {
    double a = 0.0;
    for (std::size_t i = 0; i < l.points.size(); ++i) {
        const auto& p = l.points[i];
        const auto& q = l.points[(i + 1) % l.points.size()];
        a += p[0] * q[1] - q[0] * p[1];
    }
    return a / 2.0;
}

// Ear clipping of a simple polygon (counter-clockwise): triangles as index triples.
std::vector<std::array<std::size_t, 3>> triangulate(const std::vector<std::array<double, 2>>& p) {
    std::vector<std::size_t> idx(p.size());
    for (std::size_t i = 0; i < p.size(); ++i)
        idx[i] = i;
    const auto cross = [&](std::size_t a, std::size_t b, std::size_t c) {
        return (p[b][0] - p[a][0]) * (p[c][1] - p[a][1]) - (p[b][1] - p[a][1]) * (p[c][0] - p[a][0]);
    };
    const auto inside = [&](std::size_t a, std::size_t b, std::size_t c, std::size_t q) {
        return cross(a, b, q) >= -1e-12 && cross(b, c, q) >= -1e-12 && cross(c, a, q) >= -1e-12;
    };
    std::vector<std::array<std::size_t, 3>> out;
    int guard = 0;
    while (idx.size() > 3 && guard++ < 100000) {
        bool clipped = false;
        for (std::size_t i = 0; i < idx.size(); ++i) {
            const std::size_t a = idx[(i + idx.size() - 1) % idx.size()], b = idx[i], c = idx[(i + 1) % idx.size()];
            if (cross(a, b, c) <= 1e-12)
                continue; // a reflex corner
            bool empty = true;
            for (const std::size_t q : idx)
                if (q != a && q != b && q != c && inside(a, b, c, q)) {
                    empty = false;
                    break;
                }
            if (!empty)
                continue;
            out.push_back({a, b, c});
            idx.erase(idx.begin() + std::ptrdiff_t(i));
            clipped = true;
            break;
        }
        if (!clipped)
            break; // not simple: what is left is dropped
    }
    if (idx.size() == 3)
        out.push_back({idx[0], idx[1], idx[2]});
    return out;
}

// A loop extruded `distance` along the plane's normal.
tisma::TriangleMesh extrude_loop(const std::string& plane, Loop loop, double distance) {
    if (loop_area(loop) < 0.0)
        std::reverse(loop.points.begin(), loop.points.end());
    tisma::TriangleMesh m;
    const auto at = [&](std::size_t i, double z) { return to_world(plane, loop.points[i][0], loop.points[i][1], z); };
    const double z0 = std::min(0.0, distance), z1 = std::max(0.0, distance);
    for (const auto& t : triangulate(loop.points)) {
        m.add_triangle(at(t[0], z0), at(t[2], z0), at(t[1], z0));
        m.add_triangle(at(t[0], z1), at(t[1], z1), at(t[2], z1));
    }
    for (std::size_t i = 0; i < loop.points.size(); ++i) {
        const std::size_t j = (i + 1) % loop.points.size();
        m.add_triangle(at(i, z0), at(j, z0), at(j, z1));
        m.add_triangle(at(i, z0), at(j, z1), at(i, z1));
    }
    return m;
}

// A loop revolved about the sketch's x or y axis by `angle` degrees (a full
// turn closes; less leaves it open, enough for a volume by the divergence
// theorem to be off, as the real tool would not be: the tests use 360).
tisma::TriangleMesh revolve_loop(const std::string& plane, Loop loop, char axis, double angle) {
    if (loop_area(loop) < 0.0)
        std::reverse(loop.points.begin(), loop.points.end());
    const int steps = 64;
    const double total = angle * std::numbers::pi / 180.0;
    const auto at = [&](std::size_t i, int s) {
        const double a = total * s / steps;
        const double u = loop.points[i][0], v = loop.points[i][1];
        // Turning about the y axis: x stays the radius, y the height; about x: y the radius.
        const double r = axis == 'x' ? v : u, h = axis == 'x' ? u : v;
        const double x = axis == 'x' ? h : r * std::cos(a), y = axis == 'x' ? r * std::cos(a) : h, z = r * std::sin(a);
        return to_world(plane, x, y, z);
    };
    tisma::TriangleMesh m;
    for (std::size_t i = 0; i < loop.points.size(); ++i) {
        const std::size_t j = (i + 1) % loop.points.size();
        for (int s = 0; s < steps; ++s) {
            m.add_triangle(at(i, s), at(j, s), at(j, s + 1));
            m.add_triangle(at(i, s), at(j, s + 1), at(i, s + 1));
        }
    }
    return m;
}

struct Body {
    std::string name;
    tisma::TriangleMesh mesh;
    // What made it, for a cut: an upright cylinder's radius and height.
    double cylinder_r = 0.0, cylinder_h = 0.0;
    // An axis-aligned box (a box, or a rectangle extruded), for the stage D4
    // operations the stand-in knows: its corners.
    bool box = false;
    std::array<double, 3> lo{}, hi{};
};

// The stage D4 operations, enough for the tests' volumes: what an operation
// takes away is added as a mesh turned inside out (its volume counts
// negative), what it adds as copies. The real tool makes true solids.
void append(tisma::TriangleMesh& to, const tisma::TriangleMesh& m, bool inside_out = false) {
    for (std::size_t t = 0; t < m.triangle_count(); ++t)
        if (inside_out)
            to.add_triangle(m.corner(t, 0), m.corner(t, 2), m.corner(t, 1));
        else
            to.add_triangle(m.corner(t, 0), m.corner(t, 1), m.corner(t, 2));
}
// A solid with outward faces, whatever order its triangles came in.
tisma::TriangleMesh outward(const tisma::TriangleMesh& m) {
    tisma::TriangleMesh out;
    append(out, m, m.signed_volume() < 0.0);
    return out;
}
// A prism: a polygon in the two axes other than `axis` (in their order: for
// z, x then y; for x, y then z; for y, z then x), from lo to hi along it.
tisma::TriangleMesh prism(const std::vector<std::array<double, 2>>& poly, int axis, double lo, double hi) {
    const auto at = [&](const std::array<double, 2>& q, double t) {
        double v[3];
        v[axis] = t;
        v[(axis + 1) % 3] = q[0];
        v[(axis + 2) % 3] = q[1];
        return tisma::Vec3f{float(v[0]), float(v[1]), float(v[2])};
    };
    std::vector<std::array<double, 2>> p = poly;
    double area = 0.0;
    for (std::size_t i = 0; i < p.size(); ++i)
        area += p[i][0] * p[(i + 1) % p.size()][1] - p[(i + 1) % p.size()][0] * p[i][1];
    if (area < 0.0)
        std::reverse(p.begin(), p.end());
    tisma::TriangleMesh m;
    for (std::size_t i = 1; i + 1 < p.size(); ++i) { // a fan: the polygons here are convex
        m.add_triangle(at(p[0], lo), at(p[i + 1], lo), at(p[i], lo));
        m.add_triangle(at(p[0], hi), at(p[i], hi), at(p[i + 1], hi));
    }
    for (std::size_t i = 0; i < p.size(); ++i) {
        const auto& a = p[i];
        const auto& b = p[(i + 1) % p.size()];
        m.add_triangle(at(a, lo), at(b, lo), at(b, hi));
        m.add_triangle(at(a, lo), at(b, hi), at(a, hi));
    }
    return outward(m);
}
// The box's edge through a point: the axis it runs along, and its corner in
// the other two (with the directions into the box); false when the point is
// on no edge.
bool box_edge(const Body& b, const std::array<double, 3>& p, int& axis, std::array<double, 2>& corner, std::array<double, 2>& into) {
    constexpr double eps = 1e-3;
    int on = 0, free_axis = -1;
    std::array<double, 3> c{}, d{};
    for (int a = 0; a < 3; ++a) {
        if (std::abs(p[std::size_t(a)] - b.lo[std::size_t(a)]) < eps)
            c[std::size_t(a)] = b.lo[std::size_t(a)], d[std::size_t(a)] = 1.0, ++on;
        else if (std::abs(p[std::size_t(a)] - b.hi[std::size_t(a)]) < eps)
            c[std::size_t(a)] = b.hi[std::size_t(a)], d[std::size_t(a)] = -1.0, ++on;
        else
            free_axis = a;
    }
    if (on != 2 || free_axis < 0)
        return false;
    axis = free_axis;
    corner = {c[std::size_t((axis + 1) % 3)], c[std::size_t((axis + 2) % 3)]};
    into = {d[std::size_t((axis + 1) % 3)], d[std::size_t((axis + 2) % 3)]};
    return true;
}
// ISO 273 clearance holes, fine, medium and coarse series (mm).
double clearance_hole(const std::string& thread, const std::string& fit) {
    static const std::map<std::string, std::array<double, 3>> table{
        {"M2", {2.2, 2.4, 2.6}},  {"M2.5", {2.7, 2.9, 3.1}}, {"M3", {3.2, 3.4, 3.6}}, {"M4", {4.3, 4.5, 4.8}},
        {"M5", {5.3, 5.5, 5.8}},  {"M6", {6.4, 6.6, 7.0}},   {"M8", {8.4, 9.0, 10.0}}, {"M10", {10.5, 11.0, 12.0}},
        {"M12", {13.0, 13.5, 14.5}}};
    const auto it = table.find(thread);
    if (it == table.end())
        return 0.0;
    return it->second[fit == "close" ? 0 : fit == "loose" ? 2 : 1];
}
// The tap drill of an ISO metric coarse thread (ISO 261 pitch): the
// nominal diameter less the pitch.
double tap_drill(const std::string& thread) {
    static const std::map<std::string, std::array<double, 2>> table{
        {"M2", {2, 0.4}}, {"M2.5", {2.5, 0.45}}, {"M3", {3, 0.5}},   {"M4", {4, 0.7}},   {"M5", {5, 0.8}},
        {"M6", {6, 1.0}}, {"M8", {8, 1.25}},     {"M10", {10, 1.5}}, {"M12", {12, 1.75}}, {"M16", {16, 2.0}}};
    const auto it = table.find(thread);
    return it == table.end() ? 0.0 : it->second[0] - it->second[1];
}
tisma::Vec3f turned(const tisma::Vec3f& v, char axis, double a) {
    const double c = std::cos(a), s = std::sin(a);
    if (axis == 'X')
        return {v.x, float(c * v.y - s * v.z), float(s * v.y + c * v.z)};
    if (axis == 'Y')
        return {float(c * v.x + s * v.z), v.y, float(-s * v.x + c * v.z)};
    return {float(c * v.x - s * v.y), float(s * v.x + c * v.y), v.z};
}

double number(const json& p, const char* key, const char* cmd, bool required = true, double fallback = 0.0) {
    if (!p.contains(key)) {
        if (required) {
            std::fprintf(stderr, "%s: `%s` is required\n", cmd, key);
            std::exit(1);
        }
        return fallback;
    }
    if (!p.at(key).is_number()) {
        std::fprintf(stderr, "%s: `%s` must be a number\n", cmd, key);
        std::exit(1);
    }
    return p.at(key).get<double>();
}

std::vector<double> point(const json& p, const char* key) {
    std::vector<double> v{0.0, 0.0, 0.0};
    if (p.contains(key) && p.at(key).is_array() && p.at(key).size() == 3)
        for (std::size_t i = 0; i < 3; ++i)
            v[i] = p.at(key)[i].get<double>();
    return v;
}

std::string text(const json& p, const char* key, const std::string& fallback) {
    return p.contains(key) && p.at(key).is_string() ? p.at(key).get<std::string>() : fallback;
}

void write_file(const std::string& path, const std::string& data) {
    std::ofstream out(path, std::ios::binary);
    out << data;
    if (!out) {
        std::fprintf(stderr, "%s: cannot write\n", path.c_str());
        std::exit(1);
    }
}

tisma::TriangleMesh merged(const std::vector<Body>& bodies) {
    tisma::TriangleMesh all;
    for (const Body& b : bodies)
        for (std::size_t t = 0; t < b.mesh.triangle_count(); ++t)
            all.add_triangle(b.mesh.corner(t, 0), b.mesh.corner(t, 1), b.mesh.corner(t, 2));
    return all;
}

int run(const std::vector<std::string>& args) {
    if (args.empty()) {
        std::fprintf(stderr, "usage: solidcraft-cli run <script.json> [--save FILE] [--out FILE] [--quiet]\n");
        return 2;
    }
    std::string save, script_path = args.front();
    std::vector<std::string> outs;
    bool quiet = false;
    for (std::size_t i = 1; i < args.size(); ++i) {
        if (args[i] == "--save" && i + 1 < args.size())
            save = args[++i];
        else if (args[i] == "--out" && i + 1 < args.size())
            outs.push_back(args[++i]);
        else if (args[i] == "--quiet")
            quiet = true;
        else {
            std::fprintf(stderr, "run doesn't take %s\n", args[i].c_str());
            return 2;
        }
    }
    std::ifstream in(script_path, std::ios::binary);
    if (!in) {
        std::fprintf(stderr, "%s: cannot read\n", script_path.c_str());
        return 1;
    }
    json script;
    try {
        script = json::parse(in);
    } catch (const json::exception& e) {
        std::fprintf(stderr, "%s: %s\n", script_path.c_str(), e.what());
        return 1;
    }
    const json& commands = script.is_array() ? script : script.value("commands", json::array());
    std::vector<Body> bodies;
    std::optional<SketchState> sketch, last_sketch;
    const auto point2 = [](const json& v) {
        if (!v.is_array() || v.size() != 2 || !v[0].is_number() || !v[1].is_number()) {
            std::fprintf(stderr, "a sketch point must be [x, y]\n");
            std::exit(1);
        }
        return std::array<double, 2>{v[0].get<double>(), v[1].get<double>()};
    };
    const auto in_sketch = [&](const std::string& cmd) {
        if (!sketch) {
            std::fprintf(stderr, "%s: no sketch is open\n", cmd.c_str());
            std::exit(1);
        }
    };
    const auto body_named = [&](const std::string& name) -> Body* {
        for (Body& b : bodies)
            if (b.name == name)
                return &b;
        return nullptr;
    };
    for (const json& step : commands) {
        const std::string cmd = step.value("command", std::string());
        const json p = step.value("params", json::object());
        const std::string name = text(p, "body_name", text(p, "name", "Body" + std::to_string(bodies.size() + 1)));
        const std::string operation = text(p, "operation", "new");
        if (const char* refuse = std::getenv("TISMA_FAKE_SOLIDCRAFT_REFUSE");
            refuse && *refuse && cmd.rfind("solid.", 0) == 0 && name == refuse) {
            std::fprintf(stderr, "%s: the body %s cannot be made\n", cmd.c_str(), name.c_str());
            return 1;
        }
        if (cmd == "solid.box") {
            const double l = number(p, "length", "solid.box"), w = number(p, "width", "solid.box"), h = number(p, "height", "solid.box");
            tisma::TriangleMesh m = tisma::make_box(l, w, h);
            const std::vector<double> c = point(p, "corner");
            m.translate(float(c[0] + l / 2.0), float(c[1] + w / 2.0), float(c[2]));
            Body b{name, std::move(m)};
            b.box = true;
            b.lo = {c[0], c[1], c[2]};
            b.hi = {c[0] + l, c[1] + w, c[2] + h};
            bodies.push_back(std::move(b));
        } else if (cmd == "solid.cylinder") {
            const double r = p.contains("diameter") ? number(p, "diameter", "solid.cylinder") / 2.0 : number(p, "radius", "solid.cylinder");
            const double h = number(p, "height", "solid.cylinder");
            const std::vector<double> base = point(p, "base");
            if (operation == "cut") {
                // The tube of D1: a hole down the axis of a cylinder listed in `targets`.
                if (!p.contains("targets") || !p.at("targets").is_array() || p.at("targets").empty()) {
                    std::fprintf(stderr, "solid.cylinder: a cut needs `targets`\n");
                    return 1;
                }
                Body* target = body_named(p.at("targets")[0].get<std::string>());
                if (!target || !(target->cylinder_r > 0.0) || r >= target->cylinder_r) {
                    std::fprintf(stderr, "solid.cylinder: the cut's target is not a wider cylinder\n");
                    return 1;
                }
                target->mesh = tisma::make_tube(target->cylinder_r, r, target->cylinder_h);
                continue;
            }
            tisma::TriangleMesh m = tisma::make_cylinder(r, h);
            m.translate(float(base[0]), float(base[1]), float(base[2]));
            bodies.push_back({name, std::move(m), r, h});
        } else if (cmd == "solid.sphere") {
            const double r = p.contains("diameter") ? number(p, "diameter", "solid.sphere") / 2.0 : number(p, "radius", "solid.sphere");
            tisma::TriangleMesh m = tisma::make_sphere(r);
            const std::vector<double> c = point(p, "center");
            m.translate(float(c[0]), float(c[1]), float(c[2] - r));
            bodies.push_back({name, std::move(m)});
        } else if (cmd == "solid.cone") {
            const double r = number(p, "radius", "solid.cone"), top = number(p, "top_radius", "solid.cone", false, 0.0),
                         h = number(p, "height", "solid.cone");
            tisma::TriangleMesh m = tisma::make_cone(r, top, h);
            const std::vector<double> base = point(p, "base");
            m.translate(float(base[0]), float(base[1]), float(base[2]));
            bodies.push_back({name, std::move(m)});
        } else if (cmd == "solid.torus") {
            const double major = number(p, "major", "solid.torus"), minor = number(p, "minor", "solid.torus");
            tisma::TriangleMesh m = tisma::make_torus(major, minor);
            const std::vector<double> c = point(p, "center");
            m.translate(float(c[0]), float(c[1]), float(c[2] - minor));
            bodies.push_back({name, std::move(m)});
        } else if (cmd == "sketch.create") {
            sketch = SketchState{};
            if (p.contains("plane") && p.at("plane").is_string())
                sketch->plane = p.at("plane").get<std::string>();
            if (sketch->plane != "XY" && sketch->plane != "XZ" && sketch->plane != "YZ") {
                std::fprintf(stderr, "sketch.create: `plane` must be XY, XZ or YZ\n");
                return 1;
            }
        } else if (cmd == "sketch.line") {
            in_sketch(cmd);
            Loop loop;
            if (!p.contains("points") || !p.at("points").is_array() || p.at("points").size() < 2) {
                std::fprintf(stderr, "sketch.line: `points` must list two points or more\n");
                return 1;
            }
            for (const json& v : p.at("points"))
                loop.points.push_back(point2(v));
            if (p.value("construction", false))
                ;
            else if (p.value("closed", false) && loop.points.size() >= 3)
                sketch->loops.push_back(std::move(loop));
            else
                sketch->open.push_back(std::move(loop));
        } else if (cmd == "sketch.rectangle.two_point") {
            in_sketch(cmd);
            if (!p.contains("p0") || !p.contains("p1")) {
                std::fprintf(stderr, "sketch.rectangle.two_point: `p0` and `p1` are required\n");
                return 1;
            }
            const auto a = point2(p.at("p0")), b = point2(p.at("p1"));
            if (!p.value("construction", false))
                sketch->loops.push_back({{{a[0], a[1]}, {b[0], a[1]}, {b[0], b[1]}, {a[0], b[1]}}});
        } else if (cmd == "sketch.circle.center") {
            in_sketch(cmd);
            const auto c = point2(p.value("center", json::array({0.0, 0.0})));
            const double r = p.contains("diameter") ? number(p, "diameter", "sketch.circle.center") / 2.0 : number(p, "radius", "sketch.circle.center");
            Loop loop;
            for (int i = 0; i < 64; ++i) {
                const double a = 2.0 * std::numbers::pi * i / 64;
                loop.points.push_back({c[0] + r * std::cos(a), c[1] + r * std::sin(a)});
            }
            if (!p.value("construction", false))
                sketch->loops.push_back(std::move(loop));
        } else if (cmd == "sketch.arc.center_point") {
            in_sketch(cmd); // it closes a region with the pieces it meets
            const auto c = point2(p.value("center", json::array({0.0, 0.0})));
            const auto a = point2(p.value("start", json::array({0.0, 0.0})));
            const auto b = point2(p.value("end", json::array({0.0, 0.0})));
            const double r = std::hypot(a[0] - c[0], a[1] - c[1]);
            const double a0 = std::atan2(a[1] - c[1], a[0] - c[0]);
            double sweep = std::atan2(b[1] - c[1], b[0] - c[0]) - a0;
            if (p.value("clockwise", false))
                while (sweep >= 0.0)
                    sweep -= 2.0 * std::numbers::pi;
            else
                while (sweep <= 0.0)
                    sweep += 2.0 * std::numbers::pi;
            Loop arc;
            const int n = std::max(4, int(std::ceil(std::abs(sweep) / (2.0 * std::numbers::pi) * 96.0)));
            for (int k = 0; k <= n; ++k)
                arc.points.push_back({c[0] + r * std::cos(a0 + sweep * k / n), c[1] + r * std::sin(a0 + sweep * k / n)});
            arc.points.front() = a;
            arc.points.back() = b;
            if (!p.value("construction", false) && r > 0.0)
                sketch->open.push_back(std::move(arc));
        } else if (cmd == "sketch.constraint.coincident" || cmd == "sketch.constraint.midpoint") {
            in_sketch(cmd);
            ++sketch->constraints;
        } else if (cmd == "sketch.dimension") {
            in_sketch(cmd);
            if (!p.contains("entities") || !p.at("entities").is_array() || p.at("entities").empty()) {
                std::fprintf(stderr, "sketch.dimension: `entities` is required\n");
                return 1;
            }
            ++sketch->dimensions;
        } else if (cmd == "sketch.finish") {
            in_sketch(cmd);
            join_open(*sketch);
            last_sketch = std::move(sketch);
            sketch.reset();
        } else if (cmd == "solid.extrude" || cmd == "solid.revolve") {
            if (!last_sketch || last_sketch->loops.empty()) {
                std::fprintf(stderr, "%s: no profile: the last sketch has no closed region\n", cmd.c_str());
                return 1;
            }
            tisma::TriangleMesh all;
            for (const Loop& loop : last_sketch->loops) {
                tisma::TriangleMesh m;
                if (cmd == "solid.extrude") {
                    m = extrude_loop(last_sketch->plane, loop, number(p, "distance", "solid.extrude"));
                } else {
                    const std::string axis = text(p, "axis", "y");
                    if (axis != "x" && axis != "y") {
                        std::fprintf(stderr, "solid.revolve: `axis` must be x or y here\n");
                        return 1;
                    }
                    m = revolve_loop(last_sketch->plane, loop, axis[0], number(p, "angle", "solid.revolve", false, 360.0));
                }
                for (std::size_t t = 0; t < m.triangle_count(); ++t)
                    all.add_triangle(m.corner(t, 0), m.corner(t, 1), m.corner(t, 2));
            }
            Body b{name, std::move(all)};
            // A rectangle extruded on XY is a box the D4 operations know.
            if (cmd == "solid.extrude" && last_sketch->plane == "XY" && last_sketch->loops.size() == 1 &&
                last_sketch->loops[0].points.size() == 4) {
                const auto& q = last_sketch->loops[0].points;
                const bool aligned = (q[0][0] == q[3][0] && q[1][0] == q[2][0] && q[0][1] == q[1][1] && q[2][1] == q[3][1]) ||
                                     (q[0][1] == q[3][1] && q[1][1] == q[2][1] && q[0][0] == q[1][0] && q[2][0] == q[3][0]);
                if (aligned) {
                    const tisma::BoundingBox bb = b.mesh.bounding_box();
                    b.box = true;
                    b.lo = {bb.min.x, bb.min.y, bb.min.z};
                    b.hi = {bb.max.x, bb.max.y, bb.max.z};
                }
            }
            bodies.push_back(std::move(b));
        } else if (cmd == "solid.fillet" || cmd == "solid.chamfer") {
            Body* b = body_named(text(p, "body", ""));
            if (!b || !b->box) {
                std::fprintf(stderr, "%s: the stand-in rounds only the edges of a box body\n", cmd.c_str());
                return 1;
            }
            const bool fillet = cmd == "solid.fillet";
            const double r = number(p, fillet ? "radius" : "distance", cmd.c_str());
            for (const json& e : p.value("edges", json::array())) {
                const std::vector<double> at = e.get<std::vector<double>>();
                int axis = 0;
                std::array<double, 2> c{}, d{};
                if (at.size() != 3 || !box_edge(*b, {at[0], at[1], at[2]}, axis, c, d)) {
                    std::fprintf(stderr, "%s: no edge at the point given\n", cmd.c_str());
                    return 1;
                }
                // What goes: the corner of the section beyond the round (or the cut).
                std::vector<std::array<double, 2>> region{{c[0], c[1]}};
                if (fillet) {
                    const double cx = c[0] + d[0] * r, cy = c[1] + d[1] * r;
                    for (int k = 0; k <= 16; ++k) {
                        const double t = std::numbers::pi / 2.0 * k / 16.0;
                        region.push_back({cx - d[0] * r * std::sin(t), cy - d[1] * r * std::cos(t)});
                    }
                } else {
                    region.push_back({c[0] + d[0] * r, c[1]});
                    region.push_back({c[0], c[1] + d[1] * r});
                }
                append(b->mesh, prism(region, axis, b->lo[std::size_t(axis)], b->hi[std::size_t(axis)]), true);
            }
        } else if (cmd == "solid.shell") {
            Body* b = body_named(text(p, "body", ""));
            if (!b || !b->box) {
                std::fprintf(stderr, "solid.shell: the stand-in hollows only a box body\n");
                return 1;
            }
            const double t = number(p, "thickness", "solid.shell");
            std::array<double, 3> lo = b->lo, hi = b->hi;
            for (int a = 0; a < 3; ++a)
                lo[std::size_t(a)] += t, hi[std::size_t(a)] -= t;
            if (p.contains("faces") && p.at("faces").is_array())
                for (const json& f : p.at("faces")) {
                    const std::vector<double> at = f.get<std::vector<double>>();
                    for (int a = 0; a < 3; ++a) { // the open face: the hollow reaches it
                        if (std::abs(at[std::size_t(a)] - b->lo[std::size_t(a)]) < 1e-3)
                            lo[std::size_t(a)] = b->lo[std::size_t(a)];
                        if (std::abs(at[std::size_t(a)] - b->hi[std::size_t(a)]) < 1e-3)
                            hi[std::size_t(a)] = b->hi[std::size_t(a)];
                    }
                }
            if (!(hi[0] > lo[0] && hi[1] > lo[1] && hi[2] > lo[2])) {
                std::fprintf(stderr, "solid.shell: the wall is thicker than the body\n");
                return 1;
            }
            append(b->mesh, prism({{lo[0], lo[1]}, {hi[0], lo[1]}, {hi[0], hi[1]}, {lo[0], hi[1]}}, 2, lo[2], hi[2]), true);
        } else if (cmd == "solid.hole") {
            const json targets = p.value("targets", json::array());
            Body* b = targets.empty() ? nullptr : body_named(targets[0].get<std::string>());
            if (!b) {
                std::fprintf(stderr, "solid.hole: `targets` names no body\n");
                return 1;
            }
            const double d = p.contains("thread")      ? tap_drill(text(p, "thread", ""))
                             : p.contains("clearance") ? clearance_hole(text(p, "clearance", ""), text(p, "fit", "normal"))
                                                       : number(p, "diameter", "solid.hole", false, 0.0);
            if (!(d > 0.0)) {
                std::fprintf(stderr, "solid.hole: no diameter (or an unknown clearance)\n");
                return 1;
            }
            const std::vector<double> at = point(p, "position");
            const tisma::BoundingBox bb = b->mesh.bounding_box();
            const double lo[3] = {bb.min.x, bb.min.y, bb.min.z}, hi[3] = {bb.max.x, bb.max.y, bb.max.z};
            int axis = -1;
            for (int a = 0; a < 3 && axis < 0; ++a)
                if (std::abs(at[std::size_t(a)] - lo[a]) < 1e-3 || std::abs(at[std::size_t(a)] - hi[a]) < 1e-3)
                    axis = a;
            if (axis < 0) {
                std::fprintf(stderr, "solid.hole: the position is on no outer face the stand-in knows\n");
                return 1;
            }
            const double depth = p.contains("depth") ? number(p, "depth", "solid.hole") : hi[axis] - lo[axis];
            const bool from_top = std::abs(at[std::size_t(axis)] - hi[axis]) < 1e-3;
            const double t0 = from_top ? at[std::size_t(axis)] - depth : at[std::size_t(axis)], t1 = t0 + depth;
            const double u = at[std::size_t((axis + 1) % 3)], v = at[std::size_t((axis + 2) % 3)];
            std::vector<std::array<double, 2>> circle;
            for (int k = 0; k < 64; ++k) {
                const double a = 2.0 * std::numbers::pi * k / 64.0;
                circle.push_back({u + d / 2.0 * std::cos(a), v + d / 2.0 * std::sin(a)});
            }
            append(b->mesh, prism(circle, axis, t0, t1), true);
        } else if (cmd == "solid.mirror") {
            const json list = p.value("bodies", json::array());
            Body* b = list.empty() ? nullptr : body_named(list[0].get<std::string>());
            const std::string plane = text(p, "plane", "");
            if (!b || (plane != "XY" && plane != "XZ" && plane != "YZ")) {
                std::fprintf(stderr, "solid.mirror: needs `bodies` and a `plane` (XY, XZ, YZ)\n");
                return 1;
            }
            const int flip = plane == "YZ" ? 0 : plane == "XZ" ? 1 : 2;
            tisma::TriangleMesh copy;
            for (std::size_t t = 0; t < b->mesh.triangle_count(); ++t) {
                tisma::Vec3f c[3] = {b->mesh.corner(t, 0), b->mesh.corner(t, 1), b->mesh.corner(t, 2)};
                for (tisma::Vec3f& q : c)
                    (&q.x)[flip] = -(&q.x)[flip];
                copy.add_triangle(c[0], c[2], c[1]); // a mirror turns the faces around
            }
            append(b->mesh, copy);
            b->box = false;
        } else if (cmd == "solid.pattern.circular" || cmd == "solid.pattern.rectangular") {
            const json list = p.value("bodies", json::array());
            Body* b = list.empty() ? nullptr : body_named(list[0].get<std::string>());
            const bool circular = cmd == "solid.pattern.circular";
            const int count = int(number(p, circular ? "count" : "count1", cmd.c_str()));
            if (!b || count < 2) {
                std::fprintf(stderr, "%s: needs `bodies` and two copies or more\n", cmd.c_str());
                return 1;
            }
            const tisma::TriangleMesh one = b->mesh;
            if (circular) {
                const std::string axis = text(p, "axis", "Z");
                const double angle = number(p, "angle", cmd.c_str(), false, 360.0) * std::numbers::pi / 180.0;
                const double turn_step = std::abs(angle - 2.0 * std::numbers::pi) < 1e-9 ? angle / count : angle / (count - 1);
                for (int k = 1; k < count; ++k) {
                    tisma::TriangleMesh copy;
                    for (std::size_t t = 0; t < one.triangle_count(); ++t)
                        copy.add_triangle(turned(one.corner(t, 0), axis[0], k * turn_step), turned(one.corner(t, 1), axis[0], k * turn_step),
                                          turned(one.corner(t, 2), axis[0], k * turn_step));
                    append(b->mesh, copy);
                }
            } else {
                const std::vector<double> dir = point(p, "dir1");
                const double spacing = number(p, "spacing1", cmd.c_str());
                const double len = std::hypot(dir[0], dir[1], dir[2]);
                for (int k = 1; k < count; ++k) {
                    tisma::TriangleMesh copy = one;
                    copy.translate(float(dir[0] / len * spacing * k), float(dir[1] / len * spacing * k), float(dir[2] / len * spacing * k));
                    append(b->mesh, copy);
                }
            }
            b->box = false;
        } else if (cmd == "solid.thread") {
            // A thread is drawn on the face (cosmetic) or cut into it; the
            // stand-in only checks what it is given and leaves the body as is.
            point(p, "face");
            if (tap_drill(text(p, "designation", "")) <= 0.0) {
                std::fprintf(stderr, "solid.thread: unknown `designation`\n");
                return 1;
            }
            const std::string hand = text(p, "hand", "right");
            if (hand != "right" && hand != "left" && hand != "both") {
                std::fprintf(stderr, "solid.thread: `hand` must be right, left or both\n");
                return 1;
            }
        } else if (cmd == "solid.combine") {
            Body* target = body_named(text(p, "target", ""));
            const std::string op = text(p, "operation", "join");
            if (!target || op == "intersect") {
                std::fprintf(stderr, "solid.combine: needs a `target` body (intersect is not in the stand-in)\n");
                return 1;
            }
            for (const json& t : p.value("tools", json::array())) {
                const std::string tool_name = t.get<std::string>();
                Body* tool = body_named(tool_name);
                if (!tool || tool == target) {
                    std::fprintf(stderr, "solid.combine: no tool body %s\n", tool_name.c_str());
                    return 1;
                }
                append(target->mesh, tool->mesh, op == "cut");
                bodies.erase(bodies.begin() + (tool - bodies.data()));
                target = body_named(text(p, "target", ""));
            }
            target->box = false;
        } else if (cmd == "file.export_3mf" || cmd == "file.export") {
            const std::string path = text(p, "path", "");
            if (path.empty()) {
                std::fprintf(stderr, "%s: `path` is required\n", cmd.c_str());
                return 1;
            }
            const std::string quality = text(p, "quality", "standard");
            if (quality != "standard" && quality != "fine" && quality != "finest") {
                std::fprintf(stderr, "%s: `quality` must be standard, fine or finest, not %s\n", cmd.c_str(), quality.c_str());
                return 1;
            }
            write_file(path, tisma::write_3mf(merged(bodies), bodies.size() == 1 ? bodies.front().name : "Bodies"));
        } else {
            std::fprintf(stderr, "%s: unknown command\n", cmd.c_str());
            return 1;
        }
    }
    for (const std::string& out : outs)
        write_file(out, tisma::write_3mf(merged(bodies), "Bodies"));
    if (!save.empty()) {
        std::ostringstream text_of;
        text_of << script.dump();
        write_file(save, "fake-solidcraft-design\n" + text_of.str());
    }
    if (!quiet) {
        json measured = json::array();
        double total = 0.0;
        for (const Body& b : bodies) {
            const double v = b.mesh.signed_volume();
            total += v;
            measured.push_back({{"name", b.name}, {"volume_mm3", v}});
        }
        std::printf("%s\n", json{{"body_count", bodies.size()}, {"bodies", measured}, {"total", {{"volume_mm3", total}}}}.dump(2).c_str());
    }
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    if (std::getenv("TISMA_FAKE_SOLIDCRAFT_HANG"))
        for (;;)
            std::this_thread::sleep_for(std::chrono::seconds(1));
    if (std::getenv("TISMA_FAKE_SOLIDCRAFT_BROKEN")) {
        std::fprintf(stderr, "the component is damaged\n");
        return 3;
    }
    std::vector<std::string> args(argv + 1, argv + argc);
    if (args.empty()) {
        std::fprintf(stderr, "usage: solidcraft-cli --version | commands | run <script.json> [--save FILE] [--out FILE] [--quiet]\n");
        return 2;
    }
    if (args[0] == "--version" || args[0] == "-V") {
        std::printf("solidcraft-cli 0.30.0 (tisma stand-in)\n");
        return 0;
    }
    if (args[0] == "commands") {
        json list = json::array();
        for (const char* id : kCommands)
            list.push_back({{"id", id}, {"label", id}, {"params", ""}});
        std::printf("%s\n", list.dump(2).c_str());
        return 0;
    }
    if (args[0] == "run")
        return run(std::vector<std::string>(args.begin() + 1, args.end()));
    std::fprintf(stderr, "%s: not a command\n", args[0].c_str());
    return 2;
}
