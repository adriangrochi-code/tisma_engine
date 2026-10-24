#pragma once

#include <string>
#include <vector>

namespace tisma {

// The machine's motion limits for the time estimate.
struct Kinematics {
    double max_feed[4] = {200.0, 200.0, 12.0, 120.0}; // mm/s per axis: X, Y, Z, E
    double acceleration = 1000.0;                       // mm/s2, moves that extrude
    double travel_acceleration = 1250.0;                // mm/s2, travels and retractions
    double corner_speed = 10.0;                         // mm/s through a corner of 90 degrees or more
};

// Print time from textbook kinematics (own design): every move has a
// trapezoidal speed profile (triangular when it is too short to reach its
// feed), with the feed clamped so that no axis exceeds its limit. Between two
// moves the speed is at most the lower of their feeds times the cosine of the
// turn, and never forced below corner_speed; a backward and a forward pass make
// every move's entry and exit speeds reachable with its acceleration. Moves
// are planned in batches that start and end at rest (one batch per layer).
class PrintTime {
public:
    explicit PrintTime(const Kinematics& k) : k_(k) {}

    // A move by (dx, dy, dz, de) mm at feed mm/s; extruding picks the acceleration.
    void add(double dx, double dy, double dz, double de, double feed, bool extruding);

    // Plans the moves added since the last flush, from rest to rest, and adds
    // their time to the total. Returns that time in seconds.
    double flush();

    double total() const { return total_; }

private:
    struct Move {
        double length, feed, acceleration;
        double ux, uy, uz; // direction in XYZ (all zero for a move of E alone)
    };
    Kinematics k_;
    std::vector<Move> moves_;
    double total_ = 0.0;
};

// The time of a move of length mm that enters at v0, leaves at v1 and runs at
// most at feed, with acceleration a (v0 and v1 reachable from each other).
double move_time(double length, double v0, double v1, double feed, double a);

// "1h 7m 36s", "7m 36s", "36s" (docs/spec/GCODE_OUTPUT.md, "Tiempos"); days as "2d 1h 0m 5s".
std::string format_duration(double seconds);

} // namespace tisma
