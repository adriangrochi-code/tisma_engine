#pragma once

// Uploading a file by FTP over TLS (RFC 959, FTP; RFC 4217, FTP over TLS), as
// a Bambu Lab printer takes it (printer_connections.md, «Bambu Lab en red
// local», rule 5): implicit TLS from the first byte (port 990), USER and
// PASS, PBSZ 0 and PROT P, binary type, passive mode, and a data connection
// that resumes the control connection's TLS session. Own design; see
// PROVENANCE.md.

#include <functional>
#include <stdexcept>
#include <string>

namespace tisma::device {

class FtpsError : public std::runtime_error {
public:
    FtpsError(std::string what_code, const std::string& message) : std::runtime_error(message), code(std::move(what_code)) {}
    std::string code; // unreachable, unauthorized, certificate_changed, refused, timeout, failed
};

struct FtpsConfig {
    std::string host;
    int port = 990;
    std::string user, password; // the password is a secret: never in a message
    int timeout_ms = 15000;
    // The certificate the person accepted (SHA-256, hex) and the printer's
    // name in it: the server must show one of them (empty: not checked).
    std::string pinned_sha256, expected_cn;
};

struct FtpsUpload {
    bool resumed = false; // whether the data connection resumed the TLS session
};

// Stores `bytes` as `name` in the server's root. `progress` gets the bytes
// sent so far. Throws FtpsError.
FtpsUpload ftps_upload(const FtpsConfig& config, const std::string& name, const std::string& bytes,
                       const std::function<void(std::size_t)>& progress = {});

// A passive-mode reply's port: «227 Entering Passive Mode (h1,h2,h3,h4,p1,p2)»
// gives p1 * 256 + p2; -1 when it is not one.
int pasv_port(const std::string& reply);

} // namespace tisma::device
