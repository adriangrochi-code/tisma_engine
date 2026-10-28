#pragma once

#include "ui/text.hpp"

#include <array>
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
    float ao_strength = -1.0f;      // the occlusion's strength, 0 to 1.5; negative: the preference
    int color = -1;                 // the preview's view (PreviewView, preview.md rule 1); -1: by kind
    // With screenshot: the camera after framing, {x, y, z, distance, yaw, pitch}
    // (mm and degrees); empty: the framing's own (a close look at the lines).
    std::vector<float> look;
    std::string tool; // the plate tool chosen at start: move, rotate, scale, lay, cut, measure, paint, material, or mixing (its dialog)
    int measure[2] = {-1, -1}; // with --tool measure: two triangles of the first model picked as faces
    double cut = 0.0; // above 0: the first object cut at this height over the bed (model_tools.md, rule 13)
    // The first object's own settings at start, a JSON object of groups, with
    // their window open (settings_levels.md, rule 11).
    std::string object_settings;
    // What to add to the first object at start, for screenshots: "modifier",
    // "negative" and "range", separated by commas (model_tools.md, rules 2, 3 and 6).
    std::string object_add;
    // The first object turned at start about X, Y and Z, degrees, after what
    // --object-add adds (its volumes turn with it).
    std::array<double, 3> object_turn{0.0, 0.0, 0.0};
    // Brush strokes painted at start on every model (for screenshots and tests).
    struct Stroke {
        std::string channel; // support, seam or fuzzy
        bool block = false;  // the mark: block, else enforce
        float x = 0, y = 0, z = 0, radius = 1;
    };
    std::vector<Stroke> strokes;
    bool tips = false; // with screenshot: show the first-use tips (docs/UI.md, «Ayuda»)
    int tour = -1;     // the assistant's step to show at start (0 to 6); -1: none, or the first run's
    std::string info;  // with screenshot: the setting ("group.key") whose info card opens
    std::string level;   // the settings panel's level: basic, normal, advanced or custom; empty: the remembered one
    std::string section; // the settings panel's section to show ("print/strength", "material/cooling"...)
    std::string search;  // a search for a setting, gone to as Enter would (settings_panel.md, rule 5)
    bool single_column = false;   // the panel's «See all together» (rule 4)
    bool ortho = false;           // the view orthographic (K; preview.md, rule 10)
    bool labels = false;          // the parts' names over them (E)
    bool choose_settings = false; // with screenshot: the dialog that chooses what Custom shows (rule 16)
    bool credits = false;         // with screenshot: More › Credits and sources (credits.md, rule 4)
    std::string help;             // the help panel open at this tab: problems, procedures, glossary, tutorials, calibrations
    std::string knowledge;        // the help's data folder instead of the pack's (knowledge/)
    std::string help_open;        // with help: a problem or procedure id opened in it
    std::string tutorial;         // a guided tutorial of the help's data started at its step tutorial_step
    int tutorial_step = 0;
    int ray_samples = 32;           // with screenshot and ray tracing: the samples to wait for
    bool coarse = false;            // with screenshot: draw a large plate as while the view moves (V6)
    bool view_check = true;         // with screenshot: check the 3D view's picture (no black pixels, few sparkles)
    int bench_frames = 0;           // with screenshot: draw the ready 3D view this many times more, timed
    // What draws the window: "auto" (Vulkan when this machine has it and the
    // view runs on it, else OpenGL), "vulkan" (OpenGL only if Vulkan cannot)
    // or "opengl" (PLAN.md «Vista con Vulkan»).
    std::string graphics = "auto";
    bool ray_check = false;         // with screenshot: check the GPU's shadow rays against the CPU's
    bool moire_check = false;       // with screenshot: check that the moiré filter smooths lines finer than a pixel
    std::string compare_view;       // with screenshot: compare the 3D view with this screenshot's (the other graphics')
    std::string device;             // connect at start: a serial port, or "sim" for the simulated printer
    bool device_print = false;      // with screenshot and device: print what is sliced and wait for it to finish
    bool language_given = false;
    Language language = Language::Spanish;
};

// Runs the interface until its window closes. Returns the process exit code:
// in screenshot mode, 1 when the picture or the asked-for export failed.
int run_app(const AppOptions& options);

} // namespace tisma::ui
