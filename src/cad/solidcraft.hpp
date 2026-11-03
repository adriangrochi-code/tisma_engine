#pragma once

// The bundled SolidCraft command-line tool (docs/spec/features/
// cad_design.md, rule 1, first step): the program carries a pinned
// `solidcraft-cli`, the Design tab sends it the design's script and reads
// back the bodies as a 3MF. Own design; see PROVENANCE.md.

#include "cad/design.hpp"
#include "mesh/mesh.hpp"

#include <filesystem>
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
};

// Runs the design's script through the tool, exporting its bodies as a 3MF
// at `quality` and saving the design file, in a temporary folder removed
// after. A design with nothing to build gives an empty mesh and ok.
BuildResult build_design(const std::filesystem::path& exe, const Design& design, MeshQuality quality,
                         double timeout_seconds = 120.0);

// The design built as a timeline (rule 8): when the tool refuses it, the
// first feature that fails is found (the shortest failing run of features
// from the start, by halving), and the features before it are built. The
// result is ok with `failed_feature` and `failure` set; a missing or late
// tool is an error as in build_design.
BuildResult build_timeline(const std::filesystem::path& exe, const Design& design, MeshQuality quality,
                           double timeout_seconds = 120.0);

} // namespace tisma::cad
