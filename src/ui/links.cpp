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

} // namespace tisma::ui
