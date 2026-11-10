#pragma once

// Opening a web page in the system's browser (the Tisma Cloud account's page,
// cloud.md, rule 5). Own design over each system's way: ShellExecuteW on
// Windows, open(1) on macOS, xdg-open(1) elsewhere.

#include <filesystem>
#include <string>

namespace tisma::ui {

// False when the address is not http:// or https://, or nothing opened it.
bool open_link(const std::string& url);

// The folder in the system's file manager (Help › Show Configuration Folder);
// false when it is not a folder or nothing opened it.
bool open_folder(const std::filesystem::path& folder);

} // namespace tisma::ui
