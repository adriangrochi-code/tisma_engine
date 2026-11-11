#include "cli/cli.hpp"
#include "io/number.hpp"
#include "cli/node.hpp"
#include "cli/library_cli.hpp"
#include "cli/pack_cli.hpp"
#include "cli/part_cli.hpp"
#include "cli/calibration_cli.hpp"
#include "cli/design_cli.hpp"
#include "catalog/catalog.hpp"
#include "config/profiles.hpp"

#include "io/model.hpp"
#include "io/paths.hpp"
#include "io/stl.hpp"
#include "slice/placement.hpp"
#include "io/ply.hpp"
#include "io/vtk.hpp"
#include "config/analysis_case.hpp"
#include "config/settings.hpp"
#include "fea/analysis.hpp"
#include "project/engineering_case.hpp"
#include "project/project.hpp"
#include "report/analysis_json.hpp"
#include "gcode/export.hpp"
#include "gcode/thumbnail.hpp"
#include "gcode/output.hpp"
#include "gcode/template.hpp"
#include "gcode/writer.hpp"
#include "report/stability.hpp"
#include "report/warnings.hpp"
#include "geom/medial_axis.hpp"
#include "geom/polygons.hpp"
#include "gpu/compute.hpp"
#include "mesh/indexed_mesh.hpp"
#include "mesh/inspect.hpp"
#include "gcode/preview.hpp"
#include "perimeters/sites.hpp"
#include "perimeters/transitions.hpp"
#include "slice/layers.hpp"
#include "slice/color_mix.hpp"
#include "slice/pipeline.hpp"
#include "slice/printable.hpp"
#include "slice/transform.hpp"
#include "resin/goo.hpp"
#include "resin/resin.hpp"

#include <oneapi/tbb/global_control.h>

#include <algorithm>
#include <nlohmann/json.hpp>
#include <array>
#include <cmath>
#include <cstring>
#include <sstream>
#include <map>
#include <charconv>
#include <cctype>
#include <cstdlib>
#include <chrono>
#include <exception>
#include <filesystem>
#include <iostream>
#include <fstream>
#include <functional>
#include <memory>
#include <optional>
#include <vector>
#include <ostream>
#include <string>

namespace tisma {

// Declared in cli/node.hpp.
bool write_file(const std::string& path, const std::function<void(std::ostream&)>& write) {
    // A link's file, next to it (BH16); never over a device or a folder.
    const std::filesystem::path target = write_target(path);
    if (!output_problem(target).empty())
        return false;
    std::filesystem::path part = target;
    part += ".part";
    std::error_code ignored;
    {
        std::ofstream f(part, std::ios::binary);
        if (!f)
            return false;
        write(f);
        f.close();
        if (!f) {
            std::filesystem::remove(part, ignored);
            return false;
        }
    }
    std::error_code ec;
    std::filesystem::rename(part, target, ec);
    if (ec) {
        std::filesystem::remove(part, ignored);
        return false;
    }
    return true;
}

namespace {

constexpr std::string_view kUsage =
    "usage:\n"
    "  tisma-engine slice <model> --config <settings.json> -o <out.gcode | out.goo | folder> [--warnings <out.json>]\n"
    "                     (a .goo file: for a resin printer, docs/spec/features/resin.md)\n"
    "                     [--case <case.json>]   (the load case of infill.stress_density)\n"
    "                     [--yes]   (run the settings' post-processing scripts without asking)\n"
    "                     [--summary <summary.json>]   (the nodes' summary: print time, filament, layers)\n"
    "                     [--copies <n>]   (n copies of the model, arranged; sequential.enabled prints them one by one)\n"
    "                     [--mixes <mixes.json> --mix <n>]   (the blends, as a project keeps them, and the one the model prints\n"
    "                      with: docs/spec/features/color_mixing.md, rule 15)\n"
    "                     [--align-to-y-axis]   (the copies in one column along Y)\n"
    "                     [--strict]   (a warning is an error: no G-code, exit code 4)\n"
    "                     [--ground-largest-face | --ground-face-normal X,Y,Z | --ground-face-point X,Y,Z]\n"
    "                     [--inspect-mesh <out.json|->] [--export-statistics <out.json|->] [--export-settings <out.json|->]\n"
    "  tisma-engine slice <project> -o <out.gcode> --summary <summary.json> [--sandbox] [--no-gpu] [--yes]\n"
    "                     (a project saved by the interface, as the cloud's nodes run it: docs/spec/features/cloud.md)\n"
    "  on a project's slice, analyze, aero and orient: [--config-over <partial.json>] [--set group.key=value ...]\n"
    "  aero and orient also take a model with --config <settings.json> instead of a project\n"
    "                     (changes over the project's own settings)\n"
    "  tisma-engine analyze <model> --config <settings.json> --case <case.json> -o <report.json>\n"
    "                       [--vtk <map.vtk>] [--mesh <voxels.ply>] [--progress] [--estimate]\n"
    "  tisma-engine analyze <project> -o <report.json> [--config <settings.json>] [...]\n"
    "                       (the first part with Engineering conditions, as the project keeps them)\n"
    "  tisma-engine analyze <project> -o <result.tea> --summary <summary.json> --sandbox --mode analyze\n"
    "                       --quality <fast|normal|high|extreme> --object <n> --instance <n> [--solid] [--no-gpu]\n"
    "                       (the nodes' way: progress lines, the result and a JSON summary)\n"
    "  tisma-engine aero <project> -o <result.taero> --summary <summary.json> --sandbox --object <n> --instance <n>\n"
    "                    --quality <friction|fast|normal|high|extreme> --speed <m/s> --direction <x|-x|y|-y|z|-z|x,y,z>\n"
    "  tisma-engine orient <project> -o <orientation.json> --summary <summary.json> --sandbox --object <n> --instance <n>\n"
    "                      (least support for FDM: the rotation, its cost and the cost before)\n"
    "  tisma-engine profile --printer \"<vendor> <model>\" [--nozzle <mm>] [--filament \"<vendor> <name>\"] -o <settings.json>\n"
    "                       (the settings the engine makes from its catalog, for slice --config)\n"
    "  tisma-engine profile --pack <folder> --printer \"<model file>#<variant>\" [--filament <path>] [--print <path>] -o <settings.json>\n"
    "                       (a printer of the installed profile pack, with its default filament and print profile)\n"
    "  tisma-engine pack plan|install|list --data <folder> ...   (the profile pack: tisma-engine pack for help)\n"
    "  tisma-engine library list|show|save|check|printed|delete|export|import --dir <folder> ...   (My library)\n"
    "  tisma-engine part gear|fastener <spec.json> -o <out.stl> | --defaults   (a gear, bolt, nut, washer or rod)\n"
    "  tisma-engine calibration <kind> --config <settings.json> --model <out.stl> --settings <out.json> [range]\n"
    "  tisma-engine calibration apply <kind> --value <v> --config <settings.json> -o <out.json>\n"
    "  tisma-engine design new | check <design.json> | build <design.json> --solidcraft <exe> -o <out.stl|out.3mf>\n"
    "                       [--quality standard|fine|finest] [--timeline] [--save <design.sc>]\n"
    "  tisma-engine info <model>\n"
    "  --threads <n> on any command: at most n threads of work (cloud.md, rule 19d); without it, all\n"
    "  tisma-engine --help | --version\n"
    "A model is an STL, OBJ or 3MF file.\n";

struct SliceArgs {
    std::string model;
    std::string config;
    std::string output;
    std::string warnings; // empty: not written
    std::string summary;  // slice only: the nodes' JSON summary; empty: not written
    std::string analysis_case; // analyze; slice: the load case of stress-guided infill
    std::string vtk;           // analyze only; empty: not written
    std::string mesh;          // analyze only; empty: not written
    bool progress = false;     // analyze only: progress lines on the output
    bool estimate = false;     // analyze only: the voxels, memory and time, without solving
    bool yes = false;          // slice only: run the settings' post-processing scripts without asking
    std::size_t copies = 1;    // slice only: copies of the model on the bed
    bool project = false;      // analyze only: the model is a project, with its settings and conditions
    // orca_2_5.md, O6 (slice only): warnings as errors (rule 24); the face put
    // on the bed (25); the mesh's summary (26); statistics and the resolved
    // settings, "-" for the standard output (27); copies along Y (28).
    bool strict = false;
    enum class Ground { None, Largest, Normal, Point } ground = Ground::None;
    std::array<double, 3> ground_value{0.0, 0.0, 0.0};
    std::string inspect_mesh;
    std::string statistics;
    std::string export_settings;
    bool align_y = false;
    // Blends (color_mixing.md, rule 15) and the one the model prints with.
    std::string mixes;
    int mix = 0;
};

// Returns the arguments of 'slice' or 'analyze', or writes the problem to err.
std::optional<SliceArgs> parse_slice_args(std::span<const std::string_view> args, std::ostream& err,
                                          bool analyze = false) {
    SliceArgs out;
    for (std::size_t i = 0; i < args.size(); ++i) {
        const std::string_view a = args[i];
        auto value = [&](std::string& dst) {
            if (i + 1 >= args.size()) {
                err << "error: " << a << " needs a value\n";
                return false;
            }
            dst = std::string(args[++i]);
            return true;
        };
        if (a == "--config" || a == "-c") {
            if (!value(out.config))
                return std::nullopt;
        } else if (a == "-o" || a == "--output") {
            if (!value(out.output))
                return std::nullopt;
        } else if (a == "--warnings" && !analyze) {
            if (!value(out.warnings))
                return std::nullopt;
        } else if (a == "--summary" && !analyze) {
            if (!value(out.summary))
                return std::nullopt;
        } else if (a == "--case") {
            if (!value(out.analysis_case))
                return std::nullopt;
        } else if (a == "--vtk" && analyze) {
            if (!value(out.vtk))
                return std::nullopt;
        } else if (a == "--mesh" && analyze) {
            if (!value(out.mesh))
                return std::nullopt;
        } else if (a == "--progress" && analyze) {
            out.progress = true;
        } else if (a == "--estimate" && analyze) {
            out.estimate = true;
        } else if (a == "--yes" && !analyze) {
            out.yes = true;
        } else if (a == "--strict" && !analyze) {
            out.strict = true;
        } else if (a == "--align-to-y-axis" && !analyze) {
            out.align_y = true;
        } else if (a == "--ground-largest-face" && !analyze) {
            out.ground = SliceArgs::Ground::Largest;
        } else if ((a == "--ground-face-normal" || a == "--ground-face-point") && !analyze) {
            std::string v;
            if (!value(v))
                return std::nullopt;
            std::array<double, 3> xyz{};
            char* end = v.data();
            bool ok = true;
            for (std::size_t k = 0; k < 3 && ok; ++k) {
                const char* from = end;
                xyz[k] = std::strtod(from, &end);
                ok = end != from && std::isfinite(xyz[k]) && (k == 2 ? *end == '\0' : *end++ == ',');
            }
            if (!ok || (a == "--ground-face-normal" && xyz[0] == 0.0 && xyz[1] == 0.0 && xyz[2] == 0.0)) {
                err << "error: " << a << " needs X,Y,Z" << (a == "--ground-face-normal" ? " (not all 0)" : "") << "\n";
                return std::nullopt;
            }
            out.ground = a == "--ground-face-normal" ? SliceArgs::Ground::Normal : SliceArgs::Ground::Point;
            out.ground_value = xyz;
        } else if (a == "--inspect-mesh" && !analyze) {
            if (!value(out.inspect_mesh))
                return std::nullopt;
        } else if (a == "--export-statistics" && !analyze) {
            if (!value(out.statistics))
                return std::nullopt;
        } else if (a == "--export-settings" && !analyze) {
            if (!value(out.export_settings))
                return std::nullopt;
        } else if (a == "--copies" && !analyze) {
            std::string n;
            if (!value(n))
                return std::nullopt;
            char* end = nullptr;
            const long v = std::strtol(n.c_str(), &end, 10);
            if (n.empty() || *end != '\0' || v < 1 || v > 1000) {
                err << "error: --copies needs a whole number from 1 to 1000\n";
                return std::nullopt;
            }
            out.copies = std::size_t(v);
        } else if (a == "--mixes" && !analyze) {
            if (!value(out.mixes))
                return std::nullopt;
        } else if (a == "--mix" && !analyze) {
            std::string n;
            if (!value(n))
                return std::nullopt;
            char* end = nullptr;
            const long v = std::strtol(n.c_str(), &end, 10);
            if (n.empty() || *end != '\0' || v < 2 || v > 255) {
                err << "error: --mix needs a blend's number, from 2 to 255\n";
                return std::nullopt;
            }
            out.mix = int(v);
        } else if (!a.empty() && a.front() == '-') {
            err << "error: unknown option " << a << "\n";
            return std::nullopt;
        } else if (out.model.empty()) {
            out.model = std::string(a);
        } else {
            err << "error: more than one model given\n";
            return std::nullopt;
        }
    }
    // A project carries its settings and its parts' conditions (formats.md,
    // rule 11 bis): analyze needs only it and -o; --case replaces the
    // conditions, --config the settings.
    const bool project = analyze && !out.model.empty() && is_project_file(out.model);
    out.project = project;
    if (analyze && !project &&
        (out.model.empty() || out.config.empty() || out.analysis_case.empty() || out.output.empty())) {
        err << "error: analyze needs a model, --config, --case and -o (or a project and -o)\n";
        return std::nullopt;
    }
    if (out.model.empty() || (out.config.empty() && !project) || out.output.empty()) {
        err << "error: slice needs a model, --config and -o\n";
        return std::nullopt;
    }
    return out;
}

// No output over an input or another output, nor over something that is not
// a regular file (the spec team's BH15 and BH16: -o m.stl replaced the
// model, --warnings equal to -o wrote over the G-code, -o /dev/full replaced
// the device). False, with the problem in err.
bool outputs_are_safe(const SliceArgs& a, std::ostream& err) {
    const std::pair<const char*, const std::string*> inputs[] = {
        {"the model", &a.model}, {"--config", &a.config}, {"--case", &a.analysis_case}};
    const std::pair<const char*, const std::string*> outputs[] = {
        {"-o", &a.output},           {"--warnings", &a.warnings},          {"--vtk", &a.vtk},
        {"--mesh", &a.mesh},         {"--summary", &a.summary},            {"--inspect-mesh", &a.inspect_mesh},
        {"--export-statistics", &a.statistics}, {"--export-settings", &a.export_settings}};
    for (std::size_t i = 0; i < std::size(outputs); ++i) {
        const auto& [name, path] = outputs[i];
        if (path->empty() || *path == "-") // "-": the standard output
            continue;
        // -o may be a folder: the file gets its name there (gcode_templates.md, rule 15).
        const bool folder = i == 0 && std::filesystem::is_directory(*path);
        if (const std::string why = output_problem(write_target(*path)); !why.empty() && !folder) {
            err << "error: " << name << " '" << *path << "' " << why << "\n";
            return false;
        }
        for (const auto& [other, input] : inputs)
            if (!input->empty() && same_file(*path, *input)) {
                err << "error: " << name << " '" << *path << "' is the same file as " << other << ": it would be lost\n";
                return false;
            }
        for (std::size_t j = 0; j < i; ++j)
            if (!outputs[j].second->empty() && *outputs[j].second != "-" && same_file(*path, *outputs[j].second)) {
                err << "error: " << name << " and " << outputs[j].first << " are the same file\n";
                return false;
            }
    }
    return true;
}

void print_mesh_summary(const ModelFile& stl, double seconds, std::ostream& out) {
    const BoundingBox box = stl.mesh.bounding_box();
    out << "format: "
        << (stl.format == ModelFormat::StlBinary  ? "binary"
            : stl.format == ModelFormat::StlAscii ? "ascii"
            : stl.format == ModelFormat::Obj      ? "obj"
                                                  : "3mf")
        << "\n";
    if (stl.format == ModelFormat::ThreeMf)
        out << "objects: " << stl.objects << "\nparts: " << stl.parts << "\n";
    out << "triangles: " << stl.mesh.triangle_count() << "\n";
    if (!box.empty) {
        out << "size: " << (box.max.x - box.min.x) << " x " << (box.max.y - box.min.y) << " x "
            << (box.max.z - box.min.z) << "\n";
        out << "min: " << box.min.x << " " << box.min.y << " " << box.min.z << "\n";
        out << "max: " << box.max.x << " " << box.max.y << " " << box.max.z << "\n";
    }
    out << "volume: " << stl.mesh.signed_volume() << "\n";
    out << "read_ms: " << seconds * 1000.0 << "\n";
}

int load_model(const std::string& path, ModelFile& stl, double& seconds, std::ostream& err) {
    const auto start = std::chrono::steady_clock::now();
    try {
        stl = read_model(path);
    } catch (const ModelError& e) {
        err << "error: " << path << ": " << e.what() << "\n";
        return kExitInput;
    }
    seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    return kExitOk;
}

namespace {

std::string lower(std::string_view text) {
    std::string out(text);
    for (char& c : out)
        c = char(std::tolower(static_cast<unsigned char>(c)));
    return out;
}

// One catalog entry by its full name ("Vendor Model", any case), else the
// only one holding every word; null with the reason otherwise.
template <class Entry>
const Entry* pick_entry(const std::vector<Entry>& all, const std::vector<const Entry*>& found,
                        const std::string& query, const char* what, std::string& reason,
                        std::string (*name_of)(const Entry&)) {
    for (const Entry& e : all)
        if (lower(name_of(e)) == lower(query))
            return &e;
    // The name without the maker's ("Prusament PLA" of Prusament's), when it
    // is only one's: other names holding its words do not make it ambiguous.
    const Entry* by_model = nullptr;
    int models = 0;
    const std::string tail = " " + lower(query);
    for (const Entry& e : all) {
        const std::string name = lower(name_of(e));
        if (name.size() > tail.size() && name.compare(name.size() - tail.size(), tail.size(), tail) == 0)
            by_model = &e, ++models;
    }
    if (models == 1)
        return by_model;
    if (found.size() == 1)
        return found.front();
    if (found.empty()) {
        reason = std::string("no ") + what + " in the catalog is called '" + query + "'";
    } else {
        reason = std::string("'") + query + "' names " + std::to_string(found.size()) + " " + what + "s:";
        for (std::size_t i = 0; i < found.size() && i < 8; ++i)
            reason += (i ? ", " : " ") + name_of(*found[i]);
        if (found.size() > 8)
            reason += "...";
    }
    return nullptr;
}

std::string printer_name(const PrinterModel& p) { return p.vendor + " " + p.model; }
std::string filament_name(const FilamentModel& f) { return f.vendor + " " + f.name; }

} // namespace

// The settings the engine makes from its catalog (the Android app's request,
// 2026-11-04): the printer's profile for the nozzle and, when named, the
// filament's, the rest the defaults; the same document slice --config reads.
int cmd_profile(std::span<const std::string_view> args, std::ostream& out, std::ostream& err) {
    std::string printer, filament, output, nozzle_text, usage;
    for (std::size_t i = 0; i < args.size(); ++i) {
        const std::string_view a = args[i];
        std::string* value = a == "--printer" ? &printer : a == "--filament" ? &filament : a == "--nozzle" ? &nozzle_text
                             : a == "-o"      ? &output
                                              : nullptr;
        if (value && i + 1 < args.size())
            *value = std::string(args[++i]);
        else if (usage.empty())
            usage = value ? "profile: " + std::string(a) + " needs a value" : "profile: unexpected argument '" + std::string(a) + "'";
    }
    if (usage.empty() && printer.empty())
        usage = "profile needs --printer";
    double nozzle = 0.0;
    if (usage.empty() && !nozzle_text.empty()) {
        const auto [end, ec] = chars_to(nozzle_text.data(), nozzle_text.data() + nozzle_text.size(), nozzle);
        if (ec != std::errc() || end != nozzle_text.data() + nozzle_text.size() || !(nozzle > 0.0))
            usage = "profile: --nozzle needs a diameter in mm";
    }
    if (!usage.empty()) {
        err << "error: " << usage << "\n" << kUsage;
        return kExitUsage;
    }
    const Catalog& catalog = builtin_catalog();
    std::string reason;
    const PrinterModel* p = pick_entry(catalog.printers, find_printers(catalog, printer), printer, "printer", reason, printer_name);
    if (!p) {
        err << "error: " << reason << "\n";
        return kExitInput;
    }
    if (nozzle_text.empty()) {
        nozzle = p->nozzles.empty() ? 0.4 : p->nozzles.front();
    } else if (!p->nozzles.empty() &&
               std::none_of(p->nozzles.begin(), p->nozzles.end(), [&](double n) { return std::abs(n - nozzle) < 1e-6; })) {
        std::string offered;
        for (double n : p->nozzles) {
            char buf[32];
            const auto [end, ec] = std::to_chars(buf, buf + sizeof buf, n);
            offered += (offered.empty() ? "" : ", ") + std::string(buf, ec == std::errc() ? end : buf);
        }
        err << "error: the " << printer_name(*p) << " takes nozzles of " << offered << " mm, not " << nozzle_text << "\n";
        return kExitInput;
    }
    Settings settings = with_profile(Settings{}, ProfileKind::Printer, printer_profile(*p, nozzle).settings);
    if (!filament.empty()) {
        const FilamentModel* f =
            pick_entry(catalog.filaments, find_filaments(catalog, filament), filament, "filament", reason, filament_name);
        if (!f) {
            err << "error: " << reason << "\n";
            return kExitInput;
        }
        if (!filament_offered(*f, settings.feedstock, {p->filament_diameter})) {
            err << "error: " << filament_name(*f) << " does not fit the " << printer_name(*p) << " (its filament or feedstock differs)\n";
            return kExitInput;
        }
        settings = with_profile(settings, ProfileKind::Filament, filament_profile(*f).settings);
    }
    const std::string json = settings_json(settings);
    if (output.empty() || output == "-") {
        out << json;
        return kExitOk;
    }
    if (!write_file(output, [&](std::ostream& f) { f << json; })) {
        err << "error: cannot write '" << output << "'\n";
        return kExitInput;
    }
    return kExitOk;
}

int cmd_info(std::span<const std::string_view> args, std::ostream& out, std::ostream& err) {
    if (args.size() != 1) {
        err << "error: info needs exactly one model\n" << kUsage;
        return kExitUsage;
    }
    ModelFile stl;
    double seconds = 0.0;
    if (int rc = load_model(std::string(args[0]), stl, seconds, err); rc != kExitOk)
        return rc;
    print_mesh_summary(stl, seconds, out);
    const auto start = std::chrono::steady_clock::now();
    const IndexedMesh indexed = weld_exact(stl.mesh);
    const double weld_ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    out << "vertices: " << indexed.vertices.size() << "\n";
    out << "weld_ms: " << weld_ms << "\n";

    // Models the engine refuses to slice are not sliced here either (one at
    // 1e30 mm used to grow memory without end).
    if (const std::string reason = unprintable_reason(indexed, Settings{}); !reason.empty()) {
        out << "slicing: skipped (" << reason << ")\n";
        return kExitOk;
    }

    // Planar slicing with 0.2 mm layers: level-set segments and contours.
    const BoundingBox box = stl.mesh.bounding_box();
    if (!box.empty) {
        const auto slice_start = std::chrono::steady_clock::now();
        std::vector<float> heights;
        for (const Layer& layer : plan_layers(box.min.z, box.max.z, {0.2, 0.2}))
            heights.push_back(float(layer.slice_z()));
        const auto layers = slice_levels(indexed, height_field(indexed), heights);
        const double slice_ms =
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - slice_start).count();
        std::size_t contours = 0;
        for (const auto& layer : layers)
            contours += layer.size();
        out << "layers: " << layers.size() << "\n";
        out << "contours: " << contours << "\n";
        out << "slice_ms: " << slice_ms << "\n";
        if (layers.empty())
            return kExitOk; // flat or thinner than a layer: no middle layer to look at

        // Regions: closed contours merged into polygons with holes (Clipper2).
        const auto region_start = std::chrono::steady_clock::now();
        const std::vector<Polygons> regions = layer_regions(layers);
        const double region_ms =
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - region_start).count();
        double volume = 0.0;
        for (std::size_t l = 0; l < regions.size(); ++l)
            volume += area_mm2(regions[l]) * 0.2;
        out << "region_ms: " << region_ms << "\n";

        // Medial axis of the middle layer (F2), with the centre marked, on the
        // region as the slicing builds it (simplified with the default tolerance).
        const std::size_t mid = layers.size() / 2;
        const Polygons mid_region = layer_region(simplify(to_polygons(layers[mid]), Settings{}.simplify_tolerance));
        const auto axis_start = std::chrono::steady_clock::now();
        MedialGraph axis = medial_graph(mid_region);
        mark_central(axis);
        const double axis_ms =
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - axis_start).count();
        out << "mid_layer_axis_edges: " << axis.edges.size() << "\n";
        out << "mid_layer_axis_ms: " << axis_ms << "\n";

        // Variable-width perimeters of the middle layer (F2, Kuipers et al. 2020).
        const auto walls_start = std::chrono::steady_clock::now();
        const BeadingParams beading;
        filter_marking(axis, beading.preferred_width);
        const TransitionResult transitions = apply_transitions(axis, beading);
        const std::vector<Toolpath> walls = extract_toolpaths(axis, node_beadings(axis, transitions.bead_counts, beading));
        const double walls_ms =
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - walls_start).count();
        out << "mid_layer_toolpaths: " << walls.size() << "\n";
        out << "mid_layer_toolpaths_ms: " << walls_ms << "\n";
        out << "sliced_volume: " << volume << "\n";
    }
    return kExitOk;
}

// The model as the settings' transform group leaves it (model_tools.md,
// rules 11 to 14), placed on the bed: one object, its copies, or the pieces a
// cut or a split made, side by side. False, with the problem in err, when
// pieces and copies are both asked for.
bool place_model(TriangleMesh& mesh, const Settings& settings, std::size_t copies, const std::string& name,
                 std::vector<MeshObject>& objects, std::ostream& err, TriangleMesh* negative = nullptr,
                 bool along_y = false) {
    if (negative && negative->empty())
        negative = nullptr;
    // The negative volumes (model_tools.md, rule 2) follow the part; not yet
    // through a cut, a split or copies.
    if (negative && (copies > 1 || settings.transform_cut_z || settings.transform_split)) {
        err << "error: negative volumes and support blockers or enforcers do not go with --copies, transform.cut_z or "
               "transform.split yet\n";
        return false;
    }
    std::vector<TriangleMesh> parts = apply_transform(std::move(mesh), settings, negative);
    // Every object the model makes, with the object group's extruder and
    // purges (multi_extruder.md, rules 1 and 13).
    const auto as_objects = [&] {
        for (MeshObject& o : objects) {
            o.extruder = settings.object_extruder;
            o.wipe_into_infill = settings.object_wipe_into_infill;
            o.wipe_into_object = settings.object_wipe_into_object;
        }
        return true;
    };
    if (parts.size() > 1) {
        if (copies > 1) {
            err << "error: --copies does not go with transform.cut_z or transform.split\n";
            return false;
        }
        mesh = place_objects(parts, settings, name, &objects);
        return as_objects();
    }
    mesh = parts.empty() ? TriangleMesh{} : std::move(parts.front());
    if (copies > 1) {
        if (const std::size_t fit = copies_that_fit(mesh, copies, settings, along_y); fit < copies) {
            err << "error: " << copies << " copies do not fit on the bed (machine.bed): " << fit << " fit\n";
            return false;
        }
        mesh = place_copies(mesh, copies, settings, name, &objects, along_y);
    } else {
        place_on_bed(mesh, settings, negative);
        objects.push_back({name, 0, mesh.triangle_count(), 0, 0});
    }
    return as_objects();
}

// Whether the output is a resin printer's GOO file.
bool goo_output(const std::string& path) {
    std::string ext = std::filesystem::path(path).extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return char(std::tolower(c)); });
    return ext == ".goo";
}

// Resin (resin.md): the part's images and the platform's moves in a GOO
// file, the part centred on the display unless the settings place it.
int slice_resin_file(const SliceArgs& args, Settings settings, ModelFile& model, std::ostream& out, std::ostream& err) {
    if (settings.bed.empty())
        settings.bed = {{0.0, 0.0},
                        {settings.display_width, 0.0},
                        {settings.display_width, settings.display_height},
                        {0.0, settings.display_height}};
    std::vector<MeshObject> objects;
    if (!place_model(model.mesh, settings, args.copies, std::filesystem::path(args.model).filename().string(), objects,
                     err))
        return kExitUsage;
    const auto start = std::chrono::steady_clock::now();
    ResinPrint print;
    try {
        print = slice_resin(weld_exact(model.mesh), settings);
    } catch (const ResinError& e) {
        err << "error: " << e.what() << "\n";
        return kExitModel;
    }
    out << "layers: " << print.layers.size() << "\n";
    out << "volume_mm3: " << print.volume << "\n";
    out << "weight_g: " << print.weight << "\n";
    out << "print_seconds: " << std::llround(print.seconds) << "\n";
    out << "slice_ms: " << std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count()
        << "\n";
    if (!write_file(args.output, [&](std::ostream& f) { write_goo(f, print, settings, goo_time_now()); })) {
        err << "error: cannot write '" << args.output << "'\n";
        return kExitInput;
    }
    out << "wrote " << args.output << "\n";
    // The cups (resin.md, rules 13 to 15).
    const std::vector<Warning> warnings = resin_warnings(print, std::filesystem::path(args.model).filename().string());
    out << "warnings: " << warnings.size() << "\n";
    if (!args.warnings.empty()) {
        if (!write_file(args.warnings, [&](std::ostream& report) { report << warnings_json(warnings); })) {
            err << "error: cannot write '" << args.warnings << "'\n";
            return kExitInput;
        }
        out << "wrote " << args.warnings << "\n";
    }
    return kExitOk;
}

// A text to a file, or to the standard output for "-".
bool write_text_or_stdout(const std::string& path, const std::string& text, std::ostream& out) {
    if (path == "-") {
        out << text;
        return true;
    }
    if (!write_file(path, [&](std::ostream& f) { f << text; }))
        return false;
    out << "wrote " << path << "\n";
    return true;
}

// --export-statistics (orca_2_5.md, rule 27): for each plate (one here), the
// time, the filament by extruder (mm, cm3, g, cost) and the layers, from the
// G-code's closing summary.
std::string gcode_statistics_json(const std::filesystem::path& gcode, std::size_t layers) {
    std::ifstream in(gcode, std::ios::binary);
    std::string line, time;
    std::map<std::string, std::string> lists;
    const std::pair<const char*, const char*> keys[] = {{"; filament used [mm] = ", "mm"},
                                                        {"; filament used [cm3] = ", "cm3"},
                                                        {"; filament used [g] = ", "g"},
                                                        {"; filament cost = ", "cost"}};
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        if (line.starts_with("; estimated printing time = "))
            time = line.substr(std::strlen("; estimated printing time = "));
        for (const auto& [key, name] : keys)
            if (line.starts_with(key))
                lists[name] = line.substr(std::strlen(key));
    }
    nlohmann::json filament = nlohmann::json::array();
    std::map<std::string, std::vector<double>> values;
    std::size_t extruders = 0;
    for (const auto& [name, text] : lists) {
        std::stringstream s(text);
        for (std::string item; std::getline(s, item, ',');)
            values[name].push_back(std::atof(item.c_str()));
        extruders = std::max(extruders, values[name].size());
    }
    for (std::size_t e = 0; e < extruders; ++e) {
        nlohmann::json one{{"extruder", e + 1}};
        for (const char* name : {"mm", "cm3", "g", "cost"})
            if (values.count(name) && e < values[name].size())
                one[name] = values[name][e];
        filament.push_back(one);
    }
    const nlohmann::json plate{{"plate", 1},
                               {"print_time", time},
                               {"print_seconds", parse_duration(time)},
                               {"layers", layers},
                               {"filament", filament}};
    return nlohmann::json{{"plates", nlohmann::json::array({plate})}}.dump(2);
}

int cmd_slice(std::span<const std::string_view> args, std::ostream& out, std::ostream& err) {
    const std::optional<SliceArgs> parsed = parse_slice_args(args, err);
    if (!parsed) {
        err << kUsage;
        return kExitUsage;
    }
    if (!outputs_are_safe(*parsed, err))
        return kExitInput;
    Settings settings;
    try {
        settings = load_settings(parsed->config);
    } catch (const SettingsError& e) {
        err << "error: " << parsed->config << ": " << e.what() << "\n";
        return kExitInput;
    }
    for (const std::string& group : settings.ignored_groups)
        err << "warning: settings group '" << group << "' is not used yet\n";
    // A resin printer prints a GOO file (resin.md); a .goo file is for one.
    if (settings.machine_technology == MachineTechnology::Msla && !goo_output(parsed->output)) {
        err << "error: machine.technology is msla: the output must be a .goo file\n";
        return kExitInput;
    }
    if (goo_output(parsed->output)) {
        ModelFile model;
        double seconds = 0.0;
        if (int rc = load_model(parsed->model, model, seconds, err); rc != kExitOk)
            return rc;
        out << "read " << model.mesh.triangle_count() << " triangles in " << seconds * 1000.0 << " ms\n";
        return slice_resin_file(*parsed, settings, model, out, err);
    }
    // The templates' input_filename_base (gcode_templates.md, rule 7).
    settings.input_name = std::filesystem::path(parsed->model).stem().string();
    // The speeds come from the printer's profile; those the file does not give
    // are a stock Ender-3's (docs/CONFIG.md, "Velocidades de respaldo").
    std::string fallback;
    for (const std::string& key : fill_missing_speeds(settings))
        fallback += (fallback.empty() ? "" : ", ") + key;
    if (!fallback.empty())
        err << "warning: no speed for " << fallback << ": using a stock Ender-3's\n";

    ModelFile stl;
    double seconds = 0.0;
    if (int rc = load_model(parsed->model, stl, seconds, err); rc != kExitOk)
        return rc;
    out << "read " << stl.mesh.triangle_count() << " triangles in " << seconds * 1000.0 << " ms\n";
    // A face on the bed (orca_2_5.md, rule 25), before the transforms and the placement.
    if (parsed->ground != SliceArgs::Ground::None) {
        if (!stl.negative.empty() || !stl.support_blockers.empty() || !stl.support_enforcers.empty()) {
            err << "error: --ground-* does not go with negative volumes or support blockers and enforcers yet\n";
            return kExitUsage;
        }
        const std::optional<std::array<double, 3>> normal =
            parsed->ground == SliceArgs::Ground::Largest ? largest_flat_face(stl.mesh)
            : parsed->ground == SliceArgs::Ground::Point ? face_through(stl.mesh, parsed->ground_value)
                                                         : std::optional<std::array<double, 3>>(parsed->ground_value);
        if (!normal) {
            err << "error: " << (parsed->ground == SliceArgs::Ground::Point ? "no face of the model passes through that point"
                                                                           : "the model has no face")
                << "\n";
            return kExitModel;
        }
        stl.mesh = laid_on_face(stl.mesh, *normal);
    }
    // The mesh's summary (rule 26).
    if (!parsed->inspect_mesh.empty()) {
        const std::string json = inspection_json(inspect_mesh(stl.mesh));
        if (!write_text_or_stdout(parsed->inspect_mesh, json + "\n", out)) {
            err << "error: cannot write '" << parsed->inspect_mesh << "'\n";
            return kExitInput;
        }
    }
    // The resolved settings (rule 27).
    if (!parsed->export_settings.empty() &&
        !write_text_or_stdout(parsed->export_settings, settings_json(settings) + "\n", out)) {
        err << "error: cannot write '" << parsed->export_settings << "'\n";
        return kExitInput;
    }
    // The copies, each an object of the plate (placement.md, rule 2;
    // sequential.md, rule 2), or the model alone.
    std::vector<MeshObject> objects;
    const std::string file_name = std::filesystem::path(parsed->model).filename().string();
    // The negative volumes and the support blockers and enforcers follow the
    // part together, then are taken apart again (their triangles in order).
    const std::size_t negatives = stl.negative.triangle_count(), blockers = stl.support_blockers.triangle_count();
    TriangleMesh followers = stl.negative;
    for (const TriangleMesh* more : {&stl.support_blockers, &stl.support_enforcers})
        for (std::size_t t = 0; t < more->triangle_count(); ++t)
            followers.add_triangle(more->corner(t, 0), more->corner(t, 1), more->corner(t, 2));
    if (!place_model(stl.mesh, settings, parsed->copies, file_name, objects, err, &followers, parsed->align_y))
        return kExitUsage;
    // The blends and the one the model prints with (color_mixing.md, rule 15).
    std::vector<ColorMix> mixes;
    if (!parsed->mixes.empty()) {
        std::ifstream in(parsed->mixes, std::ios::binary);
        std::ostringstream text;
        text << in.rdbuf();
        if (!in) {
            err << "error: cannot read '" << parsed->mixes << "'\n";
            return kExitInput;
        }
        try {
            mixes = parse_color_mixes(text.str());
        } catch (const ColorMixError& e) {
            err << "error: " << parsed->mixes << ": " << e.what() << "\n";
            return kExitInput;
        }
    }
    if (parsed->mix != 0) {
        const auto it = std::find_if(mixes.begin(), mixes.end(), [&](const ColorMix& m) { return m.id == parsed->mix; });
        if (it == mixes.end() || !mix_usable(*it, settings.extruders)) {
            err << "error: --mix " << parsed->mix << ": no blend with that number in --mixes that this machine ("
                << settings.extruders << " extruders) can print\n";
            return kExitUsage;
        }
        for (MeshObject& o : objects)
            o.extruder = parsed->mix;
    }
    const auto piece = [&](std::size_t from, std::size_t to) {
        TriangleMesh m;
        for (std::size_t t = from; t < to && t < followers.triangle_count(); ++t)
            m.add_triangle(followers.corner(t, 0), followers.corner(t, 1), followers.corner(t, 2));
        return m;
    };
    stl.negative = piece(0, negatives);
    SupportVolumes support_volumes;
    support_volumes.blockers = weld_exact(piece(negatives, negatives + blockers));
    support_volumes.enforcers = weld_exact(piece(negatives + blockers, followers.triangle_count()));

    const auto start = std::chrono::steady_clock::now();
    const IndexedMesh mesh = weld_exact(stl.mesh);
    const IndexedMesh negative = weld_exact(stl.negative);
    const IndexedMesh* cut = negative.triangles.empty() ? nullptr : &negative;
    if (const std::string reason = unprintable_reason(mesh, settings); !reason.empty()) {
        err << "error: " << reason << "\n";
        return kExitModel;
    }
    std::unique_ptr<gpu::Context> gpu;
    if (settings.gpu == GpuMode::Auto)
        gpu = gpu::Context::create();
    out << "gpu: " << (gpu ? gpu->device_name() : std::string("none, using the CPU")) << "\n";
    // Stress-guided infill (docs/INFILL.md): the part analysed under the load
    // case first, to know where it works hardest.
    StressField stress;
    if (settings.infill_stress_density > 0.0) {
        if (parsed->analysis_case.empty()) {
            err << "warning: infill.stress_density needs a load case (--case): the infill stays even\n";
        } else {
            AnalysisCase c;
            try {
                c = load_analysis_case(parsed->analysis_case);
            } catch (const SettingsError& e) {
                err << "error: " << parsed->analysis_case << ": " << e.what() << "\n";
                return kExitInput;
            }
            const auto analysis_start = std::chrono::steady_clock::now();
            try {
                stress = infill_stress_field(mesh, settings, c, gpu.get());
            } catch (const AnalysisError& e) {
                err << "error: the analysis for stress-guided infill: " << e.what() << "\n";
                return kExitAnalysis;
            }
            out << "stress_analysis_ms: "
                << std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - analysis_start).count()
                << "\n";
            if (stress.empty())
                err << "warning: the analysis gave no valid result: the infill stays even\n";
        }
    }
    SlicedModel sliced;
    std::vector<Warning> warnings;
    if (!fallback.empty())
        warnings.push_back({"speed_fallback", -1, 0.0,
                            "the printer's profile gives no speed for " + fallback + ": a stock Ender-3's are used", 0.0});
    try {
        sliced = slice_model(mesh, settings, gpu.get(), &stress, nullptr, objects.empty() ? nullptr : &objects, mixes.empty() ? nullptr : &mixes, cut,
                             nullptr, &support_volumes);
    } catch (const std::runtime_error& e) {
        if (!gpu)
            throw;
        err << "warning: GPU failed (" << e.what() << "), slicing on the CPU\n";
        warnings.push_back({"gpu_fallback", -1, 0.0, std::string("the GPU failed (") + e.what() + "), sliced on the CPU", 0.0});
        sliced = slice_model(mesh, settings, nullptr, &stress, nullptr, objects.empty() ? nullptr : &objects, mixes.empty() ? nullptr : &mixes, cut,
                             nullptr, &support_volumes);
    }
    const double slice_ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    if (const std::string reason = unprintable_reason(sliced, settings); !reason.empty()) {
        err << "error: " << reason << "\n";
        return kExitModel;
    }

    double volume = 0.0;
    std::size_t polygons = 0, walls = 0, solid_lines = 0, sparse_lines = 0, bridge_lines = 0;
    double top = 0.0, bottom = 0.0, sparse = 0.0, support = 0.0;
    for (std::size_t l = 0; l < sliced.layers.size(); ++l) {
        volume += area_mm2(sliced.regions[l]) * (sliced.layers[l].top - sliced.layers[l].bottom);
        polygons += sliced.regions[l].size();
        walls += sliced.walls[l].toolpaths.size();
        top += area_mm2(sliced.surfaces[l].top);
        bottom += area_mm2(sliced.surfaces[l].bottom);
        sparse += area_mm2(sliced.surfaces[l].sparse);
        solid_lines += sliced.fills[l].top.size() + sliced.fills[l].solid.size();
        sparse_lines += sliced.fills[l].sparse.size();
        bridge_lines += sliced.fills[l].bridge.size();
        if (l < sliced.support_areas.size())
            support += area_mm2(sliced.support_areas[l]);
    }
    out << "layers: " << sliced.layers.size() << "\n";
    out << "polygons: " << polygons << "\n";
    out << "wall_toolpaths: " << walls << "\n";
    out << "top_area: " << top << "\n";
    out << "bottom_area: " << bottom << "\n";
    out << "sparse_area: " << sparse << "\n";
    out << "solid_infill_lines: " << solid_lines << "\n";
    out << "sparse_infill_lines: " << sparse_lines << "\n";
    out << "bridge_lines: " << bridge_lines << "\n";
    out << "support_area: " << support << "\n";
    out << "sliced_volume: " << volume << "\n";
    out << "slice_ms: " << slice_ms << "\n";
    if (sliced.open_contours > 0)
        err << "warning: " << sliced.open_contours
            << " open contours were dropped (holes in the mesh wider than slicing.gap_closing_radius)\n";
    std::vector<Warning> model = model_warnings(sliced, settings.support_free_bridge_length);
    warnings.insert(warnings.end(), model.begin(), model.end());
    const std::vector<Warning> stability = stability_warnings(sliced, settings);
    warnings.insert(warnings.end(), stability.begin(), stability.end());
    if (const std::optional<Warning> cold = heating_warning(settings))
        warnings.push_back(*cold);
    // Into a folder: the name from output.filename_format, first without the
    // print's time and weight, then with them (gcode_templates.md, rule 15).
    std::filesystem::path target = parsed->output;
    const bool folder = std::filesystem::is_directory(target);
    try {
        if (folder)
            target /= output_filename(settings, sliced, std::nullopt);
    } catch (const TemplateError& e) {
        err << "error: " << e.what() << "\n";
        return kExitInput;
    }
    const std::vector<GcodeThumbnail> thumbnails = gcode_thumbnails(stl.mesh, settings, &warnings);
    const GcodeExport exported = export_gcode(target, sliced, settings, &warnings, thumbnails);
    if (!exported.written) {
        err << "error: " << exported.error << "\n";
        for (const GcodeProblem& p : exported.check.problems)
            err << "  line " << p.line << ": " << p.message << "\n";
        return exported.check.problem_count > 0 ? kExitGcode : kExitInput;
    }
    if (folder) {
        const std::filesystem::path final_name = target.parent_path() / output_filename(settings, sliced, read_gcode_totals(target));
        std::error_code ec;
        if (final_name != target)
            std::filesystem::rename(target, final_name, ec);
        if (ec) {
            err << "error: cannot rename " << target.string() << " to " << final_name.string() << "\n";
            return kExitInput;
        }
        target = final_name;
    }
    // Post-processing scripts, once confirmed: they run anything (rules 16 and 17).
    if (!settings.post_process.empty()) {
        out << "A post-processing script has been detected in the settings:";
        for (const std::string& script : settings.post_process)
            out << " " << script;
        out << "\nContinue (Y/N)? ";
        std::string answer = parsed->yes ? "Y" : "";
        if (!parsed->yes)
            std::getline(std::cin, answer);
        out << answer << "\n";
        if (answer != "Y" && answer != "y") {
            std::error_code ignored;
            std::filesystem::remove(target, ignored);
            err << "error: the post-processing was not confirmed: no G-code is kept\n";
            return kExitInput;
        }
        if (const std::string errors = run_post_process(settings, sliced, target); !errors.empty()) {
            err << "error: post-processing: " << errors;
            return kExitInput;
        }
    }
    out << "write_ms: " << exported.write_seconds * 1000.0 << "\n";
    out << "check_ms: " << exported.check_seconds * 1000.0 << "\n";
    out << "gcode_check: ok (" << exported.check.stats.lines << " lines, " << exported.check.stats.layers
        << " layers, " << exported.check.stats.filament << " mm of filament net of retractions)\n";
    out << "wrote " << target.string() << "\n";
    out << "warnings: " << warnings.size() << "\n";
    if (!parsed->warnings.empty()) {
        if (!write_file(parsed->warnings, [&](std::ostream& report) { report << warnings_json(warnings); })) {
            err << "error: cannot write '" << parsed->warnings << "'\n";
            return kExitInput;
        }
        out << "wrote " << parsed->warnings << "\n";
    }
    // --strict (orca_2_5.md, rule 24): a warning is an error; no G-code is
    // kept, the warnings file lists them all. Information is not a warning.
    if (parsed->strict) {
        const auto first = std::find_if(warnings.begin(), warnings.end(),
                                        [](const Warning& w) { return w.severity != "information" && w.severity != "info"; });
        if (first != warnings.end()) {
            std::error_code ignored;
            std::filesystem::remove(target, ignored);
            err << "error: --strict: " << first->message << " (" << first->kind << ")\n";
            return kExitModel;
        }
    }
    // The statistics of the plate (rule 27).
    if (!parsed->statistics.empty()) {
        const std::string json = gcode_statistics_json(target, sliced.layers.size());
        if (!write_text_or_stdout(parsed->statistics, json + "\n", out)) {
            err << "error: cannot write '" << parsed->statistics << "'\n";
            return kExitInput;
        }
    }
    if (!parsed->summary.empty()) {
        const std::string summary = slice_summary_json(target, exported.check.stats.filament, sliced.layers.size(),
                                                       settings.filament_density > 0.0);
        if (!write_file(parsed->summary, [&](std::ostream& f) { f << summary << "\n"; })) {
            err << "error: cannot write '" << parsed->summary << "'\n";
            return kExitInput;
        }
        out << "wrote " << parsed->summary << "\n";
    }
    return kExitOk;
}

// The settings, case and placed mesh of `analyze <project>`: the project's
// settings (or --config's) with the part's values changed by hand, and its
// conditions as the case (FM25), or --case's. The part is the first with
// Engineering conditions; with --case and none, the first part.
int project_analysis(const SliceArgs& a, Settings& settings, AnalysisCase& analysis, TriangleMesh& mesh,
                     std::array<double, 12>& to_part, std::ostream& out, std::ostream& err) {
    Project project;
    try {
        project = load_project(a.model);
    } catch (const std::exception& e) {
        err << "error: " << a.model << ": " << e.what() << "\n";
        return kExitInput;
    }
    for (const std::string& notice : project.notices)
        err << "warning: " << notice << "\n";
    auto it = std::find_if(project.models.begin(), project.models.end(), [](const ProjectModel& m) {
        return std::any_of(m.engineering.conditions.begin(), m.engineering.conditions.end(),
                           [](const EngineeringCondition& c) { return !c.hidden; });
    });
    if (it == project.models.end() && !a.analysis_case.empty() && !project.models.empty())
        it = project.models.begin();
    if (it == project.models.end()) {
        err << "error: no part of the project has Engineering conditions (mark them in the Engineering mode, or "
               "give --case)\n";
        return kExitInput;
    }
    if (a.config.empty()) {
        settings = std::move(project.settings);
        for (const std::string& key : fill_missing_speeds(settings))
            err << "warning: the printer's profile gives no speed for " << key << ": a stock Ender-3's is used\n";
    } else {
        try {
            settings = load_settings(a.config);
        } catch (const SettingsError& e) {
            err << "error: " << a.config << ": " << e.what() << "\n";
            return kExitInput;
        }
    }
    // The part where it is on the bed: its mesh turned and moved by its
    // build transform, and its conditions with it.
    PlacedPart part = placed_part(*it);
    mesh = std::move(part.mesh);
    ProjectEngineering placed = std::move(part.engineering);
    to_part = part.to_part;
    std::vector<std::string> notes;
    apply_engineering_overrides(settings, it->engineering, notes);
    if (a.analysis_case.empty()) {
        analysis = engineering_analysis_case(placed, mesh, {}, notes, settings.filament_type, settings.layer_height,
                                             print_conditions(settings, placed));
    } else {
        try {
            analysis = load_analysis_case(a.analysis_case);
        } catch (const SettingsError& e) {
            err << "error: " << a.analysis_case << ": " << e.what() << "\n";
            return kExitInput;
        }
    }
    for (const std::string& note : notes)
        err << "warning: " << note << "\n";
    if (a.analysis_case.empty() && (analysis.fixtures.empty() || analysis.loads.empty())) {
        err << "error: " << it->name << ": the analysis needs a fixed point or zone and a force\n";
        return kExitInput;
    }
    out << "part: " << it->name << ", " << mesh.triangle_count() << " triangles, " << analysis.fixtures.size()
        << " fixtures, " << analysis.loads.size() << " loads\n";
    return kExitOk;
}

int cmd_analyze(std::span<const std::string_view> args, std::ostream& out, std::ostream& err) {
    const std::optional<SliceArgs> parsed = parse_slice_args(args, err, true);
    if (!parsed) {
        err << kUsage;
        return kExitUsage;
    }
    if (!outputs_are_safe(*parsed, err))
        return kExitInput;
    Settings settings;
    AnalysisCase analysis;
    ModelFile stl;
    // From the analysis (on the bed) back to the part's own coordinates, for
    // the report's points: a project's build transform undone, or what
    // placement.arrange moved the model file.
    std::array<double, 12> to_part{1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0};
    if (parsed->project) {
        // A project: its settings (or --config's), and the first part with
        // Engineering conditions, where it is on the bed.
        if (int rc = project_analysis(*parsed, settings, analysis, stl.mesh, to_part, out, err); rc != kExitOk)
            return rc;
    } else {
        try {
            settings = load_settings(parsed->config);
        } catch (const SettingsError& e) {
            err << "error: " << parsed->config << ": " << e.what() << "\n";
            return kExitInput;
        }
        try {
            analysis = load_analysis_case(parsed->analysis_case);
        } catch (const SettingsError& e) {
            err << "error: " << parsed->analysis_case << ": " << e.what() << "\n";
            return kExitInput;
        }
        double seconds = 0.0;
        if (int rc = load_model(parsed->model, stl, seconds, err); rc != kExitOk)
            return rc;
        out << "read " << stl.mesh.triangle_count() << " triangles in " << seconds * 1000.0 << " ms\n";
        const BoundingBox own = stl.mesh.bounding_box();
        place_on_bed(stl.mesh, settings); // in X and Y only
        const BoundingBox placed = stl.mesh.bounding_box();
        to_part[9] = double(own.min.x) - double(placed.min.x);
        to_part[10] = double(own.min.y) - double(placed.min.y);
        to_part[11] = double(own.min.z) - double(placed.min.z);
    }
    const IndexedMesh mesh = weld_exact(stl.mesh);
    if (const std::string reason = unprintable_reason(mesh, settings); !reason.empty()) {
        err << "error: " << reason << "\n";
        return kExitModel;
    }
    std::unique_ptr<gpu::Context> gpu;
    if (settings.gpu == GpuMode::Auto && analysis.model == AnalysisModel::AsPrinted)
        gpu = gpu::Context::create();
    if (parsed->estimate) {
        AnalysisEstimate estimate;
        try {
            estimate = estimate_analysis(mesh, settings, analysis, gpu.get());
        } catch (const AnalysisError& e) {
            err << "error: " << e.what() << "\n";
            return kExitAnalysis;
        }
        out << "voxels: " << estimate.printed_voxels << " of " << estimate.grid.solid.size() << " ("
            << estimate.grid.hx << " mm, " << estimate.rows_per_layer << " rows per layer)\n";
        out << "estimated_memory_mb: " << estimate.memory_mb << "\n";
        out << "estimated_seconds: " << estimate.seconds << "\n";
        if (!write_file(parsed->output, [&](std::ostream& json) { json << analysis_estimate_json(estimate, analysis); })) {
            err << "error: cannot write '" << parsed->output << "'\n";
            return kExitInput;
        }
        out << "wrote " << parsed->output << "\n";
        return kExitOk;
    }
    AnalysisProgress progress;
    if (parsed->progress)
        progress = [&out](std::string_view stage, double done) {
            out << "progress: " << stage << " " << done << std::endl;
        };
    AnalysisReport report;
    try {
        // Stress-guided infill: the same infill the slicing would print.
        const StressField infill = settings.infill_stress_density > 0.0
                                       ? infill_stress_field(mesh, settings, analysis, gpu.get())
                                       : StressField{};
        report = run_analysis(mesh, settings, analysis, gpu.get(), progress, &infill);
    } catch (const AnalysisError& e) {
        err << "error: " << e.what() << "\n";
        return kExitAnalysis;
    }
    const ElasticityResult& e = report.result.elasticity;
    out << "voxels: " << report.grid.count() << " of " << report.grid.solid.size() << " (" << report.grid.hx
        << " mm, " << report.rows_per_layer << " rows per layer)\n";
    out << "solver: " << e.levels << " levels, " << e.iterations << " iterations, residual " << e.residual
        << (e.converged ? "" : " (not converged)") << "\n";
    out << "prepare_ms: " << report.prepare_seconds * 1000.0 << "\n";
    out << "solve_ms: " << report.solve_seconds * 1000.0 << "\n";
    out << "max_displacement_mm: " << e.max_displacement << "\n";
    out << "max_von_mises_mpa: " << e.max_von_mises << "\n";
    if (report.weakest_layer >= 0)
        out << "min_safety: " << report.result.min_safety << " at layer " << report.weakest_layer << " ("
            << failure_mode_name(report.result.mode[report.result.weakest]) << "); peak in one voxel "
            << report.result.min_peak_safety << "\n";
    else
        out << "min_safety: none (nothing is stressed)\n";
    if (report.resolution.done)
        out << "resolution: with " << report.resolution.voxel << " mm voxels min_safety " << report.resolution.min_safety
            << ", a change of " << 100.0 * report.resolution.change << " %; extrapolated "
            << report.resolution.extrapolated_min_safety << "\n";
    else
        out << "resolution: not checked (" << report.resolution.reason << ")\n";
    for (const AnalysisWarning& w : report.warnings)
        err << "warning: " << w.message << "\n";

    if (!write_file(parsed->output, [&](std::ostream& json) { json << analysis_json(report, analysis, to_part); })) {
        err << "error: cannot write '" << parsed->output << "'\n";
        return kExitInput;
    }
    out << "wrote " << parsed->output << "\n";
    if (!parsed->vtk.empty()) {
        if (!write_file(parsed->vtk, [&](std::ostream& vtk) { write_analysis_vtk(vtk, report); })) {
            err << "error: cannot write '" << parsed->vtk << "'\n";
            return kExitInput;
        }
        out << "wrote " << parsed->vtk << "\n";
    }
    if (!parsed->mesh.empty()) {
        if (!write_file(parsed->mesh, [&](std::ostream& ply) { write_voxel_mesh_ply(ply, report); })) {
            err << "error: cannot write '" << parsed->mesh << "'\n";
            return kExitInput;
        }
        out << "wrote " << parsed->mesh << "\n";
    }
    return report.valid ? kExitOk : kExitAnalysis;
}

} // namespace

int run_cli(std::span<const std::string_view> args, std::ostream& out, std::ostream& err) {
    if (args.empty()) {
        err << kUsage;
        return kExitUsage;
    }
    const std::string_view command = args[0];
    const auto rest = args.subspan(1);
    if (command == "--help" || command == "-h" || command == "help") {
        out << kUsage;
        return kExitOk;
    }
    if (command == "--version") {
        out << "tisma-engine " << TISMA_VERSION << "\n";
        return kExitOk;
    }
    // --threads <n> on any command (cloud.md, rule 19d): at most n threads of
    // work, so a node shares its cores between jobs; without it, all.
    std::vector<std::string_view> kept;
    std::optional<tbb::global_control> threads;
    for (std::size_t i = 0; i < rest.size(); ++i) {
        if (rest[i] != "--threads") {
            kept.push_back(rest[i]);
            continue;
        }
        int n = 0;
        const std::string_view value = i + 1 < rest.size() ? rest[i + 1] : std::string_view();
        const auto [end, ec] = chars_to(value.data(), value.data() + value.size(), n);
        if (value.empty() || ec != std::errc() || end != value.data() + value.size() || n < 1 || n > 4096) {
            err << "error: --threads needs a whole number from 1\n" << kUsage;
            return kExitUsage;
        }
        threads.emplace(tbb::global_control::max_allowed_parallelism, std::size_t(n));
        ++i;
    }
    const std::span<const std::string_view> args_left(kept);
    try {
        if (command == "slice")
            return node_slice_wanted(args_left) ? cmd_slice_project(args_left, out, err) : cmd_slice(args_left, out, err);
        if (command == "info")
            return cmd_info(args_left, out, err);
        if (command == "profile")
            return std::find(args_left.begin(), args_left.end(), "--pack") != args_left.end()
                       ? cmd_pack_profile(args_left, out, err)
                       : cmd_profile(args_left, out, err);
        if (command == "pack")
            return cmd_pack(args_left, out, err);
        if (command == "library")
            return cmd_library(args_left, out, err);
        if (command == "part")
            return cmd_part(args_left, out, err);
        if (command == "calibration")
            return cmd_calibration(args_left, out, err);
        if (command == "design")
            return cmd_design(args_left, out, err);
        if (command == "aero")
            return cmd_aero_project(args_left, out, err);
        if (command == "orient")
            return cmd_orient_project(args_left, out, err);
        if (command == "analyze")
            return node_analyze_wanted(args_left) ? cmd_analyze_project(args_left, out, err) : cmd_analyze(args_left, out, err);
    } catch (const std::exception& e) {
        // What the commands do not catch is the engine's own fault: said, not a crash.
        err << "error: internal: " << e.what() << "\n";
        return kExitInternal;
    }
    err << "error: unknown command '" << command << "'\n" << kUsage;
    return kExitUsage;
}

} // namespace tisma
