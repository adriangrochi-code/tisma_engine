#pragma once

#include <array>
#include <cstddef>
#include <vector>

namespace tisma {

// Learning the print time of each printer (print_time.md, rules 11, 15 and
// 16): the estimate of each finished print, by part, beside the time it
// really took; with three or more, a factor per part fitted by least squares.
enum TimePart { kTimeFirstLayer, kTimeWalls, kTimeInfill, kTimeTravel, kTimePreparation, kTimeParts };
using TimeSplit = std::array<double, kTimeParts>; // seconds by part

struct TimeRecord {
    TimeSplit estimate{};
    double actual = 0.0; // seconds, as the printer reported it or the person wrote it
};

struct TimeCalibration {
    TimeSplit factor{1.0, 1.0, 1.0, 1.0, 1.0};
    std::size_t prints = 0;     // records it was fitted with
    bool fitted = false;        // three or more records
    double typical_error = 0.0; // seconds: the fitted estimates' root mean square error
    // The estimate with the factors (as it is when not fitted).
    double adjust(const TimeSplit& estimate) const;
};

// The factors that make the parts' sum closest to the real times (least
// squares, pulled lightly towards 1 so that a few prints do not decide five
// factors alone), each kept from 0.5 to 2. Fewer than three records: not fitted.
TimeCalibration fit_time(const std::vector<TimeRecord>& records);

// Rule 16: the time left, the estimate's rest scaled by how the print is
// really going (real elapsed time over the estimate's at the same point).
double remaining_time(double estimated_total, double estimated_elapsed, double real_elapsed);

} // namespace tisma
