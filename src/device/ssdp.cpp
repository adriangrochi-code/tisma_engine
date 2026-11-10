// SSDP announcements of Bambu Lab printers. Own code; see ssdp.hpp and
// PROVENANCE.md.

#include "device/ssdp.hpp"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include <algorithm>
#include <cctype>
#include <chrono>
#include <mutex>
#include <sstream>

namespace tisma::device {
namespace {

std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return char(std::tolower(c)); });
    return s;
}

std::string trim(const std::string& s) {
    const auto a = s.find_first_not_of(" \t\r");
    const auto b = s.find_last_not_of(" \t\r");
    return a == std::string::npos ? std::string() : s.substr(a, b - a + 1);
}

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

} // namespace

std::optional<SsdpPrinter> parse_ssdp(const std::string& datagram, const std::string& sender) {
    std::istringstream in(datagram);
    std::string line;
    if (!std::getline(in, line))
        return std::nullopt;
    const std::string start = lower(trim(line));
    if (!start.starts_with("notify") && !start.starts_with("http/1.1 200") && !start.starts_with("m-search"))
        return std::nullopt;
    SsdpPrinter p;
    while (std::getline(in, line)) {
        const auto colon = line.find(':');
        if (colon == std::string::npos)
            continue;
        p.headers[lower(trim(line.substr(0, colon)))] = trim(line.substr(colon + 1));
    }
    if (const auto it = p.headers.find("usn"); it != p.headers.end())
        p.id = it->second;
    if (p.id.empty())
        return std::nullopt;
    // «uuid:<serial>» or «<serial>::…»: the serial alone.
    if (lower(p.id).starts_with("uuid:"))
        p.id = p.id.substr(5);
    if (const auto colons = p.id.find("::"); colons != std::string::npos)
        p.id = p.id.substr(0, colons);
    p.address = sender;
    if (const auto it = p.headers.find("location"); it != p.headers.end() && p.address.empty()) {
        std::string l = it->second;
        if (const auto scheme = l.find("://"); scheme != std::string::npos)
            l = l.substr(scheme + 3);
        p.address = l.substr(0, l.find_first_of(":/"));
    }
    for (const auto& [name, value] : p.headers) {
        if (name.starts_with("devmodel"))
            p.model = value;
        else if (name.starts_with("devname"))
            p.name = value;
    }
    return p;
}

SsdpListen ssdp_listen(int ms, const std::vector<int>& ports) {
    sockets_once();
    SsdpListen out;
    std::vector<Socket> sockets;
    for (const int port : ports) {
        const Socket s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (s == kNoSocket)
            continue;
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_port = htons(std::uint16_t(port));
        address.sin_addr.s_addr = htonl(INADDR_ANY);
        // Not shared on purpose: a port another program holds is reported
        // (rule 5), not stolen.
        if (bind(s, reinterpret_cast<sockaddr*>(&address), int(sizeof address)) != 0) {
            out.busy_ports.push_back(port);
            close_socket(s);
            continue;
        }
        // The SSDP multicast group too (UPnP 1.1, section 1.1.2).
        ip_mreq group{};
        inet_pton(AF_INET, "239.255.255.250", &group.imr_multiaddr);
        group.imr_interface.s_addr = htonl(INADDR_ANY);
        setsockopt(s, IPPROTO_IP, IP_ADD_MEMBERSHIP, reinterpret_cast<const char*>(&group), int(sizeof group));
        sockets.push_back(s);
    }
    const auto end = std::chrono::steady_clock::now() + std::chrono::milliseconds(ms);
    while (!sockets.empty()) {
        const auto left = std::chrono::duration_cast<std::chrono::milliseconds>(end - std::chrono::steady_clock::now()).count();
        if (left <= 0)
            break;
        fd_set ready;
        FD_ZERO(&ready);
        Socket highest = 0;
        for (const Socket s : sockets) {
            FD_SET(s, &ready);
            highest = std::max(highest, s);
        }
        timeval wait{}; // tv_usec is an int on macOS
        wait.tv_sec = decltype(wait.tv_sec)(left / 1000);
        wait.tv_usec = decltype(wait.tv_usec)((left % 1000) * 1000);
        if (select(int(highest + 1), &ready, nullptr, nullptr, &wait) <= 0)
            continue;
        for (const Socket s : sockets) {
            if (!FD_ISSET(s, &ready))
                continue;
            char buffer[2048];
            sockaddr_in from{};
            socklen_t length = socklen_t(sizeof from);
            const auto n = recvfrom(s, buffer, int(sizeof buffer), 0, reinterpret_cast<sockaddr*>(&from), &length);
            if (n <= 0)
                continue;
            char ip[INET_ADDRSTRLEN] = {};
            inet_ntop(AF_INET, &from.sin_addr, ip, sizeof ip);
            if (std::optional<SsdpPrinter> p = parse_ssdp(std::string(buffer, std::size_t(n)), ip)) {
                const auto same = std::find_if(out.printers.begin(), out.printers.end(),
                                               [&](const SsdpPrinter& q) { return q.id == p->id; });
                if (same == out.printers.end())
                    out.printers.push_back(std::move(*p));
                else
                    *same = std::move(*p); // the newest announcement
            }
        }
    }
    for (const Socket s : sockets)
        close_socket(s);
    return out;
}

} // namespace tisma::device
