// tisma: the interface of Tisma engine. Own design; see PROVENANCE.md.

#include "app.hpp"

#include "calibration/calibration.hpp"

#include <cstdio>
#include <cstdlib>
#include <sstream>
#include <string>
#include <string_view>

namespace {

constexpr const char* kUsage =
    "usage: tisma [model.stl | project.tisma | file.gcode | settings.json]...\n"
    "             [--lang es|en] [--light] [--render fast|ao|rt] [--msaa 1|2|4|8] [--fxaa on|off]\n"
    "             [--moire on|off] [--device PORT|sim [--device-print]] [--color kind|speed|width|height]\n"
    "             [--calibration dimensions|first_layer|temperature|flow|retraction|retraction_speed|\n"
    "                            pressure_advance|speed|fit|max_flow|acceleration|elephant_foot|extruder]\n"
    "             [--profile printer|filament|print=NAME]...\n"
    "             [--catalog printer|filament [--catalog-search TEXT]]\n"
    "             [--screenshot out.png [--size WIDTHxHEIGHT] [--view prepare|preview|analysis|calibration|device]\n"
    "                                   [--layer N] [--look X,Y,Z,DISTANCE,YAW,PITCH]\n"
    "                                   [--tool move|rotate|scale|lay|paint]\n"
    "                                   [--paint support|seam,enforce|block,X,Y,Z,RADIUS]...\n"
    "                                   [--export out.gcode] [--save-project out.tisma]\n"
    "                                   [--browse folder]\n"
    "                                   [--ray-samples N] [--ray-check] [--moire-check]]\n"
    "\n"
    "Opens the Tisma interface. --screenshot draws the window once everything asked\n"
    "for is ready (slicing for --view preview), writes it as a PNG and quits; with\n"
    "--export it first slices and exports the checked G-code as the Export button\n"
    "does, and exits with 1 if that fails; --save-project saves the project before\n"
    "the picture. --browse shows the file browser. --profile chooses a profile.\n"
    "--render picks the lighting: fast, ambient occlusion or ray tracing (which the\n"
    "screenshot waits --ray-samples frames for; --ray-check compares the GPU's\n"
    "shadow rays with the CPU's and exits with 1 if they differ).\n";

} // namespace

int main(int argc, char** argv) {
    tisma::ui::AppOptions options;
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg = argv[i];
        const auto value = [&]() -> const char* {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "error: %s needs a value\n%s", argv[i], kUsage);
                std::exit(2);
            }
            return argv[++i];
        };
        if (arg == "--help" || arg == "-h") {
            std::fputs(kUsage, stdout);
            return 0;
        } else if (arg == "--version") {
            std::printf("tisma %s\n", TISMA_VERSION);
            return 0;
        } else if (arg == "--screenshot") {
            options.screenshot = value();
        } else if (arg == "--size") {
            const std::string size = value();
            const std::size_t x = size.find('x');
            options.width = x == std::string::npos ? 0 : std::atoi(size.substr(0, x).c_str());
            options.height = x == std::string::npos ? 0 : std::atoi(size.substr(x + 1).c_str());
            if (options.width < 320 || options.height < 240) {
                std::fprintf(stderr, "error: --size must read WIDTHxHEIGHT, at least 320x240\n");
                return 2;
            }
        } else if (arg == "--view") {
            options.view = value();
            if (options.view != "prepare" && options.view != "preview" && options.view != "analysis" &&
                options.view != "calibration" && options.view != "device") {
                std::fprintf(stderr, "error: --view must be prepare, preview, analysis, calibration or device\n");
                return 2;
            }
        } else if (arg == "--layer") {
            options.layer = std::atoi(value());
        } else if (arg == "--tool") {
            options.tool = value();
            if (options.tool != "move" && options.tool != "rotate" && options.tool != "scale" && options.tool != "lay" &&
                options.tool != "paint") {
                std::fprintf(stderr, "error: --tool must be move, rotate, scale, lay or paint\n");
                return 2;
            }
        } else if (arg == "--paint") {
            // channel,mark,x,y,z,radius: support or seam, enforce or block, mm.
            std::string text = value();
            for (char& c : text)
                c = c == ',' ? ' ' : c;
            std::istringstream in(text);
            tisma::ui::AppOptions::Stroke stroke;
            std::string mark;
            if (!(in >> stroke.channel >> mark >> stroke.x >> stroke.y >> stroke.z >> stroke.radius) ||
                (stroke.channel != "support" && stroke.channel != "seam") || (mark != "enforce" && mark != "block") ||
                !(stroke.radius > 0.0f)) {
                std::fprintf(stderr, "error: --paint must be support|seam,enforce|block,X,Y,Z,RADIUS\n");
                return 2;
            }
            stroke.block = mark == "block";
            options.strokes.push_back(stroke);
        } else if (arg == "--look") {
            std::string text = value();
            for (char& c : text)
                c = c == ',' ? ' ' : c;
            std::istringstream in(text);
            for (float v; in >> v;)
                options.look.push_back(v);
            if (options.look.size() != 6 || !(options.look[3] > 0.0f)) {
                std::fprintf(stderr, "error: --look must read X,Y,Z,DISTANCE,YAW,PITCH (mm and degrees)\n");
                return 2;
            }
        } else if (arg == "--export") {
            options.export_path = value();
        } else if (arg == "--catalog") {
            options.catalog = value();
            if (options.catalog != "printer" && options.catalog != "filament") {
                std::fprintf(stderr, "error: --catalog must be printer or filament\n");
                return 2;
            }
        } else if (arg == "--catalog-search") {
            options.catalog_query = value();
        } else if (arg == "--save-project") {
            options.save_project = value();
        } else if (arg == "--profile") {
            const std::string choice = value();
            const std::string kind = choice.substr(0, choice.find('='));
            if (choice.find('=') == std::string::npos || (kind != "printer" && kind != "filament" && kind != "print")) {
                std::fprintf(stderr, "error: --profile must be printer=NAME, filament=NAME or print=NAME\n");
                return 2;
            }
            options.profiles.push_back(choice);
        } else if (arg == "--browse") {
            options.browse = value();
        } else if (arg == "--render") {
            const std::string_view mode = value();
            options.render = mode == "fast" ? 0 : mode == "ao" ? 1 : mode == "rt" ? 2 : -2;
            if (options.render < 0) {
                std::fprintf(stderr, "error: --render must be fast, ao or rt\n");
                return 2;
            }
        } else if (arg == "--msaa") {
            options.msaa = std::atoi(value());
            if (options.msaa < 1 || options.msaa > 16) {
                std::fprintf(stderr, "error: --msaa must be 1 to 16\n");
                return 2;
            }
        } else if (arg == "--fxaa") {
            const std::string_view on = value();
            if (on != "on" && on != "off") {
                std::fprintf(stderr, "error: --fxaa must be on or off\n");
                return 2;
            }
            options.fxaa = on == "on" ? 1 : 0;
        } else if (arg == "--moire") {
            const std::string_view on = value();
            if (on != "on" && on != "off") {
                std::fprintf(stderr, "error: --moire must be on or off\n");
                return 2;
            }
            options.moire = on == "on" ? 1 : 0;
        } else if (arg == "--ray-samples") {
            options.ray_samples = std::max(1, std::atoi(value()));
        } else if (arg == "--ray-check") {
            options.ray_check = true;
        } else if (arg == "--moire-check") {
            options.moire_check = true;
        } else if (arg == "--color") {
            const std::string_view c = value();
            options.color = c == "kind" ? 0 : c == "speed" ? 1 : c == "width" ? 2 : c == "height" ? 3 : -2;
            if (options.color == -2) {
                std::fprintf(stderr, "error: --color must be kind, speed, width or height\n");
                return 2;
            }
        } else if (arg == "--device") {
            options.device = value();
        } else if (arg == "--device-print") {
            options.device_print = true;
        } else if (arg == "--calibration") {
            options.calibration = value();
            const std::string& c = options.calibration;
            if (c != "extruder" && !tisma::calibration_kind_from_name(c)) {
                std::fprintf(stderr, "error: --calibration must be dimensions, first_layer, temperature, flow, retraction, "
                                     "retraction_speed, pressure_advance, speed, fit or extruder\n");
                return 2;
            }
        } else if (arg == "--light") {
            options.light = true;
        } else if (arg == "--lang") {
            const std::string_view lang = value();
            if (lang != "es" && lang != "en") {
                std::fprintf(stderr, "error: --lang must be es or en\n");
                return 2;
            }
            options.language_given = true;
            options.language = lang == "en" ? tisma::ui::Language::English : tisma::ui::Language::Spanish;
        } else if (arg.starts_with("--")) {
            std::fprintf(stderr, "error: unknown option %s\n%s", argv[i], kUsage);
            return 2;
        } else {
            options.files.emplace_back(arg);
        }
    }
    if ((!options.export_path.empty() || !options.save_project.empty() || !options.browse.empty() || options.ray_check ||
         options.moire_check) &&
        options.screenshot.empty()) {
        std::fprintf(stderr, "error: --export, --save-project, --browse, --ray-check and --moire-check go with --screenshot\n");
        return 2;
    }
    if (options.device_print && (options.device.empty() || options.screenshot.empty())) {
        std::fprintf(stderr, "error: --device-print goes with --device and --screenshot\n");
        return 2;
    }
    if (options.ray_check)
        options.render = 2;
    return tisma::ui::run_app(options);
}
