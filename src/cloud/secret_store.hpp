#pragma once

// The Tisma Cloud session's token kept as a password (cloud.md, rule 3): in
// the system's own store, never in interface.json, the logs or messages.
// - Windows: the Credential Manager (its API).
// - macOS: the login keychain, through the system's `security` program.
// - Linux: the Secret Service (GNOME Keyring, KWallet), through `secret-tool`
//   (libsecret's program, run apart: nothing of it is linked).
// Without a store the token lives only while the program runs, and the next
// start asks to log in again. Own design; see PROVENANCE.md.

#include <optional>
#include <string>

namespace tisma::cloud {

enum class SecretStore { None, WindowsCredentials, MacKeychain, SecretTool };

// The store this machine has (Linux: when secret-tool is on the PATH, or the
// program named by TISMA_SECRET_TOOL, which the tests use).
SecretStore secret_store();

// The secret for this server (one per server address), kept, read or forgotten.
bool store_secret(const std::string& server, const std::string& secret, std::string& error);
std::optional<std::string> load_secret(const std::string& server);
bool forget_secret(const std::string& server);

} // namespace tisma::cloud
