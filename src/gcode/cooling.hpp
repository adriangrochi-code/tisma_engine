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

    double feed(double f) const;
};

inline constexpr double kSlowestFeed = 1.0 / 60.0; // mm/s, 1 mm/min

// The slowdown for a layer whose extrusions run length_by_feed[f] mm at f mm/s
// and whose other moves (travels, retractions) take other_time seconds.
Slowdown plan_slowdown(const std::map<double, double>& length_by_feed, double other_time, double min_time,
                       double min_speed);

// The layer's time with a slowdown applied, in seconds.
double layer_time(const std::map<double, double>& length_by_feed, double other_time, const Slowdown& slowdown);

} // namespace tisma
