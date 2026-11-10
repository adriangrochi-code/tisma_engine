#include "ui/links.hpp"

#include "cad/process.hpp"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>
#endif

namespace tisma::ui {

bool open_link(const std::string& url) {
    if (url.rfind("https://", 0) != 0 && url.rfind("http://", 0) != 0)
        return false;
    for (const char c : url)
        if (static_cast<unsigned char>(c) < 0x20 || c == '"')
            return false; // never anything a shell or the browser could read as more than an address
#ifdef _WIN32
    const int n = MultiByteToWideChar(CP_UTF8, 0, url.data(), int(url.size()), nullptr, 0);
    std::wstring wide(std::size_t(n > 0 ? n : 0), L'\0');
    if (n > 0)
        MultiByteToWideChar(CP_UTF8, 0, url.data(), int(url.size()), wide.data(), n);
    return reinterpret_cast<INT_PTR>(ShellExecuteW(nullptr, L"open", wide.c_str(), nullptr, nullptr, SW_SHOWNORMAL)) > 32;
#elif defined(__APPLE__)
    return cad::run_process("/usr/bin/open", {url}, 10.0).ok();
#else
    return cad::run_process("/usr/bin/xdg-open", {url}, 10.0).started;
#endif
}

bool open_folder(const std::filesystem::path& folder) {
    std::error_code ec;
    if (!std::filesystem::is_directory(folder, ec))
        return false;
#ifdef _WIN32
    return reinterpret_cast<INT_PTR>(ShellExecuteW(nullptr, L"open", folder.wstring().c_str(), nullptr, nullptr,
                                                   SW_SHOWNORMAL)) > 32;
#elif defined(__APPLE__)
    return cad::run_process("/usr/bin/open", {folder.string()}, 10.0).ok();
#else
    return cad::run_process("/usr/bin/xdg-open", {folder.string()}, 10.0).started;
#endif
}

std::optional<bool> system_prefers_dark() {
#ifdef _WIN32
    DWORD light = 1, size = sizeof light;
    if (RegGetValueW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                     L"AppsUseLightTheme", RRF_RT_REG_DWORD, nullptr, &light, &size) != ERROR_SUCCESS)
        return std::nullopt;
    return light == 0;
#elif defined(__APPLE__)
    const cad::ProcessResult r = cad::run_process("/usr/bin/defaults", {"read", "-g", "AppleInterfaceStyle"}, 5.0);
    if (!r.started)
        return std::nullopt;
    return r.out.find("Dark") != std::string::npos; // the key is missing in light mode
#else
    for (const char* exe : {"/usr/bin/gsettings", "/bin/gsettings"}) {
        std::error_code ec;
        if (!std::filesystem::exists(exe, ec))
            continue;
        const cad::ProcessResult r = cad::run_process(exe, {"get", "org.gnome.desktop.interface", "color-scheme"}, 5.0);
        if (!r.ok())
            return std::nullopt;
        if (r.out.find("dark") != std::string::npos)
            return true;
        if (r.out.find("light") != std::string::npos || r.out.find("default") != std::string::npos)
            return false;
        return std::nullopt;
    }
    return std::nullopt;
#endif
}

} // namespace tisma::ui
