#include "net/websocket.hpp"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include <array>
#include <cctype>
#include <cstdlib>
#include <chrono>
#include <cstring>
#include <mutex>
#include <random>

#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0 // Windows has no SIGPIPE
#endif

namespace tisma::net {

namespace {

#ifdef _WIN32
using Socket = SOCKET;
constexpr Socket kNoSocket = INVALID_SOCKET;
void close_socket(Socket s) { closesocket(s); }
void sockets_once() {
    static std::once_flag once;
    std::call_once(once, [] {
        WSADATA data;
        WSAStartup(MAKEWORD(2, 2), &data);
    });
}
#else
using Socket = int;
constexpr Socket kNoSocket = -1;
void close_socket(Socket s) { ::close(s); }
void sockets_once() {}
#endif

bool readable(Socket s, int ms) {
    fd_set set;
    FD_ZERO(&set);
    FD_SET(s, &set);
    timeval tv{ms / 1000, (ms % 1000) * 1000};
    return select(int(s) + 1, &set, nullptr, nullptr, &tv) > 0;
}

std::uint32_t rotl(std::uint32_t x, int n) { return (x << n) | (x >> (32 - n)); }

} // namespace

std::string sha1(const std::string& data) {
    // FIPS 180-4, section 6.1.
    std::uint32_t h[5] = {0x67452301u, 0xEFCDAB89u, 0x98BADCFEu, 0x10325476u, 0xC3D2E1F0u};
    std::string m = data;
    const std::uint64_t bits = std::uint64_t(data.size()) * 8;
    m += char(0x80);
    while (m.size() % 64 != 56)
        m += char(0);
    for (int i = 7; i >= 0; --i)
        m += char((bits >> (8 * i)) & 0xFF);
    for (std::size_t chunk = 0; chunk < m.size(); chunk += 64) {
        std::uint32_t w[80];
        for (int i = 0; i < 16; ++i)
            w[i] = (std::uint32_t(std::uint8_t(m[chunk + 4 * i])) << 24) | (std::uint32_t(std::uint8_t(m[chunk + 4 * i + 1])) << 16) |
                   (std::uint32_t(std::uint8_t(m[chunk + 4 * i + 2])) << 8) | std::uint32_t(std::uint8_t(m[chunk + 4 * i + 3]));
        for (int i = 16; i < 80; ++i)
            w[i] = rotl(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
        std::uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4];
        for (int i = 0; i < 80; ++i) {
            std::uint32_t f, k;
            if (i < 20)
                f = (b & c) | (~b & d), k = 0x5A827999u;
            else if (i < 40)
                f = b ^ c ^ d, k = 0x6ED9EBA1u;
            else if (i < 60)
                f = (b & c) | (b & d) | (c & d), k = 0x8F1BBCDCu;
            else
                f = b ^ c ^ d, k = 0xCA62C1D6u;
            const std::uint32_t t = rotl(a, 5) + f + e + k + w[i];
            e = d, d = c, c = rotl(b, 30), b = a, a = t;
        }
        h[0] += a, h[1] += b, h[2] += c, h[3] += d, h[4] += e;
    }
    std::string out;
    for (const std::uint32_t v : h)
        for (int i = 3; i >= 0; --i)
            out += char((v >> (8 * i)) & 0xFF);
    return out;
}

std::string base64(const std::string& data) {
    static const char* table = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    std::size_t i = 0;
    for (; i + 2 < data.size(); i += 3) {
        const std::uint32_t v = (std::uint32_t(std::uint8_t(data[i])) << 16) | (std::uint32_t(std::uint8_t(data[i + 1])) << 8) |
                                std::uint32_t(std::uint8_t(data[i + 2]));
        out += table[(v >> 18) & 63], out += table[(v >> 12) & 63], out += table[(v >> 6) & 63], out += table[v & 63];
    }
    if (i + 1 == data.size()) {
        const std::uint32_t v = std::uint32_t(std::uint8_t(data[i])) << 16;
        out += table[(v >> 18) & 63], out += table[(v >> 12) & 63], out += "==";
    } else if (i + 2 == data.size()) {
        const std::uint32_t v = (std::uint32_t(std::uint8_t(data[i])) << 16) | (std::uint32_t(std::uint8_t(data[i + 1])) << 8);
        out += table[(v >> 18) & 63], out += table[(v >> 12) & 63], out += table[(v >> 6) & 63], out += '=';
    }
    return out;
}

WebSocket::~WebSocket() { close(); }

void WebSocket::close() {
    if (socket_ != -1) {
        if (!closed_)
            send_frame(0x8, {});
        close_socket(Socket(socket_));
        socket_ = -1;
    }
    closed_ = true;
}

std::unique_ptr<WebSocket> WebSocket::connect(const std::string& host, int port, const std::string& path, int timeout_ms,
                                              int& status, std::string& error) {
    sockets_once();
    status = 0;
    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    addrinfo* list = nullptr;
    if (getaddrinfo(host.c_str(), std::to_string(port).c_str(), &hints, &list) != 0 || !list) {
        error = "cannot find " + host;
        return nullptr;
    }
    const Socket s = socket(list->ai_family, list->ai_socktype, list->ai_protocol);
    if (s == kNoSocket) {
        freeaddrinfo(list);
        error = "no socket";
        return nullptr;
    }
#ifdef _WIN32
    u_long nonblocking = 1;
    ioctlsocket(s, FIONBIO, &nonblocking);
#else
    const int flags = fcntl(s, F_GETFL, 0);
    fcntl(s, F_SETFL, flags | O_NONBLOCK);
#endif
    ::connect(s, list->ai_addr, int(list->ai_addrlen));
    freeaddrinfo(list);
    fd_set w;
    FD_ZERO(&w);
    FD_SET(s, &w);
    timeval tv{timeout_ms / 1000, (timeout_ms % 1000) * 1000};
    int err = 0;
    socklen_t err_size = sizeof err;
    if (select(int(s) + 1, nullptr, &w, nullptr, &tv) <= 0 ||
        getsockopt(s, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&err), &err_size) != 0 || err != 0) {
        close_socket(s);
        error = "cannot reach " + host + ":" + std::to_string(port);
        return nullptr;
    }
#ifdef _WIN32
    nonblocking = 0;
    ioctlsocket(s, FIONBIO, &nonblocking);
#else
    fcntl(s, F_SETFL, flags);
#endif
    std::unique_ptr<WebSocket> ws(new WebSocket);
    ws->socket_ = std::intptr_t(s);
    // The opening handshake (RFC 6455, 4.1).
    std::string key_bytes(16, '\0');
    std::random_device rd;
    for (char& c : key_bytes)
        c = char(rd() & 0xFF);
    const std::string key = base64(key_bytes);
    const std::string hello = "GET " + path + " HTTP/1.1\r\nHost: " + host + ":" + std::to_string(port) +
                              "\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Key: " + key +
                              "\r\nSec-WebSocket-Version: 13\r\n\r\n";
    if (send(s, hello.data(), int(hello.size()), MSG_NOSIGNAL) != int(hello.size())) {
        error = "the connection closed";
        return nullptr;
    }
    const auto end = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
    std::string head;
    char buffer[1024];
    while (head.find("\r\n\r\n") == std::string::npos) {
        const int left = int(std::chrono::duration_cast<std::chrono::milliseconds>(end - std::chrono::steady_clock::now()).count());
        if (left <= 0 || !readable(s, left)) {
            error = "no answer to the WebSocket handshake";
            return nullptr;
        }
        const int n = int(recv(s, buffer, int(sizeof buffer), 0));
        if (n <= 0) {
            error = "the connection closed during the handshake";
            return nullptr;
        }
        head.append(buffer, std::size_t(n));
        if (head.size() > 16384) {
            error = "the handshake's answer is too long";
            return nullptr;
        }
    }
    const std::size_t head_end = head.find("\r\n\r\n") + 4;
    ws->pending_ = head.substr(head_end);
    head.resize(head_end);
    if (head.size() > 12)
        status = std::atoi(head.c_str() + 9);
    if (status != 101) {
        error = "the server answered " + std::to_string(status) + " to the WebSocket handshake";
        return nullptr;
    }
    std::string lower = head;
    for (char& c : lower)
        c = char(std::tolower(static_cast<unsigned char>(c)));
    const std::string accept = base64(sha1(key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"));
    const std::size_t at = lower.find("sec-websocket-accept:");
    std::string got = at == std::string::npos ? std::string() : head.substr(at + 21, head.find("\r\n", at) - at - 21);
    while (!got.empty() && got.front() == ' ')
        got.erase(got.begin());
    if (got != accept) {
        error = "the server's WebSocket accept key is wrong";
        return nullptr;
    }
    return ws;
}

bool WebSocket::send_frame(int opcode, const std::string& payload) {
    if (socket_ == -1)
        return false;
    std::string f;
    f += char(0x80 | opcode); // FIN
    const std::size_t n = payload.size();
    if (n < 126) {
        f += char(0x80 | n);
    } else if (n < 65536) {
        f += char(0x80 | 126);
        f += char((n >> 8) & 0xFF), f += char(n & 0xFF);
    } else {
        f += char(0x80 | 127);
        for (int i = 7; i >= 0; --i)
            f += char((std::uint64_t(n) >> (8 * i)) & 0xFF);
    }
    // A client masks every frame (RFC 6455, 5.3).
    std::random_device rd;
    std::array<char, 4> mask{};
    for (char& c : mask)
        c = char(rd() & 0xFF);
    f.append(mask.data(), 4);
    for (std::size_t i = 0; i < n; ++i)
        f += char(payload[i] ^ mask[i % 4]);
    std::size_t sent = 0;
    while (sent < f.size()) {
        const int k = int(send(Socket(socket_), f.data() + sent, int(f.size() - sent), MSG_NOSIGNAL));
        if (k <= 0)
            return false;
        sent += std::size_t(k);
    }
    return true;
}

bool WebSocket::send_text(const std::string& text) { return send_frame(0x1, text); }

bool WebSocket::read_bytes(std::size_t n, std::string& out, int timeout_ms) {
    const auto end = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
    char buffer[4096];
    while (pending_.size() < n) {
        const int left = int(std::chrono::duration_cast<std::chrono::milliseconds>(end - std::chrono::steady_clock::now()).count());
        if (left <= 0 || !readable(Socket(socket_), left))
            return false;
        const int k = int(recv(Socket(socket_), buffer, int(sizeof buffer), 0));
        if (k <= 0) {
            closed_ = true;
            return false;
        }
        pending_.append(buffer, std::size_t(k));
    }
    out = pending_.substr(0, n);
    pending_.erase(0, n);
    return true;
}

std::optional<std::string> WebSocket::receive(int timeout_ms) {
    if (socket_ == -1 || closed_)
        return std::nullopt;
    std::string message;
    for (;;) {
        std::string head;
        if (!read_bytes(2, head, timeout_ms))
            return std::nullopt;
        const bool fin = (std::uint8_t(head[0]) & 0x80) != 0;
        const int opcode = std::uint8_t(head[0]) & 0x0F;
        const bool masked = (std::uint8_t(head[1]) & 0x80) != 0;
        std::uint64_t n = std::uint8_t(head[1]) & 0x7F;
        std::string ext;
        if (n == 126) {
            if (!read_bytes(2, ext, timeout_ms))
                return std::nullopt;
            n = (std::uint64_t(std::uint8_t(ext[0])) << 8) | std::uint8_t(ext[1]);
        } else if (n == 127) {
            if (!read_bytes(8, ext, timeout_ms))
                return std::nullopt;
            n = 0;
            for (const char c : ext)
                n = (n << 8) | std::uint8_t(c);
        }
        if (n > (64u << 20)) { // 64 MB: no printer message is near that
            closed_ = true;
            return std::nullopt;
        }
        std::string mask, payload;
        if (masked && !read_bytes(4, mask, timeout_ms))
            return std::nullopt;
        if (!read_bytes(std::size_t(n), payload, timeout_ms))
            return std::nullopt;
        if (masked)
            for (std::size_t i = 0; i < payload.size(); ++i)
                payload[i] = char(payload[i] ^ mask[i % 4]);
        if (opcode == 0x8) { // close
            send_frame(0x8, {});
            closed_ = true;
            return std::nullopt;
        }
        if (opcode == 0x9) { // ping: pong with its data
            send_frame(0xA, payload);
            continue;
        }
        if (opcode == 0xA)
            continue;
        message += payload;
        if (fin)
            return message;
    }
}

} // namespace tisma::net
