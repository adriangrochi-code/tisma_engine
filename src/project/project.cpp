// Project files (docs/PROJECT.md), on nlohmann/json (MIT). Own design; see
// PROVENANCE.md.

#include "project/project.hpp"

#include "config/profiles.hpp"
#include "io/model.hpp"
#include "io/stl.hpp"
#include "io/zip.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <functional>
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

// A painting in a 3MF: Metadata/tisma_paint_<k>.bin, "TPNT", the version
// (32 bits) and painting_to_bytes. The header keeps the file from starting
// like a program, which the server refuses (a count of 23117 is "MZ").
constexpr std::string_view kPaintMagic = "TPNT";
constexpr std::uint32_t kPaintVersion = 1;

std::string paint_entry(std::size_t k) { return "Metadata/tisma_paint_" + std::to_string(k) + ".bin"; }
// An Engineering result in a 3MF (formats.md, rule 11 bis): its report's JSON.
std::string result_entry(std::size_t k) { return "Metadata/tisma_result_" + std::to_string(k) + ".json"; }

constexpr std::string_view kProjectEntry = "Metadata/tisma_project.json";

} // namespace

// The project's JSON: settings, profiles, each model's data ("mesh": its
// number among the meshes, "painting": among the paintings), mixes and
// calibration. With `offsets` false every model's offset is written as 0
// (a 3MF carries it in its build item).
static ordered_json project_json(const Project& project, bool offsets) {
    ordered_json profiles = ordered_json::object();
    for (const ProfileKind kind : kProfileKinds)
        if (const std::string& name = project.profiles[std::size_t(kind)]; !name.empty())
            profiles[std::string(profile_kind_name(kind))] = name;
    ordered_json models = ordered_json::array();
    std::size_t paintings = 0, results = 0;
    for (std::size_t i = 0; i < project.models.size(); ++i) {
        const ProjectModel& m = project.models[i];
        models.push_back({{"name", m.name}, {"mesh", i}, {"offset", offsets ? m.offset : std::array<double, 3>{}}});
        if (offsets && m.turn != std::array<double, 9>{1, 0, 0, 0, 1, 0, 0, 0, 1})
            models.back()["turn"] = m.turn;
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
        for (const ProjectVolume& v : m.volumes) {
            ordered_json item = {{"kind", v.negative ? "negative" : "modifier"},
                                 {"shape", v.cylinder ? "cylinder" : "box"},
                                 {"offset", v.offset},
                                 {"size", v.size}};
            if (v.turn != std::array<double, 9>{1, 0, 0, 0, 1, 0, 0, 0, 1})
                item["turn"] = v.turn;
            if (!v.settings.empty())
                item["settings"] = ordered_json::parse(v.settings);
            models.back()["volumes"].push_back(std::move(item));
        }
        for (const ProjectRange& r : m.ranges) {
            ordered_json item = {{"from", r.from}, {"to", r.to}};
            if (!r.settings.empty())
                item["settings"] = ordered_json::parse(r.settings);
            models.back()["ranges"].push_back(std::move(item));
        }
        if (!m.engineering.empty()) {
            models.back()["engineering"] = ordered_json::parse(engineering_json(m.engineering));
            // In a 3MF the mesh goes out placed: the conditions' points, in
            // the part's own coordinates, move with it when read (FM24).
            // Read by older versions, which placed a 3MF's mesh and moved
            // the points with it.
            if (!offsets && m.offset != std::array<double, 3>{})
                models.back()["engineering_moved"] = m.offset;
        }
        // Each last result in its own part of the package, in order: an RSLT
        // chunk, or Metadata/tisma_result_<k>.json.
        for (const EngineeringResult& r : m.engineering.results)
            models.back()["engineering_results"].push_back(
                {{"kind", r.kind}, {"fingerprint", r.fingerprint}, {"file", results++}});
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
    return root;
}

std::string write_project(const Project& project) {
    const ordered_json root = project_json(project, true);
    std::string out(kMagic);
    put_le(out, kVersion, 4);
    put_chunk(out, "JSON", root.dump(2));
    for (const ProjectModel& m : project.models)
        put_chunk(out, "MESH", stl_binary(m.mesh, m.name));
    for (const ProjectModel& m : project.models)
        if (!m.painting.empty())
            put_chunk(out, "PNT ", painting_to_bytes(m.painting));
    for (const ProjectModel& m : project.models)
        for (const EngineeringResult& r : m.engineering.results)
            put_chunk(out, "RSLT", r.report);
    return out;
}

// Where a project's meshes and paintings are: the MESH and PNT chunks of the
// engine's own file, or the build items and the paint files of a 3MF.
struct ProjectParts {
    std::size_t meshes = 0, paintings = 0, results = 0;
    std::function<TriangleMesh(std::size_t)> mesh;    // throws StlError or ModelError
    std::function<std::string(std::size_t)> painting; // its bytes (painting_to_bytes)
    std::function<std::string(std::size_t)> result;   // an Engineering result's report
    // A 3MF's build items: each mesh's turn and offset on the bed, the mesh
    // left in the part's own coordinates (formats.md, rules 8 and 11 bis).
    std::vector<std::array<double, 9>> turns;
    std::vector<std::array<double, 3>> offsets;
};

static Project project_from_json(std::string_view json_text, const ProjectParts& source) {
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
        std::vector<bool> used(source.meshes, false), painted(source.paintings, false);
        for (std::size_t i = 0; i < models.size(); ++i) {
            const ordered_json& m = models[i];
            const std::string what = "models[" + std::to_string(i) + "]";
            if (!m.is_object() || !m.contains("mesh") || !m.at("mesh").is_number_unsigned())
                fail(what + ".mesh: must be the number of a MESH chunk");
            const auto index = m.at("mesh").get<std::size_t>();
            if (index >= source.meshes)
                fail(what + ".mesh: there is no MESH chunk " + std::to_string(index));
            if (used[index])
                fail(what + ".mesh: MESH chunk " + std::to_string(index) + " belongs to another model");
            used[index] = true;
            ProjectModel model;
            model.name = m.value("name", std::string());
            try {
                model.mesh = source.mesh(index);
            } catch (const std::runtime_error& e) { // the mesh's StlError or ModelError
                fail(what + ": " + e.what());
            }
            if (m.contains("offset")) {
                const ordered_json& o = m.at("offset");
                if (!o.is_array() || o.size() != 3)
                    fail(what + ".offset: must be [x, y, z]");
                for (std::size_t k = 0; k < 3; ++k)
                    model.offset[k] = number(o[k], what + ".offset");
            }
            if (m.contains("turn")) {
                const ordered_json& t = m.at("turn");
                if (!t.is_array() || t.size() != 9)
                    fail(what + ".turn: must be 9 numbers, by rows");
                for (std::size_t k = 0; k < 9; ++k)
                    model.turn[k] = number(t[k], what + ".turn");
            }
            if (index < source.turns.size()) { // a 3MF: its build item's
                model.turn = source.turns[index];
                model.offset = source.offsets[index];
            }
            if (m.contains("painting")) {
                if (!m.at("painting").is_number_unsigned() || m.at("painting").get<std::size_t>() >= source.paintings)
                    fail(what + ".painting: must be the number of a PNT chunk");
                const auto k = m.at("painting").get<std::size_t>();
                if (painted[k])
                    fail(what + ".painting: PNT chunk " + std::to_string(k) + " belongs to another model");
                painted[k] = true;
                try {
                    model.painting = painting_from_bytes(source.painting(k), model.mesh.triangle_count());
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
            const auto three = [&](const ordered_json& o, const char* key, const std::string& where) {
                std::array<double, 3> a{};
                const auto it = o.find(key);
                if (it == o.end() || !it->is_array() || it->size() != 3)
                    fail(where + "." + key + ": must be [x, y, z] in mm");
                for (std::size_t k = 0; k < 3; ++k) {
                    if (!(*it)[k].is_number())
                        fail(where + "." + key + ": must be [x, y, z] in mm");
                    a[k] = (*it)[k].get<double>();
                }
                return a;
            };
            if (m.contains("volumes")) {
                if (!m.at("volumes").is_array())
                    fail(what + ".volumes: must be a list");
                for (std::size_t k = 0; k < m.at("volumes").size(); ++k) {
                    const ordered_json& v = m.at("volumes")[k];
                    const std::string at = what + ".volumes[" + std::to_string(k) + "]";
                    if (!v.is_object())
                        fail(at + ": must be an object");
                    ProjectVolume volume;
                    const std::string kind = v.value("kind", std::string("modifier")), shape = v.value("shape", std::string("box"));
                    if ((kind != "modifier" && kind != "negative") || (shape != "box" && shape != "cylinder"))
                        fail(at + ": kind must be modifier or negative, shape box or cylinder");
                    volume.negative = kind == "negative";
                    volume.cylinder = shape == "cylinder";
                    volume.offset = three(v, "offset", at);
                    volume.size = three(v, "size", at);
                    if (v.contains("turn")) {
                        const ordered_json& turn = v.at("turn");
                        if (!turn.is_array() || turn.size() != 9 ||
                            !std::all_of(turn.begin(), turn.end(), [](const ordered_json& c) { return c.is_number(); }))
                            fail(at + ".turn: must be 9 numbers (a turn, by rows)");
                        for (std::size_t c = 0; c < 9; ++c)
                            volume.turn[c] = turn[c].get<double>();
                    }
                    if (v.contains("settings")) {
                        if (!v.at("settings").is_object())
                            fail(at + ".settings: must be an object of groups");
                        volume.settings = v.at("settings").dump();
                    }
                    model.volumes.push_back(std::move(volume));
                }
            }
            if (m.contains("ranges")) {
                if (!m.at("ranges").is_array())
                    fail(what + ".ranges: must be a list");
                for (std::size_t k = 0; k < m.at("ranges").size(); ++k) {
                    const ordered_json& r = m.at("ranges")[k];
                    const std::string at = what + ".ranges[" + std::to_string(k) + "]";
                    if (!r.is_object() || !r.contains("from") || !r.contains("to") || !r.at("from").is_number() ||
                        !r.at("to").is_number() || !(r.at("to").get<double>() > r.at("from").get<double>()))
                        fail(at + ": must have from and to in mm, to above from");
                    ProjectRange range{r.at("from").get<double>(), r.at("to").get<double>(), {}};
                    if (r.contains("settings")) {
                        if (!r.at("settings").is_object())
                            fail(at + ".settings: must be an object of groups");
                        range.settings = r.at("settings").dump();
                    }
                    model.ranges.push_back(std::move(range));
                }
            }
            // Engineering data (rule 11 bis): what this program does not read
            // is left out with a notice, never the part.
            if (m.contains("engineering")) {
                try {
                    model.engineering = engineering_from_json(m.at("engineering").dump());
                    // "engineering_moved" is for older versions, which
                    // placed a 3MF's mesh; here the mesh and the points stay
                    // in the part's own coordinates.
                    if (const std::size_t dropped = drop_conditions_beyond(model.engineering, model.mesh.triangle_count()))
                        p.notices.push_back(model.name + ": " + std::to_string(dropped) +
                                            " Engineering condition(s) no longer fit the mesh and were removed");
                } catch (const EngineeringError& e) {
                    p.notices.push_back(model.name + ": its Engineering data was not read (" + e.what() + ")");
                }
            }
            if (m.contains("engineering_results")) {
                const ordered_json& list = m.at("engineering_results");
                bool ok = list.is_array();
                std::vector<EngineeringResult> results;
                for (std::size_t k = 0; ok && k < list.size(); ++k) {
                    const ordered_json& r = list[k];
                    ok = r.is_object() && r.contains("kind") && r.at("kind").is_string() && r.contains("fingerprint") &&
                         r.at("fingerprint").is_string() && r.contains("file") && r.at("file").is_number_unsigned() &&
                         r.at("file").get<std::size_t>() < source.results;
                    if (ok)
                        results.push_back({r.at("kind").get<std::string>(), r.at("fingerprint").get<std::string>(),
                                           source.result(r.at("file").get<std::size_t>())});
                }
                if (ok)
                    model.engineering.results = std::move(results);
                else
                    p.notices.push_back(model.name + ": its last analysis results were not read");
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

static Project read_project_checked(std::string_view data) {
    if (data.size() < kMagic.size() + 4 || data.substr(0, kMagic.size()) != kMagic)
        fail("not a project file");
    const auto version = get_le(data, kMagic.size(), 4);
    if (version > kVersion)
        fail("version " + std::to_string(version) + " is newer than this program reads");

    std::string_view json_text;
    bool has_json = false;
    std::vector<std::string_view> meshes, paintings, results;
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
        } else if (tag == "RSLT") {
            results.push_back(bytes);
        }
    }
    if (!has_json)
        fail("no JSON chunk");

    ProjectParts source;
    source.meshes = meshes.size();
    source.paintings = paintings.size();
    source.mesh = [&](std::size_t i) { return parse_stl(meshes[i]).mesh; };
    source.painting = [&](std::size_t i) { return std::string(paintings[i]); };
    source.results = results.size();
    source.result = [&](std::size_t i) { return std::string(results[i]); };
    return project_from_json(json_text, source);
}

std::string write_project_3mf(const Project& project) {
    const ordered_json root = project_json(project, false);
    std::vector<ThreeMfObject> objects;
    std::vector<ZipEntryToWrite> extra{{std::string(kProjectEntry), root.dump(1)}};
    for (const ProjectModel& m : project.models) {
        objects.push_back({m.name, &m.mesh, m.offset, m.turn});
        if (!m.painting.empty()) {
            std::string bytes(kPaintMagic);
            put_le(bytes, kPaintVersion, 4);
            bytes += painting_to_bytes(m.painting);
            extra.push_back({paint_entry(extra.size() - 1), std::move(bytes)});
        }
    }
    std::size_t results = 0;
    for (const ProjectModel& m : project.models)
        for (const EngineeringResult& r : m.engineering.results)
            extra.push_back({result_entry(results++), r.report});
    return write_3mf_objects(objects, std::move(extra));
}

// A 3MF with the engine's data: each build item, placed, is a model's mesh;
// the JSON says the rest (formats.md, rule 11).
static Project read_project_3mf(std::string_view data) {
    std::optional<std::string> json_text;
    std::vector<ThreeMfItem> items;
    std::vector<std::string> paintings; // Metadata/tisma_paint_0.bin, 1, ... while they are there
    std::vector<std::string> results;   // Metadata/tisma_result_0.json, 1, ... likewise
    try {
        items = read_3mf_items(data, &json_text, kProjectEntry, true);
        const ZipArchive zip(data);
        for (const ZipArchive::Entry* e; (e = zip.find(paint_entry(paintings.size())));) {
            const std::string bytes = zip.read(*e);
            const std::string what = paint_entry(paintings.size());
            if (bytes.size() < 8 || !bytes.starts_with(kPaintMagic))
                fail(what + ": not a painting");
            if (get_le(bytes, 4, 4) > kPaintVersion)
                fail(what + ": newer than this program reads");
            paintings.push_back(bytes.substr(8));
        }
        for (const ZipArchive::Entry* e; (e = zip.find(result_entry(results.size())));)
            results.push_back(zip.read(*e));
    } catch (const ModelError& e) {
        fail(e.what());
    } catch (const ZipError& e) {
        fail(std::string("3MF: ") + e.what());
    }
    if (!json_text)
        fail("this 3MF has no Tisma project data (" + std::string(kProjectEntry) +
             "): projects of other slicers are not read yet");
    ProjectParts source;
    source.meshes = items.size();
    source.paintings = paintings.size();
    source.mesh = [&](std::size_t i) { return std::move(items[i].mesh); };
    source.painting = [&](std::size_t i) { return paintings[i]; };
    for (const ThreeMfItem& item : items) {
        source.turns.push_back(item.turn);
        source.offsets.push_back(item.offset);
    }
    source.results = results.size();
    source.result = [&](std::size_t i) { return results[i]; };
    return project_from_json(*json_text, source);
}

Project read_project(std::string_view data) {
    try {
        if (looks_like_zip(data))
            return read_project_3mf(data);
        return read_project_checked(data);
    } catch (const nlohmann::json::exception& e) {
        fail(std::string("not a valid project: ") + e.what());
    }
}

void save_project(const std::filesystem::path& path, const Project& project) {
    std::string ext = text_of(path.extension());
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return char(std::tolower(c)); });
    const std::string bytes = ext == ".3mf" ? write_project_3mf(project) : write_project(project);
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

bool looks_like_project(std::string_view data) {
    if (data.starts_with(kMagic))
        return true;
    if (!looks_like_zip(data))
        return false;
    try {
        return ZipArchive(data).find(kProjectEntry) != nullptr;
    } catch (const std::exception&) {
        return false;
    }
}

bool is_project_file(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    char head[8] = {};
    in.read(head, sizeof head);
    const std::string_view start(head, std::size_t(in.gcount()));
    if (start.starts_with(kMagic))
        return true;
    if (!start.starts_with("PK"))
        return false;
    try {
        return looks_like_project(read_file(path));
    } catch (const ProjectError&) {
        return false;
    }
}

Project load_project(const std::filesystem::path& path) { return read_project(read_file(path)); }

} // namespace tisma
