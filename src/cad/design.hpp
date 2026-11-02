#pragma once

// A design of the Design tab (docs/spec/features/cad_design.md): the
// features in the order they were made, each a SolidCraft command with its
// parameters, so the whole design is the script that rebuilds it (rule 1,
// first step) and the project can keep it (rule 22). Own design, over
// SolidCraft's public command names; see PROVENANCE.md.

#include "cad/sketch.hpp"

#include <array>
#include <cstddef>
#include <string>
#include <vector>

namespace tisma::cad {

// The primitives of stage D1 (rule 3, Nuevo): each stands on z = 0, centred
// on the origin in X and Y, as the plate's shapes do.
enum class Primitive { Box, Cylinder, Sphere, Cone, Tube, Torus };

// The sizes a primitive takes, in mm, in this order:
// - box: length (X), width (Y), height (Z);
// - cylinder: diameter, height;
// - sphere: diameter;
// - cone: base diameter, top diameter (0: a point), height;
// - tube: outer diameter, inner diameter, height;
// - torus: ring diameter (through the middle of its section), section diameter.
inline constexpr std::size_t kMaxSizes = 3;
std::size_t size_count(Primitive kind);
// The sizes a new primitive starts with.
std::array<double, kMaxSizes> default_sizes(Primitive kind);
// Whether these sizes make a solid (every needed size above 0, a tube's hole
// inside it, a cone's top no wider than its base).
bool sizes_ok(Primitive kind, const std::array<double, kMaxSizes>& sizes);
// The SolidCraft command a primitive is (rule 1): "solid.box" and the like.
const char* command_of(Primitive kind);

struct Feature {
    std::string name; // unique in the design: the body's name too
    // A primitive with its sizes (stage D1), or a sketch with the operation
    // that makes a body of it (stage D2).
    enum class Type { Primitive, Sketch } type = Type::Primitive;
    Primitive kind = Primitive::Box;
    std::array<double, kMaxSizes> sizes{20.0, 20.0, 20.0};
    Sketch sketch;
    SketchOperation operation;
    bool suppressed = false; // the eye in Historial (rule 3): left out of the build
    // Whether it would make a body: a primitive's sizes make a solid, a
    // sketch has a closed region.
    bool buildable() const;
};

struct Design {
    std::string name = "Design";
    std::vector<Feature> features;

    // Whether a build would make a body: a feature not suppressed with sizes
    // that make a solid.
    bool has_body() const;
    // A name no feature has yet: `base`, else `base 2`, `base 3`...
    std::string free_name(const std::string& base) const;
};

// How finely the bodies are made into a mesh (SolidCraft's export qualities):
// the chord tolerance as a share of the body's size, 2e-4, 5e-5 or 1e-5.
enum class MeshQuality { Standard, Fine, Finest };
const char* quality_name(MeshQuality q);
// The coarsest quality that keeps the mesh within `chord_tolerance` mm of a
// body about `size` mm across (cad.chord_tolerance; the angle tolerance has
// no counterpart in SolidCraft's export and is kept for the engine's own
// mesher later).
MeshQuality quality_for(double chord_tolerance, double size);
// The largest size among the design's features that would be built, mm.
double nominal_size(const Design& design);

// The commands that build the design, one JSON object per command, as
// SolidCraft's `run` takes them: {"command": "solid.box", "params": {...}}.
// Suppressed features are left out.
std::vector<std::string> build_commands(const Design& design);
// The whole script {"commands": [...]}: the design's commands, then, with
// `export_3mf` not empty, an export of every body to that path at `quality`.
std::string script_json(const Design& design, const std::string& export_3mf = {}, MeshQuality quality = MeshQuality::Standard);

// The design read back from the JSON `design_json` gives (the project keeps
// it). Throws std::runtime_error on bad input.
std::string design_json(const Design& design);
Design parse_design(const std::string& json);

} // namespace tisma::cad
