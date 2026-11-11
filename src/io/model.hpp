#pragma once

#include "io/zip.hpp"
#include "mesh/mesh.hpp"

#include <array>
#include <optional>
#include <vector>

#include <cstddef>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <string_view>

namespace tisma {

class ModelError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

enum class ModelFormat { StlBinary, StlAscii, Obj, ThreeMf };

// A model file read into one mesh, in millimetres (docs/spec/features/
// formats.md, rules 1 to 5 and 7 to 8).
struct ModelFile {
    TriangleMesh mesh;
    ModelFormat format = ModelFormat::StlBinary;
    std::string name;        // an STL's solid name or header; empty otherwise
    std::size_t objects = 1; // objects on the 3MF's build plate
    std::size_t parts = 1;   // parts of those objects (meshes, or the volumes a slicer project lists)
    // A slicer project's negative volumes (model_tools.md, rule 2), in the
    // same coordinates: taken away from the part when slicing.
    TriangleMesh negative;
    // Its other kinds of part (project_import.md, rule 4), in the same
    // coordinates: modifiers (each volume apart), support blockers and
    // support enforcers.
    std::vector<TriangleMesh> modifiers;
    TriangleMesh support_blockers, support_enforcers;
    // Each part's triangles in `mesh` (formats.md, FM9): its range, the build
    // item it belongs to (from 0), its name (the volume's in a slicer
    // project, else the object's) and its extruder (0: not given).
    struct Piece {
        std::string name;
        std::size_t object = 0;
        std::size_t first = 0, count = 0;
        int extruder = 0;
    };
    std::vector<Piece> pieces;
};

// A Wavefront OBJ file: its vertices ("v") and faces ("f", any number of
// corners, split into a fan; indices from 1, or negative from the end), in
// millimetres. The rest (normals, texture coordinates, groups, materials) is
// ignored. Throws ModelError.
ModelFile parse_obj(std::string_view text);

// A 3MF package (3MF Core Specification): the objects on its build plate,
// each with the transform of its build item and of its components, nested
// or in other model files of the package (Production Extension, p:path), in
// the model's unit. Objects of type support, surface or other, and items
// marked not printable, are left out; so are the volumes a slicer project
// lists as not being parts (modifiers, blockers and the like, from
// Metadata/Slic3r_PE_model.config), but for the negative volumes, which go
// to ModelFile::negative. Throws ModelError.
ModelFile parse_3mf(std::string_view package);

// Each printable item on a 3MF package's build plate as its own mesh, placed
// (its transforms applied, in millimetres), in the build's order, with its
// object's name; and, with `extra`, the package's file `extra_name` (none if
// it has no such file). Throws ModelError.
// With `own_frame`, each mesh is left in its object's own coordinates and
// its build item's transform is given apart: bed = turn (by rows) · p + offset.
struct ThreeMfItem {
    std::string name;
    TriangleMesh mesh;
    std::array<double, 9> turn{1, 0, 0, 0, 1, 0, 0, 0, 1};
    std::array<double, 3> offset{0.0, 0.0, 0.0};
};
std::vector<ThreeMfItem> read_3mf_items(std::string_view package, std::optional<std::string>* extra = nullptr,
                                        std::string_view extra_name = {}, bool own_frame = false);

// A 3MF package with these objects, one build item each, set on the bed by
// `turn` (by rows: rotation and scale) and moved by `offset` (mm), its
// vertices written so they read back to the same floats, and these other
// files in it (formats.md, rules 8 and 11: the engine's own data).
struct ThreeMfObject {
    std::string name;
    const TriangleMesh* mesh = nullptr;
    std::array<double, 3> offset{0.0, 0.0, 0.0};
    std::array<double, 9> turn{1, 0, 0, 0, 1, 0, 0, 0, 1};
};
std::string write_3mf_objects(const std::vector<ThreeMfObject>& objects, std::vector<ZipEntryToWrite> extra = {});

// A 3MF package with one object of this mesh on its build plate, named, in
// millimetres (3MF Core Specification): the model file, its content types
// and its relationships, stored without compression.
std::string write_3mf(const TriangleMesh& mesh, std::string_view name);

// The model in memory: AMF, STEP, SVG and ZIP are not read yet and say so;
// a ZIP package is a 3MF; otherwise the extension says (".obj"); otherwise it
// is an STL. Throws ModelError.
ModelFile parse_model(std::string_view data, std::string_view extension);

// The mesh as Wavefront OBJ text: each distinct corner once as "v x y z"
// (written so it reads back to the same float), then "f a b c" per triangle
// (1-based), with a comment naming it. Read back by parse_model, it gives the
// same triangles in the same order.
std::string obj_text(const TriangleMesh& mesh, std::string_view name = {});

// Reads and parses a model file. Throws ModelError.
ModelFile read_model(const std::filesystem::path& path);

// What the format is called in messages ("binary STL", "3MF").
const char* format_name(ModelFormat format);

} // namespace tisma
