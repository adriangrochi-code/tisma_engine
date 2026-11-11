// Driving the bundled solidcraft-cli with JSON scripts (cad_design.md, rule
// 1, first step): `commands` once, `run script --save design.sc` with an
// export command at the end, the 3MF read back by the engine's own reader.
// Own design, over the tool's documented command line; see PROVENANCE.md.

#include "cad/solidcraft.hpp"

#include "cad/process.hpp"
#include "io/model.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <sstream>

#ifdef _WIN32
#include <process.h>
#define TISMA_GETPID _getpid
#else
#include <unistd.h>
#define TISMA_GETPID getpid
#endif

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

// The folder every build's own folder goes in, one for this run of the
// program: the session is started allowed to write there (solidcraft-cli
// 0.30.0 refuses paths outside the folders it is given with --allow-path).
// Named by the process and the time, and removed with everything in it
// when the program ends.
const std::filesystem::path& design_root() {
    struct Root {
        std::filesystem::path path;
        Root() {
            std::error_code ec;
            const std::filesystem::path base = std::filesystem::temp_directory_path(ec);
            if (ec)
                return;
            const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
            for (int tries = 0; tries < 100 && path.empty(); ++tries) {
                const std::filesystem::path candidate =
                    base / ("tisma-design-" + std::to_string(long(TISMA_GETPID())) + "-" + std::to_string((now + tries) % 1000000007));
                if (std::filesystem::create_directory(candidate, ec) && !ec)
                    path = candidate;
                ec.clear();
            }
        }
        ~Root() {
            if (!path.empty()) {
                std::error_code ec;
                std::filesystem::remove_all(path, ec);
            }
        }
    };
    static const Root root;
    return root.path;
}

// A folder of its own for one build, removed when the build ends.
class TempFolder {
public:
    TempFolder() {
        static std::atomic<unsigned> counter{0};
        std::error_code ec;
        const std::filesystem::path base = design_root().empty() ? std::filesystem::temp_directory_path(ec) : design_root();
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

SolidCraftSession::SolidCraftSession(std::filesystem::path exe) : exe_(std::move(exe)), last_use_(std::chrono::steady_clock::now()) {}
SolidCraftSession::~SolidCraftSession() { close(); }

bool SolidCraftSession::open() const {
    std::lock_guard<std::mutex> g(lock_);
    return process_ && process_->running();
}

void SolidCraftSession::close() {
    std::lock_guard<std::mutex> g(lock_);
    if (process_)
        process_->stop();
    process_.reset();
}

void SolidCraftSession::close_if_idle(double seconds) {
    {
        std::lock_guard<std::mutex> g(lock_);
        if (!process_ || std::chrono::steady_clock::now() - last_use_ < std::chrono::duration<double>(seconds))
            return;
    }
    close();
}

bool SolidCraftSession::start(std::string& error) {
    process_ = std::make_unique<ChildProcess>();
    // Headless (no app to connect to), allowed to write in the builds' folder.
    std::vector<std::string> args{"mcp", "--headless"};
    if (!design_root().empty())
        args.insert(args.end(), {"--allow-path", design_root().string()});
    if (!process_->start(exe_, args, error)) {
        process_.reset();
        return false;
    }
    if (starts_ > 0)
        ++restarts_;
    ++starts_;
    // The MCP handshake: initialize, its answer, then «initialized».
    const nlohmann::json init{{"jsonrpc", "2.0"},
                              {"id", next_id_++},
                              {"method", "initialize"},
                              {"params",
                               {{"protocolVersion", "2025-06-18"},
                                {"capabilities", nlohmann::json::object()},
                                {"clientInfo", {{"name", "tisma"}, {"version", kSolidCraftVersion}}}}}};
    if (!process_->write_line(init.dump())) {
        error = "the design component closed at once";
        process_.reset();
        return false;
    }
    for (;;) {
        const std::optional<std::string> line = process_->read_line(30.0);
        if (!line) {
            error = "the design component did not answer its start";
            process_->stop();
            process_.reset();
            return false;
        }
        const nlohmann::json j = nlohmann::json::parse(*line, nullptr, false);
        if (j.is_object() && j.contains("id") && j["id"] == init["id"]) {
            if (j.contains("error")) {
                error = "the design component refused the session: " + j["error"].value("message", std::string());
                process_->stop();
                process_.reset();
                return false;
            }
            break;
        }
    }
    process_->write_line(nlohmann::json{{"jsonrpc", "2.0"}, {"method", "notifications/initialized"}}.dump());
    return true;
}

bool SolidCraftSession::call(const std::string& tool, const std::string& arguments_json, std::string& text, bool& is_error,
                             double timeout_seconds) {
    text.clear();
    is_error = false;
    if (!process_ || !process_->running())
        return false;
    last_use_ = std::chrono::steady_clock::now();
    const long id = next_id_++;
    nlohmann::json request{{"jsonrpc", "2.0"}, {"id", id}, {"method", "tools/call"}};
    request["params"] = {{"name", tool}, {"arguments", nlohmann::json::parse(arguments_json)}};
    if (!process_->write_line(request.dump()))
        return false;
    const auto until = std::chrono::steady_clock::now() + std::chrono::duration<double>(timeout_seconds > 0 ? timeout_seconds : 3600.0);
    while (std::chrono::steady_clock::now() < until) {
        const double left = std::chrono::duration<double>(until - std::chrono::steady_clock::now()).count();
        const std::optional<std::string> line = process_->read_line(std::max(0.01, left));
        if (!line) {
            if (!process_->running())
                return false;
            continue;
        }
        const nlohmann::json j = nlohmann::json::parse(*line, nullptr, false);
        if (!j.is_object() || !j.contains("id") || j["id"] != id)
            continue; // a notification or another answer
        last_use_ = std::chrono::steady_clock::now();
        if (j.contains("error")) {
            is_error = true;
            text = j["error"].value("message", std::string("error"));
            return true;
        }
        const nlohmann::json& r = j.value("result", nlohmann::json::object());
        is_error = r.value("isError", false);
        for (const nlohmann::json& part : r.value("content", nlohmann::json::array()))
            if (part.value("type", std::string()) == "text")
                text += (text.empty() ? "" : "\n") + part.value("text", std::string());
        return true;
    }
    // Late: the session is not trusted any more.
    process_->stop();
    return false;
}

namespace {

// One build through the session: a fresh design, the script as one batch
// (it exports the bodies), then measure and save. False when the session
// went away on the way (then `r` is not filled).
bool session_build(SolidCraftSession& s, const std::string& script, const std::filesystem::path& out,
                   const std::filesystem::path& saved, double timeout, BuildResult& r) {
    std::string text;
    bool error = false;
    if (!s.call("new_design", "{}", text, error, timeout))
        return false;
    const nlohmann::json parsed = nlohmann::json::parse(script);
    const nlohmann::json batch{{"commands", parsed.value("commands", nlohmann::json::array())}, {"rollback", true}, {"verbose", false}};
    if (!s.call("batch", batch.dump(), text, error, timeout))
        return false;
    if (error) {
        r.error = "the design component refused the design: " + text;
        r.refused = true;
        return true;
    }
    if (!s.call("measure", "{}", text, error, timeout))
        return false;
    r.measure = error ? std::string() : text;
    if (!s.call("save", nlohmann::json{{"path", saved.string()}}.dump(), text, error, timeout))
        return false;
    (void)out;
    r.ok = true;
    return true;
}

} // namespace

BuildResult build_design(const std::filesystem::path& exe, const Design& design, MeshQuality quality,
                         double timeout_seconds, SolidCraftSession* session) {
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
    if (session) {
        // Rule 20 bis: the open session; if it had died, open again (once)
        // and rebuild from the script.
        BuildResult r;
        bool done = false;
        for (int attempt = 0; attempt < 2 && !done; ++attempt) {
            if (!session->open()) {
                std::string error;
                const bool had_started = session->starts() > 0;
                if (!session->start(error)) {
                    result.error = error;
                    return result;
                }
                if (had_started)
                    result.session_restarted = true;
            }
            r = BuildResult{};
            done = session_build(*session, read_file(script), out, saved, timeout_seconds, r);
            if (!done)
                session->close();
        }
        if (!done) {
            result.error = "the design component did not answer in time";
            return result;
        }
        if (!r.ok) {
            result.error = r.error;
            result.refused = r.refused;
            return result;
        }
        result.measure = r.measure;
    } else {
    const ProcessResult run = run_process(exe, {"run", script.string(), "--save", saved.string()}, timeout_seconds);
    if (!run.ok()) {
        result.error = run.timed_out ? "the design component did not answer in time"
                       : !run.started ? run.error
                                      : "the design component refused the design: " + complaint(run);
        result.refused = run.started && !run.timed_out;
        return result;
    }
    result.measure = run.out;
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
    result.volume_mm3 = result.mesh.signed_volume();
    result.ok = true;
    return result;
}

BuildResult build_timeline(const std::filesystem::path& exe, const Design& design, MeshQuality quality,
                           double timeout_seconds, SolidCraftSession* session) {
    BuildResult whole = build_design(exe, design, quality, timeout_seconds, session);
    if (whole.ok || !whole.refused)
        return whole;
    // The features that take part, in order; the design up to the k-th of
    // them (the later ones suppressed).
    std::vector<std::size_t> active;
    for (std::size_t i = 0; i < design.features.size(); ++i)
        if (!design.features[i].suppressed && design.features[i].buildable())
            active.push_back(i);
    if (active.empty())
        return whole;
    const auto upto = [&](std::size_t k) {
        Design d = design;
        for (std::size_t j = k; j < active.size(); ++j)
            d.features[active[j]].suppressed = true;
        return d;
    };
    // Builds of the first lo features pass; of the first hi, fail.
    std::size_t lo = 0, hi = active.size();
    std::string why = whole.error;
    while (hi - lo > 1) {
        const std::size_t mid = (lo + hi) / 2;
        BuildResult r = build_design(exe, upto(mid), quality, timeout_seconds, session);
        if (r.ok) {
            lo = mid;
        } else if (r.refused) {
            hi = mid;
            why = r.error;
        } else {
            return r; // the tool went missing or late on the way
        }
    }
    BuildResult before = build_design(exe, upto(lo), quality, timeout_seconds, session);
    if (!before.ok)
        return before.refused ? whole : before;
    before.failed_feature = active[hi - 1];
    before.failure = why;
    return before;
}

} // namespace tisma::cad
