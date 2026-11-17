#include "device/flashforge.hpp"

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

#include <algorithm>
#include <chrono>
#include <cstring>
#include <mutex>

namespace tisma::device {

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

int u16(const std::string& d, std::size_t at) {
    return (int(static_cast<unsigned char>(d[at])) << 8) | int(static_cast<unsigned char>(d[at + 1]));
}

std::string text_at(const std::string& d, std::size_t at, std::size_t size) {
    std::string s = d.substr(at, size);
    s.resize(std::strlen(s.c_str()));
    return s;
}

// Waits up to `ms` for the socket to be readable.
bool readable(Socket s, int ms) {
    fd_set set;
    FD_ZERO(&set);
    FD_SET(s, &set);
    timeval tv{ms / 1000, (ms % 1000) * 1000};
    return select(int(s) + 1, &set, nullptr, nullptr, &tv) > 0;
}

} // namespace

std::optional<FlashforgePrinter> parse_flashforge_discovery(const std::string& d, const std::string& sender) {
    FlashforgePrinter p;
    p.address = sender;
    if (d.size() >= 196 && d.size() >= 0x92 + 128) {
        p.modern = true;
        p.name = text_at(d, 0, 128);
        p.tcp_port = u16(d, 0x84);
        p.status = u16(d, 0x8A);
        p.http_port = u16(d, 0x8E);
        p.serial = text_at(d, 0x92, 128);
        return p;
    }
    if (d.size() == 140) {
        p.name = text_at(d, 0, 128);
        p.tcp_port = u16(d, 0x84);
        p.status = u16(d, 0x8A);
        p.http_port = 0;
        return p;
    }
    return std::nullopt;
}

std::vector<FlashforgePrinter> flashforge_discover(int ms, const std::vector<std::pair<std::string, int>>& extra) {
    sockets_once();
    std::vector<FlashforgePrinter> found;
    const Socket s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s == kNoSocket)
        return found;
    int yes = 1;
    setsockopt(s, SOL_SOCKET, SO_BROADCAST, reinterpret_cast<const char*>(&yes), sizeof yes);
    const auto send_to = [&](const char* ip, int port) {
        sockaddr_in to{};
        to.sin_family = AF_INET;
        to.sin_port = htons(std::uint16_t(port));
        inet_pton(AF_INET, ip, &to.sin_addr);
        const char payload[] = "tisma"; // the printers ignore what it says
        sendto(s, payload, int(sizeof payload - 1), 0, reinterpret_cast<const sockaddr*>(&to), int(sizeof to));
    };
    send_to("225.0.0.9", 19000);
    send_to("255.255.255.255", 48899);
    send_to("225.0.0.9", 8899);
    for (const auto& [ip, port] : extra)
        send_to(ip.c_str(), port);
    const auto end = std::chrono::steady_clock::now() + std::chrono::milliseconds(ms);
    char buffer[1024];
    for (;;) {
        const int left = int(std::chrono::duration_cast<std::chrono::milliseconds>(end - std::chrono::steady_clock::now()).count());
        if (left <= 0 || !readable(s, left))
            break;
        sockaddr_in from{};
        socklen_t from_size = sizeof from;
        const int n = int(recvfrom(s, buffer, int(sizeof buffer), 0, reinterpret_cast<sockaddr*>(&from), &from_size));
        if (n <= 0)
            continue;
        char ip[INET_ADDRSTRLEN] = {};
        inet_ntop(AF_INET, &from.sin_addr, ip, sizeof ip);
        if (auto p = parse_flashforge_discovery(std::string(buffer, std::size_t(n)), ip))
            if (std::none_of(found.begin(), found.end(), [&](const FlashforgePrinter& q) { return q.address == p->address && q.modern == p->modern; }))
                found.push_back(std::move(*p));
    }
    close_socket(s);
    return found;
}

FlashforgeSession flashforge_tcp(const std::string& host, int port, const std::vector<std::string>& commands, int timeout_ms) {
    sockets_once();
    FlashforgeSession out;
    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    addrinfo* list = nullptr;
    if (getaddrinfo(host.c_str(), std::to_string(port).c_str(), &hints, &list) != 0 || !list) {
        out.code = "unreachable";
        out.error = "cannot reach the printer at " + host + ":" + std::to_string(port) +
                    "; check that it is on and on the same network";
        return out;
    }
    const Socket s = socket(list->ai_family, list->ai_socktype, list->ai_protocol);
    // A connection that does not come within the time is cut (FF1.6).
#ifdef _WIN32
    u_long nonblocking = 1;
    ioctlsocket(s, FIONBIO, &nonblocking);
#else
    const int flags = fcntl(s, F_GETFL, 0);
    fcntl(s, F_SETFL, flags | O_NONBLOCK);
#endif
    connect(s, list->ai_addr, int(list->ai_addrlen));
    freeaddrinfo(list);
    fd_set w;
    FD_ZERO(&w);
    FD_SET(s, &w);
    timeval tv{timeout_ms / 1000, (timeout_ms % 1000) * 1000};
    int err = 0;
    socklen_t err_size = sizeof err;
    if (s == kNoSocket || select(int(s) + 1, nullptr, &w, nullptr, &tv) <= 0 ||
        getsockopt(s, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&err), &err_size) != 0 || err != 0) {
        if (s != kNoSocket)
            close_socket(s);
        out.code = "unreachable";
        out.error = "cannot reach the printer at " + host + ":" + std::to_string(port) +
                    "; check that it is on and on the same network";
        return out;
    }
#ifdef _WIN32
    nonblocking = 0;
    ioctlsocket(s, FIONBIO, &nonblocking);
#else
    fcntl(s, F_SETFL, flags);
#endif
    // One command and its answer, up to the line «ok».
    std::string pending;
    const auto ask = [&](const std::string& command, std::string& answer) {
        const std::string line = "~" + command + "\r\n";
        if (send(s, line.data(), int(line.size()), 0) != int(line.size()))
            return false;
        const auto end = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
        char buffer[1024];
        for (;;) {
            for (std::size_t at = pending.find('\n'); at != std::string::npos; at = pending.find('\n')) {
                std::string l = pending.substr(0, at);
                pending.erase(0, at + 1);
                while (!l.empty() && (l.back() == '\r' || l.back() == ' '))
                    l.pop_back();
                answer += l + "\n";
                // The answer ends with «ok» (or «ok.»); a refused session may
                // end at «Control Failed.» with no «ok».
                if (l == "ok" || l == "ok." || l.find("Control Failed") != std::string::npos)
                    return true;
            }
            const int left =
                int(std::chrono::duration_cast<std::chrono::milliseconds>(end - std::chrono::steady_clock::now()).count());
            if (left <= 0 || !readable(s, left))
                return false;
            const int n = int(recv(s, buffer, int(sizeof buffer), 0));
            if (n <= 0)
                return false;
            pending.append(buffer, std::size_t(n));
        }
    };
    std::string hello;
    if (!ask("M601 S1", hello)) {
        close_socket(s);
        out.code = "bad_answer";
        out.error = "the printer at " + host + " does not answer as a Flashforge printer";
        return out;
    }
    if (hello.find("Control Failed") != std::string::npos) {
        // Nothing more is sent: another program has the printer (FF1.5).
        close_socket(s);
        out.code = "conflict";
        out.error = "another program is controlling the printer";
        return out;
    }
    out.ok = true;
    for (const std::string& c : commands) {
        std::string answer;
        if (!ask(c, answer)) {
            out.ok = false;
            out.code = "bad_answer";
            out.error = "the printer did not answer " + c;
            break;
        }
        out.answers.push_back(answer);
    }
    std::string bye;
    ask("M602", bye);
    close_socket(s);
    return out;
}

} // namespace tisma::device
