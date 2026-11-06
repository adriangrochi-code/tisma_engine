// FTP over implicit TLS (RFC 959, RFC 4217). Own code; see ftps.hpp and
// PROVENANCE.md.

#include "device/ftps.hpp"

#include "net/tls.hpp"

#include <algorithm>
#include <cctype>
#include <memory>

namespace tisma::device {

int pasv_port(const std::string& reply) {
    const auto open = reply.find('('), close = reply.find(')', open == std::string::npos ? 0 : open);
    if (!reply.starts_with("227") || open == std::string::npos || close == std::string::npos)
        return -1;
    int numbers[6];
    int count = 0;
    long value = -1;
    for (std::size_t i = open + 1; i <= close; ++i) {
        const char c = reply[i];
        if (std::isdigit(static_cast<unsigned char>(c))) {
            value = (value < 0 ? 0 : value * 10) + (c - '0');
            if (value > 255)
                return -1;
        } else if (c == ',' || c == ')') {
            if (value < 0 || count == 6)
                return -1;
            numbers[count++] = int(value);
            value = -1;
        } else if (c != ' ') {
            return -1;
        }
    }
    return count == 6 ? numbers[4] * 256 + numbers[5] : -1;
}

namespace {

// The control connection: commands and their replies (RFC 959, 4.2).
class Control {
public:
    Control(std::unique_ptr<net::TlsStream> s, int timeout_ms) : s_(std::move(s)), timeout_ms_(timeout_ms) {}
    // A whole reply, a multi-line one included («123-…» up to «123 …»).
    std::string reply() {
        std::string first;
        for (;;) {
            const std::string line = next_line();
            if (first.empty()) {
                if (line.size() < 3)
                    throw FtpsError("failed", "FTPS: an unreadable reply");
                first = line;
                if (line.size() < 4 || line[3] != '-')
                    return line;
            } else if (line.size() >= 4 && line.compare(0, 3, first, 0, 3) == 0 && line[3] == ' ') {
                return first;
            }
        }
    }
    std::string command(const std::string& text) {
        s_->write(text + "\r\n");
        return reply();
    }
    net::TlsStream& stream() { return *s_; }

private:
    std::string next_line() {
        for (;;) {
            if (const auto end = buffer_.find("\r\n"); end != std::string::npos) {
                std::string line = buffer_.substr(0, end);
                buffer_.erase(0, end + 2);
                return line;
            }
            try {
                buffer_ += s_->read(4096, timeout_ms_);
            } catch (const net::TlsTimeout&) {
                throw FtpsError("timeout", "FTPS: the printer did not answer in time");
            } catch (const net::TlsError& e) {
                throw FtpsError("failed", std::string("FTPS: ") + e.what());
            }
        }
    }
    std::unique_ptr<net::TlsStream> s_;
    int timeout_ms_;
    std::string buffer_;
};

bool is(const std::string& reply, char first) { return !reply.empty() && reply[0] == first; }

} // namespace

FtpsUpload ftps_upload(const FtpsConfig& config, const std::string& name, const std::string& bytes,
                       const std::function<void(std::size_t)>& progress) {
    std::unique_ptr<net::TlsStream> tls;
    try {
        tls = net::TlsStream::connect(config.host, config.port, config.timeout_ms);
    } catch (const net::TlsError& e) {
        throw FtpsError("unreachable", std::string("FTPS: ") + e.what());
    }
    if (!config.pinned_sha256.empty() && tls->peer_sha256() != config.pinned_sha256 &&
        (config.expected_cn.empty() || tls->peer_cn() != config.expected_cn))
        throw FtpsError("certificate_changed", "FTPS: the printer's certificate is not the one accepted");
    Control c(std::move(tls), config.timeout_ms);
    if (!is(c.reply(), '2'))
        throw FtpsError("refused", "FTPS: the printer did not greet");
    std::string r = c.command("USER " + config.user);
    if (is(r, '3'))
        r = c.command("PASS " + config.password);
    if (r.starts_with("530"))
        throw FtpsError("unauthorized", "FTPS: the printer refused the access code");
    if (!is(r, '2'))
        throw FtpsError("refused", "FTPS: login refused: " + r.substr(0, 3));
    // Protected data (RFC 4217, 9), binary, passive.
    if (!is(c.command("PBSZ 0"), '2') || !is(c.command("PROT P"), '2') || !is(c.command("TYPE I"), '2'))
        throw FtpsError("refused", "FTPS: the printer refused protected binary transfers");
    r = c.command("PASV");
    const int port = pasv_port(r);
    if (port <= 0)
        throw FtpsError("failed", "FTPS: no passive port in «" + r + "»");
    // The address in the reply is not used: the data goes to the same host
    // (a printer behind NAT may give its own address).
    c.stream().write("STOR " + name + "\r\n");
    std::unique_ptr<net::TlsStream> data;
    try {
        data = net::TlsStream::connect(config.host, port, config.timeout_ms, &c.stream());
    } catch (const net::TlsError& e) {
        throw FtpsError("failed", std::string("FTPS data: ") + e.what());
    }
    r = c.reply();
    if (!is(r, '1'))
        throw FtpsError("refused", "FTPS: the printer refused the file: " + r);
    FtpsUpload out;
    out.resumed = data->resumed();
    constexpr std::size_t kChunk = 64 * 1024;
    try {
        for (std::size_t at = 0; at < bytes.size(); at += kChunk) {
            data->write(std::string_view(bytes).substr(at, kChunk));
            if (progress)
                progress(std::min(bytes.size(), at + kChunk));
        }
        data->close();
    } catch (const net::TlsError& e) {
        throw FtpsError("failed", std::string("FTPS data: ") + e.what());
    }
    r = c.reply();
    if (!is(r, '2'))
        throw FtpsError("failed", "FTPS: the printer did not keep the file: " + r);
    try {
        c.command("QUIT");
    } catch (const FtpsError&) {
    }
    return out;
}

} // namespace tisma::device
