#pragma once

// The system's Open and Save dialogs on macOS (file_dialog_macos.mm). Runs at
// once and returns true; `chosen` is empty when cancelled.

#include <filesystem>
#include <string>
#include <vector>

namespace tisma::ui {

bool macos_file_dialog(bool save, const std::string& title, const std::vector<std::string>& extensions,
                       const std::filesystem::path& folder, const std::string& name, std::filesystem::path& chosen);

} // namespace tisma::ui
