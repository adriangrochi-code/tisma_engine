// Driving the bundled solidcraft-cli with JSON scripts (cad_design.md, rule
// 1, first step): `commands` once, `run script --save design.sc` with an
// export command at the end, the 3MF read back by the engine's own reader.
// Own design, over the tool's documented command line; see PROVENANCE.md.

#include "cad/solidcraft.hpp"

#include "cad/process.hpp"
#include "io/model.hpp"

#include <nlohmann/json.hpp>

#include <atomic>
#include <cstdlib>
#include <fstream>
#include <sstream>

namespace tisma::cad {

std::filesystem::path bundled_solidcraft(const std::filesystem::path& exe_dir) {
    if (const char* given = std::getenv("TISMA_SOLIDCRAFT"); given && *given) {
        const std::filesystem::path p(given);
        return std::filesystem::is_regular_file(p) ? p : std::filesystem::path();
    }
#ifdef _WIN32
    const char* name = "solidcraft-cli.exe";
#else
    const char* name = "solidcraft-cli";
#endif
    const std::filesystem::path beside = exe_dir / "solidcraft" / name;
    std::error_code ec;
    return std::filesystem::is_regular_file(beside, ec) ? beside : std::filesystem::path();
}

SolidCraftInfo probe_solidcraft(const std::filesystem::path& exe, double timeout_seconds) {
    SolidCraftInfo info;
    info.exe = exe;
    if (exe.empty()) {
        info.error = "the design component is not here";
        return info;
    }
    const ProcessResult version = run_process(exe, {"--version"}, timeout_seconds);
    if (!version.ok()) {
        info.error = version.timed_out ? "the design component does not answer"
                     : version.started ? "the design component failed to start (exit " + std::to_string(version.status) + ")"
                                       : version.error;
        return info;
    }
    std::string v = version.out;
    while (!v.empty() && (v.back() == '\n' || v.back() == '\r' || v.back() == ' '))
        v.pop_back();
    info.version = v;
    const ProcessResult listed = run_process(exe, {"commands"}, timeout_seconds);
    if (!listed.ok()) {
        info.error = listed.timed_out ? "the design component does not answer" : "the design component cannot list its commands";
        return info;
    }
    try {
        const nlohmann::json j = nlohmann::json::parse(listed.out);
        const nlohmann::json& list = j.is_array() ? j : j.value("commands", nlohmann::json::array());
        for (const nlohmann::json& c : list) {
            if (c.is_string())
                info.commands.insert(c.get<std::string>());
            else if (c.is_object() && c.contains("id") && c.at("id").is_string())
                info.commands.insert(c.at("id").get<std::string>());
        }
    } catch (const nlohmann::json::exception&) {
        info.error = "the design component's command list cannot be read";
        return info;
    }
    if (info.commands.empty()) {
        info.error = "the design component lists no commands";
        return info;
    }
    info.available = true;
    return info;
}

namespace {

// A folder of its own for one build, removed when the build ends.
class TempFolder {
public:
    TempFolder() {
        static std::atomic<unsigned> counter{0};
        std::error_code ec;
        const std::filesystem::path base = std::filesystem::temp_directory_path(ec);
        if (ec)
            return;
        for (int tries = 0; tries < 100 && path_.empty(); ++tries) {
            const std::filesystem::path candidate =
                base / ("tisma-design-" + std::to_string(std::rand() % 1000000) + "-" + std::to_string(counter++));
            if (std::filesystem::create_directory(candidate, ec) && !ec)
                path_ = candidate;
            ec.clear();
        }
    }
    ~TempFolder() {
        if (!path_.empty()) {
            std::error_code ec;
            std::filesystem::remove_all(path_, ec);
        }
    }
    const std::filesystem::path& path() const { return path_; }

private:
    std::filesystem::path path_;
};

std::string read_file(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    std::ostringstream text;
    text << in.rdbuf();
    return text.str();
}

// The tool's complaint, trimmed: its last line of stderr, or of stdout.
std::string complaint(const ProcessResult& r) {
    for (const std::string* text : {&r.err, &r.out}) {
        std::string t = *text;
        while (!t.empty() && (t.back() == '\n' || t.back() == '\r' || t.back() == ' '))
            t.pop_back();
        if (t.empty())
            continue;
        const std::size_t nl = t.rfind('\n');
        return nl == std::string::npos ? t : t.substr(nl + 1);
    }
    return "exit " + std::to_string(r.status);
}

} // namespace

BuildResult build_design(const std::filesystem::path& exe, const Design& design, MeshQuality quality,
                         double timeout_seconds) {
    BuildResult result;
    if (!design.has_body()) {
        result.ok = true;
        return result;
    }
    if (exe.empty()) {
        result.error = "the design component is not here";
        return result;
    }
    TempFolder folder;
    if (folder.path().empty()) {
        result.error = "cannot make a temporary folder";
        return result;
    }
    const std::filesystem::path script = folder.path() / "design.json", out = folder.path() / "bodies.3mf",
                                saved = folder.path() / "design.sc";
    {
        std::ofstream file(script, std::ios::binary);
        file << script_json(design, out.string(), quality);
        if (!file) {
            result.error = "cannot write the script";
            return result;
        }
    }
    const ProcessResult run = run_process(exe, {"run", script.string(), "--save", saved.string()}, timeout_seconds);
    if (!run.ok()) {
        result.error = run.timed_out ? "the design component did not answer in time"
                       : !run.started ? run.error
                                      : "the design component refused the design: " + complaint(run);
        return result;
    }
    std::error_code ec;
    if (!std::filesystem::is_regular_file(out, ec)) {
        result.error = "the design component wrote no bodies";
        return result;
    }
    try {
        const ModelFile model = read_model(out);
        result.mesh = model.mesh;
    } catch (const std::exception& e) {
        result.error = std::string("the bodies cannot be read: ") + e.what();
        return result;
    }
    result.design_file = read_file(saved);
    result.measure = run.out;
    result.volume_mm3 = result.mesh.signed_volume();
    result.ok = true;
    return result;
}

} // namespace tisma::cad
