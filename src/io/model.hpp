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
struct ThreeMfItem {
    std::string name;
    TriangleMesh mesh;
};
std::vector<ThreeMfItem> read_3mf_items(std::string_view package, std::optional<std::string>* extra = nullptr,
                                        std::string_view extra_name = {});

// A 3MF package with these objects, one build item each, moved by `offset`
// (mm), its vertices written so they read back to the same floats, and these
// other files in it (formats.md, rule 11: the engine's own data).
struct ThreeMfObject {
    std::string name;
    const TriangleMesh* mesh = nullptr;
    std::array<double, 3> offset{0.0, 0.0, 0.0};
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

// Reads and parses a model file. Throws ModelError.
ModelFile read_model(const std::filesystem::path& path);

// What the format is called in messages ("binary STL", "3MF").
const char* format_name(ModelFormat format);

} // namespace tisma
