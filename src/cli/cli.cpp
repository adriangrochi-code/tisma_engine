#include "cli/cli.hpp"

#include "io/stl.hpp"
#include "mesh/indexed_mesh.hpp"

#include <chrono>
#include <filesystem>
#include <optional>
#include <ostream>
#include <string>

namespace tisma {
namespace {

constexpr std::string_view kUsage =
    "usage:\n"
    "  tisma-engine slice <model.stl> --config <settings.json> -o <out.gcode>\n"
    "  tisma-engine info <model.stl>\n"
    "  tisma-engine --help | --version\n";

struct SliceArgs {
    std::string model;
    std::string config;
    std::string output;
};

// Returns the arguments of 'slice', or writes the problem to err.
std::optional<SliceArgs> parse_slice_args(std::span<const std::string_view> args, std::ostream& err) {
    SliceArgs out;
    for (std::size_t i = 0; i < args.size(); ++i) {
        const std::string_view a = args[i];
        auto value = [&](std::string& dst) {
            if (i + 1 >= args.size()) {
                err << "error: " << a << " needs a value\n";
                return false;
            }
            dst = std::string(args[++i]);
            return true;
        };
        if (a == "--config" || a == "-c") {
            if (!value(out.config))
                return std::nullopt;
        } else if (a == "-o" || a == "--output") {
            if (!value(out.output))
                return std::nullopt;
        } else if (!a.empty() && a.front() == '-') {
            err << "error: unknown option " << a << "\n";
            return std::nullopt;
        } else if (out.model.empty()) {
            out.model = std::string(a);
        } else {
            err << "error: more than one model given\n";
            return std::nullopt;
        }
    }
    if (out.model.empty() || out.config.empty() || out.output.empty()) {
        err << "error: slice needs a model, --config and -o\n";
        return std::nullopt;
    }
    return out;
}

void print_mesh_summary(const StlFile& stl, double seconds, std::ostream& out) {
    const BoundingBox box = stl.mesh.bounding_box();
    out << "format: " << (stl.format == StlFormat::Binary ? "binary" : "ascii") << "\n";
    out << "triangles: " << stl.mesh.triangle_count() << "\n";
    if (!box.empty) {
        out << "size: " << (box.max.x - box.min.x) << " x " << (box.max.y - box.min.y) << " x "
            << (box.max.z - box.min.z) << "\n";
        out << "min: " << box.min.x << " " << box.min.y << " " << box.min.z << "\n";
        out << "max: " << box.max.x << " " << box.max.y << " " << box.max.z << "\n";
    }
    out << "volume: " << stl.mesh.signed_volume() << "\n";
    out << "read_ms: " << seconds * 1000.0 << "\n";
}

int read_model(const std::string& path, StlFile& stl, double& seconds, std::ostream& err) {
    const auto start = std::chrono::steady_clock::now();
    try {
        stl = read_stl(path);
    } catch (const StlError& e) {
        err << "error: " << path << ": " << e.what() << "\n";
        return kExitInput;
    }
    seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    return kExitOk;
}

int cmd_info(std::span<const std::string_view> args, std::ostream& out, std::ostream& err) {
    if (args.size() != 1) {
        err << "error: info needs exactly one model\n" << kUsage;
        return kExitUsage;
    }
    StlFile stl;
    double seconds = 0.0;
    if (int rc = read_model(std::string(args[0]), stl, seconds, err); rc != kExitOk)
        return rc;
    print_mesh_summary(stl, seconds, out);
    const auto start = std::chrono::steady_clock::now();
    const IndexedMesh indexed = weld_exact(stl.mesh);
    const double weld_ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    out << "vertices: " << indexed.vertices.size() << "\n";
    out << "weld_ms: " << weld_ms << "\n";
    return kExitOk;
}

int cmd_slice(std::span<const std::string_view> args, std::ostream& out, std::ostream& err) {
    const std::optional<SliceArgs> parsed = parse_slice_args(args, err);
    if (!parsed) {
        err << kUsage;
        return kExitUsage;
    }
    std::error_code ec;
    if (!std::filesystem::is_regular_file(parsed->config, ec)) {
        err << "error: config file not found: " << parsed->config << "\n";
        return kExitInput;
    }
    StlFile stl;
    double seconds = 0.0;
    if (int rc = read_model(parsed->model, stl, seconds, err); rc != kExitOk)
        return rc;
    out << "read " << stl.mesh.triangle_count() << " triangles in " << seconds * 1000.0 << " ms\n";
    err << "slice: not implemented yet\n";
    return kExitNotImplemented;
}

} // namespace

int run_cli(std::span<const std::string_view> args, std::ostream& out, std::ostream& err) {
    if (args.empty()) {
        err << kUsage;
        return kExitUsage;
    }
    const std::string_view command = args[0];
    const auto rest = args.subspan(1);
    if (command == "--help" || command == "-h" || command == "help") {
        out << kUsage;
        return kExitOk;
    }
    if (command == "--version") {
        out << "tisma-engine " << TISMA_VERSION << "\n";
        return kExitOk;
    }
    if (command == "slice")
        return cmd_slice(rest, out, err);
    if (command == "info")
        return cmd_info(rest, out, err);
    err << "error: unknown command '" << command << "'\n" << kUsage;
    return kExitUsage;
}

} // namespace tisma
