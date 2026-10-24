#pragma once

#include "config/settings.hpp"
#include "mesh/mesh.hpp"

#include <string>
#include <vector>

// Printer calibrations (docs/CALIBRATION.md): the test parts, generated here
// rather than read from files, the height changes that make each segment of
// a tower try one value, and the compensation computed from what the user
// measures on the printed part. Own design; see PROVENANCE.md.
namespace tisma {

// One segment of a tower: the heights it spans and the value it tries.
struct CalibrationSegment {
    double z0 = 0.0, z1 = 0.0; // mm
    double value = 0.0;
};

struct CalibrationPrint {
    std::string kind;   // "dimensions", "temperature", "retraction", "flow", "speed"
    std::string unit;   // of the segments' values: "C", "mm", "%"
    TriangleMesh mesh;  // centred on the origin, standing on z = 0
    std::vector<HeightChange> changes;      // for Settings::changes
    std::vector<CalibrationSegment> segments;
};

// The dimensional test: a block x by y by z mm with a round hole of diameter
// `hole` through it along Z. Its sides differ (30 by 20 by default), so the
// printed X and Y cannot be mixed up when measuring.
struct DimensionTest {
    double x = 30.0, y = 20.0, z = 15.0, hole = 10.0;
};
CalibrationPrint dimension_test(const DimensionTest& test = {});

// The values a tower tries, from start to end by step (step's sign follows
// end - start); at most 40 segments.
std::vector<double> tower_values(double start, double end, double step);

// Towers of segments `segment` mm tall, each one a value, joined by a 1 mm
// neck narrower than the segments so they show apart and every segment has a
// top surface. The changes set the value from the bottom of each segment.
// Temperature: a 24 x 12 mm block with a 45 degree overhang on one side.
CalibrationPrint temperature_tower(double start, double end, double step, double segment = 8.0);
// Retraction length: two 8 x 8 mm columns 30 mm apart, so every layer travels
// between them and retracts.
CalibrationPrint retraction_tower(double start, double end, double step, double segment = 5.0);
// Flow (extrusion.flow, %): a 20 x 20 mm tower; each segment's top surface
// shows the flow.
CalibrationPrint flow_tower(double start, double end, double step, double segment = 5.0);
// Speed (% of every extrusion's feed): a 30 x 30 mm tower; the segment where
// walls start to fail is the limit of the hot end.
CalibrationPrint speed_tower(double start, double end, double step, double segment = 5.0);

// What the user measured on the printed dimensional test, in mm.
struct Measurements {
    double x = 0.0, y = 0.0, z = 0.0, hole = 0.0;
};

struct Compensation {
    double scale_x = 100.0, scale_y = 100.0, scale_z = 100.0; // %
    double hole_offset = 0.0;                                 // mm
};

// The compensation that makes the next print match the design: each scale
// times nominal / measured (the part was printed with the current scales),
// and the holes moved by half of what is left of the hole's error once the
// new X and Y scales are applied. Clamped to the ranges of docs/CONFIG.md.
// A measurement of 0 (not taken) leaves its value as it is.
Compensation compensation_from(const DimensionTest& nominal, const Measurements& measured, const Settings& current);

} // namespace tisma
