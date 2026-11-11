// My library. Own code; see library.hpp and PROVENANCE.md.

#include "library/library.hpp"

#include "catalog/pack.hpp"
#include "config/settings.hpp"
#include "io/model.hpp"
#include "io/zip.hpp"
#include "project/project.hpp"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <fstream>
#include <iterator>
#include <map>
#include <regex>
#include <set>
#include <sstream>
#include <system_error>
#include <vector>

namespace tisma {
namespace {

namespace fs = std::filesystem;
using nlohmann::json;

[[noreturn]] void fail(const std::string& code, const std::string& en, const std::string& es) {
    throw LibraryError(code, en, es);
}

std::string read_file(const fs::path& p, const char* what) {
    std::ifstream in(p, std::ios::binary);
    if (!in)
        fail("input", std::string("cannot read the ") + what + ": " + p.filename().string(),
             std::string("no se puede leer el archivo: ") + p.filename().string());
    return std::string(std::istreambuf_iterator<char>(in), {});
}

// Written whole, then put in place: a cut never leaves half a file.
void write_file(const fs::path& p, std::string_view data) {
    if (!p.parent_path().empty()) // a bare name goes in the current folder (BH57)
        fs::create_directories(p.parent_path());
    const fs::path tmp = p.string() + ".part";
    {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        out.write(data.data(), std::streamsize(data.size()));
        if (!out)
            fail("write", "cannot write in the library", "no se puede escribir en la biblioteca");
    }
    fs::rename(tmp, p);
}

void copy_in(const fs::path& from, const fs::path& to, const char* what) { write_file(to, read_file(from, what)); }

std::string now_utc() {
    const std::time_t t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::tm tm{};
#ifdef _WIN32
    gmtime_s(&tm, &t);
#else
    gmtime_r(&t, &tm);
#endif
    char text[32];
    std::strftime(text, sizeof text, "%Y-%m-%dT%H:%M:%SZ", &tm);
    return text;
}

// Entry, recipe and analysis ids name folders: letters, digits, '-' and '_'.
bool valid_id(std::string_view id) {
    return !id.empty() && id.size() <= 64 && std::all_of(id.begin(), id.end(), [](char c) {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_';
    });
}

fs::path entry_dir(const fs::path& dir, const std::string& id) {
    if (!valid_id(id))
        fail("entry", "no entry '" + id + "'", "no hay una entrada «" + id + "»");
    return dir / id;
}

json read_entry(const fs::path& dir, const std::string& id) {
    const fs::path file = entry_dir(dir, id) / "entry.json";
    if (!fs::is_regular_file(file))
        fail("entry", "no entry '" + id + "'", "no hay una entrada «" + id + "»");
    json e = json::parse(read_file(file, "entry"), nullptr, false);
    if (!e.is_object() || e.value("format", "") != "tisma-library-entry")
        fail("entry", "the entry '" + id + "' is damaged", "la entrada «" + id + "» está dañada");
    return e;
}

void write_entry(const fs::path& dir, json& e) {
    e["modified"] = now_utc();
    write_file(dir / e.at("id").get<std::string>() / "entry.json", e.dump(1));
}

std::uintmax_t bytes_in(const fs::path& d) {
    std::uintmax_t total = 0;
    std::error_code ec;
    for (const auto& f : fs::recursive_directory_iterator(d, ec))
        if (f.is_regular_file(ec))
            total += f.file_size(ec);
    return total;
}

json parse_json_file(const fs::path& p, const char* what) {
    json j = json::parse(read_file(p, what), nullptr, false);
    if (j.is_discarded())
        fail("input", std::string("the ") + what + " is not JSON", "el archivo no es JSON: " + p.filename().string());
    return j;
}

// The next free number among a list's ids.
std::string next_id(const json& list) {
    int top = 0;
    for (const json& item : list)
        if (item.contains("id") && item.at("id").is_string())
            top = std::max(top, std::atoi(item.at("id").get<std::string>().c_str()));
    return std::to_string(top + 1);
}

// The settings that change the strength (model_library.md, rules 2 and 3):
// layer, walls, infill and its pattern, material.
constexpr const char* kStrengthKeys[] = {"layers.height", "walls.count", "infill.density", "infill.pattern",
                                          "extrusion.type"};

std::map<std::string, std::string> described(const std::string& settings_text) {
    try {
        std::map<std::string, std::string> out;
        const Settings s = parse_settings(settings_text);
        for (auto& [key, value] : describe(s))
            out[key] = value;
        out["extrusion.type"] = s.filament_type; // the material, saved but not described
        return out;
    } catch (const std::exception& e) {
        fail("settings", std::string("the settings are not valid: ") + e.what(), "los ajustes no son válidos");
    }
}

// The entry's folder as the format has it (rule 9): what an import may bring.
bool allowed_file(const std::string& rest) {
    static const std::regex kAllowed(
        R"((entry\.json|project\.3mf|thumb\.png|recipes/[A-Za-z0-9_-]{1,64}/settings\.json|)"
        R"(analyses/[A-Za-z0-9_-]{1,64}\.(tea|taero|json)|photos/[A-Za-z0-9_-]{1,64}\.(jpg|jpeg|png|webp)))");
    return std::regex_match(rest, kAllowed);
}

} // namespace

json library_list(const fs::path& dir) {
    json entries = json::array();
    std::error_code ec;
    std::vector<fs::path> folders;
    for (const auto& f : fs::directory_iterator(dir, ec))
        if (f.is_directory() && valid_id(f.path().filename().string()) && fs::is_regular_file(f.path() / "entry.json"))
            folders.push_back(f.path());
    std::sort(folders.begin(), folders.end());
    for (const fs::path& folder : folders) {
        const std::string id = folder.filename().string();
        json e;
        try {
            e = read_entry(dir, id);
        } catch (const LibraryError&) {
            continue; // a damaged entry is left out of the list, not lost
        }
        std::set<std::string> printers, materials;
        for (const json& r : e.value("recipes", json::array())) {
            const json recipe = r.value("recipe", json::object());
            for (const char* k : {"label", "printer"})
                if (recipe.contains(k) && recipe.at(k).is_string() && !recipe.at(k).get<std::string>().empty()) {
                    printers.insert(recipe.at(k).get<std::string>());
                    break;
                }
            if (recipe.contains("filament") && recipe.at("filament").is_string())
                materials.insert(recipe.at("filament").get<std::string>());
        }
        entries.push_back({{"id", id},
                           {"name", e.value("name", id)},
                           {"tags", e.value("tags", json::array())},
                           {"printers", printers},
                           {"materials", materials},
                           {"recipes", e.value("recipes", json::array()).size()},
                           {"analyses", e.value("analyses", json::array()).size()},
                           {"bytes", bytes_in(folder)},
                           {"modified", e.value("modified", "")},
                           {"thumb", fs::is_regular_file(folder / "thumb.png") ? json("thumb.png") : json()}});
    }
    return {{"entries", entries}};
}

json library_show(const fs::path& dir, const std::string& id) { return read_entry(dir, id); }

json library_save(const fs::path& dir, const LibrarySave& r) {
    fs::create_directories(dir);
    const json recipe = parse_json_file(r.recipe, "recipe");
    if (!recipe.is_object())
        fail("input", "the recipe must be a JSON object", "la receta tiene que ser un objeto JSON");
    const std::string settings = read_file(r.settings, "settings");
    described(settings); // the engine reads them, or nothing is kept
    json e;
    if (r.entry) {
        e = read_entry(dir, *r.entry);
    } else {
        int n = 1;
        while (fs::exists(dir / ("e" + std::to_string(n))))
            ++n;
        const std::string id = "e" + std::to_string(n);
        e = {{"format", "tisma-library-entry"}, {"version", 1}, {"id", id}, {"name", id}, {"tags", json::array()},
             {"notes", ""}, {"recipes", json::array()}, {"analyses", json::array()}, {"printed", json::array()}};
    }
    if (r.name)
        e["name"] = *r.name;
    const fs::path folder = dir / e.at("id").get<std::string>();
    // The project as a 3MF package (rule 1): a bare STL or OBJ, as the phone's
    // plate holds it, becomes one, with the settings it was sliced with.
    const std::string model = read_file(r.model, "model");
    if (looks_like_zip(model)) {
        write_file(folder / "project.3mf", model);
    } else {
        Project project;
        try {
            ModelFile file = parse_model(model, r.model.extension().string());
            project.settings = parse_settings(settings);
            ProjectModel one;
            one.name = r.model.stem().string();
            one.mesh = std::move(file.mesh);
            project.models.push_back(std::move(one));
        } catch (const std::exception& error) {
            fail("input", std::string("the model cannot be read: ") + error.what(), "no se puede leer el modelo");
        }
        write_file(folder / "project.3mf", write_project_3mf(project));
    }
    if (r.thumb)
        copy_in(*r.thumb, folder / "thumb.png", "picture");
    // The same printer, nozzle, filament and print profile: that recipe again.
    const auto key = [](const json& x) {
        json k = json::array();
        for (const char* name : {"printer", "nozzle", "filament", "print"})
            k.push_back(x.contains(name) ? x.at(name) : json());
        return k;
    };
    json& recipes = e["recipes"];
    std::string rid;
    for (const json& old : recipes)
        if (key(old.value("recipe", json::object())) == key(recipe))
            rid = old.value("id", "");
    if (!valid_id(rid))
        rid = next_id(recipes);
    json summary;
    if (r.summary) {
        const json s = parse_json_file(*r.summary, "summary");
        const auto num = [&](const char* k) { return s.contains(k) && s.at(k).is_number() ? s.at(k) : json(); };
        summary = {{"time_s", num("print_time_s")},
                   {"filament_m", s.contains("filament_mm") && s.at("filament_mm").is_number()
                                      ? json(s.at("filament_mm").get<double>() / 1000.0)
                                      : json()},
                   {"filament_g", num("filament_g")},
                   {"layers", num("layers")}};
    }
    write_file(folder / "recipes" / rid / "settings.json", settings);
    const std::string sha = sha256_hex(settings);
    json kept = json::array();
    for (json& old : recipes)
        if (old.value("id", "") != rid)
            kept.push_back(std::move(old));
    kept.push_back({{"id", rid}, {"recipe", recipe}, {"engine", r.engine}, {"settings_sha256", sha},
                    {"modified", now_utc()}, {"summary", summary}});
    recipes = std::move(kept);
    if (r.analysis) {
        json& analyses = e["analyses"];
        const std::string aid = next_id(analyses);
        const bool cfd = r.analysis->extension() == ".taero";
        const std::string file = "analyses/" + aid + (cfd ? ".taero" : ".tea");
        copy_in(*r.analysis, folder / file, "analysis");
        json s = json::object();
        if (r.analysis_summary) {
            s = parse_json_file(*r.analysis_summary, "analysis summary");
            write_file(folder / "analyses" / (aid + ".json"), s.dump(1));
        }
        // The settings it was computed with: the strength's, to check later.
        json strength = json::object();
        const auto values = described(settings);
        for (const char* k : kStrengthKeys)
            if (const auto it = values.find(k); it != values.end())
                strength[k] = it->second;
        analyses.push_back({{"id", aid}, {"kind", cfd ? "cfd" : "fea"}, {"file", file}, {"summary", s},
                            {"settings_sha256", sha}, {"strength_settings", strength}});
    }
    write_entry(dir, e);
    return e;
}

json library_check(const fs::path& dir, const std::string& id, const std::string& recipe_id, const fs::path& settings,
                   const std::string& engine) {
    const json e = read_entry(dir, id);
    std::optional<json> recipe;
    for (const json& r : e.value("recipes", json::array()))
        if (r.value("id", "") == recipe_id)
            recipe = r;
    if (!recipe || !valid_id(recipe_id))
        fail("recipe", "no recipe '" + recipe_id + "'", "no hay una receta «" + recipe_id + "»");
    const std::string then_text = read_file(entry_dir(dir, id) / "recipes" / recipe_id / "settings.json", "recipe settings");
    const auto now = described(read_file(settings, "settings"));
    // Keys of the kept settings this engine does not know (rule 7): named, and
    // the default used.
    json missing = json::array();
    std::set<std::string> known;
    for (const auto& [k, v] : describe(Settings{}))
        known.insert(k);
    const json then_json = json::parse(then_text, nullptr, false);
    if (then_json.is_object())
        for (const auto& [group, keys] : then_json.items()) {
            if (group == "changes" || !keys.is_object())
                continue;
            for (const auto& [key, value] : keys.items())
                if (!known.contains(group + "." + key) && key != "variables" && key != "custom_parameters" &&
                    key != "macros")
                    missing.push_back(group + "." + key);
        }
    std::map<std::string, std::string> then;
    try {
        then = described(then_text);
    } catch (const LibraryError&) {
        // Settings an older engine wrote that this one refuses: none equal.
    }
    json analyses = json::array();
    for (const json& a : e.value("analyses", json::array())) {
        json changed = json::array();
        const json strength = a.value("strength_settings", json::object());
        for (const char* k : kStrengthKeys) {
            const auto it = now.find(k);
            const std::string value = it == now.end() ? std::string() : it->second;
            if (!strength.contains(k) || !strength.at(k).is_string() || strength.at(k).get<std::string>() != value)
                changed.push_back(k);
        }
        analyses.push_back({{"id", a.value("id", "")}, {"valid", changed.empty()}, {"changed", changed}});
    }
    return {{"same_settings", !then.empty() && then == now},
            {"older_engine", recipe->value("engine", "") != engine},
            {"missing_keys", missing},
            {"analyses", analyses}};
}

json library_printed(const fs::path& dir, const std::string& id, const std::string& date,
                     const std::optional<fs::path>& photo, const std::string& note) {
    json e = read_entry(dir, id);
    json& printed = e["printed"];
    if (!printed.is_array())
        printed = json::array();
    std::string kept;
    if (photo) {
        std::string ext = photo->extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return char(std::tolower(c)); });
        if (ext != ".jpg" && ext != ".jpeg" && ext != ".png" && ext != ".webp")
            fail("input", "the photo must be JPEG, PNG or WebP", "la foto tiene que ser JPEG, PNG o WebP");
        int n = int(printed.size()) + 1;
        while (fs::exists(entry_dir(dir, id) / "photos" / (std::to_string(n) + ext)))
            ++n;
        kept = "photos/" + std::to_string(n) + ext;
        copy_in(*photo, entry_dir(dir, id) / kept, "photo");
    }
    printed.push_back({{"date", date.empty() ? now_utc() : date}, {"photo", kept}, {"note", note}});
    write_entry(dir, e);
    return e;
}

void library_delete(const fs::path& dir, const std::string& id, const std::optional<std::string>& recipe,
                    const std::optional<std::string>& analysis) {
    const fs::path folder = entry_dir(dir, id);
    json e = read_entry(dir, id);
    if (!recipe && !analysis) {
        fs::remove_all(folder);
        return;
    }
    const auto drop = [&](const char* list, const std::string& which, auto files) {
        json kept = json::array();
        bool found = false;
        for (json& item : e[list])
            if (item.value("id", "") == which)
                found = true;
            else
                kept.push_back(std::move(item));
        if (!found || !valid_id(which))
            fail("entry", std::string("no such ") + list + " item '" + which + "'", "no existe «" + which + "»");
        e[list] = std::move(kept);
        files(which);
    };
    if (recipe)
        drop("recipes", *recipe, [&](const std::string& r) { fs::remove_all(folder / "recipes" / r); });
    if (analysis)
        drop("analyses", *analysis, [&](const std::string& a) {
            for (const char* ext : {".tea", ".taero", ".json"})
                fs::remove(folder / "analyses" / (a + ext));
        });
    write_entry(dir, e);
}

void library_export(const fs::path& dir, const std::string& id, const fs::path& file) {
    const fs::path folder = entry_dir(dir, id);
    read_entry(dir, id);
    std::vector<ZipEntryToWrite> files;
    std::vector<fs::path> paths;
    for (const auto& f : fs::recursive_directory_iterator(folder))
        if (f.is_regular_file())
            paths.push_back(f.path());
    std::sort(paths.begin(), paths.end());
    for (const fs::path& p : paths) {
        const std::string rest = fs::relative(p, folder).generic_string();
        if (allowed_file(rest)) // nothing else leaves the library
            files.push_back({id + "/" + rest, read_file(p, "entry file")});
    }
    write_file(file, write_zip_stored(files));
}

json library_import(const fs::path& file, const fs::path& dir, LibraryImport mode) {
    const std::string data = read_file(file, "library file");
    const auto refuse = [] {
        fail("rejected", "The file has unsafe paths or programs", "El archivo trae rutas o programas no permitidos");
    };
    std::optional<ZipArchive> zip;
    try {
        zip.emplace(data);
    } catch (const ZipError&) {
        fail("input", "not a .tismalib file", "no es un archivo .tismalib");
    }
    // One folder, the format's files only, none outside it (ML6); checked
    // whole before anything is written.
    std::string id;
    std::uint64_t total = 0;
    bool has_entry = false;
    for (const ZipArchive::Entry& z : zip->entries()) {
        const std::string& name = z.name;
        if (!name.empty() && name.back() == '/')
            continue; // a folder
        const std::size_t slash = name.find('/');
        if (slash == std::string::npos)
            refuse();
        const std::string top = name.substr(0, slash), rest = name.substr(slash + 1);
        if (!valid_id(top) || (!id.empty() && top != id) || !allowed_file(rest))
            refuse();
        id = top;
        has_entry |= rest == "entry.json";
        total += z.size;
    }
    if (!has_entry)
        fail("input", "the file has no entry.json", "el archivo no trae entry.json");
    if (total > (std::uint64_t(4) << 30))
        fail("input", "the file is too large", "el archivo es demasiado grande");
    std::string target = id;
    if (fs::exists(dir / id)) {
        if (mode == LibraryImport::Ask)
            throw LibraryError("exists", id, id);
        if (mode == LibraryImport::AsNew) {
            int n = 1;
            while (fs::exists(dir / (id + "_" + std::to_string(n))))
                ++n;
            target = id + "_" + std::to_string(n);
        }
    }
    // Read and checked in a folder of its own, then moved in place.
    const fs::path staging = dir / (".import-" + target);
    fs::remove_all(staging);
    try {
        for (const ZipArchive::Entry& z : zip->entries())
            if (z.name.back() != '/')
                write_file(staging / z.name.substr(id.size() + 1), zip->read(z));
        json e = json::parse(read_file(staging / "entry.json", "entry"), nullptr, false);
        if (!e.is_object() || e.value("format", "") != "tisma-library-entry")
            fail("input", "the entry in the file is damaged", "la entrada del archivo está dañada");
        e["id"] = target;
        write_file(staging / "entry.json", e.dump(1));
        if (fs::exists(dir / target))
            fs::remove_all(dir / target); // Replace
        fs::rename(staging, dir / target);
        return e;
    } catch (...) {
        std::error_code ec;
        fs::remove_all(staging, ec);
        throw;
    }
}

} // namespace tisma
