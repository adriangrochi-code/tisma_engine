#pragma once

#include "calibration/calibration.hpp"
#include "config/settings.hpp"
#include "mesh/mesh.hpp"
#include "mesh/paint.hpp"
#include "project/engineering.hpp"
#include "slice/color_mix.hpp"

#include <array>
#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

// Project files (docs/PROJECT.md): everything needed to pick a print up where
// it was left, in one file: the settings, the chosen profiles, the models and
// the calibration being printed. Own design; see PROVENANCE.md.
namespace tisma {

class ProjectError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

// A project saved by a newer version of the program (docs/PROJECT.md,
// «Errores»): the program must be updated to read it.
class ProjectTooNew : public ProjectError {
public:
    using ProjectError::ProjectError;
};

// A modifier or negative volume added to a model, a box or a cylinder placed
// from the model's box (the centre in X and Y, the bottom in Z), and a height
// range of it (model_tools.md, rules 2, 3 and 6).
struct ProjectVolume {
    bool negative = false;
    bool cylinder = false;
    std::array<double, 3> offset{0.0, 0.0, 0.0}, size{10.0, 10.0, 10.0};
    std::string settings{};
    std::array<double, 9> turn{1, 0, 0, 0, 1, 0, 0, 0, 1}; // its own turn, by rows
    bool lattice = false;                               // a lattice (structural_optimization.md, rule 7)
    double lattice_cell = 0.0, lattice_diameter = 0.0; // mm
};
struct ProjectRange {
    double from = 0.0, to = 2.0;
    std::string settings{};
};

struct ProjectModel {
    std::string name;                // the file it came from, or the calibration's name
    TriangleMesh mesh;               // as it was loaded, in its own coordinates
    std::array<double, 3> offset{};  // where it was on the bed, mm (see `turn`)
    Painting painting;               // its painted faces (docs/PAINTING.md), by triangle of the mesh
    // Its extruder (from 1, or a blend's number), and whether a filament
    // change may purge into its sparse infill or into all of it
    // (multi_extruder.md, rules 1 and 13).
    int extruder = 1;
    bool wipe_into_infill = false, wipe_into_object = false;
    // Its own settings (model_tools.md, rule 7): a JSON object of groups
    // over the profile's; empty: none.
    std::string settings{};
    std::vector<ProjectVolume> volumes{};
    std::vector<ProjectRange> ranges{};
    // Its Engineering data (formats.md, rule 11 bis): conditions in its own
    // coordinates and triangles, values changed by hand, options.
    ProjectEngineering engineering{};
    // Its turn and scale on the bed, by rows: a point p of the mesh is at
    // turn · p + offset (formats.md, rule 8: a 3MF's build item transform).
    std::array<double, 9> turn{1, 0, 0, 0, 1, 0, 0, 0, 1};
    // Whether it is printed (interface.md, «Menús»: Printable); a model that
    // is not stays on the bed and out of the slice.
    bool printable = true;
};

// The calibration being printed: which one, and the value of each segment.
struct ProjectCalibration {
    std::string kind; // as CalibrationPrint::kind
    std::string unit;
    std::vector<CalibrationSegment> segments;
};

struct Project {
    Settings settings;
    std::array<std::string, 3> profiles; // the chosen profile of each ProfileKind, "" for none
    std::vector<ProjectModel> models;
    std::optional<ProjectCalibration> calibration;
    // The project's mixed colours and the colour of each filament
    // (color_mixing.md): they travel with the models, not with a profile.
    std::vector<ColorMix> mixes;
    std::vector<std::string> filament_colors;
    // What opening the file left out, for the person (not saved): Engineering
    // data this program does not read, conditions that no longer fit their mesh.
    std::vector<std::string> notices;
};

// The file: "TISMAPRJ", the format version (32 bits), then chunks, each a
// four-letter tag, its size (64 bits) and its bytes, all little-endian. The
// first chunk is "JSON" (the settings, profiles, models and calibration); each
// model's mesh follows in a "MESH" chunk, as a binary STL, then each painted
// model's painting in a "PNT " chunk (painting_to_bytes). Readers skip tags
// they do not know.
std::string write_project(const Project& project);
// The same project as a 3MF package (formats.md, rules 6 to 11): each model
// an object of the standard 3D/3dmodel.model, its offset in its build item,
// so any 3MF reader sees the plate; the rest in Metadata/tisma_project.json
// (the JSON chunk's data) and each painting in Metadata/tisma_paint_<k>.bin
// ("TPNT", a 32-bit version, painting_to_bytes).
std::string write_project_3mf(const Project& project);
// Either file, told by its first bytes. A 3MF's models come back in their
// own coordinates, with their build items' turn and offset. Throws ProjectError on a damaged or
// newer file, or a 3MF without the engine's data, naming what is wrong.
Project read_project(std::string_view data);

// Whether these bytes are a project file: the engine's own (its first eight
// bytes) or a 3MF with Metadata/tisma_project.json.
bool looks_like_project(std::string_view data);

// The same for a file: only its first bytes, or a whole 3MF.
bool is_project_file(const std::filesystem::path& path);

// Writes the file aside and moves it over (a 3MF package when the name ends
// in .3mf, the engine's own file otherwise), so a failed save keeps the old one.
void save_project(const std::filesystem::path& path, const Project& project);
Project load_project(const std::filesystem::path& path);

} // namespace tisma
