// Reading the printer's limits. Own code; see printer_limits.hpp.

#include "device/printer_limits.hpp"

#include "config/settings.hpp"

#include <nlohmann/json.hpp>

#include <array>
#include <cctype>
#include <cstdlib>
#include <sstream>

namespace tisma::device {
namespace {

// The letter-number parameters after a command on a line: «M203 X500.00 Y500 E25».
std::map<char, double> parameters(const std::string& line, std::size_t after) {
    std::map<char, double> out;
    std::istringstream in(line.substr(after));
    std::string word;
    while (in >> word) {
        if (word[0] == ';')
            break; // a comment
        if (word.size() < 2 || !std::isalpha(static_cast<unsigned char>(word[0])))
            continue;
        char* end = nullptr;
        const double v = std::strtod(word.c_str() + 1, &end);
        if (end != word.c_str() + 1)
            out[char(std::toupper(static_cast<unsigned char>(word[0])))] = v;
    }
    return out;
}

// The position of `command` as a word on the line («M203» but not «M2030»), or npos.
std::size_t command_at(const std::string& line, const std::string& command) {
    std::size_t at = 0;
    while ((at = line.find(command, at)) != std::string::npos) {
        const bool start = at == 0 || !std::isalnum(static_cast<unsigned char>(line[at - 1]));
        const std::size_t after = at + command.size();
        const bool finish = after >= line.size() || !std::isdigit(static_cast<unsigned char>(line[after]));
        if (start && finish)
            return at;
        at = after;
    }
    return std::string::npos;
}

// A number after «key:» inside «{...}» text: «x:200». The key starts after
// «{» or «,», so «max:{» is not read as «x:».
bool area_value(const std::string& text, std::size_t from, std::size_t to, const char* key, double& out) {
    const std::string k = std::string(key) + ":";
    for (std::size_t at = text.find(k, from); at != std::string::npos && at < to; at = text.find(k, at + 1)) {
        if (at > 0 && (text[at - 1] == '{' || text[at - 1] == ',')) {
            out = std::strtod(text.c_str() + at + k.size(), nullptr);
            return true;
        }
    }
    return false;
}

} // namespace

PrinterLimits parse_marlin_limits(const std::string& m115, const std::string& m503) {
    PrinterLimits out;
    // «FIRMWARE_NAME:Marlin 2.1.3 (Aug  1 2024 12:00:00) SOURCE_CODE_URL:…»
    if (const std::size_t at = m115.find("FIRMWARE_NAME:"); at != std::string::npos) {
        std::string name = m115.substr(at + 14);
        name = name.substr(0, name.find_first_of("(\r\n"));
        if (const std::size_t next = name.find(" SOURCE_CODE_URL"); next != std::string::npos)
            name = name.substr(0, next);
        while (!name.empty() && name.back() == ' ')
            name.pop_back();
        out.source = name;
    }
    // The work area, or the full one when no work area is given.
    if (const std::size_t area = m115.find("area:{"); area != std::string::npos) {
        std::size_t box = m115.find("work:{", area);
        if (box == std::string::npos)
            box = m115.find("full:{", area);
        if (box != std::string::npos) {
            const std::size_t min_at = m115.find("min:{", box), max_at = m115.find("max:{", box);
            const std::size_t min_end = min_at == std::string::npos ? std::string::npos : m115.find('}', min_at);
            const std::size_t max_end = max_at == std::string::npos ? std::string::npos : m115.find('}', max_at);
            double x0 = 0, y0 = 0, x1 = 0, y1 = 0, z1 = 0;
            if (min_end != std::string::npos && max_end != std::string::npos && area_value(m115, min_at, min_end, "x", x0) &&
                area_value(m115, min_at, min_end, "y", y0) && area_value(m115, max_at, max_end, "x", x1) &&
                area_value(m115, max_at, max_end, "y", y1) && x1 > x0 && y1 > y0) {
                out.has_area = true;
                out.min_x = x0, out.min_y = y0, out.max_x = x1, out.max_y = y1;
                if (area_value(m115, max_at, max_end, "z", z1) && z1 > 0)
                    out.max_z = z1;
            }
        }
    }
    // M503, in the units it uses (G20 inches, G21 mm).
    double unit = 1.0;
    std::istringstream lines(m503);
    std::string line;
    const auto set = [&](const char* name, const std::map<char, double>& p, char key) {
        if (const auto it = p.find(key); it != p.end() && it->second > 0.0)
            out.values[name] = it->second * unit;
    };
    while (std::getline(lines, line)) {
        if (command_at(line, "G20") != std::string::npos)
            unit = 25.4;
        else if (command_at(line, "G21") != std::string::npos)
            unit = 1.0;
        std::size_t at;
        if ((at = command_at(line, "M203")) != std::string::npos) {
            const auto p = parameters(line, at + 4);
            set("max_feed_x", p, 'X'), set("max_feed_y", p, 'Y'), set("max_feed_z", p, 'Z'), set("max_feed_e", p, 'E');
        } else if ((at = command_at(line, "M201")) != std::string::npos) {
            const auto p = parameters(line, at + 4);
            set("max_accel_x", p, 'X'), set("max_accel_y", p, 'Y'), set("max_accel_z", p, 'Z'),
                set("max_accel_e", p, 'E');
        } else if ((at = command_at(line, "M204")) != std::string::npos) {
            const auto p = parameters(line, at + 4);
            set("acceleration", p, 'P'), set("retract_acceleration", p, 'R'), set("travel_acceleration", p, 'T');
        } else if ((at = command_at(line, "M205")) != std::string::npos) {
            const auto p = parameters(line, at + 4);
            set("jerk_x", p, 'X'), set("jerk_y", p, 'Y'), set("jerk_z", p, 'Z'), set("jerk_e", p, 'E');
            set("junction_deviation", p, 'J'), set("min_extrude_feed", p, 'S'), set("min_travel_feed", p, 'T');
        } else if ((at = command_at(line, "M900")) != std::string::npos) {
            const auto p = parameters(line, at + 4);
            if (const auto it = p.find('K'); it != p.end() && it->second >= 0.0) {
                out.has_pressure_advance = true;
                out.pressure_advance = it->second;
            }
        }
    }
    return out;
}

PrinterLimits parse_klipper_limits(const nlohmann::json& toolhead, const nlohmann::json& settings) {
    PrinterLimits out;
    out.source = "Klipper";
    const auto number = [](const nlohmann::json& j, const char* key, double& v) {
        if (!j.is_object() || !j.contains(key) || !j[key].is_number())
            return false;
        v = j[key].get<double>();
        return v > 0.0;
    };
    double v = 0;
    if (number(toolhead, "max_velocity", v))
        out.values["max_feed_x"] = out.values["max_feed_y"] = v;
    if (number(toolhead, "max_accel", v)) {
        out.values["max_accel_x"] = out.values["max_accel_y"] = v;
        out.values["acceleration"] = out.values["travel_acceleration"] = out.values["retract_acceleration"] = v;
    }
    if (number(toolhead, "square_corner_velocity", v))
        out.values["corner_speed"] = v;
    if (toolhead.is_object() && toolhead.contains("axis_minimum") && toolhead.contains("axis_maximum") &&
        toolhead["axis_minimum"].is_array() && toolhead["axis_maximum"].is_array() && toolhead["axis_minimum"].size() >= 3 &&
        toolhead["axis_maximum"].size() >= 3) {
        const auto& lo = toolhead["axis_minimum"];
        const auto& hi = toolhead["axis_maximum"];
        if (lo[0].is_number() && lo[1].is_number() && hi[0].is_number() && hi[1].is_number() && hi[2].is_number() &&
            hi[0].get<double>() > lo[0].get<double>() && hi[1].get<double>() > lo[1].get<double>()) {
            out.has_area = true;
            out.min_x = lo[0].get<double>(), out.min_y = lo[1].get<double>();
            out.max_x = hi[0].get<double>(), out.max_y = hi[1].get<double>(), out.max_z = hi[2].get<double>();
        }
    }
    // Klipper's configuration keeps the values as written in printer.cfg: numbers, or numbers as text.
    const auto config = [](const nlohmann::json& s, const char* section, const char* key, double& out_v) {
        if (!s.is_object() || !s.contains(section) || !s[section].is_object() || !s[section].contains(key))
            return false;
        const nlohmann::json& j = s[section][key];
        if (j.is_number())
            out_v = j.get<double>();
        else if (j.is_string())
            out_v = std::atof(j.get<std::string>().c_str());
        else
            return false;
        return true;
    };
    if (config(settings, "printer", "max_z_velocity", v) && v > 0.0)
        out.values["max_feed_z"] = v;
    if (config(settings, "printer", "max_z_accel", v) && v > 0.0)
        out.values["max_accel_z"] = v;
    if (config(settings, "extruder", "pressure_advance", v) && v >= 0.0) {
        out.has_pressure_advance = true;
        out.pressure_advance = v;
    }
    return out;
}

std::vector<std::string> apply_limits(Settings& settings, const PrinterLimits& limits, bool only_empty) {
    std::vector<std::string> changed;
    for (const auto& [name, value] : limits.values) {
        double* field = limit_named(settings, name);
        if (!field || (only_empty && *field > 0.0) || *field == value)
            continue;
        *field = value;
        changed.push_back(name);
    }
    if (limits.has_area && (!only_empty || settings.bed.empty())) {
        const std::vector<std::array<double, 2>> bed = {
            {limits.min_x, limits.min_y}, {limits.max_x, limits.min_y}, {limits.max_x, limits.max_y}, {limits.min_x, limits.max_y}};
        if (settings.bed != bed) {
            settings.bed = bed;
            changed.push_back("bed");
        }
    }
    if (limits.max_z > 0.0 && (!only_empty || settings.max_height <= 0.0) && settings.max_height != limits.max_z) {
        settings.max_height = limits.max_z;
        changed.push_back("max_height");
    }
    if (limits.has_pressure_advance && (!only_empty || settings.pressure_advance <= 0.0) &&
        settings.pressure_advance != limits.pressure_advance) {
        settings.pressure_advance = limits.pressure_advance;
        changed.push_back("pressure_advance");
    }
    return changed;
}

} // namespace tisma::device
