// Project files (docs/PROJECT.md), on nlohmann/json (MIT). Own design; see
// PROVENANCE.md.

#include "project/project.hpp"

#include "config/profiles.hpp"
#include "io/stl.hpp"

#include <nlohmann/json.hpp>

#include <cmath>
#include <cstdint>
#include <fstream>
#include <sstream>

namespace tisma {
namespace {

using nlohmann::ordered_json;

constexpr std::string_view kMagic = "TISMAPRJ";
constexpr std::uint32_t kVersion = 1;

// A path as UTF-8 text for messages (string() can throw on Windows for
// names the code page cannot hold).
std::string text_of(const std::filesystem::path& path) {
    const std::u8string text = path.u8string();
    return std::string(text.begin(), text.end());
}

[[noreturn]] void fail(const std::string& message) { throw ProjectError("project: " + message); }

void put_le(std::string& out, std::uint64_t value, int bytes) {
    for (int i = 0; i < bytes; ++i)
        out += static_cast<char>((value >> (8 * i)) & 0xFF);
}

std::uint64_t get_le(std::string_view data, std::size_t at, int bytes) {
    std::uint64_t value = 0;
    for (int i = 0; i < bytes; ++i)
        value |= std::uint64_t(static_cast<unsigned char>(data[at + std::size_t(i)])) << (8 * i);
    return value;
}

void put_chunk(std::string& out, std::string_view tag, std::string_view bytes) {
    out += tag;
    put_le(out, bytes.size(), 8);
    out += bytes;
}

std::string read_file(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in)
        fail("cannot open '" + text_of(path) + "'");
    std::ostringstream text;
    text << in.rdbuf();
    return text.str();
}

double number(const ordered_json& j, const std::string& what) {
    if (!j.is_number() || !std::isfinite(j.get<double>()))
        fail(what + ": must be a number");
    return j.get<double>();
}

} // namespace

std::string write_project(const Project& project) {
    ordered_json profiles = ordered_json::object();
    for (const ProfileKind kind : kProfileKinds)
        if (const std::string& name = project.profiles[std::size_t(kind)]; !name.empty())
            profiles[std::string(profile_kind_name(kind))] = name;
    ordered_json models = ordered_json::array();
    std::size_t paintings = 0;
    for (std::size_t i = 0; i < project.models.size(); ++i) {
        const ProjectModel& m = project.models[i];
        models.push_back({{"name", m.name}, {"mesh", i}, {"offset", m.offset}});
        if (!m.painting.empty())
            models.back()["painting"] = paintings++;
        if (m.extruder != 1)
            models.back()["extruder"] = m.extruder;
        if (m.wipe_into_infill)
            models.back()["wipe_into_infill"] = true;
        if (m.wipe_into_object)
            models.back()["wipe_into_object"] = true;
        if (!m.settings.empty())
            models.back()["settings"] = ordered_json::parse(m.settings);
    }
    ordered_json root = {
        {"format", "tisma-project"},
        {"version", kVersion},
        {"settings", ordered_json::parse(settings_json(project.settings))},
        {"profiles", profiles},
        {"models", models},
    };
    // The blends in the file the spec gives them (color_mixing.md, rule 15).
    if (!project.mixes.empty() || !project.filament_colors.empty())
        root["color_mixing"] = ordered_json::parse(color_mixes_json(project.mixes, project.filament_colors));
    if (project.calibration) {
        ordered_json segments = ordered_json::array();
        for (const CalibrationSegment& s : project.calibration->segments)
            segments.push_back({s.z0, s.z1, s.value});
        root["calibration"] = {
            {"kind", project.calibration->kind}, {"unit", project.calibration->unit}, {"segments", segments}};
    }

    std::string out(kMagic);
    put_le(out, kVersion, 4);
    put_chunk(out, "JSON", root.dump(2));
    for (const ProjectModel& m : project.models)
        put_chunk(out, "MESH", stl_binary(m.mesh, m.name));
    for (const ProjectModel& m : project.models)
        if (!m.painting.empty())
            put_chunk(out, "PNT ", painting_to_bytes(m.painting));
    return out;
}

static Project read_project_checked(std::string_view data) {
    if (data.size() < kMagic.size() + 4 || data.substr(0, kMagic.size()) != kMagic)
        fail("not a project file");
    const auto version = get_le(data, kMagic.size(), 4);
    if (version > kVersion)
        fail("version " + std::to_string(version) + " is newer than this program reads");

    std::string_view json_text;
    bool has_json = false;
    std::vector<std::string_view> meshes, paintings;
    for (std::size_t at = kMagic.size() + 4; at < data.size();) {
        if (data.size() - at < 12)
            fail("cut short in a chunk header");
        const std::string_view tag = data.substr(at, 4);
        const std::uint64_t size = get_le(data, at + 4, 8);
        at += 12;
        if (size > data.size() - at)
            fail("the " + std::string(tag) + " chunk is cut short");
        const std::string_view bytes = data.substr(at, std::size_t(size));
        at += std::size_t(size);
        if (!has_json && tag != "JSON")
            fail("the first chunk must be JSON");
        if (tag == "JSON") {
            if (has_json)
                fail("more than one JSON chunk");
            json_text = bytes;
            has_json = true;
        } else if (tag == "MESH") {
            meshes.push_back(bytes);
        } else if (tag == "PNT ") {
            paintings.push_back(bytes);
        }
    }
    if (!has_json)
        fail("no JSON chunk");

    if (!json_depth_ok(json_text))
        fail("the JSON chunk is nested more than 64 levels deep");
    ordered_json root;
    try {
        root = ordered_json::parse(json_text.begin(), json_text.end());
    } catch (const ordered_json::parse_error& e) {
        fail(std::string("the JSON chunk is not valid JSON: ") + e.what());
    }
    if (!root.is_object() || root.value("format", std::string()) != "tisma-project")
        fail("\"format\" must be \"tisma-project\"");

    Project p;
    try {
        p.settings = parse_settings(root.contains("settings") ? root.at("settings").dump() : "{}");
    } catch (const SettingsError& e) {
        fail(e.what());
    }
    if (root.contains("profiles")) {
        const ordered_json& profiles = root.at("profiles");
        if (!profiles.is_object())
            fail("profiles: must be an object");
        for (const auto& [kind_name, name] : profiles.items()) {
            const auto kind = profile_kind_from_name(kind_name);
            if (!kind || !name.is_string())
                fail("profiles." + kind_name + ": must be a printer, filament or print profile's name");
            p.profiles[std::size_t(*kind)] = name.get<std::string>();
        }
    }
    if (root.contains("models")) {
        const ordered_json& models = root.at("models");
        if (!models.is_array())
            fail("models: must be a list");
        // Each model has its own MESH chunk (write_project): one chunk named
        // by many models multiplied its memory (300 references to a 20 000
        // triangle chunk took 200 MB from a 3 kB larger file).
        std::vector<bool> used(meshes.size(), false), painted(paintings.size(), false);
        for (std::size_t i = 0; i < models.size(); ++i) {
            const ordered_json& m = models[i];
            const std::string what = "models[" + std::to_string(i) + "]";
            if (!m.is_object() || !m.contains("mesh") || !m.at("mesh").is_number_unsigned())
                fail(what + ".mesh: must be the number of a MESH chunk");
            const auto index = m.at("mesh").get<std::size_t>();
            if (index >= meshes.size())
                fail(what + ".mesh: there is no MESH chunk " + std::to_string(index));
            if (used[index])
                fail(what + ".mesh: MESH chunk " + std::to_string(index) + " belongs to another model");
            used[index] = true;
            ProjectModel model;
            model.name = m.value("name", std::string());
            try {
                model.mesh = parse_stl(meshes[index]).mesh;
            } catch (const StlError& e) {
                fail(what + ": " + e.what());
            }
            if (m.contains("offset")) {
                const ordered_json& o = m.at("offset");
                if (!o.is_array() || o.size() != 3)
                    fail(what + ".offset: must be [x, y, z]");
                for (std::size_t k = 0; k < 3; ++k)
                    model.offset[k] = number(o[k], what + ".offset");
            }
            if (m.contains("painting")) {
                if (!m.at("painting").is_number_unsigned() || m.at("painting").get<std::size_t>() >= paintings.size())
                    fail(what + ".painting: must be the number of a PNT chunk");
                const auto k = m.at("painting").get<std::size_t>();
                if (painted[k])
                    fail(what + ".painting: PNT chunk " + std::to_string(k) + " belongs to another model");
                painted[k] = true;
                try {
                    model.painting = painting_from_bytes(paintings[k], model.mesh.triangle_count());
                } catch (const std::invalid_argument& e) {
                    fail(what + ".painting: " + e.what());
                }
            }
            if (m.contains("extruder")) {
                if (!m.at("extruder").is_number_integer() || m.at("extruder").get<int>() < 1 ||
                    m.at("extruder").get<int>() > 255)
                    fail(what + ".extruder: must be from 1 to 255");
                model.extruder = m.at("extruder").get<int>();
            }
            for (const auto& [key, flag] : {std::pair{"wipe_into_infill", &model.wipe_into_infill},
                                            std::pair{"wipe_into_object", &model.wipe_into_object}})
                if (m.contains(key)) {
                    if (!m.at(key).is_boolean())
                        fail(what + "." + key + ": must be true or false");
                    *flag = m.at(key).get<bool>();
                }
            if (m.contains("settings")) {
                if (!m.at("settings").is_object())
                    fail(what + ".settings: must be an object of groups");
                model.settings = m.at("settings").dump();
                try {
                    own_settings(p.settings, model.settings, model.name);
                } catch (const SettingsError& e) {
                    fail(what + ".settings: " + e.what());
                }
            }
            p.models.push_back(std::move(model));
        }
    }
    if (root.contains("color_mixing")) {
        const ordered_json& c = root.at("color_mixing");
        try {
            p.mixes = parse_color_mixes(c.dump());
        } catch (const ColorMixError& e) {
            fail(e.what());
        }
        if (c.is_object() && c.contains("physical_extruders") && c.at("physical_extruders").is_array())
            for (const ordered_json& f : c.at("physical_extruders"))
                p.filament_colors.push_back(f.is_object() && f.contains("color") && f.at("color").is_string()
                                                ? f.at("color").get<std::string>()
                                                : std::string());
    }
    if (root.contains("calibration")) {
        const ordered_json& c = root.at("calibration");
        if (!c.is_object() || !c.contains("segments") || !c.at("segments").is_array())
            fail("calibration: must have a list of segments");
        ProjectCalibration cal;
        cal.kind = c.value("kind", std::string());
        cal.unit = c.value("unit", std::string());
        for (const ordered_json& s : c.at("segments")) {
            if (!s.is_array() || s.size() != 3)
                fail("calibration.segments: each must be [z0, z1, value]");
            cal.segments.push_back({number(s[0], "calibration.segments"), number(s[1], "calibration.segments"),
                                    number(s[2], "calibration.segments")});
        }
        p.calibration = std::move(cal);
    }
    return p;
}

Project read_project(std::string_view data) {
    try {
        return read_project_checked(data);
    } catch (const nlohmann::json::exception& e) {
        fail(std::string("not a valid project: ") + e.what());
    }
}

void save_project(const std::filesystem::path& path, const Project& project) {
    const std::string bytes = write_project(project);
    std::filesystem::path temporary = path;
    temporary += ".tmp";
    {
        std::ofstream out(temporary, std::ios::binary);
        out.write(bytes.data(), std::streamsize(bytes.size()));
        if (!out)
            fail("cannot write '" + text_of(temporary) + "'");
    }
    std::error_code ec;
    std::filesystem::rename(temporary, path, ec);
    if (ec) {
        std::filesystem::remove(temporary, ec);
        fail("cannot write '" + text_of(path) + "'");
    }
}

Project load_project(const std::filesystem::path& path) { return read_project(read_file(path)); }

} // namespace tisma
