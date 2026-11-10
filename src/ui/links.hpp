#pragma once

// Opening a web page in the system's browser (the Tisma Cloud account's page,
// cloud.md, rule 5). Own design over each system's way: ShellExecuteW on
// Windows, open(1) on macOS, xdg-open(1) elsewhere.

#include <filesystem>
#include <optional>
#include <string>

namespace tisma::ui {

// False when the address is not http:// or https://, or nothing opened it.
bool open_link(const std::string& url);

// The folder in the system's file manager (Help › Show Configuration Folder);
// false when it is not a folder or nothing opened it.
bool open_folder(const std::filesystem::path& folder);

// Whether the system asks for dark windows (Preferences › Appearance «Like the
// system»): Windows' AppsUseLightTheme, macOS's AppleInterfaceStyle, GNOME's
// color-scheme; nothing when it does not say.
std::optional<bool> system_prefers_dark();

} // namespace tisma::ui
