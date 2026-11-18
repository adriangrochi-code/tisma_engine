#pragma once

#include <array>
#include <cstddef>
#include <map>
#include <optional>
#include <string>
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

// Rule 11: the real time of a finished print as the printer reports it, in
// seconds: Marlin's answer to M31 ("echo:Print time: 1h 2m 3s", with days
// too, or "... 3 min, 4 sec"), or Klipper's print_stats.print_duration (a
// JSON number after "print_duration"). None when the text holds neither.
std::optional<double> parse_print_duration(const std::string& text);

// The pairs kept by printer (its name in the printer profile), the newest
// kTimeRecordsKept of each; saved as a small JSON file.
inline constexpr std::size_t kTimeRecordsKept = 50;
struct TimeHistory {
    std::map<std::string, std::vector<TimeRecord>> printers;
    void add(const std::string& printer, const TimeRecord& record);
    TimeCalibration calibration(const std::string& printer) const; // fit_time of its records
};
std::string time_history_json(const TimeHistory& history);
// Records that cannot be read (a part missing, a time not above 0) are left out.
TimeHistory parse_time_history(const std::string& json);
// False when the file cannot be written; loading a missing or broken file gives an empty history.
bool save_time_history(const TimeHistory& history, const std::string& path);
TimeHistory load_time_history(const std::string& path);

} // namespace tisma
