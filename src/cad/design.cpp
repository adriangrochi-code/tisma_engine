// The Design tab's designs as SolidCraft command scripts. Own design, over
// SolidCraft's public command names (its docs/scripts.md and command
// reference, read 2026-11-02); see PROVENANCE.md.

#include "cad/design.hpp"

#include "cad/constraints.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <stdexcept>

namespace tisma::cad {

using json = nlohmann::ordered_json;

std::size_t size_count(Primitive kind) {
    switch (kind) {
    case Primitive::Box: return 3;
    case Primitive::Cylinder: return 2;
    case Primitive::Sphere: return 1;
    case Primitive::Cone: return 3;
    case Primitive::Tube: return 3;
    case Primitive::Torus: return 2;
    }
    return 0;
}

std::array<double, kMaxSizes> default_sizes(Primitive kind) {
    switch (kind) {
    case Primitive::Box: return {20.0, 20.0, 20.0};
    case Primitive::Cylinder: return {20.0, 20.0, 0.0};
    case Primitive::Sphere: return {20.0, 0.0, 0.0};
    case Primitive::Cone: return {20.0, 0.0, 20.0};
    case Primitive::Tube: return {20.0, 12.0, 20.0};
    case Primitive::Torus: return {30.0, 8.0, 0.0};
    }
    return {};
}

bool sizes_ok(Primitive kind, const std::array<double, kMaxSizes>& s) {
    const auto positive = [](double v) { return std::isfinite(v) && v > 0.0; };
    switch (kind) {
    case Primitive::Box: return positive(s[0]) && positive(s[1]) && positive(s[2]);
    case Primitive::Cylinder: return positive(s[0]) && positive(s[1]);
    case Primitive::Sphere: return positive(s[0]);
    case Primitive::Cone: return positive(s[0]) && positive(s[2]) && std::isfinite(s[1]) && s[1] >= 0.0 && s[1] <= s[0];
    case Primitive::Tube: return positive(s[0]) && positive(s[1]) && positive(s[2]) && s[1] < s[0];
    case Primitive::Torus: return positive(s[0]) && positive(s[1]) && s[1] < s[0];
    }
    return false;
}

const char* command_of(Primitive kind) {
    switch (kind) {
    case Primitive::Box: return "solid.box";
    case Primitive::Cylinder: return "solid.cylinder";
    case Primitive::Sphere: return "solid.sphere";
    case Primitive::Cone: return "solid.cone";
    case Primitive::Tube: return "solid.cylinder"; // a cylinder with a cylinder cut out
    case Primitive::Torus: return "solid.torus";
    }
    return "";
}

bool Feature::buildable() const {
    return type == Type::Sketch ? sketch.has_region() : sizes_ok(kind, sizes);
}

bool Design::has_body() const {
    return std::any_of(features.begin(), features.end(), [](const Feature& f) { return !f.suppressed && f.buildable(); });
}

std::string Design::free_name(const std::string& base) const {
    const auto taken = [&](const std::string& candidate) {
        return std::any_of(features.begin(), features.end(), [&](const Feature& f) { return f.name == candidate; });
    };
    if (!taken(base))
        return base;
    for (int n = 2;; ++n)
        if (const std::string candidate = base + " " + std::to_string(n); !taken(candidate))
            return candidate;
}

const char* quality_name(MeshQuality q) {
    return q == MeshQuality::Finest ? "finest" : q == MeshQuality::Fine ? "fine" : "standard";
}

MeshQuality quality_for(double chord_tolerance, double size) {
    if (!(chord_tolerance > 0.0) || !(size > 0.0))
        return MeshQuality::Standard;
    if (2e-4 * size <= chord_tolerance)
        return MeshQuality::Standard;
    if (5e-5 * size <= chord_tolerance)
        return MeshQuality::Fine;
    return MeshQuality::Finest;
}

double nominal_size(const Design& design) {
    double size = 0.0;
    for (const Feature& f : design.features) {
        if (f.suppressed || !f.buildable())
            continue;
        if (f.type == Feature::Type::Sketch) {
            double lo_x = 0.0, hi_x = 0.0, lo_y = 0.0, hi_y = 0.0;
            bool any = false;
            for (const Entity& e : f.sketch.entities)
                for (std::size_t k = 0; k < (e.kind == Entity::Kind::Rectangle ? 4 : e.points.size()); ++k) {
                    const Pt p = point_of(e, k);
                    const double r = e.kind == Entity::Kind::Circle ? e.radius : 0.0;
                    lo_x = any ? std::min(lo_x, p.x - r) : p.x - r, hi_x = any ? std::max(hi_x, p.x + r) : p.x + r;
                    lo_y = any ? std::min(lo_y, p.y - r) : p.y - r, hi_y = any ? std::max(hi_y, p.y + r) : p.y + r;
                    any = true;
                }
            size = std::max({size, hi_x - lo_x, hi_y - lo_y,
                             f.operation.kind == SketchOperation::Kind::Extrude ? f.operation.distance : 0.0});
            continue;
        }
        for (std::size_t i = 0; i < size_count(f.kind); ++i)
            size = std::max(size, f.sizes[i]);
    }
    return size;
}

namespace {

json command(const char* name, json params) {
    return json{{"command", name}, {"params", std::move(params)}};
}

// The commands of one feature: the body stands on z = 0, centred in X and Y.
void feature_commands(const Feature& f, std::vector<json>& out) {
    if (f.type == Feature::Type::Sketch) {
        for (const std::string& c : sketch_commands(f.sketch, f.operation, f.name))
            out.push_back(json::parse(c));
        return;
    }
    const auto& s = f.sizes;
    switch (f.kind) {
    case Primitive::Box:
        out.push_back(command("solid.box", {{"length", s[0]},
                                            {"width", s[1]},
                                            {"height", s[2]},
                                            {"corner", json::array({-s[0] / 2.0, -s[1] / 2.0, 0.0})},
                                            {"operation", "new"},
                                            {"body_name", f.name},
                                            {"name", f.name}}));
        break;
    case Primitive::Cylinder:
        out.push_back(command("solid.cylinder", {{"diameter", s[0]},
                                                 {"height", s[1]},
                                                 {"base", json::array({0.0, 0.0, 0.0})},
                                                 {"axis", "Z"},
                                                 {"operation", "new"},
                                                 {"body_name", f.name},
                                                 {"name", f.name}}));
        break;
    case Primitive::Sphere:
        out.push_back(command("solid.sphere", {{"diameter", s[0]},
                                               {"center", json::array({0.0, 0.0, s[0] / 2.0})},
                                               {"operation", "new"},
                                               {"body_name", f.name},
                                               {"name", f.name}}));
        break;
    case Primitive::Cone:
        out.push_back(command("solid.cone", {{"radius", s[0] / 2.0},
                                             {"top_radius", s[1] / 2.0},
                                             {"height", s[2]},
                                             {"base", json::array({0.0, 0.0, 0.0})},
                                             {"axis", "Z"},
                                             {"operation", "new"},
                                             {"body_name", f.name},
                                             {"name", f.name}}));
        break;
    case Primitive::Tube:
        out.push_back(command("solid.cylinder", {{"diameter", s[0]},
                                                 {"height", s[2]},
                                                 {"base", json::array({0.0, 0.0, 0.0})},
                                                 {"axis", "Z"},
                                                 {"operation", "new"},
                                                 {"body_name", f.name},
                                                 {"name", f.name}}));
        // The hole: a longer cylinder cut out of this body alone.
        out.push_back(command("solid.cylinder", {{"diameter", s[1]},
                                                 {"height", s[2] + 2.0},
                                                 {"base", json::array({0.0, 0.0, -1.0})},
                                                 {"axis", "Z"},
                                                 {"operation", "cut"},
                                                 {"targets", json::array({f.name})},
                                                 {"name", f.name + " hole"}}));
        break;
    case Primitive::Torus:
        out.push_back(command("solid.torus", {{"major", s[0] / 2.0},
                                              {"minor", s[1] / 2.0},
                                              {"center", json::array({0.0, 0.0, s[1] / 2.0})},
                                              {"axis", "Z"},
                                              {"operation", "new"},
                                              {"body_name", f.name},
                                              {"name", f.name}}));
        break;
    }
}

const char* kind_name(Primitive kind) {
    switch (kind) {
    case Primitive::Box: return "box";
    case Primitive::Cylinder: return "cylinder";
    case Primitive::Sphere: return "sphere";
    case Primitive::Cone: return "cone";
    case Primitive::Tube: return "tube";
    case Primitive::Torus: return "torus";
    }
    return "";
}

} // namespace

Measure target_measure(const Feature& f, const std::string& target) {
    if (f.type == Feature::Type::Primitive) {
        for (std::size_t i = 0; i < size_count(f.kind); ++i)
            if (target == "size" + std::to_string(i))
                return Measure::Length;
        return Measure::None;
    }
    if (target == "distance")
        return Measure::Length;
    if (target == "angle")
        return Measure::Angle;
    // "e<k>.width" and the like: entity k of the sketch, of the right kind.
    if (target.size() > 2 && target[0] == 'e') {
        const std::size_t dot = target.find('.');
        if (dot == std::string::npos)
            return Measure::None;
        char* end = nullptr;
        const unsigned long k = std::strtoul(target.c_str() + 1, &end, 10);
        if (end != target.c_str() + dot || k >= f.sketch.entities.size())
            return Measure::None;
        const std::string what = target.substr(dot + 1);
        const Entity::Kind kind = f.sketch.entities[k].kind;
        if ((kind == Entity::Kind::Rectangle && (what == "width" || what == "height")) ||
            (kind == Entity::Kind::Circle && what == "diameter"))
            return Measure::Length;
    }
    return Measure::None;
}

std::vector<std::string> apply_parameters(Design& design) {
    std::vector<std::string> problems;
    const ParameterValues values = evaluate_parameters(design.parameters);
    for (const auto& [name, error] : values.errors)
        problems.push_back("parameter " + name + ": " + error);
    for (Feature& f : design.features)
        for (const auto& [target, expression] : f.bindings) {
            const Measure wanted = target_measure(f, target);
            const auto fail = [&](const std::string& why) { problems.push_back(f.name + ": " + target + ": " + why); };
            if (wanted == Measure::None) {
                fail("this feature has no such size");
                continue;
            }
            const Evaluation e = evaluate(expression, values.values);
            if (!e.value) {
                fail(e.error);
                continue;
            }
            if (e.value->measure != Measure::None && e.value->measure != wanted) {
                fail(wanted == Measure::Length ? "an angle where a length goes" : "a length where an angle goes");
                continue;
            }
            const double v = e.value->value;
            if (!(v > 0.0) && target != "angle") {
                fail("not above 0");
                continue;
            }
            if (target == "distance") {
                f.operation.distance = v;
            } else if (target == "angle") {
                f.operation.angle = v;
            } else if (target.rfind("size", 0) == 0) {
                f.sizes[std::size_t(target[4] - '0')] = v;
            } else {
                // A rectangle's second corner moves, the first stays; its
                // dimensions follow (rule 5).
                const std::size_t k = std::size_t(std::strtoul(target.c_str() + 1, nullptr, 10));
                set_entity_size(f.sketch, k, target.substr(target.find('.') + 1), v);
            }
        }
    return problems;
}

std::vector<std::string> build_commands(const Design& given) {
    Design design = given;
    apply_parameters(design);
    // The sketches as their constraints place them (rule 5).
    for (Feature& f : design.features)
        if (f.type == Feature::Type::Sketch && !f.sketch.rules.empty())
            solve_sketch(f.sketch);
    std::vector<json> commands;
    for (const Feature& f : design.features)
        if (!f.suppressed && f.buildable())
            feature_commands(f, commands);
    std::vector<std::string> out;
    out.reserve(commands.size());
    for (const json& c : commands)
        out.push_back(c.dump());
    return out;
}

std::string script_json(const Design& given, const std::string& export_3mf, MeshQuality quality) {
    Design design = given;
    apply_parameters(design);
    json commands = json::array();
    for (const Feature& f : design.features)
        if (!f.suppressed && f.buildable()) {
            std::vector<json> own;
            feature_commands(f, own);
            for (json& c : own)
                commands.push_back(std::move(c));
        }
    if (!export_3mf.empty())
        commands.push_back(command("file.export_3mf", {{"path", export_3mf}, {"quality", quality_name(quality)}}));
    return json{{"commands", std::move(commands)}}.dump(1);
}

std::string design_json(const Design& design) {
    json features = json::array();
    for (const Feature& f : design.features) {
        if (f.type == Feature::Type::Sketch) {
            features.push_back({{"name", f.name},
                                {"kind", "sketch"},
                                {"sketch", json::parse(sketch_json(f.sketch, f.operation))},
                                {"suppressed", f.suppressed}});
            if (!f.bindings.empty())
                features.back()["bindings"] = f.bindings;
            continue;
        }
        json sizes = json::array();
        for (std::size_t i = 0; i < size_count(f.kind); ++i)
            sizes.push_back(f.sizes[i]);
        features.push_back({{"name", f.name}, {"kind", kind_name(f.kind)}, {"sizes", sizes}, {"suppressed", f.suppressed}});
        if (!f.bindings.empty())
            features.back()["bindings"] = f.bindings;
    }
    json out{{"format", "tisma-design-1"}, {"name", design.name}, {"features", std::move(features)}};
    if (!design.parameters.empty()) {
        json parameters = json::array();
        for (const Parameter& p : design.parameters)
            parameters.push_back({{"name", p.name}, {"expression", p.expression}});
        out["parameters"] = std::move(parameters);
    }
    return out.dump(1);
}

namespace {

void read_bindings(const json& f, Feature& feature) {
    if (!f.contains("bindings"))
        return;
    const json& b = f.at("bindings");
    if (!b.is_object())
        throw std::runtime_error("feature \"" + feature.name + "\": bindings must be an object");
    for (const auto& [target, expression] : b.items()) {
        if (!expression.is_string())
            throw std::runtime_error("feature \"" + feature.name + "\": a binding is not text");
        feature.bindings[target] = expression.get<std::string>();
    }
}

} // namespace

Design parse_design(const std::string& text) {
    json j;
    try {
        j = json::parse(text);
    } catch (const json::exception& e) {
        throw std::runtime_error(std::string("the design is not JSON: ") + e.what());
    }
    if (!j.is_object() || j.value("format", "") != "tisma-design-1")
        throw std::runtime_error("the design is not a tisma-design-1 document");
    Design d;
    d.name = j.value("name", std::string("Design"));
    for (const json& f : j.value("features", json::array())) {
        if (!f.is_object())
            throw std::runtime_error("a feature is not an object");
        Feature feature;
        feature.name = f.value("name", std::string());
        const std::string kind = f.value("kind", std::string());
        if (kind == "sketch") {
            feature.type = Feature::Type::Sketch;
            parse_sketch(f.value("sketch", json::object()).dump(), feature.sketch, feature.operation);
            feature.suppressed = f.value("suppressed", false);
            read_bindings(f, feature);
            d.features.push_back(std::move(feature));
            continue;
        }
        bool known = false;
        for (const Primitive p : {Primitive::Box, Primitive::Cylinder, Primitive::Sphere, Primitive::Cone, Primitive::Tube, Primitive::Torus})
            if (kind == kind_name(p))
                feature.kind = p, known = true;
        if (!known)
            throw std::runtime_error("unknown feature kind \"" + kind + "\"");
        feature.sizes = default_sizes(feature.kind);
        const json sizes = f.value("sizes", json::array());
        if (!sizes.is_array() || sizes.size() != size_count(feature.kind))
            throw std::runtime_error("feature \"" + feature.name + "\": wrong number of sizes");
        for (std::size_t i = 0; i < sizes.size(); ++i) {
            if (!sizes[i].is_number())
                throw std::runtime_error("feature \"" + feature.name + "\": a size is not a number");
            feature.sizes[i] = sizes[i].get<double>();
        }
        feature.suppressed = f.value("suppressed", false);
        read_bindings(f, feature);
        d.features.push_back(std::move(feature));
    }
    for (const json& p : j.value("parameters", json::array())) {
        if (!p.is_object() || !p.contains("name") || !p.at("name").is_string() || !p.contains("expression") ||
            !p.at("expression").is_string())
            throw std::runtime_error("a parameter needs a name and an expression");
        d.parameters.push_back({p.at("name").get<std::string>(), p.at("expression").get<std::string>()});
    }
    return d;
}

} // namespace tisma::cad
