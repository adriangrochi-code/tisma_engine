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

namespace {
constexpr std::pair<OperationKind, const char*> kOperationNames[] = {
    {OperationKind::Fillet, "fillet"},
    {OperationKind::Chamfer, "chamfer"},
    {OperationKind::Shell, "shell"},
    {OperationKind::Hole, "hole"},
    {OperationKind::Mirror, "mirror"},
    {OperationKind::PatternCircular, "pattern_circular"},
    {OperationKind::PatternRectangular, "pattern_rectangular"},
    {OperationKind::Combine, "combine"},
    {OperationKind::Thread, "thread"},
    {OperationKind::Move, "move"},
    {OperationKind::Split, "split"},
    {OperationKind::Draft, "draft"},
};
} // namespace

const char* operation_name(OperationKind kind) {
    for (const auto& [k, n] : kOperationNames)
        if (k == kind)
            return n;
    return "";
}

std::optional<OperationKind> operation_named(const std::string& name) {
    for (const auto& [k, n] : kOperationNames)
        if (name == n)
            return k;
    return std::nullopt;
}

const char* operation_command(OperationKind kind) {
    switch (kind) {
    case OperationKind::Fillet: return "solid.fillet";
    case OperationKind::Chamfer: return "solid.chamfer";
    case OperationKind::Shell: return "solid.shell";
    case OperationKind::Hole: return "solid.hole";
    case OperationKind::Mirror: return "solid.mirror";
    case OperationKind::PatternCircular: return "solid.pattern.circular";
    case OperationKind::PatternRectangular: return "solid.pattern.rectangular";
    case OperationKind::Combine: return "solid.combine";
    case OperationKind::Thread: return "solid.thread";
    case OperationKind::Move: return "solid.move";
    case OperationKind::Split: return "solid.split_body";
    case OperationKind::Draft: return "solid.draft";
    }
    return "";
}

bool Feature::buildable() const {
    if (type == Type::Operation) {
        if (op.target.empty())
            return false;
        switch (op.kind) {
        case OperationKind::Fillet:
        case OperationKind::Chamfer: return !op.refs.empty() && op.value > 0.0;
        case OperationKind::Shell: return op.value > 0.0;
        case OperationKind::Hole: return !op.refs.empty() && (!op.standard.empty() || op.value > 0.0) && op.depth >= 0.0;
        case OperationKind::Mirror: return op.plane == "XY" || op.plane == "XZ" || op.plane == "YZ";
        case OperationKind::PatternCircular: return op.count >= 2 && op.angle > 0.0;
        case OperationKind::PatternRectangular:
            return op.count >= 2 && op.spacing > 0.0 &&
                   std::hypot(op.direction[0], op.direction[1], op.direction[2]) > 0.0;
        case OperationKind::Combine: return !op.tools.empty();
        case OperationKind::Thread: return !op.refs.empty() && !op.standard.empty() && op.depth >= 0.0;
        case OperationKind::Move:
            return std::isfinite(op.angle) && (op.angle != 0.0 || op.offset != std::array<double, 3>{0.0, 0.0, 0.0} || op.copy);
        case OperationKind::Split:
        case OperationKind::Draft:
            if (op.plane != "XY" && op.plane != "XZ" && op.plane != "YZ")
                return false;
            return op.kind == OperationKind::Split || (!op.refs.empty() && op.value > 0.0 && op.value < 89.0);
        }
        return false;
    }
    return type == Type::Sketch ? sketch.has_region() : sizes_ok(kind, sizes);
}

bool Design::has_body() const {
    return std::any_of(features.begin(), features.end(), [](const Feature& f) {
        return !f.suppressed && f.buildable() && f.type != Feature::Type::Operation;
    });
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
        if (f.suppressed || !f.buildable() || f.type == Feature::Type::Operation)
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
json point3(const std::array<double, 3>& p) { return json::array({p[0], p[1], p[2]}); }

std::array<double, 3> axis_vector(char axis) {
    return axis == 'X' ? std::array<double, 3>{1, 0, 0} : axis == 'Y' ? std::array<double, 3>{0, 1, 0} : std::array<double, 3>{0, 0, 1};
}

// A plane through the origin by its name, or moved along its normal as
// {origin, normal}.
json plane_param(const std::string& plane, double offset) {
    if (offset == 0.0)
        return plane;
    const std::array<double, 3> n = plane == "XY" ? std::array<double, 3>{0, 0, 1}
                                    : plane == "XZ" ? std::array<double, 3>{0, 1, 0}
                                                    : std::array<double, 3>{1, 0, 0};
    return json{{"origin", point3({n[0] * offset, n[1] * offset, n[2] * offset})}, {"normal", point3(n)}};
}

// An operation on a body (stage D4), as SolidCraft's command reference
// names its parameters.
void operation_commands(const Feature& f, std::vector<json>& out) {
    const BodyOperation& o = f.op;
    json refs = json::array();
    for (const auto& r : o.refs)
        refs.push_back(point3(r));
    const char* name = operation_command(o.kind);
    switch (o.kind) {
    case OperationKind::Fillet:
        out.push_back(command(name, {{"edges", refs}, {"radius", o.value}, {"body", o.target}, {"name", f.name}}));
        break;
    case OperationKind::Chamfer:
        out.push_back(command(name, {{"edges", refs}, {"distance", o.value}, {"body", o.target}, {"name", f.name}}));
        break;
    case OperationKind::Shell:
        out.push_back(command(name, {{"faces", refs.empty() ? json("none") : refs}, {"thickness", o.value}, {"body", o.target},
                                     {"name", f.name}}));
        break;
    case OperationKind::Hole:
        // One hole at each point, through all unless a depth is given.
        for (const json& at : refs) {
            json p{{"position", at}, {"targets", json::array({o.target})}, {"name", f.name}};
            if (!o.standard.empty() && o.tapped) {
                p["thread"] = o.standard;
                p["hand"] = o.hand;
                if (o.modeled)
                    p["modeled"] = true;
            } else if (!o.standard.empty())
                p["clearance"] = o.standard, p["fit"] = o.fit;
            else
                p["diameter"] = o.value;
            if (o.depth > 0.0)
                p["depth"] = o.depth;
            out.push_back(command(name, std::move(p)));
        }
        break;
    case OperationKind::Mirror:
        out.push_back(command(name, {{"bodies", json::array({o.target})}, {"plane", o.plane}, {"combine", true}, {"name", f.name}}));
        break;
    case OperationKind::PatternCircular:
        out.push_back(command(name, {{"bodies", json::array({o.target})},
                                     {"axis", std::string(1, o.axis)},
                                     {"count", o.count},
                                     {"angle", o.angle},
                                     {"name", f.name}}));
        break;
    case OperationKind::PatternRectangular:
        out.push_back(command(name, {{"bodies", json::array({o.target})},
                                     {"dir1", point3(o.direction)},
                                     {"count1", o.count},
                                     {"spacing1", o.spacing},
                                     {"name", f.name}}));
        break;
    case OperationKind::Thread:
        // One thread on each cylindrical face picked.
        for (const json& at : refs) {
            json p{{"face", at}, {"designation", o.standard}, {"hand", o.hand}, {"modeled", o.modeled}, {"name", f.name}};
            if (o.depth > 0.0)
                p["length"] = o.depth;
            out.push_back(command(name, std::move(p)));
        }
        break;
    case OperationKind::Move: {
        json p{{"bodies", json::array({o.target})}, {"name", f.name}};
        if (o.offset != std::array<double, 3>{0.0, 0.0, 0.0})
            p["translate"] = point3(o.offset);
        if (o.angle != 0.0)
            p["axis"] = point3(axis_vector(o.axis)), p["angle"] = o.angle;
        if (o.copy)
            p["copy"] = true;
        out.push_back(command(name, std::move(p)));
        break;
    }
    case OperationKind::Split:
        out.push_back(command(name, {{"body", o.target}, {"plane", plane_param(o.plane, o.plane_offset)}, {"name", f.name}}));
        break;
    case OperationKind::Draft:
        out.push_back(command(name, {{"faces", refs},
                                     {"angle", o.value},
                                     {"neutral", plane_param(o.plane, o.plane_offset)},
                                     {"body", o.target},
                                     {"name", f.name}}));
        break;
    case OperationKind::Combine:
        out.push_back(command(name, {{"target", o.target}, {"tools", o.tools}, {"operation", o.boolean}, {"name", f.name}}));
        break;
    }
}

void feature_commands(const Feature& f, std::vector<json>& out) {
    if (f.type == Feature::Type::Operation) {
        operation_commands(f, out);
        return;
    }
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
    if (f.type == Feature::Type::Operation)
        return target == "value" || target == "spacing" ? Measure::Length : target == "angle" ? Measure::Angle : Measure::None;
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
            if (f.type == Feature::Type::Operation) {
                (target == "value" ? f.op.value : target == "spacing" ? f.op.spacing : f.op.angle) = v;
            } else if (target == "distance") {
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
    // The same commands as build_commands: parameters applied and sketches
    // solved (the constraints reach the part).
    json commands = json::array();
    for (const std::string& c : build_commands(given))
        commands.push_back(json::parse(c));
    if (!export_3mf.empty())
        commands.push_back(command("file.export_3mf", {{"path", export_3mf}, {"quality", quality_name(quality)}}));
    return json{{"commands", std::move(commands)}}.dump(1);
}

std::string design_json(const Design& design) {
    json features = json::array();
    for (const Feature& f : design.features) {
        if (f.type == Feature::Type::Operation) {
            const BodyOperation& o = f.op;
            json refs = json::array();
            for (const auto& r : o.refs)
                refs.push_back(point3(r));
            features.push_back({{"name", f.name},
                                {"kind", "operation"},
                                {"operation",
                                 {{"kind", operation_name(o.kind)},
                                  {"target", o.target},
                                  {"refs", refs},
                                  {"value", o.value},
                                  {"standard", o.standard},
                                  {"fit", o.fit},
                                  {"depth", o.depth},
                                  {"tapped", o.tapped},
                                  {"modeled", o.modeled},
                                  {"hand", o.hand},
                                  {"offset", point3(o.offset)},
                                  {"copy", o.copy},
                                  {"plane_offset", o.plane_offset},
                                  {"count", o.count},
                                  {"axis", std::string(1, o.axis)},
                                  {"angle", o.angle},
                                  {"direction", point3(o.direction)},
                                  {"spacing", o.spacing},
                                  {"plane", o.plane},
                                  {"tools", o.tools},
                                  {"boolean", o.boolean}}},
                                {"suppressed", f.suppressed}});
            if (!f.bindings.empty())
                features.back()["bindings"] = f.bindings;
            continue;
        }
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
        if (kind == "operation") {
            feature.type = Feature::Type::Operation;
            const json o = f.value("operation", json::object());
            const std::optional<OperationKind> k = operation_named(o.value("kind", std::string()));
            if (!k)
                throw std::runtime_error("feature \"" + feature.name + "\": unknown operation");
            BodyOperation& op = feature.op;
            op.kind = *k;
            op.target = o.value("target", std::string());
            const auto read3 = [&](const json& v) {
                if (!v.is_array() || v.size() != 3 || !v[0].is_number() || !v[1].is_number() || !v[2].is_number())
                    throw std::runtime_error("feature \"" + feature.name + "\": a point is not [x, y, z]");
                return std::array<double, 3>{v[0].get<double>(), v[1].get<double>(), v[2].get<double>()};
            };
            for (const json& r : o.value("refs", json::array()))
                op.refs.push_back(read3(r));
            op.value = o.value("value", op.value);
            op.standard = o.value("standard", std::string());
            op.fit = o.value("fit", op.fit);
            op.depth = o.value("depth", 0.0);
            op.tapped = o.value("tapped", false);
            op.modeled = o.value("modeled", false);
            op.hand = o.value("hand", op.hand);
            if (o.contains("offset"))
                op.offset = read3(o.at("offset"));
            op.copy = o.value("copy", false);
            op.plane_offset = o.value("plane_offset", 0.0);
            op.count = o.value("count", op.count);
            const std::string axis = o.value("axis", std::string("Z"));
            op.axis = axis.empty() ? 'Z' : axis[0];
            op.angle = o.value("angle", op.angle);
            if (o.contains("direction"))
                op.direction = read3(o.at("direction"));
            op.spacing = o.value("spacing", op.spacing);
            op.plane = o.value("plane", op.plane);
            for (const json& t : o.value("tools", json::array()))
                if (t.is_string())
                    op.tools.push_back(t.get<std::string>());
            op.boolean = o.value("boolean", op.boolean);
            feature.suppressed = f.value("suppressed", false);
            read_bindings(f, feature);
            d.features.push_back(std::move(feature));
            continue;
        }
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
