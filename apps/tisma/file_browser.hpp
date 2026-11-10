#pragma once

#include "ui/files.hpp"

#include <imgui.h>

#include <filesystem>
#include <string>
#include <vector>

namespace tisma::ui {

// A file chooser drawn with Dear ImGui, the same on every system (no GTK or
// other toolkit). On Windows, once use_native(true), the system's own Open and
// Save As dialogs instead (the testers' BH40); scripted and screenshot runs
// keep the drawn one. Own design; see PROVENANCE.md.
class FileBrowser {
public:
    // Whether open() shows the system's dialog where there is one (Windows).
    static void use_native(bool native) { native_ = native; }

    enum class Mode { Open, Save };

    // Opens it; extensions are lower case with the dot (empty: every file).
    // In Save mode `name` is the proposed file name.
    void open(Mode mode, std::string title, std::vector<std::string> extensions, const std::filesystem::path& folder,
              std::string name = {});
    bool is_open() const { return open_; }
    void close() { open_ = pending_open_ = confirm_replace_ = false; } // as Cancel (the scripted tests)

    // Draws it while open. Returns true once, with the chosen path, when the
    // user picks a file (in Save mode, after confirming a replacement).
    bool draw(std::filesystem::path& chosen, ImTextureRef folder_icon, ImVec4 icon_tint);

    const std::filesystem::path& folder() const { return folder_; }

private:
    void go(const std::filesystem::path& folder);
    // The system's dialog, run at once; false where there is none.
    bool run_native();

    static inline bool native_ = false;
    bool native_done_ = false;               // the system's dialog closed: draw() hands its answer once
    std::filesystem::path native_chosen_;    // its answer; empty: cancelled
    std::filesystem::path target() const;

    Mode mode_ = Mode::Open;
    std::string title_;
    std::vector<std::string> extensions_;
    std::filesystem::path folder_;
    std::vector<FolderEntry> entries_;
    bool readable_ = true;
    std::string name_;
    std::string folder_text_;
    bool show_all_ = false;
    bool open_ = false, pending_open_ = false, confirm_replace_ = false;
    std::filesystem::path replace_for_; // the file the "Replace?" question is about
    int selected_ = -1;
};

} // namespace tisma::ui
