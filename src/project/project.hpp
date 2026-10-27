#pragma once

#include "calibration/calibration.hpp"
#include "config/settings.hpp"
#include "mesh/mesh.hpp"
#include "mesh/paint.hpp"
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

struct ProjectModel {
    std::string name;                // the file it came from, or the calibration's name
    TriangleMesh mesh;               // as it was loaded, in its own coordinates
    std::array<double, 3> offset{};  // where it was on the bed, mm
    Painting painting;               // its painted faces (docs/PAINTING.md), by triangle of the mesh
    // Its extruder (from 1, or a blend's number), and whether a filament
    // change may purge into its sparse infill or into all of it
    // (multi_extruder.md, rules 1 and 13).
    int extruder = 1;
    bool wipe_into_infill = false, wipe_into_object = false;
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
};

// The file: "TISMAPRJ", the format version (32 bits), then chunks, each a
// four-letter tag, its size (64 bits) and its bytes, all little-endian. The
// first chunk is "JSON" (the settings, profiles, models and calibration); each
// model's mesh follows in a "MESH" chunk, as a binary STL, then each painted
// model's painting in a "PNT " chunk (painting_to_bytes). Readers skip tags
// they do not know.
std::string write_project(const Project& project);
// Throws ProjectError on a damaged or newer file, naming what is wrong.
Project read_project(std::string_view data);

// Writes the file aside and moves it over, so a failed save keeps the old one.
void save_project(const std::filesystem::path& path, const Project& project);
Project load_project(const std::filesystem::path& path);

} // namespace tisma
