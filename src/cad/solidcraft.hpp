#pragma once

// The bundled SolidCraft command-line tool (docs/spec/features/
// cad_design.md, rule 1, first step): the program carries a pinned
// `solidcraft-cli`, the Design tab sends it the design's script and reads
// back the bodies as a 3MF. Own design; see PROVENANCE.md.

#include "cad/design.hpp"
#include "mesh/mesh.hpp"

#include <chrono>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <string>

namespace tisma::cad {

// The version the program is built to carry (PLAN.md, «Espacio de diseño
// CAD»): the release whose solidcraft-cli the installer bundles.
inline constexpr const char* kSolidCraftVersion = "0.30.0";

// Where the bundled tool is: the TISMA_SOLIDCRAFT environment variable (the
// tests' stand-in), else `solidcraft/solidcraft-cli` (`.exe` on Windows)
// next to the program `exe`. Empty when neither exists.
std::filesystem::path bundled_solidcraft(const std::filesystem::path& exe_dir);

// What was learnt about the tool at start (rule 1: `commands` asked once;
// rule 22 bis: missing, damaged or not answering).
struct SolidCraftInfo {
    bool available = false;
    std::filesystem::path exe;
    std::string version;            // what --version printed
    std::set<std::string> commands; // the command ids that version has
    std::string error;              // why it is not available
    bool has(const std::string& command) const { return commands.count(command) > 0; }
};

// Asks the tool its version and its commands. `exe` empty: not available.
SolidCraftInfo probe_solidcraft(const std::filesystem::path& exe, double timeout_seconds = 30.0);

// A built design: its bodies as one mesh (standing where the design put
// them), SolidCraft's design file to keep (rule 22), and what it measured.
struct BuildResult {
    bool ok = false;
    std::string error; // in the person's terms: the command that failed and why
    TriangleMesh mesh;
    std::string design_file; // the `.sc` bytes
    std::string measure;     // the JSON `run` printed (inspect.measure)
    double volume_mm3 = 0.0; // from the mesh
    bool refused = false;    // the tool ran and said no (not missing, not late)
    // The timeline (rule 8): the first feature that fails, its reason in
    // `failure`; the bodies are those of the features before it, and the
    // ones after it wait.
    std::optional<std::size_t> failed_feature;
    std::string failure;
    bool session_restarted = false; // the session had died and was opened again for this build
};

class ChildProcess;

// SolidCraft kept open while designing (rule 20 bis): `solidcraft-cli mcp`,
// a Model Context Protocol server over stdio (JSON-RPC 2.0, a line per
// message; its tools new_design, batch, measure and save, from SolidCraft's
// public docs/mcp.md), started once and sent every build. When it dies the
// next build starts it again and rebuilds from the script; `restarts`
// counts those. Safe to use from one thread at a time (a lock guards it).
class SolidCraftSession {
public:
    explicit SolidCraftSession(std::filesystem::path exe);
    ~SolidCraftSession();
    const std::filesystem::path& exe() const { return exe_; }
    bool open() const;
    void close();
    // Closed after `seconds` without a build (rule 20 bis: ten minutes).
    void close_if_idle(double seconds);
    int starts() const { return starts_; }
    int restarts() const { return restarts_; }
    // One tool call; the text of its content, and whether it was an error.
    // False when the session is gone (no answer in time, or it ended).
    bool call(const std::string& tool, const std::string& arguments_json, std::string& text, bool& is_error,
              double timeout_seconds);
    bool start(std::string& error);

private:
    std::filesystem::path exe_;
    std::unique_ptr<ChildProcess> process_;
    long next_id_ = 1;
    int starts_ = 0, restarts_ = 0;
    std::chrono::steady_clock::time_point last_use_;
    mutable std::mutex lock_;
};

// Runs the design's script through the tool, exporting its bodies as a 3MF
// at `quality` and saving the design file, in a temporary folder removed
// after. A design with nothing to build gives an empty mesh and ok.
BuildResult build_design(const std::filesystem::path& exe, const Design& design, MeshQuality quality,
                         double timeout_seconds = 120.0, SolidCraftSession* session = nullptr);

// The design built as a timeline (rule 8): when the tool refuses it, the
// first feature that fails is found (the shortest failing run of features
// from the start, by halving), and the features before it are built. The
// result is ok with `failed_feature` and `failure` set; a missing or late
// tool is an error as in build_design.
BuildResult build_timeline(const std::filesystem::path& exe, const Design& design, MeshQuality quality,
                           double timeout_seconds = 120.0, SolidCraftSession* session = nullptr);

} // namespace tisma::cad
