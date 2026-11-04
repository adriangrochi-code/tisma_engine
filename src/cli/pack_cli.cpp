#include "cli/pack_cli.hpp"

#include "catalog/pack.hpp"
#include "cli/cli.hpp"
#include "config/profiles.hpp"
#include "config/settings.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace tisma {

namespace fs = std::filesystem;
using nlohmann::json;

namespace {

constexpr std::string_view kPackUsage =
    "usage:\n"
    "  tisma-engine pack plan --data <folder> [--index <index.json>] [--vendors <id,id,...>] [--url <base url>]\n"
    "      what to download, as JSON: without --index only the index; with the downloaded index, the rest\n"
    "      (only the vendors whose archive changed; without --vendors, the installed ones, or all)\n"
    "  tisma-engine pack install --data <folder> --from <folder with the downloaded files>\n"
    "      checks size and SHA-256, unpacks safely and switches the pack at once; progress lines on stderr\n"
    "  tisma-engine pack list --data <folder>\n"
    "  tisma-engine profile --pack <folder> --printer \"<model file>#<variant>\" [--filament <path>] [--print <path>]\n"
    "                       [-o <settings.json>]   (the variant's defaults when --filament or --print is left out)\n"
    "The pack lives in <folder>/pack.\n";

// "--name value" pairs; an unknown argument or a missing value is a usage error.
bool options(std::span<const std::string_view> args, std::map<std::string, std::string>& got,
             const std::vector<std::string>& known, std::string& problem) {
    for (std::size_t i = 0; i < args.size(); ++i) {
        const std::string a(args[i]);
        if (std::find(known.begin(), known.end(), a) == known.end()) {
            problem = "unexpected argument '" + a + "'";
            return false;
        }
        if (i + 1 >= args.size()) {
            problem = a + " needs a value";
            return false;
        }
        got[a] = std::string(args[++i]);
    }
    return true;
}

bool read_all(const fs::path& path, std::string& data) {
    std::ifstream in(path, std::ios::binary);
    if (!in)
        return false;
    std::ostringstream s;
    s << in.rdbuf();
    data = s.str();
    return true;
}

std::vector<std::string> split_list(const std::string& text) {
    std::vector<std::string> out;
    std::string item;
    std::istringstream s(text);
    while (std::getline(s, item, ','))
        if (!item.empty())
            out.push_back(item);
    return out;
}

int usage(std::ostream& err, const std::string& problem) {
    err << "error: " << problem << "\n" << kPackUsage;
    return kExitUsage;
}

int pack_plan_cmd(std::span<const std::string_view> args, std::ostream& out, std::ostream& err) {
    std::map<std::string, std::string> o;
    std::string problem;
    if (!options(args, o, {"--data", "--index", "--vendors", "--url"}, problem))
        return usage(err, "pack plan: " + problem);
    if (!o.count("--data"))
        return usage(err, "pack plan needs --data");
    const fs::path pack = fs::path(o["--data"]) / "pack";
    std::optional<PackIndex> installed;
    if (fs::exists(pack / "index.json"))
        installed = pack_installed(pack);
    std::optional<PackIndex> index;
    if (o.count("--index")) {
        std::string text;
        if (!read_all(o["--index"], text)) {
            err << "error: cannot read " << o["--index"] << "\n";
            return kExitInput;
        }
        index = parse_pack_index(text);
    }
    std::vector<std::string> vendors = split_list(o["--vendors"]);
    if (vendors.empty() && installed)
        for (const PackVendor& v : installed->vendors)
            if (!v.archive.sha256.empty())
                vendors.push_back(v.id);
    const auto plan = pack_plan(index ? &*index : nullptr, o["--url"], vendors, installed ? &*installed : nullptr);
    json downloads = json::array();
    for (const PackDownload& d : plan)
        downloads.push_back({{"url", d.url}, {"name", d.name}, {"size", d.size}, {"sha256", d.sha256}});
    json result = {{"downloads", downloads},
                   {"installed", installed ? json(installed->pack_version) : json(nullptr)},
                   {"published", index ? json(index->pack_version) : json(nullptr)}};
    out << result.dump(1) << "\n";
    return kExitOk;
}

int pack_install_cmd(std::span<const std::string_view> args, std::ostream& out, std::ostream& err) {
    std::map<std::string, std::string> o;
    std::string problem;
    if (!options(args, o, {"--data", "--from"}, problem))
        return usage(err, "pack install: " + problem);
    if (!o.count("--data") || !o.count("--from"))
        return usage(err, "pack install needs --data and --from");
    const PackInstallReport r = pack_install(fs::path(o["--data"]) / "pack", o["--from"],
                                             [&](const std::string& line) { err << "progress: " << line << "\n"; });
    json result = {{"pack_version", r.pack_version}, {"installed", r.installed}, {"kept", r.kept},
                   {"discarded", r.discarded}, {"warnings", r.warnings}};
    out << result.dump(1) << "\n";
    return kExitOk;
}

int pack_list_cmd(std::span<const std::string_view> args, std::ostream& out, std::ostream& err) {
    std::map<std::string, std::string> o;
    std::string problem;
    if (!options(args, o, {"--data"}, problem))
        return usage(err, "pack list: " + problem);
    if (!o.count("--data"))
        return usage(err, "pack list needs --data");
    const PackIndex index = pack_installed(fs::path(o["--data"]) / "pack");
    json vendors = json::array();
    for (const PackVendor& v : index.vendors) {
        json models = json::array();
        for (const PackModel& m : v.models) {
            json variants = json::array();
            for (const PackVariant& x : m.variants)
                variants.push_back({{"id", x.id}, {"nozzle", x.nozzle}, {"printer", m.file + "#" + x.id}});
            models.push_back({{"id", m.id}, {"name", m.name}, {"technology", m.technology}, {"family", m.family},
                              {"default_variant", m.default_variant}, {"variants", variants}});
        }
        vendors.push_back({{"id", v.id}, {"name", v.name}, {"installed", !v.archive.sha256.empty()},
                           {"license", v.license}, {"origin", v.origin}, {"models", models}});
    }
    out << json({{"pack_version", index.pack_version}, {"vendors", vendors}}).dump(1) << "\n";
    return kExitOk;
}

} // namespace

int cmd_pack(std::span<const std::string_view> args, std::ostream& out, std::ostream& err) {
    if (args.empty())
        return usage(err, "pack needs plan, install or list");
    const std::string_view sub = args[0];
    try {
        if (sub == "plan")
            return pack_plan_cmd(args.subspan(1), out, err);
        if (sub == "install")
            return pack_install_cmd(args.subspan(1), out, err);
        if (sub == "list")
            return pack_list_cmd(args.subspan(1), out, err);
    } catch (const PackError& e) {
        err << "error: " << e.what() << "\n";
        return kExitInput;
    }
    return usage(err, "unknown pack command '" + std::string(sub) + "'");
}

int cmd_pack_profile(std::span<const std::string_view> args, std::ostream& out, std::ostream& err) {
    std::map<std::string, std::string> o;
    std::string problem;
    if (!options(args, o, {"--pack", "--printer", "--filament", "--print", "-o"}, problem))
        return usage(err, "profile: " + problem);
    if (!o.count("--printer"))
        return usage(err, "profile --pack needs --printer \"<model file>#<variant>\"");
    try {
        const fs::path pack = fs::path(o["--pack"]) / "pack";
        const PackIndex index = pack_installed(pack);
        const std::string printer = o["--printer"];
        const auto hash = printer.rfind('#');
        if (hash == std::string::npos)
            return usage(err, "profile: --printer needs \"<model file>#<variant>\"");
        const std::string file = printer.substr(0, hash), variant = printer.substr(hash + 1);
        const auto number = std::find(index.printers.begin(), index.printers.end(), printer);
        if (number == index.printers.end()) {
            err << "error: the pack has no printer \"" << printer << "\"\n";
            return kExitInput;
        }
        // The model file as the index names it, under vendors/ when given without it.
        const auto read_pack = [&](const std::string& path, std::string& data) {
            if (safe_pack_path(path) && read_all(pack / path, data))
                return true;
            return safe_pack_path("vendors/" + path) && read_all(pack / ("vendors/" + path), data);
        };
        std::string model_text;
        if (!read_pack(file, model_text)) {
            err << "error: the printer's model file is not installed: " << file << "\n";
            return kExitInput;
        }
        std::vector<std::string> warnings;
        const Profile printer_profile = pack_printer_profile(model_text, variant, warnings);
        Settings settings = with_profile(Settings{}, ProfileKind::Printer, printer_profile.settings);
        // Rule 13: the variant's first filament and its print profile by default.
        std::string filament = o["--filament"], print = o["--print"];
        const json model = json::parse(model_text, nullptr, false);
        if (model.is_object() && model.contains("variants") && model.at("variants").is_array())
            for (const json& v : model.at("variants"))
                if (v.is_object() && v.value("id", "") == variant && v.contains("defaults") && v.at("defaults").is_object()) {
                    const json& d = v.at("defaults");
                    if (filament.empty() && d.contains("filament") && d.at("filament").is_array() && !d.at("filament").empty() &&
                        d.at("filament").front().is_string())
                        filament = d.at("filament").front().get<std::string>();
                    if (print.empty() && d.contains("print") && d.at("print").is_string())
                        print = d.at("print").get<std::string>();
                }
        std::vector<std::vector<int>> sets;
        std::string sets_text;
        if (read_all(pack / "compatible.json", sets_text))
            sets = parse_pack_compatible(sets_text);
        const int printer_number = int(number - index.printers.begin());
        for (const auto& [path, kind] : {std::pair{filament, ProfileKind::Filament}, std::pair{print, ProfileKind::Print}}) {
            if (path.empty())
                continue;
            std::string text;
            if (!read_pack(path, text)) {
                err << "error: not in the installed pack: " << path << "\n";
                return kExitInput;
            }
            const PackEntry entry = parse_pack_entry(text, warnings);
            if (entry.profile.kind != kind) {
                err << "error: " << path << " is not a " << (kind == ProfileKind::Filament ? "filament" : "print") << " profile\n";
                return kExitInput;
            }
            if (!pack_compatible(entry.compatible, sets, printer_number)) { // rule 12, PK7
                err << "error: " << entry.profile.name << " is not for the " << printer_profile.name << "\n";
                return kExitInput;
            }
            settings = with_profile(settings, kind, entry.profile.settings);
        }
        for (const std::string& w : warnings)
            err << "warning: " << w << "\n";
        const std::string text = settings_json(settings);
        if (!o.count("-o") || o["-o"] == "-") {
            out << text;
            return kExitOk;
        }
        std::ofstream f(o["-o"], std::ios::binary | std::ios::trunc);
        f << text;
        if (!f) {
            err << "error: cannot write " << o["-o"] << "\n";
            return kExitInput;
        }
        return kExitOk;
    } catch (const PackError& e) {
        err << "error: " << e.what() << "\n";
        return kExitInput;
    }
}

} // namespace tisma
