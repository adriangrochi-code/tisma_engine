#include "cli/part_cli.hpp"

#include "cad/fasteners.hpp"
#include "cad/gears.hpp"
#include "cli/cli.hpp"
#include "io/stl.hpp"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace tisma {

namespace {

constexpr std::string_view kPartUsage =
    "usage:\n"
    "  tisma-engine part gear|fastener <spec.json> -o <out.stl>\n"
    "      the part from its spec (the same JSON the Design tab keeps); its sizes as JSON on the output\n"
    "  tisma-engine part gear|fastener --defaults\n"
    "      a default spec to start from\n";

int usage(std::ostream& err, const std::string& problem) {
    err << "error: " << problem << "\n" << kPartUsage;
    return kExitUsage;
}

} // namespace

int cmd_part(std::span<const std::string_view> args, std::ostream& out, std::ostream& err) {
    if (args.empty())
        return usage(err, "part needs gear or fastener");
    const std::string kind(args[0]);
    if (kind != "gear" && kind != "fastener")
        return usage(err, "unknown part '" + kind + "'");
    std::string spec_path, output;
    bool defaults = false;
    for (std::size_t i = 1; i < args.size(); ++i) {
        const std::string_view a = args[i];
        if (a == "--defaults")
            defaults = true;
        else if ((a == "-o" || a == "--output") && i + 1 < args.size())
            output = std::string(args[++i]);
        else if (!a.starts_with("-") && spec_path.empty())
            spec_path = std::string(a);
        else
            return usage(err, "part: unexpected argument '" + std::string(a) + "'");
    }
    if (defaults) {
        out << (kind == "gear" ? cad::gear_json(cad::GearSpec{}) : cad::fastener_json(cad::FastenerSpec{})) << "\n";
        return kExitOk;
    }
    if (spec_path.empty() || output.empty())
        return usage(err, "part needs the spec and -o");
    std::ifstream in(std::filesystem::path(spec_path), std::ios::binary);
    if (!in) {
        err << "error: cannot read " << spec_path << "\n";
        return kExitInput;
    }
    std::ostringstream text;
    text << in.rdbuf();
    TriangleMesh mesh;
    nlohmann::json sizes;
    try {
        if (kind == "gear") {
            const cad::GearSpec spec = cad::parse_gear(text.str());
            if (const std::string why = cad::check_gear(spec); !why.empty()) {
                err << "error: " << why << "\n";
                return kExitInput;
            }
            mesh = cad::make_gear(spec);
            const cad::GearDims d = cad::gear_dims(spec);
            sizes = {{"pitch_diameter", d.pitch_diameter}, {"outside_diameter", d.outside_diameter},
                     {"root_diameter", d.root_diameter},   {"base_diameter", d.base_diameter},
                     {"circular_pitch", d.circular_pitch}, {"tooth_thickness", d.tooth_thickness}};
        } else {
            const cad::FastenerSpec spec = cad::parse_fastener(text.str());
            if (const std::string why = cad::check_fastener(spec); !why.empty()) {
                err << "error: " << why << "\n";
                return kExitInput;
            }
            mesh = cad::make_fastener(spec);
        }
    } catch (const std::exception& e) {
        err << "error: " << spec_path << ": " << e.what() << "\n";
        return kExitInput;
    }
    const std::string stl = stl_binary(mesh, "tisma-engine part " + kind);
    std::ofstream f(std::filesystem::path(output), std::ios::binary | std::ios::trunc);
    f.write(stl.data(), std::streamsize(stl.size()));
    if (!f) {
        err << "error: cannot write " << output << "\n";
        return kExitInput;
    }
    sizes["triangles"] = mesh.triangle_count();
    out << sizes.dump(1) << "\n";
    return kExitOk;
}

} // namespace tisma
