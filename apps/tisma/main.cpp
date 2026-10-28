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
    "usage: tisma [model.stl|.obj|.3mf | project.tisma | file.gcode | settings.json]...\n"
    "             [--lang es|en] [--light] [--render fast|ao|rt] [--ao-strength S] [--msaa 1|2|4|8] [--fxaa on|off]\n"
    "             [--moire on|off] [--device PORT|sim [--device-print]] [--color VIEW]\n"
    "             [--calibration dimensions|first_layer|temperature|flow|retraction|retraction_speed|\n"
    "                            pressure_advance|speed|fit|max_flow|acceleration|elephant_foot|\n"
    "                            resin_exposure|resin_measure|extruder]\n"
    "             [--profile printer|filament|print=NAME]...\n"
    "             [--catalog printer|filament [--catalog-search TEXT]]\n"
    "             [--screenshot out.png [--size WIDTHxHEIGHT] [--view prepare|preview|analysis|calibration|device]\n"
    "                                   [--layer N] [--look X,Y,Z,DISTANCE,YAW,PITCH]\n"
    "                                   [--tool move|rotate|scale|lay|paint|material|mixing]\n"
    "                                   [--paint support|seam|fuzzy,enforce|block,X,Y,Z,RADIUS]...\n"
    "                                   [--tips] [--tour STEP] [--info GROUP.KEY]\n"
    "                                   [--level basic|normal|advanced|custom] [--section TAB/SECTION]\n"
    "                                   [--search TEXT] [--single-column] [--choose-settings] [--credits] [--help-panel TAB]\n"
    "                                   [--ortho] [--labels]\n"
    "                                   [--export out.gcode] [--save-project out.tisma]\n"
    "                                   [--browse folder]\n"
    "                                   [--ray-samples N] [--ray-check] [--moire-check]\n"
    "                                   [--bench-frames N] [--graphics auto|vulkan|opengl] [--coarse]\n"
    "                                   [--compare-view other.png] [--no-view-check]]\n"
    "\n"
    "Opens the Tisma interface. --screenshot draws the window once everything asked\n"
    "for is ready (slicing for --view preview), writes it as a PNG and quits; with\n"
    "--export it first slices and exports the checked G-code as the Export button\n"
    "does, and exits with 1 if that fails; --save-project saves the project before\n"
    "the picture. --browse shows the file browser. --profile chooses a profile.\n"
    "--render picks the lighting: fast, ambient occlusion or ray tracing (which the\n"
    "screenshot waits --ray-samples frames for; --ray-check compares the GPU's\n"
    "shadow rays with the CPU's and exits with 1 if they differ). --bench-frames\n"
    "draws the ready 3D view N more times and prints the time per frame.\n"
    "--graphics chooses what draws the window: auto (the default) finds whether\n"
    "this machine has Vulkan and takes OpenGL if not; vulkan asks for it (in\n"
    "progress: the view without its ambient occlusion and ray tracing yet, so auto\n"
    "takes OpenGL for now). --compare-view compares the 3D view with the one in\n"
    "another screenshot (drawn by the other graphics) and exits with 1 if more\n"
    "than 0.5% of its pixels differ by more than 24 levels. --coarse draws a plate of\n"
    "more than a million triangles as while the view moves: its coarse copy.\n"
    "Every screenshot of the Prepare or Preview tab also checks the 3D view's own\n"
    "picture and exits with 1 if a pixel is black (a shader's NaN) or more than\n"
    "0.1% of them sparkle (a pixel far from all its neighbours); --no-view-check\n"
    "skips that. --pick-catalog-printer \"Vendor Model\" picks that printer in the\n"
    "printer card's list, as a click on its row would (a test of that path).\n";

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
                options.tool != "paint" && options.tool != "material" && options.tool != "mixing" && options.tool != "cut" &&
                options.tool != "measure") {
                std::fprintf(stderr, "error: --tool must be move, rotate, scale, lay, cut, measure, paint, material or mixing\n");
                return 2;
            }
        } else if (arg == "--object-add") {
            options.object_add = value();
        } else if (arg == "--object-turn") {
            auto& d = options.object_turn;
            if (std::sscanf(value(), "%lf,%lf,%lf", &d[0], &d[1], &d[2]) != 3) {
                std::fprintf(stderr, "error: --object-turn takes three angles in degrees, as 0,0,45\n");
                return 2;
            }
        } else if (arg == "--object-settings") {
            options.object_settings = value();
        } else if (arg == "--measure") {
            // Two triangles of the first model picked as faces by the Measure tool.
            if (std::sscanf(value(), "%d,%d", &options.measure[0], &options.measure[1]) != 2) {
                std::fprintf(stderr, "error: --measure takes two triangle numbers, as 0,5\n");
                return 2;
            }
        } else if (arg == "--cut") {
            options.cut = std::atof(value()); // cut the first object at this height over the bed
        } else if (arg == "--info") {
            options.info = value();
        } else if (arg == "--level") {
            options.level = value();
            if (options.level != "basic" && options.level != "normal" && options.level != "advanced" &&
                options.level != "custom") {
                std::fprintf(stderr, "error: --level must be basic, normal, advanced or custom\n");
                return 2;
            }
        } else if (arg == "--search") {
            options.search = value();
        } else if (arg == "--single-column") {
            options.single_column = true;
        } else if (arg == "--ortho") {
            options.ortho = true;
        } else if (arg == "--labels") {
            options.labels = true;
        } else if (arg == "--choose-settings") {
            options.choose_settings = true;
        } else if (arg == "--credits") {
            options.credits = true;
        } else if (arg == "--knowledge") {
            options.knowledge = value();
        } else if (arg == "--tutorial") {
            // ID or ID,STEP (from 1): a guided tutorial of the help's data on screen.
            const std::string t = value();
            options.tutorial = t.substr(0, t.find(','));
            if (t.find(',') != std::string::npos)
                options.tutorial_step = std::max(0, std::atoi(t.c_str() + t.find(',') + 1) - 1);
        } else if (arg == "--help-item") {
            options.help_open = value();
        } else if (arg == "--help-panel") {
            options.help = value(); // problems, procedures, glossary, tutorials, calibrations
        } else if (arg == "--section") {
            options.section = value(); // print/quality, material/cooling, printer/machine...
        } else if (arg == "--tips") {
            options.tips = true;
        } else if (arg == "--tour") {
            const std::string text = value();
            options.tour = text.size() == 1 && text[0] >= '0' && text[0] <= '6' ? text[0] - '0' : -2;
            if (options.tour < 0) {
                std::fprintf(stderr, "error: --tour must be a step from 0 to 6\n");
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
                (stroke.channel != "support" && stroke.channel != "seam" && stroke.channel != "fuzzy") ||
                (mark != "enforce" && mark != "block") ||
                !(stroke.radius > 0.0f)) {
                std::fprintf(stderr, "error: --paint must be support|seam|fuzzy,enforce|block,X,Y,Z,RADIUS\n");
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
        } else if (arg == "--ao-strength") {
            const double strength = std::atof(value());
            if (!(strength >= 0.0 && strength <= 1.5)) {
                std::fprintf(stderr, "error: --ao-strength must be between 0 and 1.5\n");
                return 2;
            }
            options.ao_strength = float(strength);
        } else if (arg == "--moire") {
            const std::string_view on = value();
            if (on != "on" && on != "off") {
                std::fprintf(stderr, "error: --moire must be on or off\n");
                return 2;
            }
            options.moire = on == "on" ? 1 : 0;
        } else if (arg == "--ray-samples") {
            options.ray_samples = std::max(1, std::atoi(value()));
        } else if (arg == "--graphics") {
            options.graphics = value();
            if (options.graphics != "auto" && options.graphics != "vulkan" && options.graphics != "opengl") {
                std::fprintf(stderr, "error: --graphics must be auto, vulkan or opengl\n");
                return 2;
            }
        } else if (arg == "--bench-frames") {
            options.bench_frames = std::max(1, std::atoi(value()));
        } else if (arg == "--coarse") {
            options.coarse = true;
        } else if (arg == "--pick-catalog-printer") {
            options.pick_catalog_printer = value();
        } else if (arg == "--no-view-check") {
            options.view_check = false;
        } else if (arg == "--compare-view") {
            options.compare_view = value();
        } else if (arg == "--ray-check") {
            options.ray_check = true;
        } else if (arg == "--moire-check") {
            options.moire_check = true;
        } else if (arg == "--color") {
            const std::string_view c = value();
            // The preview's views, in the order of preview.md (rule 1).
            static constexpr std::string_view views[] = {"kind",        "height",     "width",          "speed",
                                                         "actual_speed", "fan",        "temperature",    "flow",
                                                         "actual_flow", "layer_time", "layer_time_log", "tool",
                                                         "color_print"};
            options.color = -2;
            for (int k = 0; k < int(std::size(views)); ++k)
                if (c == views[k])
                    options.color = k;
            if (options.color == -2) {
                std::fprintf(stderr, "error: --color must be kind, height, width, speed, actual_speed, fan, temperature, "
                                     "flow, actual_flow, layer_time, layer_time_log, tool or color_print\n");
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
                                     "retraction_speed, pressure_advance, speed, fit, max_flow, acceleration, "
                                     "elephant_foot, resin_exposure, resin_measure or extruder\n");
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
         options.moire_check || !options.compare_view.empty()) &&
        options.screenshot.empty()) {
        std::fprintf(stderr, "error: --export, --save-project, --browse, --ray-check, --moire-check and --compare-view go "
                             "with --screenshot\n");
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
