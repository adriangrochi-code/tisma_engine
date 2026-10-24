#pragma once

#include "ui/text.hpp"

#include <string>
#include <vector>

namespace tisma::ui {

struct AppOptions {
    std::vector<std::string> files; // to open at start: models (.stl), G-code or settings (.json)
    int width = 1440, height = 900; // window size, in screen units
    std::string screenshot;         // when set: draw, write the window to this PNG and quit
    std::string view = "prepare";   // the tab for the screenshot: prepare, preview or analysis
    int layer = -1;                 // the top layer shown in the preview (1-based); -1: all
    std::string export_path;        // with screenshot: after slicing, export the G-code here first
    std::string browse;             // with screenshot: show the file browser on this folder
    bool light = false;             // light theme
    bool language_given = false;
    Language language = Language::Spanish;
};

// Runs the interface until its window closes. Returns the process exit code:
// in screenshot mode, 1 when the picture or the asked-for export failed.
int run_app(const AppOptions& options);

} // namespace tisma::ui
