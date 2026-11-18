// Learning each printer's print time (print_time.md, rules 11, 15 and 16).
// Own design: ridge-regularised least squares on the five parts' factors,
// solved from the normal equations by Gaussian elimination; see PROVENANCE.md.

#include "gcode/time_learning.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <sstream>

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

std::optional<double> parse_print_duration(const std::string& text) {
    // Klipper: "print_duration": 1234.5
    if (const std::size_t k = text.find("print_duration"); k != std::string::npos) {
        std::size_t i = k + 14;
        while (i < text.size() && (text[i] == '"' || text[i] == ':' || std::isspace(static_cast<unsigned char>(text[i]))))
            ++i;
        const char* start = text.c_str() + i;
        char* end = nullptr;
        const double v = std::strtod(start, &end);
        if (end != start && std::isfinite(v) && v >= 0.0)
            return v;
        return std::nullopt;
    }
    // Marlin M31: numbers, each followed by its unit (d, h, m or min, s or sec).
    const std::size_t k = text.find("Print time:");
    if (k == std::string::npos)
        return std::nullopt;
    double total = 0.0;
    bool any = false;
    std::size_t i = k + 11;
    while (i < text.size() && text[i] != '\n') {
        if (!std::isdigit(static_cast<unsigned char>(text[i]))) {
            ++i;
            continue;
        }
        const char* start = text.c_str() + i;
        char* end = nullptr;
        const double v = std::strtod(start, &end);
        i += std::size_t(end - start);
        while (i < text.size() && text[i] == ' ')
            ++i;
        const char u = i < text.size() ? char(std::tolower(static_cast<unsigned char>(text[i]))) : '\0';
        const double unit = u == 'd' ? 86400.0 : u == 'h' ? 3600.0 : u == 'm' ? 60.0 : u == 's' ? 1.0 : 0.0;
        if (unit == 0.0)
            return std::nullopt;
        total += v * unit;
        any = true;
        while (i < text.size() && std::isalpha(static_cast<unsigned char>(text[i])))
            ++i;
    }
    return any ? std::optional<double>(total) : std::nullopt;
}

void TimeHistory::add(const std::string& printer, const TimeRecord& record) {
    std::vector<TimeRecord>& list = printers[printer];
    list.push_back(record);
    if (list.size() > kTimeRecordsKept)
        list.erase(list.begin(), list.begin() + std::ptrdiff_t(list.size() - kTimeRecordsKept));
}

TimeCalibration TimeHistory::calibration(const std::string& printer) const {
    const auto it = printers.find(printer);
    return fit_time(it == printers.end() ? std::vector<TimeRecord>{} : it->second);
}

std::string time_history_json(const TimeHistory& history) {
    nlohmann::json root = nlohmann::json::object();
    for (const auto& [name, list] : history.printers) {
        nlohmann::json items = nlohmann::json::array();
        for (const TimeRecord& r : list)
            items.push_back({{"estimate", std::vector<double>(r.estimate.begin(), r.estimate.end())}, {"actual", r.actual}});
        root[name] = std::move(items);
    }
    return nlohmann::json{{"version", 1}, {"printers", std::move(root)}}.dump(1);
}

TimeHistory parse_time_history(const std::string& json) {
    TimeHistory out;
    const nlohmann::json root = nlohmann::json::parse(json, nullptr, false);
    if (!root.is_object() || !root.contains("printers") || !root["printers"].is_object())
        return out;
    for (const auto& [name, items] : root["printers"].items()) {
        if (!items.is_array())
            continue;
        for (const nlohmann::json& item : items) {
            if (!item.is_object() || !item.contains("estimate") || !item.contains("actual"))
                continue;
            const nlohmann::json& e = item["estimate"];
            const nlohmann::json& a = item["actual"];
            if (!e.is_array() || e.size() != kTimeParts || !a.is_number() || !(a.get<double>() > 0.0))
                continue;
            TimeRecord r;
            bool ok = true;
            for (std::size_t k = 0; k < kTimeParts; ++k) {
                ok = ok && e[k].is_number() && e[k].get<double>() >= 0.0;
                r.estimate[k] = ok ? e[k].get<double>() : 0.0;
            }
            r.actual = a.get<double>();
            if (ok)
                out.add(name, r);
        }
    }
    return out;
}

bool save_time_history(const TimeHistory& history, const std::string& path) {
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    if (!f)
        return false;
    f << time_history_json(history);
    return bool(f);
}

TimeHistory load_time_history(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f)
        return {};
    std::ostringstream text;
    text << f.rdbuf();
    return parse_time_history(text.str());
}

} // namespace tisma
