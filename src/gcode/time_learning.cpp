// Learning each printer's print time (print_time.md, rules 11, 15 and 16).
// Own design: ridge-regularised least squares on the five parts' factors,
// solved from the normal equations by Gaussian elimination; see PROVENANCE.md.

#include "gcode/time_learning.hpp"

#include <algorithm>
#include <cmath>

namespace tisma {

double TimeCalibration::adjust(const TimeSplit& estimate) const {
    double total = 0.0;
    for (std::size_t k = 0; k < kTimeParts; ++k)
        total += factor[k] * estimate[k];
    return total;
}

TimeCalibration fit_time(const std::vector<TimeRecord>& records) {
    TimeCalibration out;
    out.prints = records.size();
    if (records.size() < 3)
        return out;
    constexpr std::size_t n = kTimeParts;
    // Normal equations of sum_i (sum_k f_k e_ik - a_i)^2 + lambda sum_k (f_k - 1)^2.
    std::array<std::array<double, n + 1>, n> m{};
    double scale = 0.0;
    for (const TimeRecord& r : records)
        for (std::size_t k = 0; k < n; ++k)
            scale += r.estimate[k] * r.estimate[k];
    const double lambda = 0.05 * scale / double(records.size() * n);
    for (const TimeRecord& r : records)
        for (std::size_t a = 0; a < n; ++a) {
            for (std::size_t b = 0; b < n; ++b)
                m[a][b] += r.estimate[a] * r.estimate[b];
            m[a][n] += r.estimate[a] * r.actual;
        }
    for (std::size_t a = 0; a < n; ++a) {
        m[a][a] += lambda;
        m[a][n] += lambda;
    }
    // Gaussian elimination with partial pivoting.
    for (std::size_t c = 0; c < n; ++c) {
        std::size_t pivot = c;
        for (std::size_t r = c + 1; r < n; ++r)
            if (std::abs(m[r][c]) > std::abs(m[pivot][c]))
                pivot = r;
        std::swap(m[c], m[pivot]);
        if (!(std::abs(m[c][c]) > 1e-12))
            return out;
        for (std::size_t r = 0; r < n; ++r) {
            if (r == c)
                continue;
            const double f = m[r][c] / m[c][c];
            for (std::size_t k = c; k <= n; ++k)
                m[r][k] -= f * m[c][k];
        }
    }
    for (std::size_t k = 0; k < n; ++k)
        out.factor[k] = std::clamp(m[k][n] / m[k][k], 0.5, 2.0);
    out.fitted = true;
    double sum = 0.0;
    for (const TimeRecord& r : records) {
        const double e = out.adjust(r.estimate) - r.actual;
        sum += e * e;
    }
    out.typical_error = std::sqrt(sum / double(records.size()));
    return out;
}

double remaining_time(double estimated_total, double estimated_elapsed, double real_elapsed) {
    const double rest = std::max(0.0, estimated_total - estimated_elapsed);
    if (!(estimated_elapsed > 0.0) || !(real_elapsed > 0.0))
        return rest;
    return rest * real_elapsed / estimated_elapsed;
}

} // namespace tisma
