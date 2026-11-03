#pragma once

// A design of the Design tab (docs/spec/features/cad_design.md): the
// features in the order they were made, each a SolidCraft command with its
// parameters, so the whole design is the script that rebuilds it (rule 1,
// first step) and the project can keep it (rule 22). Own design, over
// SolidCraft's public command names; see PROVENANCE.md.

#include "cad/parameters.hpp"
#include "cad/sketch.hpp"

#include <array>
#include <map>
#include <optional>
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

// The operations of stage D4 on a body made before them (rule 7), as
// SolidCraft's commands take them (its public command reference): edges and
// faces named by a point on them.
enum class OperationKind { Fillet, Chamfer, Shell, Hole, Mirror, PatternCircular, PatternRectangular, Combine, Thread, Move, Split, Draft };
// "fillet", "chamfer"... and back; and the SolidCraft command it is.
const char* operation_name(OperationKind kind);
std::optional<OperationKind> operation_named(const std::string& name);
const char* operation_command(OperationKind kind);

struct BodyOperation {
    OperationKind kind = OperationKind::Fillet;
    std::string target; // the body it works on: a feature's name
    // Points on the edges (fillet, chamfer), on the faces left open (shell;
    // none: a closed hollow body) or where the holes go, on a face.
    std::vector<std::array<double, 3>> refs;
    double value = 2.0; // fillet radius, chamfer distance, shell thickness, hole diameter (mm)
    // A hole by ISO 273 ("M5", with fit close, normal or loose); empty: the
    // diameter is `value`. Depth 0: through all.
    std::string standard;
    std::string fit = "normal";
    double depth = 0.0;
    // A tapped hole (the tap drill of `standard`, its thread cosmetic unless
    // modeled), and a thread on a cylindrical face (`standard` is its
    // designation, "M8"; `depth` its length, 0 the whole face).
    bool tapped = false;
    bool modeled = false;
    std::string hand = "right";
    // Move: the offset, and a turn of `angle` about `axis` (both optional);
    // a copy keeps the original. Split: at `plane` moved `plane_offset` mm
    // along its normal. Draft: the faces picked tilted `value` degrees from
    // the neutral `plane`.
    std::array<double, 3> offset{0.0, 0.0, 0.0};
    bool copy = false;
    double plane_offset = 0.0;
    // Patterns: how many in all; circular about an axis (X, Y or Z through
    // the origin) over an angle; rectangular along a direction at a spacing.
    int count = 6;
    char axis = 'Z';
    double angle = 360.0;
    std::array<double, 3> direction{1.0, 0.0, 0.0};
    double spacing = 30.0;
    std::string plane = "YZ"; // mirror: XY, XZ or YZ through the origin, joined to the body
    // Combine: the other bodies, and join, cut or intersect.
    std::vector<std::string> tools;
    std::string boolean = "cut";
    friend bool operator==(const BodyOperation&, const BodyOperation&) = default;
};

struct Feature {
    std::string name; // unique in the design: the body's name too
    // A primitive with its sizes (stage D1), a sketch with the operation that
    // makes a body of it (stage D2), or an operation on a body (stage D4).
    enum class Type { Primitive, Sketch, Operation } type = Type::Primitive;
    Primitive kind = Primitive::Box;
    std::array<double, kMaxSizes> sizes{20.0, 20.0, 20.0};
    Sketch sketch;
    SketchOperation operation;
    BodyOperation op; // Type::Operation
    bool suppressed = false; // the eye in Historial (rule 3): left out of the build
    // Sizes tied to the design's parameters (rule 9): what they set, and the
    // expression. Targets: "size0" to "size2" (a primitive's sizes),
    // "distance" and "angle" (the sketch's extrusion or revolution),
    // "e<k>.width", "e<k>.height" (a rectangle) and "e<k>.diameter" (a
    // circle) for the sketch's entity k; "value", "spacing" and "angle" (an
    // operation's size, a pattern's spacing and angle).
    std::map<std::string, std::string> bindings;
    // Whether it would make a body, or change one: a primitive's sizes make a
    // solid, a sketch has a closed region, an operation has a body and what
    // it needs (edges, a positive size, two or more copies, tools).
    bool buildable() const;
};

struct Design {
    std::string name = "Design";
    std::vector<Feature> features;
    std::vector<Parameter> parameters; // rule 9, in the order they were made

    // Whether a build would make a body: a feature not suppressed with sizes
    // that make a solid.
    bool has_body() const;
    // A name no feature has yet: `base`, else `base 2`, `base 3`...
    std::string free_name(const std::string& base) const;
};

// What a binding target measures: a length, or an angle ("angle"); None for
// a target the feature does not have.
Measure target_measure(const Feature& feature, const std::string& target);

// Sets every bound size from the parameters (rule 9): changing a parameter
// rebuilds the part. Returns what could not be set, one line each
// ("box 1: size0: unknown name \"ancho\""), the size left as it was.
std::vector<std::string> apply_parameters(Design& design);

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
// Suppressed features are left out; bound sizes take their parameters'
// values first (apply_parameters, on a copy).
std::vector<std::string> build_commands(const Design& design);
// The whole script {"commands": [...]}: the design's commands, then, with
// `export_3mf` not empty, an export of every body to that path at `quality`.
std::string script_json(const Design& design, const std::string& export_3mf = {}, MeshQuality quality = MeshQuality::Standard);

// The design read back from the JSON `design_json` gives (the project keeps
// it). Throws std::runtime_error on bad input.
std::string design_json(const Design& design);
Design parse_design(const std::string& json);

} // namespace tisma::cad
