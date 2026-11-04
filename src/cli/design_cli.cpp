#include "cli/design_cli.hpp"

#include "cad/design.hpp"
#include "cad/solidcraft.hpp"
#include "cli/cli.hpp"
#include "io/model.hpp"
#include "io/stl.hpp"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <string>

namespace tisma {

namespace {

constexpr std::string_view kDesignUsage =
    "usage:\n"
    "  tisma-engine design new                    an empty design's JSON\n"
    "  tisma-engine design check <design.json>    whether it reads and its parameters bind; problems as JSON\n"
    "  tisma-engine design build <design.json> --solidcraft <solidcraft-cli> -o <out.stl|out.3mf>\n"
    "                            [--quality standard|fine|finest] [--timeline] [--save <design.sc>]\n"
    "      the bodies through SolidCraft; on the output {ok, volume_mm3, measure, failed_feature, failure}\n";

int usage(std::ostream& err, const std::string& problem) {
    err << "error: " << problem << "\n" << kDesignUsage;
    return kExitUsage;
}

bool read_text(const std::string& path, std::string& text) {
    std::ifstream in(std::filesystem::path(path), std::ios::binary);
    if (!in)
        return false;
    std::ostringstream s;
    s << in.rdbuf();
    text = s.str();
    return true;
}

bool write_text(const std::filesystem::path& path, const std::string& text) {
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    f.write(text.data(), std::streamsize(text.size()));
    return bool(f);
}

} // namespace

int cmd_design(std::span<const std::string_view> args, std::ostream& out, std::ostream& err) {
    if (args.empty())
        return usage(err, "design needs new, check or build");
    const std::string sub(args[0]);
    if (sub == "new") {
        out << cad::design_json(cad::Design{}) << "\n";
        return kExitOk;
    }
    if (sub != "check" && sub != "build")
        return usage(err, "unknown design command '" + sub + "'");
    std::string path;
    std::map<std::string, std::string> o;
    bool timeline = false;
    for (std::size_t i = 1; i < args.size(); ++i) {
        const std::string a(args[i]);
        if (a == "--timeline")
            timeline = true;
        else if ((a == "--solidcraft" || a == "-o" || a == "--quality" || a == "--save") && i + 1 < args.size())
            o[a] = std::string(args[++i]);
        else if (!a.starts_with("-") && path.empty())
            path = a;
        else
            return usage(err, "design: unexpected argument '" + a + "'");
    }
    if (path.empty())
        return usage(err, "design " + sub + " needs the design's JSON");
    std::string text;
    if (!read_text(path, text)) {
        err << "error: cannot read " << path << "\n";
        return kExitInput;
    }
    cad::Design design;
    try {
        design = cad::parse_design(text);
    } catch (const std::exception& e) {
        err << "error: " << path << ": " << e.what() << "\n";
        return kExitInput;
    }
    if (sub == "check") {
        cad::Design bound = design;
        const std::vector<std::string> problems = cad::apply_parameters(bound);
        out << nlohmann::json({{"ok", problems.empty()}, {"problems", problems}, {"features", design.features.size()},
                               {"has_body", design.has_body()}})
                   .dump(1)
            << "\n";
        return kExitOk;
    }
    if (!o.count("--solidcraft") || !o.count("-o"))
        return usage(err, "design build needs --solidcraft and -o");
    cad::MeshQuality quality = cad::MeshQuality::Standard;
    if (o.count("--quality")) {
        const std::string q = o["--quality"];
        if (q == "fine")
            quality = cad::MeshQuality::Fine;
        else if (q == "finest")
            quality = cad::MeshQuality::Finest;
        else if (q != "standard")
            return usage(err, "design build: --quality is standard, fine or finest");
    }
    const std::filesystem::path exe(o["--solidcraft"]);
    const cad::BuildResult r =
        timeline ? cad::build_timeline(exe, design, quality) : cad::build_design(exe, design, quality);
    nlohmann::json result = {{"ok", r.ok}, {"volume_mm3", r.volume_mm3}, {"triangles", r.mesh.triangle_count()}};
    result["measure"] = r.measure.empty() ? nlohmann::json(nullptr) : nlohmann::json::parse(r.measure, nullptr, false);
    if (r.failed_feature) {
        result["failed_feature"] = *r.failed_feature;
        result["failure"] = r.failure;
    }
    if (!r.ok) {
        result["error"] = r.error;
        result["refused"] = r.refused;
        out << result.dump(1) << "\n";
        err << "error: " << r.error << "\n";
        return kExitInput;
    }
    const std::filesystem::path target(o["-o"]);
    const bool three_mf = target.extension() == ".3mf";
    const std::string bytes =
        three_mf ? write_3mf(r.mesh, design.name) : stl_binary(r.mesh, "tisma-engine design " + design.name);
    if (!write_text(target, bytes) || (o.count("--save") && !write_text(std::filesystem::path(o["--save"]), r.design_file))) {
        err << "error: cannot write the outputs\n";
        return kExitInput;
    }
    out << result.dump(1) << "\n";
    return kExitOk;
}

} // namespace tisma
