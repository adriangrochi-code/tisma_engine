#include "slice/belt.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <optional>
#include <sstream>

namespace tisma {
namespace {

double radians(const Settings& s) {
    return s.belt_angle * std::numbers::pi / 180.0;
}

void put(std::string& out, char axis, double v) {
    char text[32];
    const auto [end, ec] = std::to_chars(text, text + sizeof text, std::round(v * 1000.0) / 1000.0,
                                         std::chars_format::fixed, 3);
    out.push_back(' ');
    out.push_back(axis);
    if (ec == std::errc())
        out.append(text, end);
}

} // namespace

IndexedMesh belt_mesh(const IndexedMesh& mesh, const Settings& s) {
    IndexedMesh out = mesh;
    if (out.vertices.empty())
        return out;
    double y0 = 1e300, z0 = 1e300;
    for (const Vec3f& v : out.vertices)
        y0 = std::min(y0, double(v.y)), z0 = std::min(z0, double(v.z));
    const double a = radians(s), sin_a = std::sin(a), cos_a = std::cos(a);
    for (Vec3f& v : out.vertices) {
        const double y = double(v.y) - y0, z = double(v.z) - z0;
        v.y = float(z * sin_a - y * cos_a);
        v.z = float(y * sin_a + z * cos_a);
    }
    return out;
}

std::array<double, 3> belt_axes(const Settings& s, double x, double v, double layer) {
    const double a = radians(s);
    return {x, layer / std::sin(a), v + layer / std::tan(a)};
}

std::string belt_gcode(std::string_view text, const Settings& s) {
    std::string out;
    out.reserve(text.size() + text.size() / 8);
    // The engine's moves: from the first layer to the end G-code (the last
    // ;TYPE:Custom, which the user's end G-code follows).
    const std::size_t first = text.find(";LAYER_CHANGE\n");
    const std::size_t last = text.rfind("\n;TYPE:Custom\n");
    if (first == std::string_view::npos) {
        out.append(text);
        return out;
    }
    const std::size_t stop = last != std::string_view::npos && last > first ? last : text.size();
    out.append(text.substr(0, first));
    double v = 0.0, layer = 0.0;
    bool known_v = false, known_s = false;
    std::optional<double> belt_out, gantry_out; // what was last written
    std::size_t at = first;
    while (at < stop) {
        std::size_t end = text.find('\n', at);
        if (end == std::string_view::npos || end > stop)
            end = stop;
        const std::string_view line = text.substr(at, end - at);
        at = end + 1;
        const bool move = line.size() > 2 && line[0] == 'G' && (line[1] == '1' || line[1] == '0') && line[2] == ' ';
        if (!move) {
            out.append(line).push_back('\n');
            continue;
        }
        const std::size_t semicolon = line.find(';');
        const std::string_view words =
            line.substr(3, semicolon == std::string_view::npos ? std::string_view::npos : semicolon - 3);
        std::string rest; // the other words, as they were
        std::optional<double> nx, ny, nz;
        std::istringstream in{std::string(words)};
        for (std::string w; in >> w;) {
            const double value = std::atof(w.c_str() + 1);
            if (w[0] == 'X')
                nx = value;
            else if (w[0] == 'Y')
                ny = value;
            else if (w[0] == 'Z')
                nz = value;
            else
                rest += " " + w;
        }
        if (ny)
            v = *ny, known_v = true;
        if (nz)
            layer = *nz, known_s = true;
        out.append(line.substr(0, 2));
        if (nx)
            put(out, 'X', *nx);
        // The gantry once both are known (the first layer's Z comes before
        // its first point: the belt goes there alone).
        const char belt = s.belt_is_z ? 'Z' : 'Y', gantry = s.belt_is_z ? 'Y' : 'Z';
        const std::array<double, 3> m = belt_axes(s, 0.0, v, layer);
        if ((ny || nz) && known_v && known_s && (!gantry_out || std::abs(*gantry_out - m[2]) > 5e-4))
            put(out, gantry, m[2]), gantry_out = m[2];
        if (nz && (!belt_out || std::abs(*belt_out - m[1]) > 5e-4))
            put(out, belt, m[1]), belt_out = m[1];
        out.append(rest);
        if (semicolon != std::string_view::npos)
            out.append(" ").append(line.substr(semicolon));
        out.push_back('\n');
    }
    if (stop < text.size())
        out.append(text.substr(stop));
    return out;
}

void belt_preview(GcodePreview& preview, const Settings& s, double y0) {
    const double a = radians(s), sin_a = std::sin(a), cos_a = std::cos(a);
    const auto turn = [&](float& y, float& z) {
        const double belt = s.belt_is_z ? z : y, gantry = s.belt_is_z ? y : z;
        y = float(y0 + belt - gantry * cos_a);
        z = float(gantry * sin_a);
    };
    for (PreviewMove& m : preview.moves)
        turn(m.y0, m.z0), turn(m.y1, m.z1);
    if (preview.extrusions.empty())
        return;
    preview.min = {1e30f, 1e30f, 1e30f}, preview.max = {-1e30f, -1e30f, -1e30f};
    for (const std::uint32_t i : preview.extrusions) {
        const PreviewMove& m = preview.moves[i];
        for (const auto& p : {std::array<float, 3>{m.x0, m.y0, m.z0}, std::array<float, 3>{m.x1, m.y1, m.z1}})
            for (std::size_t k = 0; k < 3; ++k)
                preview.min[k] = std::min(preview.min[k], p[k]), preview.max[k] = std::max(preview.max[k], p[k]);
    }
}

} // namespace tisma
