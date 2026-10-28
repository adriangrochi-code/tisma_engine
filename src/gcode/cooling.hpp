#pragma once

#include <limits>
#include <map>

namespace tisma {

// How a layer's extrusion speeds change so that the layer lasts at least the
// minimum layer time (docs/spec/features/cooling.md, rule 2): every extrusion
// faster than a common speed v goes at v, and the slower ones stay.
//  - stage 1: v, the highest that reaches the time, no lower than the
//    minimum print speed;
//  - stage 2, when that is not enough: v lower, never below 1 mm/min.
// The surface walls (the outer wall, and with cooling.slowdown_logic
// consistent_surface the wall that touches it; cooling_more.md, rule 5, and
// docs/GLOSS.md) keep their speed while the rest slowing down to the minimum
// speed is enough; then they take their own common speed, no lower than the
// minimum; in stage 2 they go at v with the rest. Bridges and walls over the
// air are never slowed down (rule 2, K5): their time is among the others.
struct Slowdown {
    double cap = std::numeric_limits<double>::infinity();         // v
    double surface_cap = std::numeric_limits<double>::infinity(); // the surface walls' v
    int stage = 0; // 0: unchanged

    double feed(double f, bool surface = false) const { return std::min(f, surface ? surface_cap : cap); }
};

inline constexpr double kSlowestFeed = 1.0 / 60.0; // mm/s, 1 mm/min

// The slowdown for a layer whose extrusions run rest[f] mm at f mm/s, besides
// its surface walls' surface[f], and whose other moves (travels,
// retractions, what is never slowed down) take other_time seconds.
Slowdown plan_slowdown(const std::map<double, double>& rest, const std::map<double, double>& surface,
                       double other_time, double min_time, double min_speed);

// The layer's time with a slowdown applied, in seconds.
double layer_time(const std::map<double, double>& rest, const std::map<double, double>& surface, double other_time,
                  const Slowdown& slowdown);

// The part-cooling fan, in percent, of a layer that takes t seconds and was
// not slowed down (docs/spec/features/cooling.md, rule 3): fan_max at
// min_time or less, then straight down to fan_min at below_time, and off from
// there, or fan_min when always.
double layer_fan(double t, double min_time, double below_time, double fan_min, double fan_max, bool always);

} // namespace tisma
