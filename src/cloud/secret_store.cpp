#include "cloud/secret_store.hpp"

#include "cad/process.hpp"

#include <cstdlib>
#include <filesystem>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <wincred.h>
#endif

// Written from each system's documentation: the Windows Credential Manager
// API (CredWriteW, CredReadW, CredDeleteW), macOS's security(1) and
// libsecret's secret-tool(1). Own design.

namespace tisma::cloud {

namespace {

constexpr const char* kService = "Tisma Cloud";

#ifdef _WIN32

std::wstring target(const std::string& server) {
    const std::string name = std::string(kService) + "/" + server;
    const int n = MultiByteToWideChar(CP_UTF8, 0, name.data(), int(name.size()), nullptr, 0);
    std::wstring out(std::size_t(n > 0 ? n : 0), L'\0');
    if (n > 0)
        MultiByteToWideChar(CP_UTF8, 0, name.data(), int(name.size()), out.data(), n);
    return out;
}

#else

// A program on the PATH, by name.
std::filesystem::path on_path(const std::string& name) {
    const char* path = std::getenv("PATH");
    if (!path)
        return {};
    const std::string all = path;
    for (std::size_t start = 0; start <= all.size();) {
        std::size_t end = all.find(':', start);
        if (end == std::string::npos)
            end = all.size();
        const std::filesystem::path dir = all.substr(start, end - start);
        std::error_code ec;
        if (!dir.empty() && std::filesystem::is_regular_file(dir / name, ec))
            return dir / name;
        start = end + 1;
    }
    return {};
}

std::filesystem::path secret_tool() {
    if (const char* forced = std::getenv("TISMA_SECRET_TOOL"); forced && *forced)
        return forced;
    return on_path("secret-tool");
}

#ifdef __APPLE__
const std::filesystem::path kSecurity = "/usr/bin/security";
#endif

#endif

} // namespace

SecretStore secret_store() {
#if defined(_WIN32)
    return SecretStore::WindowsCredentials;
#elif defined(__APPLE__)
    std::error_code ec;
    return std::filesystem::exists(kSecurity, ec) ? SecretStore::MacKeychain : SecretStore::None;
#else
    return secret_tool().empty() ? SecretStore::None : SecretStore::SecretTool;
#endif
}

bool store_secret(const std::string& server, const std::string& secret, std::string& error) {
#if defined(_WIN32)
    std::wstring name = target(server);
    CREDENTIALW cred{};
    cred.Type = CRED_TYPE_GENERIC;
    cred.TargetName = name.data();
    cred.CredentialBlobSize = DWORD(secret.size());
    cred.CredentialBlob = reinterpret_cast<LPBYTE>(const_cast<char*>(secret.data()));
    cred.Persist = CRED_PERSIST_LOCAL_MACHINE;
    if (!CredWriteW(&cred, 0)) {
        error = "the Credential Manager refused it (error " + std::to_string(GetLastError()) + ")";
        return false;
    }
    return true;
#elif defined(__APPLE__)
    if (secret_store() == SecretStore::None) {
        error = "this system has no keychain program";
        return false;
    }
    // -w with the secret as its argument is what security(1) offers; -U
    // replaces an older one.
    const cad::ProcessResult r =
        cad::run_process(kSecurity, {"add-generic-password", "-U", "-s", kService, "-a", server, "-w", secret}, 20.0);
    if (!r.ok()) {
        error = "the keychain refused it: " + (r.error.empty() ? r.err : r.error);
        return false;
    }
    return true;
#else
    const std::filesystem::path tool = secret_tool();
    if (tool.empty()) {
        error = "this system has no secret store (secret-tool)";
        return false;
    }
    // The secret goes on stdin, never among the arguments.
    const cad::ProcessResult r =
        cad::run_process(tool, {"store", "--label=" + std::string(kService), "service", "tisma-cloud", "server", server},
                         20.0, secret);
    if (!r.ok()) {
        error = "the secret store refused it: " + (r.error.empty() ? r.err : r.error);
        return false;
    }
    return true;
#endif
}

std::optional<std::string> load_secret(const std::string& server) {
#if defined(_WIN32)
    const std::wstring name = target(server);
    PCREDENTIALW cred = nullptr;
    if (!CredReadW(name.c_str(), CRED_TYPE_GENERIC, 0, &cred))
        return std::nullopt;
    std::string secret(reinterpret_cast<const char*>(cred->CredentialBlob), cred->CredentialBlobSize);
    CredFree(cred);
    return secret;
#else
#if defined(__APPLE__)
    if (secret_store() == SecretStore::None)
        return std::nullopt;
    const cad::ProcessResult r =
        cad::run_process(kSecurity, {"find-generic-password", "-s", kService, "-a", server, "-w"}, 20.0);
#else
    const std::filesystem::path tool = secret_tool();
    if (tool.empty())
        return std::nullopt;
    const cad::ProcessResult r = cad::run_process(tool, {"lookup", "service", "tisma-cloud", "server", server}, 20.0);
#endif
    if (!r.ok() || r.out.empty())
        return std::nullopt;
    std::string secret = r.out;
    while (!secret.empty() && (secret.back() == '\n' || secret.back() == '\r'))
        secret.pop_back();
    return secret;
#endif
}

bool forget_secret(const std::string& server) {
#if defined(_WIN32)
    return CredDeleteW(target(server).c_str(), CRED_TYPE_GENERIC, 0) || GetLastError() == ERROR_NOT_FOUND;
#elif defined(__APPLE__)
    if (secret_store() == SecretStore::None)
        return false;
    return cad::run_process(kSecurity, {"delete-generic-password", "-s", kService, "-a", server}, 20.0).started;
#else
    const std::filesystem::path tool = secret_tool();
    if (tool.empty())
        return false;
    return cad::run_process(tool, {"clear", "service", "tisma-cloud", "server", server}, 20.0).ok();
#endif
}

} // namespace tisma::cloud
