// Own design; see PROVENANCE.md.

#include "project/engineering.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <map>

namespace tisma {

namespace {

using nlohmann::ordered_json;

// The version of this JSON: a newer one is not read (rule 11 bis).
constexpr int kEngineeringVersion = 1;

const std::map<std::string, ConditionKind>& kinds() {
    static const std::map<std::string, ConditionKind> k = {{"fixed_point", ConditionKind::FixedPoint},
                                                           {"fixed_zone", ConditionKind::FixedZone},
                                                           {"force_point", ConditionKind::ForcePoint},
                                                           {"force_zone", ConditionKind::ForceZone},
                                                           {"limit", ConditionKind::Limit}};
    return k;
}

[[noreturn]] void fail(const std::string& what) { throw EngineeringError("engineering: " + what); }

double finite(const ordered_json& j, const std::string& key, const std::string& at) {
    if (!j.contains(key) || !j.at(key).is_number() || !std::isfinite(j.at(key).get<double>()))
        fail(at + "." + key + ": must be a number");
    return j.at(key).get<double>();
}

std::array<double, 3> triple(const ordered_json& j, const std::string& key, const std::string& at) {
    if (!j.contains(key) || !j.at(key).is_array() || j.at(key).size() != 3)
        fail(at + "." + key + ": must be [x, y, z]");
    std::array<double, 3> v{};
    for (std::size_t i = 0; i < 3; ++i) {
        if (!j.at(key)[i].is_number() || !std::isfinite(j.at(key)[i].get<double>()))
            fail(at + "." + key + ": must be [x, y, z]");
        v[i] = j.at(key)[i].get<double>();
    }
    return v;
}

std::string text(const ordered_json& j, const std::string& key, const std::string& fallback, const std::string& at) {
    if (!j.contains(key))
        return fallback;
    if (!j.at(key).is_string())
        fail(at + "." + key + ": must be text");
    return j.at(key).get<std::string>();
}

bool flag(const ordered_json& j, const std::string& key, const std::string& at) {
    if (!j.contains(key))
        return false;
    if (!j.at(key).is_boolean())
        fail(at + "." + key + ": must be true or false");
    return j.at(key).get<bool>();
}

} // namespace

const char* condition_kind_name(ConditionKind kind) {
    for (const auto& [name, k] : kinds())
        if (k == kind)
            return name.c_str();
    return "fixed_zone";
}

bool ProjectEngineering::empty() const {
    return conditions.empty() && results.empty() && overrides == EngineeringOverrides{} && working_temperature == 23.0 &&
           structural == StructuralOptions{} && aerodynamics == AerodynamicsInputs{};
}

std::string engineering_json(const ProjectEngineering& e) {
    ordered_json conditions = ordered_json::array();
    for (const EngineeringCondition& c : e.conditions) {
        ordered_json item = {{"kind", condition_kind_name(c.kind)}, {"number", c.number}};
        if (c.hidden)
            item["hidden"] = true;
        if (c.is_point())
            item["point"] = c.point, item["normal"] = c.normal, item["radius"] = c.radius;
        else
            item["triangles"] = c.triangles;
        if (c.is_force()) {
            item["newtons"] = c.newtons;
            if (c.kilograms)
                item["kilograms"] = *c.kilograms;
            if (c.direction == ForceDirection::Normal)
                item["direction"] = "normal";
            else if (c.direction == ForceDirection::Axis)
                item["direction"] = std::string(c.axis < 0 ? "-" : "+") + "xyz"[std::abs(c.axis) - 1];
            else
                item["direction"] = c.vector;
        }
        if (c.kind == ConditionKind::Limit) {
            item["limit"] = c.limit;
            item["limit_unit"] = c.limit_percent ? "%" : "mm";
        }
        conditions.push_back(std::move(item));
    }
    ordered_json overrides = ordered_json::object();
    if (e.overrides.material)
        overrides["material"] = *e.overrides.material;
    if (e.overrides.extrusion_temperature)
        overrides["extrusion_temperature"] = *e.overrides.extrusion_temperature;
    if (e.overrides.layer_height)
        overrides["layer_height"] = *e.overrides.layer_height;
    if (e.overrides.first_layer_height)
        overrides["first_layer_height"] = *e.overrides.first_layer_height;
    if (e.overrides.line_width)
        overrides["line_width"] = *e.overrides.line_width;
    const ordered_json root = {
        {"version", kEngineeringVersion},
        {"conditions", conditions},
        {"overrides", overrides},
        {"working_temperature", e.working_temperature},
        {"structural",
         {{"safety_factor", e.structural.safety_factor},
          {"quality", e.structural.quality},
          {"print_direction", e.structural.print_direction},
          {"cloud", e.structural.cloud}}},
        {"aerodynamics",
         {{"direction", e.aerodynamics.direction},
          {"speed", e.aerodynamics.speed},
          {"quality", e.aerodynamics.quality},
          {"cloud", e.aerodynamics.cloud}}},
    };
    return root.dump();
}

ProjectEngineering engineering_from_json(const std::string& json) {
    ordered_json root;
    try {
        root = ordered_json::parse(json);
    } catch (const nlohmann::json::exception& e) {
        fail(e.what());
    }
    if (!root.is_object())
        fail("must be an object");
    if (!root.contains("version") || !root.at("version").is_number_integer())
        fail("no version");
    if (root.at("version").get<int>() > kEngineeringVersion)
        fail("version " + std::to_string(root.at("version").get<int>()) + " is newer than this program reads");
    ProjectEngineering e;
    if (root.contains("conditions")) {
        if (!root.at("conditions").is_array())
            fail("conditions: must be a list");
        for (std::size_t k = 0; k < root.at("conditions").size(); ++k) {
            const ordered_json& j = root.at("conditions")[k];
            const std::string at = "conditions[" + std::to_string(k) + "]";
            if (!j.is_object())
                fail(at + ": must be an object");
            EngineeringCondition c;
            const std::string kind = text(j, "kind", "", at);
            const auto it = kinds().find(kind);
            if (it == kinds().end())
                fail(at + ".kind: unknown '" + kind + "'");
            c.kind = it->second;
            if (!j.contains("number") || !j.at("number").is_number_integer() || j.at("number").get<int>() < 1)
                fail(at + ".number: must be a whole number from 1");
            c.number = j.at("number").get<int>();
            c.hidden = flag(j, "hidden", at);
            if (c.is_point()) {
                c.point = triple(j, "point", at);
                c.normal = triple(j, "normal", at);
                c.radius = finite(j, "radius", at);
                if (!(c.radius > 0.0))
                    fail(at + ".radius: must be more than 0");
            } else {
                if (!j.contains("triangles") || !j.at("triangles").is_array())
                    fail(at + ".triangles: must be a list");
                for (const ordered_json& t : j.at("triangles")) {
                    if (!t.is_number_unsigned() || t.get<std::uint64_t>() > 0xFFFFFFFFu)
                        fail(at + ".triangles: must be triangle numbers");
                    c.triangles.push_back(t.get<std::uint32_t>());
                }
            }
            if (c.is_force()) {
                c.newtons = finite(j, "newtons", at);
                if (j.contains("kilograms"))
                    c.kilograms = finite(j, "kilograms", at);
                const ordered_json& d = j.contains("direction") ? j.at("direction") : ordered_json("normal");
                if (d.is_string() && d.get<std::string>() == "normal") {
                    c.direction = ForceDirection::Normal;
                } else if (d.is_string()) {
                    const std::string s = d.get<std::string>();
                    if (s.size() != 2 || (s[0] != '+' && s[0] != '-') || s[1] < 'x' || s[1] > 'z')
                        fail(at + ".direction: must be normal, +x to -z, or [x, y, z]");
                    c.direction = ForceDirection::Axis;
                    c.axis = (s[1] - 'x' + 1) * (s[0] == '-' ? -1 : 1);
                } else {
                    c.direction = ForceDirection::Vector;
                    c.vector = triple(j, "direction", at);
                }
            }
            if (c.kind == ConditionKind::Limit) {
                c.limit = finite(j, "limit", at);
                const std::string unit = text(j, "limit_unit", "mm", at);
                if (unit != "mm" && unit != "%")
                    fail(at + ".limit_unit: must be mm or %");
                c.limit_percent = unit == "%";
            }
            e.conditions.push_back(std::move(c));
        }
    }
    if (root.contains("overrides")) {
        const ordered_json& o = root.at("overrides");
        if (!o.is_object())
            fail("overrides: must be an object");
        if (o.contains("material"))
            e.overrides.material = text(o, "material", "", "overrides");
        if (o.contains("extrusion_temperature"))
            e.overrides.extrusion_temperature = finite(o, "extrusion_temperature", "overrides");
        if (o.contains("layer_height"))
            e.overrides.layer_height = finite(o, "layer_height", "overrides");
        if (o.contains("first_layer_height"))
            e.overrides.first_layer_height = finite(o, "first_layer_height", "overrides");
        if (o.contains("line_width"))
            e.overrides.line_width = finite(o, "line_width", "overrides");
    }
    if (root.contains("working_temperature"))
        e.working_temperature = finite(root, "working_temperature", "engineering");
    if (root.contains("structural")) {
        const ordered_json& s = root.at("structural");
        if (!s.is_object())
            fail("structural: must be an object");
        if (s.contains("safety_factor"))
            e.structural.safety_factor = finite(s, "safety_factor", "structural");
        e.structural.quality = text(s, "quality", e.structural.quality, "structural");
        e.structural.print_direction = text(s, "print_direction", e.structural.print_direction, "structural");
        e.structural.cloud = flag(s, "cloud", "structural");
    }
    if (root.contains("aerodynamics")) {
        const ordered_json& a = root.at("aerodynamics");
        if (!a.is_object())
            fail("aerodynamics: must be an object");
        e.aerodynamics.direction = text(a, "direction", e.aerodynamics.direction, "aerodynamics");
        if (a.contains("speed"))
            e.aerodynamics.speed = finite(a, "speed", "aerodynamics");
        e.aerodynamics.quality = text(a, "quality", e.aerodynamics.quality, "aerodynamics");
        e.aerodynamics.cloud = flag(a, "cloud", "aerodynamics");
    }
    return e;
}

std::string engineering_fingerprint(const TriangleMesh& mesh, const ProjectEngineering& e, std::string_view settings) {
    std::uint64_t h = 1469598103934665603ULL;
    const auto add = [&](const void* data, std::size_t size) {
        const auto* bytes = static_cast<const unsigned char*>(data);
        for (std::size_t i = 0; i < size; ++i) {
            h ^= bytes[i];
            h *= 1099511628211ULL;
        }
    };
    // Where the part stands does not count (moving it keeps its result,
    // FM24): its corners and points from its box's corner, in micrometres.
    const BoundingBox box = mesh.bounding_box();
    const double origin[3] = {box.min.x, box.min.y, box.min.z};
    const auto micrometres = [](double v) { return static_cast<long long>(std::llround(v * 1000.0)); };
    for (const Vec3f& q : mesh.corners()) {
        const long long c[3] = {micrometres(q.x - origin[0]), micrometres(q.y - origin[1]), micrometres(q.z - origin[2])};
        add(c, sizeof c);
    }
    ProjectEngineering inputs = e;
    inputs.results.clear();
    for (EngineeringCondition& c : inputs.conditions)
        if (c.is_point())
            for (std::size_t a = 0; a < 3; ++a)
                c.point[a] = double(micrometres(c.point[a] - origin[a])) / 1000.0;
    const std::string json = engineering_json(inputs);
    add(json.data(), json.size());
    add(settings.data(), settings.size());
    char text[17];
    std::snprintf(text, sizeof text, "%016llx", static_cast<unsigned long long>(h));
    return text;
}

bool result_stale(const EngineeringResult& r, const TriangleMesh& mesh, const ProjectEngineering& e,
                  std::string_view settings) {
    return r.fingerprint != engineering_fingerprint(mesh, e, settings);
}

std::size_t drop_conditions_beyond(ProjectEngineering& e, std::size_t triangles) {
    const auto before = e.conditions.size();
    std::erase_if(e.conditions, [&](const EngineeringCondition& c) {
        return !c.is_point() && std::any_of(c.triangles.begin(), c.triangles.end(),
                                            [&](std::uint32_t t) { return t >= triangles; });
    });
    return before - e.conditions.size();
}

void move_engineering(ProjectEngineering& e, const std::array<double, 9>& a, const std::array<double, 3>& b) {
    // The cofactor matrix: det(a) times the inverse transpose, by rows.
    const std::array<double, 9> c{a[4] * a[8] - a[5] * a[7], a[5] * a[6] - a[3] * a[8], a[3] * a[7] - a[4] * a[6],
                                  a[2] * a[7] - a[1] * a[8], a[0] * a[8] - a[2] * a[6], a[1] * a[6] - a[0] * a[7],
                                  a[1] * a[5] - a[2] * a[4], a[2] * a[3] - a[0] * a[5], a[0] * a[4] - a[1] * a[3]};
    const double det = a[0] * c[0] + a[1] * c[1] + a[2] * c[2];
    for (EngineeringCondition& k : e.conditions) {
        const std::array<double, 3> p = k.point, n = k.normal;
        std::array<double, 3> m{};
        for (std::size_t r = 0; r < 3; ++r) {
            k.point[r] = a[3 * r] * p[0] + a[3 * r + 1] * p[1] + a[3 * r + 2] * p[2] + b[r];
            m[r] = (c[3 * r] * n[0] + c[3 * r + 1] * n[1] + c[3 * r + 2] * n[2]) * (det < 0.0 ? -1.0 : 1.0);
        }
        const double len = std::sqrt(m[0] * m[0] + m[1] * m[1] + m[2] * m[2]);
        if (len > 0.0)
            k.normal = {m[0] / len, m[1] / len, m[2] / len};
    }
}

} // namespace tisma
