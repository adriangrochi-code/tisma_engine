#include "device/sdcp.hpp"

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
#include <chrono>
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

std::string text(const nlohmann::json& j, const char* key) {
    return j.is_object() && j.contains(key) && j.at(key).is_string() ? j.at(key).get<std::string>() : std::string();
}

} // namespace

std::optional<SdcpPrinter> parse_sdcp_discovery(const std::string& datagram, const std::string& sender) {
    const nlohmann::json j = nlohmann::json::parse(datagram, nullptr, false);
    if (!j.is_object() || !j.contains("Data") || !j["Data"].is_object())
        return std::nullopt;
    const nlohmann::json& d = j["Data"];
    SdcpPrinter p;
    p.name = text(d, "Name");
    p.machine = text(d, "MachineName");
    p.brand = text(d, "BrandName");
    p.address = text(d, "MainboardIP");
    if (p.address.empty())
        p.address = sender;
    p.mainboard_id = text(d, "MainboardID");
    p.firmware = text(d, "FirmwareVersion");
    if (p.mainboard_id.empty())
        return std::nullopt;
    return p;
}

std::vector<SdcpPrinter> sdcp_discover(int ms, const std::vector<std::pair<std::string, int>>& extra) {
    sockets_once();
    std::vector<SdcpPrinter> found;
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
        const char payload[] = "M99999";
        sendto(s, payload, int(sizeof payload - 1), 0, reinterpret_cast<const sockaddr*>(&to), int(sizeof to));
    };
    send_to("255.255.255.255", 3000);
    for (const auto& [ip, port] : extra)
        send_to(ip.c_str(), port);
    const auto end = std::chrono::steady_clock::now() + std::chrono::milliseconds(ms);
    char buffer[4096];
    for (;;) {
        const int left = int(std::chrono::duration_cast<std::chrono::milliseconds>(end - std::chrono::steady_clock::now()).count());
        if (left <= 0)
            break;
        fd_set set;
        FD_ZERO(&set);
        FD_SET(s, &set);
        timeval tv{left / 1000, (left % 1000) * 1000};
        if (select(int(s) + 1, &set, nullptr, nullptr, &tv) <= 0)
            break;
        sockaddr_in from{};
        socklen_t from_size = sizeof from;
        const int n = int(recvfrom(s, buffer, int(sizeof buffer), 0, reinterpret_cast<sockaddr*>(&from), &from_size));
        if (n <= 0)
            continue;
        char ip[INET_ADDRSTRLEN] = {};
        inet_ntop(AF_INET, &from.sin_addr, ip, sizeof ip);
        if (auto p = parse_sdcp_discovery(std::string(buffer, std::size_t(n)), ip))
            if (std::none_of(found.begin(), found.end(), [&](const SdcpPrinter& q) { return q.mainboard_id == p->mainboard_id; }))
                found.push_back(std::move(*p));
    }
    close_socket(s);
    return found;
}

std::string sdcp_ack_text(int command, int ack) {
    if (ack == 0)
        return "done";
    if (command == 128) { // start printing
        switch (ack) {
        case 1: return "the printer is busy";
        case 2: return "the file is not on the printer";
        case 3: return "the file's MD5 check failed";
        case 4: return "the printer could not read the file";
        case 5: return "the file's resolution does not match the printer";
        case 6: return "the printer does not know the file's format";
        case 7: return "the file is for another printer model";
        default: break;
        }
    }
    if (command == 255) { // cancel a transfer
        switch (ack) {
        case 1: return "no transfer is under way";
        case 2: return "the printer is already checking the file";
        case 3: return "the file was not found";
        default: break;
        }
    }
    return "the printer refused it (code " + std::to_string(ack) + ")";
}

const char* sdcp_print_state(int status) {
    switch (status) {
    case 0: return "idle";
    case 1: return "homing";
    case 2: return "dropping";
    case 3: return "exposing";
    case 4: return "lifting";
    case 5: return "pausing";
    case 6: return "paused";
    case 7: return "stopping";
    case 8: return "stopped";
    case 9: return "complete";
    case 10: return "checking the file";
    default: return "busy";
    }
}

} // namespace tisma::device
