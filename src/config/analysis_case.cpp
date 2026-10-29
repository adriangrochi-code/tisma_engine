// Reader for analysis cases (docs/ANALYSIS.md), on nlohmann/json (MIT). Own
// design; see PROVENANCE.md.

#include "config/analysis_case.hpp"

#include "config/settings.hpp"
#include "fea/materials.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <fstream>
#include <map>
#include <sstream>

namespace tisma {
namespace {

using nlohmann::json;

[[noreturn]] void fail(const std::string& key, const std::string& message) {
    throw SettingsError("case: " + key + ": " + message);
}

// Rejects members of an object that are not in known.
void check_members(const json& object, const std::string& key, const std::vector<std::string>& known) {
    if (!object.is_object())
        fail(key, "must be an object");
    for (const auto& [name, value] : object.items())
        if (std::find(known.begin(), known.end(), name) == known.end())
            fail(key + "." + name, "unknown key (see docs/ANALYSIS.md)");
}

double number(const json& v, const std::string& key) {
    if (!v.is_number())
        fail(key, "must be a number");
    return v.get<double>();
}

std::string name(const json& item, const std::string& key, const std::string& fallback) {
    if (!item.contains("name"))
        return fallback;
    if (!item.at("name").is_string())
        fail(key + ".name", "must be a string");
    return item.at("name").get<std::string>();
}

double positive(const json& v, const std::string& key) {
    const double x = number(v, key);
    if (!(x > 0.0))
        fail(key, "must be greater than 0");
    return x;
}

template <typename Enum>
Enum choice(const json& v, const std::string& key, const std::map<std::string, Enum>& options) {
    if (!v.is_string())
        fail(key, "must be a string");
    const auto it = options.find(v.get<std::string>());
    if (it == options.end()) {
        std::string list;
        for (const auto& [option, value] : options)
            list += (list.empty() ? "" : ", ") + option;
        fail(key, "must be one of " + list);
    }
    return it->second;
}

OrthotropicMaterial material_from(const MaterialRecord* record, const std::string& key, const std::string& id) {
    if (!record) {
        std::string list;
        for (const MaterialRecord& r : material_records())
            list += (list.empty() ? "" : ", ") + std::string(r.id);
        fail(key, "unknown material '" + id + "'; known: " + list);
    }
    return as_printed(*record);
}

void read_material(const json& v, AnalysisCase& c) {
    if (v.is_string()) {
        c.material_id = v.get<std::string>();
        c.material = material_from(find_material(c.material_id), "material", c.material_id);
        return;
    }
    // A record with some constants changed: {"base": id, "zt": 20, ...}.
    struct Field {
        const char* name;
        double OrthotropicMaterial::*member;
    };
    static constexpr Field kFields[] = {
        {"e1", &OrthotropicMaterial::e1},     {"e2", &OrthotropicMaterial::e2},     {"e3", &OrthotropicMaterial::e3},
        {"g12", &OrthotropicMaterial::g12},   {"g13", &OrthotropicMaterial::g13},   {"g23", &OrthotropicMaterial::g23},
        {"nu12", &OrthotropicMaterial::nu12}, {"nu13", &OrthotropicMaterial::nu13}, {"nu23", &OrthotropicMaterial::nu23},
        {"xt", &OrthotropicMaterial::xt},     {"xc", &OrthotropicMaterial::xc},     {"yt", &OrthotropicMaterial::yt},
        {"yc", &OrthotropicMaterial::yc},     {"zt", &OrthotropicMaterial::zt},     {"zc", &OrthotropicMaterial::zc},
        {"s12", &OrthotropicMaterial::s12},   {"s13", &OrthotropicMaterial::s13},   {"s23", &OrthotropicMaterial::s23}};
    std::vector<std::string> known = {"base"};
    for (const Field& f : kFields)
        known.push_back(f.name);
    check_members(v, "material", known);
    if (v.contains("base")) {
        if (!v.at("base").is_string())
            fail("material.base", "must be a string");
        c.material_id = v.at("base").get<std::string>();
    }
    c.material = material_from(find_material(c.material_id), "material.base", c.material_id);
    for (const Field& f : kFields)
        if (v.contains(f.name))
            c.material.*f.member = positive(v.at(f.name), std::string("material.") + f.name);
    try {
        local_stiffness(c.material);
    } catch (const std::invalid_argument&) {
        fail("material", "these constants are not a valid material (the compliance is not positive definite)");
    }
}

Region read_region(const json& item, const std::string& key) {
    Region r;
    const bool face = item.contains("face"), box = item.contains("box");
    if (face == box)
        fail(key, "needs either \"face\" or \"box\"");
    if (face) {
        r.face = choice(item.at("face"), key + ".face",
                        std::map<std::string, PartFace>{{"bottom", PartFace::Bottom}, {"top", PartFace::Top},
                                                        {"x_min", PartFace::XMin},    {"x_max", PartFace::XMax},
                                                        {"y_min", PartFace::YMin},    {"y_max", PartFace::YMax}});
        return r;
    }
    const json& b = item.at("box");
    check_members(b, key + ".box", {"min", "max"});
    for (const char* end : {"min", "max"}) {
        if (!b.contains(end))
            continue;
        const std::string k = key + ".box." + end;
        const json& corner = b.at(end);
        if (!corner.is_array() || corner.size() != 3)
            fail(k, "must be [x, y, z] (null for no limit)");
        for (std::size_t a = 0; a < 3; ++a) {
            if (corner[a].is_null())
                continue;
            (end[1] == 'i' ? r.box.min : r.box.max)[a] = number(corner[a], k + "[" + std::to_string(a) + "]");
        }
    }
    return r;
}

} // namespace

static AnalysisCase parse_analysis_case_checked(std::string_view text) {
    json root;
    try {
        root = json::parse(text.begin(), text.end());
    } catch (const json::parse_error& e) {
        throw SettingsError(std::string("case: not valid JSON: ") + e.what());
    }
    if (!root.is_object())
        throw SettingsError("case: the top level must be an object");
    check_members(root, "case", {"material", "model", "quality", "voxel", "rows_per_layer", "max_voxels",
                                 "resolution_check", "fixtures", "loads", "solver"});
    AnalysisCase c;
    c.material = as_printed(*find_material(c.material_id));
    if (root.contains("material"))
        read_material(root.at("material"), c);
    if (root.contains("model"))
        c.model = choice(root.at("model"), "model",
                         std::map<std::string, AnalysisModel>{{"as_printed", AnalysisModel::AsPrinted},
                                                              {"solid", AnalysisModel::Solid}});
    if (root.contains("quality"))
        c.quality = choice(root.at("quality"), "quality",
                           std::map<std::string, AnalysisQuality>{{"normal", AnalysisQuality::Normal},
                                                                  {"high", AnalysisQuality::High}});
    if (root.contains("voxel"))
        c.voxel = positive(root.at("voxel"), "voxel");
    if (root.contains("rows_per_layer")) {
        const json& v = root.at("rows_per_layer");
        if (!v.is_number_integer() || v.get<long long>() < 1 || v.get<long long>() > 64)
            fail("rows_per_layer", "must be a whole number from 1 to 64");
        c.rows_per_layer = int(v.get<long long>());
    }
    if (root.contains("max_voxels")) {
        const json& v = root.at("max_voxels");
        if (!v.is_number_integer() || v.get<long long>() < 1)
            fail("max_voxels", "must be a whole number greater than 0");
        c.max_voxels = std::size_t(v.get<long long>());
    }
    if (root.contains("resolution_check")) {
        if (!root.at("resolution_check").is_boolean())
            fail("resolution_check", "must be true or false");
        c.resolution_check = root.at("resolution_check").get<bool>();
    }
    if (root.contains("fixtures")) {
        const json& list = root.at("fixtures");
        if (!list.is_array())
            fail("fixtures", "must be a list");
        for (std::size_t i = 0; i < list.size(); ++i) {
            const std::string key = "fixtures[" + std::to_string(i) + "]";
            check_members(list[i], key, {"name", "face", "box", "axes"});
            CaseFixture f;
            f.name = name(list[i], key, "fixture " + std::to_string(i + 1));
            f.region = read_region(list[i], key);
            if (list[i].contains("axes")) {
                const json& axes = list[i].at("axes");
                const std::string a = axes.is_string() ? axes.get<std::string>() : std::string();
                if (a.empty() || a.find_first_not_of("xyz") != std::string::npos)
                    fail(key + ".axes", "must name the fixed axes, for example \"xyz\" or \"z\"");
                f.x = a.find('x') != std::string::npos;
                f.y = a.find('y') != std::string::npos;
                f.z = a.find('z') != std::string::npos;
            }
            c.fixtures.push_back(f);
        }
    }
    if (root.contains("loads")) {
        const json& list = root.at("loads");
        if (!list.is_array())
            fail("loads", "must be a list");
        for (std::size_t i = 0; i < list.size(); ++i) {
            const std::string key = "loads[" + std::to_string(i) + "]";
            check_members(list[i], key, {"name", "face", "box", "force"});
            CaseLoad l;
            l.name = name(list[i], key, "load " + std::to_string(i + 1));
            l.region = read_region(list[i], key);
            if (!list[i].contains("force"))
                fail(key, "needs \"force\": [fx, fy, fz] in N");
            const json& force = list[i].at("force");
            if (!force.is_array() || force.size() != 3)
                fail(key + ".force", "must be [fx, fy, fz] in N");
            for (std::size_t a = 0; a < 3; ++a)
                l.force[a] = number(force[a], key + ".force[" + std::to_string(a) + "]");
            c.loads.push_back(l);
        }
    }
    if (root.contains("solver")) {
        const json& s = root.at("solver");
        check_members(s, "solver", {"tolerance", "max_iterations", "preconditioner"});
        if (s.contains("tolerance"))
            c.solver.tolerance = positive(s.at("tolerance"), "solver.tolerance");
        if (s.contains("max_iterations")) {
            const json& v = s.at("max_iterations");
            if (!v.is_number_integer() || v.get<long long>() < 1 || v.get<long long>() > 1'000'000)
                fail("solver.max_iterations", "must be a whole number from 1 to 1000000");
            c.solver.max_iterations = int(v.get<long long>());
        }
        if (s.contains("preconditioner"))
            c.solver.preconditioner = choice(s.at("preconditioner"), "solver.preconditioner",
                                             std::map<std::string, Preconditioner>{{"multigrid", Preconditioner::Multigrid},
                                                                                   {"jacobi", Preconditioner::Jacobi}});
    }
    return c;
}

AnalysisCase parse_analysis_case(std::string_view text) {
    if (!json_depth_ok(text))
        throw SettingsError("case: nested more than 64 levels deep");
    try {
        return parse_analysis_case_checked(text);
    } catch (const nlohmann::json::exception& e) {
        throw SettingsError(std::string("case: ") + e.what());
    }
}

AnalysisCase load_analysis_case(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in)
        throw SettingsError("case: cannot open '" + path.string() + "'");
    std::ostringstream text;
    text << in.rdbuf();
    return parse_analysis_case(text.str());
}

} // namespace tisma
