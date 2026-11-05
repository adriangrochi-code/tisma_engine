#pragma once

// TLS streams for the printers of the local network (printer_connections.md,
// «Bambu Lab en red local»): a client that does not check the certificate
// against an authority but gives its SHA-256 and its name (CN), so the caller
// can pin it once the person confirms it (rule 4), and that can resume the
// session of another connection (the FTPS data connection, rule 5). A
// listener with a self-signed certificate serves the tests' simulators. Over
// Mbed TLS (Apache-2.0, THIRD_PARTY.md). Own design; see PROVENANCE.md.

#include <cstddef>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>

namespace tisma::net {

class TlsError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

// Thrown by a read that waited its whole timeout.
class TlsTimeout : public TlsError {
public:
    using TlsError::TlsError;
};

class TlsStream {
public:
    ~TlsStream();
    TlsStream(const TlsStream&) = delete;
    TlsStream& operator=(const TlsStream&) = delete;

    // TCP to host:port, then the handshake (TLS 1.2, which every printer
    // speaks and whose session id is enough to resume). With `resume_from`,
    // offers that stream's session. Throws TlsError.
    static std::unique_ptr<TlsStream> connect(const std::string& host, int port, int timeout_ms,
                                              const TlsStream* resume_from = nullptr);

    void write(std::string_view bytes);
    // What arrives, up to `max` bytes; throws TlsTimeout after timeout_ms
    // with nothing, TlsError when the other side closed or failed.
    std::string read(std::size_t max, int timeout_ms);
    // Exactly n bytes, or throws.
    std::string read_exact(std::size_t n, int timeout_ms);
    void close();

    // The peer's certificate: the SHA-256 of its DER, 64 lowercase hex
    // digits, and its common name (a Bambu Lab printer's serial number).
    const std::string& peer_sha256() const;
    const std::string& peer_cn() const;
    // Whether the handshake resumed the session offered (client side).
    bool resumed() const;

private:
    friend class TlsListener;
    struct Impl;
    explicit TlsStream(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> impl_;
};

// For the tests' simulators: listens on 127.0.0.1 on a free port with a new
// self-signed certificate named `cn`, and keeps a session cache, so a client
// can resume.
class TlsListener {
public:
    explicit TlsListener(const std::string& cn);
    ~TlsListener();
    TlsListener(const TlsListener&) = delete;
    TlsListener& operator=(const TlsListener&) = delete;

    int port() const;
    std::string cert_sha256() const;
    // The next connection after its handshake; nullptr after timeout_ms.
    std::unique_ptr<TlsStream> accept(int timeout_ms);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace tisma::net
