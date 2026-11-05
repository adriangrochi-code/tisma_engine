// Multicast DNS service discovery. Own code; see mdns.hpp and PROVENANCE.md.

#include "device/mdns.hpp"

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
#include <cstdio>
#include <mutex>

namespace tisma::device {
namespace {

constexpr std::uint16_t kPtr = 12, kSrv = 33, kTxt = 16, kA = 1;

std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return char(std::tolower(c)); });
    return s;
}

// Reads a name at `at` (RFC 1035 4.1.4: labels, or a pointer to an earlier
// one); `at` moves past it. False on a damaged name or a pointer loop.
bool read_name(const std::vector<std::uint8_t>& p, std::size_t& at, std::string& out) {
    out.clear();
    std::size_t pos = at;
    bool jumped = false;
    for (int hops = 0; hops < 64; ++hops) {
        if (pos >= p.size())
            return false;
        const std::uint8_t len = p[pos];
        if (len == 0) {
            if (!jumped)
                at = pos + 1;
            if (out.empty())
                out = ".";
            else
                out.pop_back(); // the last dot
            return true;
        }
        if ((len & 0xC0) == 0xC0) {
            if (pos + 1 >= p.size())
                return false;
            const std::size_t target = std::size_t(len & 0x3F) << 8 | p[pos + 1];
            if (!jumped)
                at = pos + 2;
            jumped = true;
            if (target >= pos)
                return false; // a pointer must go back
            pos = target;
            continue;
        }
        if ((len & 0xC0) != 0 || pos + 1 + len > p.size())
            return false;
        out.append(reinterpret_cast<const char*>(&p[pos + 1]), len);
        out += '.';
        pos += 1 + std::size_t(len);
    }
    return false;
}

std::uint16_t get16(const std::vector<std::uint8_t>& p, std::size_t at) { return std::uint16_t(p[at] << 8 | p[at + 1]); }

void put_name(std::vector<std::uint8_t>& out, const std::string& name) {
    std::size_t start = 0;
    while (start < name.size()) {
        std::size_t dot = name.find('.', start);
        if (dot == std::string::npos)
            dot = name.size();
        const std::size_t len = std::min<std::size_t>(dot - start, 63);
        out.push_back(std::uint8_t(len));
        out.insert(out.end(), name.begin() + std::ptrdiff_t(start), name.begin() + std::ptrdiff_t(start + len));
        start = dot + 1;
    }
    out.push_back(0);
}

} // namespace

std::string MdnsService::url() const {
    std::string where = address.empty() ? host : address;
    std::string path = "/";
    if (const auto it = txt.find("path"); it != txt.end() && !it->second.empty())
        path = it->second.front() == '/' ? it->second : "/" + it->second;
    return "http://" + where + (port > 0 && port != 80 ? ":" + std::to_string(port) : std::string()) + path;
}

std::vector<std::uint8_t> mdns_query(const std::string& service) {
    // Header: id 0, flags 0 (a standard query), one question.
    std::vector<std::uint8_t> out = {0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0};
    put_name(out, service);
    out.insert(out.end(), {0, std::uint8_t(kPtr), 0x80, 0x01}); // PTR, class IN with the QU bit
    return out;
}

std::vector<MdnsService> parse_mdns(const std::vector<std::uint8_t>& p, const std::string& service) {
    struct Rr {
        std::string name;
        std::uint16_t type = 0;
        std::size_t data = 0, length = 0;
    };
    std::vector<Rr> records;
    if (p.size() < 12)
        return {};
    const int questions = get16(p, 4);
    const int total = get16(p, 6) + get16(p, 8) + get16(p, 10);
    std::size_t at = 12;
    std::string name;
    for (int q = 0; q < questions; ++q) {
        if (!read_name(p, at, name) || at + 4 > p.size())
            return {};
        at += 4;
    }
    for (int r = 0; r < total; ++r) {
        Rr rr;
        if (!read_name(p, at, rr.name) || at + 10 > p.size())
            break;
        rr.type = get16(p, at);
        rr.length = get16(p, at + 8);
        rr.data = at + 10;
        if (rr.data + rr.length > p.size())
            break;
        at = rr.data + rr.length;
        records.push_back(rr);
    }
    const std::string type = lower(service);
    std::vector<MdnsService> out;
    for (const Rr& r : records) {
        if (r.type != kPtr || lower(r.name) != type)
            continue;
        MdnsService s;
        std::size_t pos = r.data;
        if (!read_name(p, pos, s.instance))
            continue;
        for (const Rr& o : records) {
            if (lower(o.name) != lower(s.instance))
                continue;
            if (o.type == kSrv && o.length >= 7) {
                s.port = get16(p, o.data + 4);
                std::size_t hp = o.data + 6;
                read_name(p, hp, s.host);
            } else if (o.type == kTxt) {
                std::size_t q = o.data;
                while (q < o.data + o.length) {
                    const std::size_t len = p[q];
                    if (q + 1 + len > o.data + o.length)
                        break;
                    const std::string entry(reinterpret_cast<const char*>(&p[q + 1]), len);
                    const std::size_t eq = entry.find('=');
                    if (!entry.empty())
                        s.txt[lower(entry.substr(0, eq))] = eq == std::string::npos ? std::string() : entry.substr(eq + 1);
                    q += 1 + len;
                }
            }
        }
        for (const Rr& o : records)
            if (o.type == kA && o.length == 4 && !s.host.empty() && lower(o.name) == lower(s.host)) {
                char ip[16];
                std::snprintf(ip, sizeof ip, "%u.%u.%u.%u", p[o.data], p[o.data + 1], p[o.data + 2], p[o.data + 3]);
                s.address = ip;
            }
        out.push_back(std::move(s));
    }
    return out;
}

#ifdef _WIN32
using Socket = SOCKET;
constexpr Socket kNoSocket = INVALID_SOCKET;
static void close_socket(Socket s) { closesocket(s); }
static void sockets_once() {
    static std::once_flag once;
    std::call_once(once, [] {
        WSADATA data;
        WSAStartup(MAKEWORD(2, 2), &data);
    });
}
#else
using Socket = int;
constexpr Socket kNoSocket = -1;
static void close_socket(Socket s) { ::close(s); }
static void sockets_once() {}
#endif

std::vector<MdnsService> mdns_browse(const std::string& service, int ms) {
    sockets_once();
    std::vector<MdnsService> found;
    // Any port: with the QU bit the answers come back to it (RFC 6762, 5.4
    // and 6.7), so no other program's port 5353 is needed.
    const Socket s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s == kNoSocket)
        return found;
    sockaddr_in to{};
    to.sin_family = AF_INET;
    to.sin_port = htons(5353);
    inet_pton(AF_INET, "224.0.0.251", &to.sin_addr);
    const std::vector<std::uint8_t> q = mdns_query(service);
    sendto(s, reinterpret_cast<const char*>(q.data()), int(q.size()), 0, reinterpret_cast<const sockaddr*>(&to),
           int(sizeof to));
    const auto end = std::chrono::steady_clock::now() + std::chrono::milliseconds(ms);
    for (;;) {
        const auto left =
            std::chrono::duration_cast<std::chrono::milliseconds>(end - std::chrono::steady_clock::now()).count();
        if (left <= 0)
            break;
        fd_set ready;
        FD_ZERO(&ready);
        FD_SET(s, &ready);
        timeval wait{long(left / 1000), long((left % 1000) * 1000)};
        if (select(int(s + 1), &ready, nullptr, nullptr, &wait) <= 0)
            continue;
        std::vector<std::uint8_t> buffer(9000);
        const auto n = recvfrom(s, reinterpret_cast<char*>(buffer.data()), int(buffer.size()), 0, nullptr, nullptr);
        if (n <= 0)
            continue;
        buffer.resize(std::size_t(n));
        for (MdnsService& m : parse_mdns(buffer, service)) {
            const auto same = std::find_if(found.begin(), found.end(),
                                           [&](const MdnsService& o) { return lower(o.instance) == lower(m.instance); });
            if (same == found.end())
                found.push_back(std::move(m));
            else
                *same = std::move(m);
        }
    }
    close_socket(s);
    return found;
}

} // namespace tisma::device
