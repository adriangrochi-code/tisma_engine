#pragma once

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace tisma {

// The machine's motion limits for the time estimate.
struct Kinematics {
    double max_feed[4] = {200.0, 200.0, 12.0, 120.0}; // mm/s per axis: X, Y, Z, E
    double acceleration = 1000.0;                       // mm/s2, moves that extrude
    double travel_acceleration = 1250.0;                // mm/s2, travels and retractions
    double corner_speed = 10.0;                         // mm/s through a corner of 90 degrees or more
    // The rest of the machine's limits (gcode_firmware.md, rule 2); 0: not known.
    double retract_acceleration = 0.0; // mm/s2 of moves of E alone; 0: the travels'
    double max_accel[4] = {0.0, 0.0, 0.0, 0.0}; // mm/s2 per axis
    double jerk[4] = {0.0, 0.0, 0.0, 0.0};      // mm/s per axis a junction changes at once
    double junction_deviation = 0.0;            // mm; in place of the jerk and corner_speed
    double min_extrude_feed = 0.0, min_travel_feed = 0.0; // mm/s
};

// Print time from textbook kinematics (own design): every move has a
// trapezoidal speed profile (triangular when it is too short to reach its
// feed), with the feed raised to the slowest the machine moves and clamped so
// that no axis exceeds its limit, and the acceleration clamped the same way by
// each axis' own. Between two moves the speed is at most the lower of their
// feeds and:
// - with a junction deviation, the speed of the circle that rounds the corner
//   that far from it at the lower of their accelerations (r = d sin(a/2) /
//   (1 - sin(a/2)) for the angle a between them, v = sqrt(acceleration r));
// - with jerks, the speed at which no axis changes its own by more than its jerk;
// - otherwise their feed times the cosine of the turn, never forced below
//   corner_speed.
// A backward and a forward pass make every move's entry and exit speeds
// reachable with its acceleration. Moves are planned in batches that start
// and end at rest (one batch per layer).
class PrintTime {
public:
    explicit PrintTime(const Kinematics& k) : k_(k) {}

    // A move by (dx, dy, dz, de) mm at feed mm/s; extruding picks the acceleration.
    void add(double dx, double dy, double dz, double de, double feed, bool extruding);

    // Plans the moves added since the last flush, from rest to rest, and adds
    // their time to the total. Returns that time in seconds.
    double flush();

    // The accelerations of the moves added from now on (a command changed them).
    void set_accelerations(double print, double travel) {
        k_.acceleration = print;
        k_.travel_acceleration = travel;
    }

    double total() const { return total_; }

    // A mark between the moves added so far and the next ones: once they are
    // flushed, time_at(mark) is the time from the start up to it.
    std::size_t mark();
    double time_at(std::size_t mark) const { return mark_times_[mark]; }

private:
    struct Move {
        double length, feed, acceleration;
        double ux, uy, uz; // direction in XYZ (all zero for a move of E alone)
        double ue;         // E per mm of the move, for the extruder's jerk
    };
    Kinematics k_;
    std::vector<Move> moves_;
    double total_ = 0.0;
    std::vector<std::pair<std::size_t, std::size_t>> pending_marks_; // (mark, moves before it) of this batch
    std::vector<double> mark_times_;
};

// The time of a move of length mm that enters at v0, leaves at v1 and runs at
// most at feed, with acceleration a (v0 and v1 reachable from each other).
double move_time(double length, double v0, double v1, double feed, double a);

// "1h 7m 36s", "7m 36s", "36s" (docs/spec/GCODE_OUTPUT.md, "Tiempos"); days as "2d 1h 0m 5s".
std::string format_duration(double seconds);

} // namespace tisma
