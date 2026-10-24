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
// top surface. The changes set the value from the bottom of each segment, and
// the value is written in relief on the segment's front (-Y) face.
//
// Temperature: each segment (at least 10 mm) is a 24 x 12 mm block with
// - on the left (-X), an overhang ridge whose angle from the vertical grows
//   along it from 30 to 72 degrees, to see from which angle it deforms;
// - on the right (+X), a flat shelf 8 mm out with no support under it (where
//   flat roofs without support are tried, such as arc overhangs);
// - at the back (+Y), a 45 degree bracket with a 2.4 mm square pin standing
//   free on it, to push over by hand: it breaks between layers, so the
//   segment whose pin resists most bonds its layers best;
// - on the front, the temperature in relief, a diamond and a ring: detail
//   that shows how the temperature treats small features.
CalibrationPrint temperature_tower(double start, double end, double step, double segment = 10.0);
// Retraction length: two 8 x 8 mm columns 30 mm apart, so every layer travels
// between them and retracts.
CalibrationPrint retraction_tower(double start, double end, double step, double segment = 5.0);
// Flow (extrusion.flow, %): a 20 x 20 mm tower; each segment's top surface
// shows the flow.
CalibrationPrint flow_tower(double start, double end, double step, double segment = 5.0);
// Speed (% of every extrusion's feed): a 30 x 30 mm tower; the segment where
// walls start to fail is the limit of the hot end.
CalibrationPrint speed_tower(double start, double end, double step, double segment = 5.0);

// The calibrations the interface creates with one click, their ranges chosen
// from the current settings (docs/CALIBRATION.md, "Rangos automáticos").
enum class CalibrationKind { Dimensions, Temperature, Retraction, Flow, Speed };
CalibrationPrint automatic_calibration(CalibrationKind kind, const Settings& settings);

// Writes the value of the segment that printed best into the settings
// (the nozzle temperature variable, the retraction length, the flow, or the
// extrusion speeds scaled by the percentage) and removes the tower's height
// changes. Returns false for Dimensions (those use compensation_from).
bool apply_calibration(CalibrationKind kind, double value, Settings& settings);

// Text in relief: the strokes of a seven-segment style font (digits, '.',
// '-'), each a segment between two points of a cell `width` by `height`, the
// cells `width + gap` apart. Own font.
struct Stroke {
    double x0, y0, x1, y1;
};
std::vector<Stroke> label_strokes(const std::string& text, double width, double height, double gap);

// The text in relief on a face of constant y (`face_y`), facing -Y when
// `towards_minus_y`, its first cell's lower left corner at (x, z), the strokes
// `stroke` mm thick and standing `depth` mm out of the face.
void add_label(TriangleMesh& mesh, const std::string& text, double x, double z, double face_y, bool towards_minus_y,
               double height, double stroke, double depth);

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
