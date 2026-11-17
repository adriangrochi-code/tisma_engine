#pragma once
// The older Flashforge printers (Adventurer 3 and 4) faked on this machine
// for the tests (printer_connections.md, FF1.1 and FF1.5): a TCP server of
// «~» commands answering as the community's flashforge-api-docs wiki shows,
// and a UDP port that answers the discovery datagram with a modern (280
// bytes) and an older (140 bytes) printer. Own design.

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

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace fake_flashforge {

#ifdef _WIN32
using Socket = SOCKET;
inline void close_socket(Socket s) { closesocket(s); }
inline void start() {
    WSADATA data;
    WSAStartup(MAKEWORD(2, 2), &data);
}
#else
using Socket = int;
inline void close_socket(Socket s) { ::close(s); }
inline void start() {}
#endif

inline bool readable(Socket s, int ms) {
    fd_set set;
    FD_ZERO(&set);
    FD_SET(s, &set);
    timeval tv{ms / 1000, (ms % 1000) * 1000};
    return select(int(s) + 1, &set, nullptr, nullptr, &tv) > 0;
}

inline Socket bound(int type, int& port) {
    start();
    const Socket s = socket(AF_INET, type, type == SOCK_STREAM ? IPPROTO_TCP : IPPROTO_UDP);
    sockaddr_in a{};
    a.sin_family = AF_INET;
    a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    bind(s, reinterpret_cast<sockaddr*>(&a), sizeof a);
    socklen_t size = sizeof a;
    getsockname(s, reinterpret_cast<sockaddr*>(&a), &size);
    port = ntohs(a.sin_port);
    return s;
}

// An Adventurer 4 on TCP: M601 S1 opens (or «Control Failed»), M115, M119,
// M105, M24, M25, M26, M602; every command kept.
class Legacy {
public:
    explicit Legacy(bool control_failed = false) : failed_(control_failed) {
        server_ = bound(SOCK_STREAM, port_);
        listen(server_, 4);
        thread_ = std::thread([this] { serve(); });
    }
    ~Legacy() {
        stop_ = true;
        thread_.join();
        close_socket(server_);
    }
    int port() const { return port_; }
    std::vector<std::string> commands() {
        std::lock_guard lock(mutex_);
        return commands_;
    }

private:
    void serve() {
        while (!stop_) {
            if (!readable(server_, 100))
                continue;
            const Socket c = accept(server_, nullptr, nullptr);
            std::string pending;
            char buffer[512];
            bool open = true;
            while (open && !stop_) {
                if (!readable(c, 100))
                    continue;
                const int n = int(recv(c, buffer, int(sizeof buffer), 0));
                if (n <= 0)
                    break;
                pending.append(buffer, std::size_t(n));
                for (std::size_t at = pending.find('\n'); at != std::string::npos; at = pending.find('\n')) {
                    std::string line = pending.substr(0, at);
                    pending.erase(0, at + 1);
                    while (!line.empty() && line.back() == '\r')
                        line.pop_back();
                    if (line.empty() || line[0] != '~')
                        continue;
                    const std::string cmd = line.substr(1);
                    {
                        std::lock_guard lock(mutex_);
                        commands_.push_back(cmd);
                    }
                    const std::string code = cmd.substr(0, cmd.find(' '));
                    std::string answer = "CMD " + code + " Received.\r\n";
                    if (code == "M601") {
                        if (failed_) {
                            answer += "Control Failed.\r\n";
                            send(c, answer.data(), int(answer.size()), 0);
                            open = false;
                            break;
                        }
                        answer += "Control Success V2.1.\r\n";
                    } else if (code == "M115") {
                        answer += "Machine Type: Flashforge Adventurer 4\r\nMachine Name: Taller\r\nFirmware: v2.2.3\r\n"
                                  "SN: SNOLD456\r\nX: 220 Y: 200 Z: 250\r\nTool Count: 1\r\nMac Address:88:A9:A7:00:00:01 \r\n";
                    } else if (code == "M119") {
                        answer += "Endstop: X-max:1 Y-max:0 Z-max:0\r\nMachineStatus: " + state_ +
                                  "\r\nMoveMode: READY\r\nStatus: S:1 L:0 J:0 F:0\r\nLED: 1\r\nCurrentFile: cubo.gx\r\n";
                    } else if (code == "M105") {
                        answer += "T0:205/210 B:58/60\r\n";
                    } else if (code == "M25") {
                        state_ = "PAUSED";
                    } else if (code == "M24") {
                        state_ = "BUILDING_FROM_SD";
                    } else if (code == "M26") {
                        state_ = "READY";
                    } else if (code == "M602") {
                        answer += "Control Release.\r\n";
                    }
                    answer += "ok\r\n";
                    send(c, answer.data(), int(answer.size()), 0);
                }
            }
            close_socket(c);
        }
    }
    bool failed_;
    std::string state_ = "BUILDING_FROM_SD";
    Socket server_{};
    int port_ = 0;
    std::atomic<bool> stop_{false};
    std::thread thread_;
    std::mutex mutex_;
    std::vector<std::string> commands_;
};

// A UDP port that answers any datagram with a modern printer's 280 bytes and
// an older one's 140 bytes, as two printers on one address would.
class Discovery {
public:
    Discovery() {
        socket_ = bound(SOCK_DGRAM, port_);
        thread_ = std::thread([this] { serve(); });
    }
    ~Discovery() {
        stop_ = true;
        thread_.join();
        close_socket(socket_);
    }
    int port() const { return port_; }

    static std::string modern() {
        std::string d(280, '\0');
        put(d, 0, "Adventurer 5M");
        put16(d, 0x84, 8899);
        put16(d, 0x86, 0x2B71);
        put16(d, 0x88, 0x0024);
        put16(d, 0x8C, 0x5A02);
        put16(d, 0x8E, 8898);
        put(d, 0x92, "SNFAKE123");
        return d;
    }
    static std::string legacy() {
        std::string d(140, '\0');
        put(d, 0, "Adventurer 4");
        put16(d, 0x84, 8899);
        put16(d, 0x86, 0x2B71);
        put16(d, 0x88, 0x001D);
        return d;
    }

private:
    static void put(std::string& d, std::size_t at, const std::string& text) { d.replace(at, text.size(), text); }
    static void put16(std::string& d, std::size_t at, int v) {
        d[at] = char((v >> 8) & 0xFF);
        d[at + 1] = char(v & 0xFF);
    }
    void serve() {
        char buffer[256];
        while (!stop_) {
            if (!readable(socket_, 100))
                continue;
            sockaddr_in from{};
            socklen_t size = sizeof from;
            if (recvfrom(socket_, buffer, int(sizeof buffer), 0, reinterpret_cast<sockaddr*>(&from), &size) < 0)
                continue;
            for (const std::string& d : {modern(), legacy()})
                sendto(socket_, d.data(), int(d.size()), 0, reinterpret_cast<sockaddr*>(&from), size);
        }
    }
    Socket socket_{};
    int port_ = 0;
    std::atomic<bool> stop_{false};
    std::thread thread_;
};

} // namespace fake_flashforge
