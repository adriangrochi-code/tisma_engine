#pragma once

#include "ui/files.hpp"

#include <imgui.h>

#include <filesystem>
#include <string>
#include <vector>

namespace tisma::ui {

// A file chooser drawn with Dear ImGui, the same on every system (no GTK or
// other toolkit). Own design; see PROVENANCE.md.
class FileBrowser {
public:
    enum class Mode { Open, Save };

    // Opens it; extensions are lower case with the dot (empty: every file).
    // In Save mode `name` is the proposed file name.
    void open(Mode mode, std::string title, std::vector<std::string> extensions, const std::filesystem::path& folder,
              std::string name = {});
    bool is_open() const { return open_; }

    // Draws it while open. Returns true once, with the chosen path, when the
    // user picks a file (in Save mode, after confirming a replacement).
    bool draw(std::filesystem::path& chosen, ImTextureRef folder_icon, ImVec4 icon_tint);

    const std::filesystem::path& folder() const { return folder_; }

private:
    void go(const std::filesystem::path& folder);
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
    int selected_ = -1;
};

} // namespace tisma::ui
