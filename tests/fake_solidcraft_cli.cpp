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
                                     "engine.commands"};

// The sketch being drawn: its plane and its closed loops as polygons (a chain
// closed, a rectangle, a circle as 64 sides), in sketch coordinates.
struct Loop {
    std::vector<std::array<double, 2>> points;
};
struct SketchState {
    std::string plane = "XY";
    std::vector<Loop> loops;
    int constraints = 0, dimensions = 0;
};

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
};

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
            bodies.push_back({name, std::move(m)});
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
            if (p.value("closed", false) && loop.points.size() >= 3 && !p.value("construction", false))
                sketch->loops.push_back(std::move(loop));
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
            in_sketch(cmd); // drawn, but it closes no region of its own here
            point2(p.value("center", json::array({0.0, 0.0})));
            point2(p.value("start", json::array({0.0, 0.0})));
            point2(p.value("end", json::array({0.0, 0.0})));
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
            bodies.push_back({name, std::move(all)});
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
