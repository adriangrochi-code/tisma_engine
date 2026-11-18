#pragma once

// A small WebSocket client (RFC 6455) over plain TCP, for printers on the
// local network that speak it (Elegoo's SDCP, printer_connections.md): the
// opening handshake with Sec-WebSocket-Key and its Accept checked (SHA-1 of
// FIPS 180-4, Base64 of RFC 4648), masked text frames out, text frames in,
// ping answered with pong, close. No TLS, no extensions. Own design; see
// PROVENANCE.md.

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

namespace tisma::net {

// SHA-1 (FIPS 180-4) and Base64 (RFC 4648, with padding), for the handshake.
std::string sha1(const std::string& data); // 20 bytes
std::string base64(const std::string& data);

class WebSocket {
public:
    ~WebSocket();
    // Connects to ws://host:port/path; nothing on failure, with `status` the
    // HTTP status the server answered (0 when nothing answered) and `error`
    // why, in English.
    static std::unique_ptr<WebSocket> connect(const std::string& host, int port, const std::string& path, int timeout_ms,
                                              int& status, std::string& error);
    bool send_text(const std::string& text);
    // The next text message, waiting up to timeout_ms; nothing when none came
    // in time or the connection closed (closed() then says so).
    std::optional<std::string> receive(int timeout_ms);
    bool closed() const { return closed_; }
    void close();

private:
    WebSocket() = default;
    bool send_frame(int opcode, const std::string& payload);
    bool read_bytes(std::size_t n, std::string& out, int timeout_ms);
    std::intptr_t socket_ = -1;
    std::string pending_;
    bool closed_ = false;
};

} // namespace tisma::net
