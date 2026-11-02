// A stand-in for the bundled solidcraft-cli (docs/spec/features/
// cad_design.md, rule 1), for the tests: it answers `--version`, `commands`
// and `run <script> [--save FILE] [--out FILE] [--quiet]` the way the real
// tool's documented command line does, builds the D1 primitives with the
// engine's own shapes, and writes the 3MF with the engine's writer. With
// TISMA_FAKE_SOLIDCRAFT_HANG set it never answers; with
// TISMA_FAKE_SOLIDCRAFT_BROKEN it fails at once (rule 22 bis).

#include "io/model.hpp"
#include "mesh/mesh.hpp"
#include "mesh/shapes.hpp"

#include <nlohmann/json.hpp>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

using json = nlohmann::json;

namespace {

constexpr const char* kCommands[] = {"solid.box",      "solid.cylinder", "solid.sphere",   "solid.cone",
                                     "solid.torus",    "solid.extrude",  "sketch.create",  "file.export",
                                     "file.export_3mf", "file.save_as",  "inspect.measure", "engine.commands"};

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
