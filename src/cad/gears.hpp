// Involute gears for printing: spur, helical and herringbone, with a bore
// and a backlash so printed gears mesh (the owner's request, 2026-11-02).
// Own design from the textbook involute geometry (the involute of the base
// circle, the standard proportions: addendum m, dedendum 1.25 m, ISO 53's
// basic rack); see PROVENANCE.md.

#pragma once

#include "mesh/mesh.hpp"

#include <string>
#include <string_view>

namespace tisma::cad {

struct GearSpec {
    int teeth = 20;
    double module = 1.5;          // mm: pitch diameter / teeth
    double pressure_angle = 20.0; // degrees
    double width = 8.0;           // face width, mm
    double bore = 5.0;            // hole on the axis, mm; 0: none
    double helix_angle = 0.0;     // degrees; 0: spur
    bool herringbone = false;     // the helix turns back at mid width
    bool left_hand = false;       // the helix turns the other way (a helical gear's mate)
    double backlash = 0.15;       // mm taken off each tooth on the pitch circle
    // A rack instead: `teeth` along X, straight flanks at the pressure angle,
    // on a base `rack_base` mm thick under the roots; no bore, no helix.
    bool rack = false;
    double rack_base = 3.0;
};

struct GearDims {
    double pitch_diameter = 0, outside_diameter = 0, root_diameter = 0, base_diameter = 0;
    double circular_pitch = 0, tooth_thickness = 0; // on the pitch circle, after the backlash
};
GearDims gear_dims(const GearSpec& spec);

// The distance between the axes of two gears of the same module that mesh;
// for a rack, from the mating gear's axis to the rack's pitch line.
double center_distance(const GearSpec& a, int other_teeth);

// A rack's pitch line above its bottom face.
double rack_pitch_height(const GearSpec& rack);

// Empty when the gear can be made, else why not (English).
std::string check_gear(const GearSpec& spec);

// The radius of the tooth outline at polar angle `angle` (radians) from the
// middle of a tooth, for one tooth pitch: the tip, the involute flanks, the
// root circle.
double gear_outline_radius(const GearSpec& spec, double angle);

// The gear on the bed, its axis on Z through the origin, a tooth's middle on
// +X at the bottom; closed and facing out. A rack lies along X from the
// origin, its teeth towards +Y, its face width along Z. Throws std::invalid_argument when
// check_gear() does not pass.
TriangleMesh make_gear(const GearSpec& spec);

std::string gear_json(const GearSpec& spec);
GearSpec parse_gear(std::string_view json); // throws std::runtime_error

} // namespace tisma::cad
