// tisma: the interface of Tisma engine. Own design; see PROVENANCE.md.

#include "app.hpp"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <string_view>

namespace {

constexpr const char* kUsage =
    "usage: tisma [model.stl | file.gcode | settings.json]...\n"
    "             [--lang es|en] [--light]\n"
    "             [--screenshot out.png [--size WIDTHxHEIGHT] [--view prepare|preview|analysis] [--layer N]\n"
    "                                   [--export out.gcode] [--browse folder]]\n"
    "\n"
    "Opens the Tisma interface. --screenshot draws the window once everything asked\n"
    "for is ready (slicing for --view preview), writes it as a PNG and quits; with\n"
    "--export it first slices and exports the checked G-code as the Export button\n"
    "does, and exits with 1 if that fails. --browse shows the file browser.\n";

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
            if (options.view != "prepare" && options.view != "preview" && options.view != "analysis") {
                std::fprintf(stderr, "error: --view must be prepare, preview or analysis\n");
                return 2;
            }
        } else if (arg == "--layer") {
            options.layer = std::atoi(value());
        } else if (arg == "--export") {
            options.export_path = value();
        } else if (arg == "--browse") {
            options.browse = value();
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
    if ((!options.export_path.empty() || !options.browse.empty()) && options.screenshot.empty()) {
        std::fprintf(stderr, "error: --export and --browse go with --screenshot\n");
        return 2;
    }
    return tisma::ui::run_app(options);
}
