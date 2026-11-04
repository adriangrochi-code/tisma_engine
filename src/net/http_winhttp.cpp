// The HTTP client on Windows: WinHTTP, the system's own (its certificates,
// its proxy settings, TLS by Schannel). Written from Microsoft's WinHTTP
// documentation; own design, see PROVENANCE.md.

#include "net/http.hpp"
#include "net/http_impl.hpp"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <winhttp.h>

#include <algorithm>
#include <string>
#include <vector>

namespace tisma::net {

namespace {

std::wstring wide(const std::string& text) {
    if (text.empty())
        return {};
    const int n = MultiByteToWideChar(CP_UTF8, 0, text.data(), int(text.size()), nullptr, 0);
    std::wstring out(std::size_t(n > 0 ? n : 0), L'\0');
    if (n > 0)
        MultiByteToWideChar(CP_UTF8, 0, text.data(), int(text.size()), out.data(), n);
    return out;
}

// A WinHTTP handle closed when it goes.
struct Handle {
    HINTERNET h = nullptr;
    explicit Handle(HINTERNET handle) : h(handle) {}
    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;
    ~Handle() {
        if (h)
            WinHttpCloseHandle(h);
    }
    explicit operator bool() const { return h != nullptr; }
};

// The last WinHTTP failure, in words for the common ones.
std::string last_error(const char* what) {
    const DWORD code = GetLastError();
    std::string text;
    switch (code) {
    case ERROR_WINHTTP_TIMEOUT: text = "the server did not answer in time"; break;
    case ERROR_WINHTTP_NAME_NOT_RESOLVED: text = "the server's name was not found"; break;
    case ERROR_WINHTTP_CANNOT_CONNECT: text = "could not connect to the server"; break;
    case ERROR_WINHTTP_CONNECTION_ERROR: text = "the connection was lost"; break;
    case ERROR_WINHTTP_SECURE_FAILURE: text = "the server's certificate is not trusted"; break;
    default: text = "WinHTTP error " + std::to_string(unsigned(code)); break;
    }
    return std::string(what) + ": " + text;
}

} // namespace

bool https_available() { return true; }

Response perform(const Request& request) {
    Response out;
    Url url;
    if (!parse_url(request.url, url)) {
        out.error = "not an http:// or https:// address: " + request.url;
        return out;
    }
    std::string body, form_type;
    if (!prepare_body(request, body, form_type, out.error))
        return out;

    // The system's proxy (Windows 8.1 and later), or the older default.
    Handle session(WinHttpOpen(L"Tisma", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME,
                               WINHTTP_NO_PROXY_BYPASS, 0));
    if (!session)
        session.h = WinHttpOpen(L"Tisma", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME,
                                WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session) {
        out.error = last_error("opening WinHTTP");
        return out;
    }
    const int connect_ms = std::max(1, request.connect_timeout_s) * 1000;
    const int total_ms = std::max(1, request.timeout_s) * 1000;
    WinHttpSetTimeouts(session.h, connect_ms, connect_ms, total_ms, total_ms);

    Handle connection(WinHttpConnect(session.h, wide(url.host).c_str(), INTERNET_PORT(url.port), 0));
    if (!connection) {
        out.error = last_error("connecting");
        return out;
    }
    Handle req(WinHttpOpenRequest(connection.h, wide(request.method).c_str(), wide(url.path).c_str(), nullptr,
                                  WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                  url.https ? WINHTTP_FLAG_SECURE : 0));
    if (!req) {
        out.error = last_error("opening the request");
        return out;
    }
    // No redirects: the API answers where it is asked.
    DWORD no_redirects = WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
    WinHttpSetOption(req.h, WINHTTP_OPTION_REDIRECT_POLICY, &no_redirects, sizeof(no_redirects));

    std::string headers;
    bool has_type = false;
    for (const auto& [name, value] : request.headers) {
        if (_stricmp(name.c_str(), "Content-Type") == 0) {
            if (!form_type.empty())
                continue;
            has_type = true;
        }
        headers += name + ": " + value + "\r\n";
    }
    if (!form_type.empty())
        headers += "Content-Type: " + form_type + "\r\n";
    else if (!has_type && !body.empty())
        headers += "Content-Type: application/octet-stream\r\n";
    const std::wstring wide_headers = wide(headers);

    const DWORD size = DWORD(body.size());
    if (!WinHttpSendRequest(req.h, wide_headers.empty() ? WINHTTP_NO_ADDITIONAL_HEADERS : wide_headers.c_str(),
                            wide_headers.empty() ? 0 : DWORD(-1L),
                            body.empty() ? WINHTTP_NO_REQUEST_DATA : static_cast<LPVOID>(body.data()), size, size, 0) ||
        !WinHttpReceiveResponse(req.h, nullptr)) {
        out.error = last_error("sending the request");
        return out;
    }
    DWORD status = 0, status_size = sizeof(status);
    if (!WinHttpQueryHeaders(req.h, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX,
                             &status, &status_size, WINHTTP_NO_HEADER_INDEX)) {
        out.error = last_error("reading the answer");
        return out;
    }
    // The challenge of a server that asks for a user and a password (Digest).
    if (status == 401) {
        DWORD length = 0;
        WinHttpQueryHeaders(req.h, WINHTTP_QUERY_WWW_AUTHENTICATE, WINHTTP_HEADER_NAME_BY_INDEX, WINHTTP_NO_OUTPUT_BUFFER,
                            &length, WINHTTP_NO_HEADER_INDEX);
        if (GetLastError() == ERROR_INSUFFICIENT_BUFFER && length > 0) {
            std::wstring header(length / sizeof(wchar_t) + 1, L'\0');
            if (WinHttpQueryHeaders(req.h, WINHTTP_QUERY_WWW_AUTHENTICATE, WINHTTP_HEADER_NAME_BY_INDEX, header.data(), &length,
                                    WINHTTP_NO_HEADER_INDEX)) {
                header.resize(length / sizeof(wchar_t));
                const int bytes = WideCharToMultiByte(CP_UTF8, 0, header.c_str(), int(header.size()), nullptr, 0, nullptr, nullptr);
                std::string narrow(std::size_t(std::max(bytes, 0)), '\0');
                if (bytes > 0)
                    WideCharToMultiByte(CP_UTF8, 0, header.c_str(), int(header.size()), narrow.data(), bytes, nullptr, nullptr);
                out.www_authenticate = narrow;
            }
        }
    }
    std::vector<char> chunk;
    for (;;) {
        DWORD available = 0;
        if (!WinHttpQueryDataAvailable(req.h, &available)) {
            out.error = last_error("reading the answer");
            return out;
        }
        if (available == 0)
            break;
        if (request.max_bytes > 0 && out.body.size() + available > request.max_bytes) {
            out.error = "the answer is larger than " + std::to_string(request.max_bytes) + " bytes";
            out.body.clear();
            return out;
        }
        chunk.resize(available);
        DWORD read = 0;
        if (!WinHttpReadData(req.h, chunk.data(), available, &read)) {
            out.error = last_error("reading the answer");
            return out;
        }
        out.body.append(chunk.data(), read);
    }
    out.status = int(status);
    return out;
}

} // namespace tisma::net
