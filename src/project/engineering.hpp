#pragma once

#include "mesh/mesh.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

// What a part keeps of the Engineering section in the project
// (formats.md, rule 11 bis; engineering_panel.md, rule 17): its structural
// conditions, the values changed by hand, the options of the Structural and
// Aerodynamics tabs. Points and zones are in the part's own coordinates and
// triangles, so moving or turning the part on the bed keeps them where they
// were on it. Own design; see PROVENANCE.md.
namespace tisma {

enum class ConditionKind { FixedPoint, FixedZone, ForcePoint, ForceZone, Limit };
// A force's direction: along the surface's normal, into the part; along a
// signed axis; or a vector.
enum class ForceDirection { Normal, Axis, Vector };

struct EngineeringCondition {
    ConditionKind kind = ConditionKind::FixedZone;
    int number = 1;      // its number in the list ("Zona fija 1")
    bool hidden = false; // the eye in the list: kept but not drawn nor used
    // A point: where it is on the part, the surface's normal there, and the
    // disc's radius, mm.
    std::array<double, 3> point{}, normal{0.0, 0.0, 1.0};
    double radius = 5.0;
    // A zone (and a limit): the triangles of the part's mesh that form it.
    std::vector<std::uint32_t> triangles;
    // A force: its value in N; the kg typed, only to show it again the same.
    double newtons = 0.0;
    std::optional<double> kilograms;
    ForceDirection direction = ForceDirection::Normal;
    int axis = -3; // with Axis: 1, 2, 3 for X, Y, Z, negative for the minus side
    std::array<double, 3> vector{0.0, 0.0, -1.0};
    // A limit: how far the zone may move, mm or % of the part's largest size.
    double limit = 0.0;
    bool limit_percent = false;

    bool is_point() const { return kind == ConditionKind::FixedPoint || kind == ConditionKind::ForcePoint; }
    bool is_force() const { return kind == ConditionKind::ForcePoint || kind == ConditionKind::ForceZone; }
};

// The values of the profile changed by hand in the panel (rule 3): only
// those are kept; the rest is read from the profile again.
struct EngineeringOverrides {
    std::optional<std::string> material; // a record of fea/materials.hpp
    std::optional<double> extrusion_temperature, layer_height, first_layer_height, line_width;
    bool operator==(const EngineeringOverrides&) const = default;
};

// The Structural tab's options (engineering_panel.md, rule 10).
struct StructuralOptions {
    double safety_factor = 2.0;
    std::string quality = "normal";   // as AnalysisQuality: fast, normal, high
    std::string print_direction = "z"; // z, or the belt printer's
    bool cloud = false;
    bool operator==(const StructuralOptions&) const = default;
};

// The Aerodynamics tab's inputs (rule 15).
struct AerodynamicsInputs {
    std::string direction = "+x"; // +x, -x, +y, -y, +z, -z, or "view"
    double speed = 10.0;          // m/s
    std::string quality = "fast"; // friction, fast, normal, high
    bool cloud = false;
    bool operator==(const AerodynamicsInputs&) const = default;
};

// The last result of an analysis (rule 11 bis): the report as the engine
// writes it (JSON text, which starts with '{', so never with a program's
// magic bytes), and the fingerprint of what it was computed from.
struct EngineeringResult {
    std::string kind;        // "structural" or "aerodynamics"
    std::string fingerprint; // engineering_fingerprint when it was computed
    std::string report;      // the report's JSON
    bool operator==(const EngineeringResult&) const = default;
};

struct ProjectEngineering {
    std::vector<EngineeringCondition> conditions;
    EngineeringOverrides overrides;
    double working_temperature = 23.0; // °C, always kept (rule 4)
    StructuralOptions structural;
    AerodynamicsInputs aerodynamics;
    // The last result of each analysis; kept apart in the package, like the
    // painting (engineering_json leaves them out).
    std::vector<EngineeringResult> results;

    // Nothing to keep: a project without Engineering data opens as before.
    bool empty() const;
};

// The JSON of a part's Engineering data (an object), and back. The reader
// throws EngineeringError on what it does not understand, so the project
// can open without it and say so (rule 11 bis).
struct EngineeringError : std::runtime_error {
    using std::runtime_error::runtime_error;
};
std::string engineering_json(const ProjectEngineering& e);
ProjectEngineering engineering_from_json(const std::string& json);

// The conditions that no longer fit a mesh of `triangles` triangles (a zone
// with a triangle it does not have) are removed; returns how many.
std::size_t drop_conditions_beyond(ProjectEngineering& e, std::size_t triangles);

const char* condition_kind_name(ConditionKind kind);

// The conditions of a part whose mesh became `a` p + `b` of itself (by rows;
// placed on the bed, mirrored): points move with it and normals turn with it
// (by the cofactor of a, so a mirror or an uneven scale keeps them normal).
// Triangle numbers, forces' axes and vectors stay: they are the world's.
void move_engineering(ProjectEngineering& e, const std::array<double, 9>& a, const std::array<double, 3>& b);

// What a result depends on, as 16 hexadecimal digits (FNV-1a, 64 bits): the
// part's mesh, its conditions, values and options (not its results), and the
// settings' JSON. A result whose fingerprint differs from the current one is
// stale («Desactualizado», engineering_panel.md, rule 14).
std::string engineering_fingerprint(const TriangleMesh& mesh, const ProjectEngineering& e, std::string_view settings);
bool result_stale(const EngineeringResult& r, const TriangleMesh& mesh, const ProjectEngineering& e,
                  std::string_view settings);

} // namespace tisma
