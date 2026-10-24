#pragma once

#include "ui/text.hpp"

#include <string>
#include <vector>

namespace tisma::ui {

struct AppOptions {
    std::vector<std::string> files; // to open at start: models (.stl), projects (.tisma), G-code or settings (.json)
    int width = 1440, height = 900; // window size, in screen units
    std::string screenshot;         // when set: draw, write the window to this PNG and quit
    std::string view = "prepare";   // the tab for the screenshot: prepare, preview, analysis or calibration
    int layer = -1;                 // the top layer shown in the preview (1-based); -1: all
    std::string export_path;        // with screenshot: after slicing, export the G-code here first
    std::string browse;             // with screenshot: show the file browser on this folder
    std::string calibration;        // create this calibration at start (calibration_kind_name), or "extruder"
    std::vector<std::string> profiles; // "kind=name": profiles chosen at start (docs/PROFILES.md)
    std::string save_project;       // with screenshot: save the project here once everything is ready
    std::string catalog;            // "printer" or "filament": open the catalog at start (docs/CATALOG.md)
    std::string catalog_query;      // what to search in it
    bool light = false;             // light theme
    int render = -1;                // RenderMode (0 fast, 1 ambient occlusion, 2 ray tracing); -1: the preference
    int msaa = -1;                  // multisampling samples; -1: the preference
    int fxaa = -1;                  // 0 off, 1 on; -1: the preference
    int moire = -1;                 // the moiré filter: 0 off, 1 on; -1: the preference
    int ray_samples = 32;           // with screenshot and ray tracing: the samples to wait for
    bool ray_check = false;         // with screenshot: check the GPU's shadow rays against the CPU's
    bool moire_check = false;       // with screenshot: check that the moiré filter smooths lines finer than a pixel
    bool language_given = false;
    Language language = Language::Spanish;
};

// Runs the interface until its window closes. Returns the process exit code:
// in screenshot mode, 1 when the picture or the asked-for export failed.
int run_app(const AppOptions& options);

} // namespace tisma::ui
