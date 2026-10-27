#pragma once

#include <limits>
#include <map>

namespace tisma {

// How a layer's extrusion speeds change so that the layer lasts at least the
// minimum layer time (docs/spec/BEHAVIOR.md, "Enfriamiento"). A feed f becomes
// min(max(f * scale, min(f, floor)), cap):
//  - stage 1 scales every extrusion down by the same factor, but not below the
//    minimum print speed (floor); extrusions already slower stay;
//  - stage 2, when that is not enough, puts every extrusion at one common
//    speed (cap), the highest that reaches the time; extrusions already slower
//    stay; never below 1 mm/min.
struct Slowdown {
    double scale = 1.0;
    double floor = 0.0;
    double cap = std::numeric_limits<double>::infinity();
    int stage = 0; // 0: unchanged
    // The outer walls keep their speed (docs/GLOSS.md, G1 and G2): a wall
    // printed slower comes out of a hotter nozzle and shines differently.
    bool spare_outer = false;

    double feed(double f) const;
    double feed(double f, bool outer) const { return spare_outer && outer ? f : feed(f); }
};

inline constexpr double kSlowestFeed = 1.0 / 60.0; // mm/s, 1 mm/min

// The slowdown for a layer whose extrusions run length_by_feed[f] mm at f mm/s
// and whose other moves (travels, retractions) take other_time seconds.
Slowdown plan_slowdown(const std::map<double, double>& length_by_feed, double other_time, double min_time,
                       double min_speed);

// The same, sparing the outer walls when it can (cooling.keep_outer_speed):
// outer_by_feed holds the outer walls' part of length_by_feed. The other
// extrusions slow down first, as stage 1 does, not below the minimum print
// speed; only when that is not enough does the plan above take every
// extrusion, the outer walls too.
Slowdown plan_slowdown(const std::map<double, double>& length_by_feed, const std::map<double, double>& outer_by_feed,
                       double other_time, double min_time, double min_speed);

// The layer's time with a slowdown applied, in seconds; with the outer walls'
// part, for a slowdown that spares them.
double layer_time(const std::map<double, double>& length_by_feed, double other_time, const Slowdown& slowdown);
double layer_time(const std::map<double, double>& length_by_feed, const std::map<double, double>& outer_by_feed,
                  double other_time, const Slowdown& slowdown);

// The part-cooling fan, in percent, of a layer that takes t seconds and was
// not slowed down (docs/spec/features/cooling.md, rule 3): fan_max at
// min_time or less, then straight down to fan_min at below_time, and off from
// there, or fan_min when always.
double layer_fan(double t, double min_time, double below_time, double fan_min, double fan_max, bool always);

} // namespace tisma
