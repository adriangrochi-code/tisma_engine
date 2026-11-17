#pragma once
// An Elegoo printer by SDCP v3 faked on this machine for the tests
// (printer_connections.md, EL1): one TCP port that is the WebSocket
// (/websocket) and the upload (/uploadFile/upload), as the printer's 3030,
// and a UDP port that answers «M99999» with its JSON. Own design.

#include "fake_flashforge_net.hpp" // the socket basics
#include "net/websocket.hpp"

#include <nlohmann/json.hpp>

#include <atomic>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace fake_sdcp {

using fake_flashforge::Socket;
using fake_flashforge::bound;
using fake_flashforge::close_socket;
using fake_flashforge::readable;

struct Piece {
    std::string md5, offset, uuid, total;
    std::size_t bytes = 0;
};

class Printer {
public:
    static constexpr const char* kId = "000000000001d354";

    Printer() {
        tcp_ = bound(SOCK_STREAM, port_);
        listen(tcp_, 8);
        udp_ = bound(SOCK_DGRAM, udp_port_);
        threads_.emplace_back([this] { serve_tcp(); });
        threads_.emplace_back([this] { serve_udp(); });
    }
    ~Printer() {
        stop_ = true;
        for (std::thread& t : threads_)
            t.join();
        while (active_ > 0) // the connections' threads end with stop_
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        close_socket(tcp_);
        close_socket(udp_);
    }
    int port() const { return port_; }
    int udp_port() const { return udp_port_; }
    std::string url() const { return "127.0.0.1:" + std::to_string(port_); }

    // What the tests set and read.
    std::mutex mutex;
    int status = 0;        // PrintInfo.Status
    int start_ack = 0;     // the Ack to Cmd 128
    int fail_piece = -1;   // a piece answered with -2 (its index)
    bool too_many = false; // the WebSocket refused with 500
    int answers_to_m99999 = 2;
    std::vector<Piece> pieces;
    std::string file;      // the file put together
    std::string started_file; // Cmd 128's Filename
    std::vector<std::pair<int, std::string>> commands; // Cmd and RequestID
    std::vector<std::string> answered;                 // the RequestIDs answered

private:
    nlohmann::json status_message() {
        return {{"Status",
                 {{"CurrentStatus", {status == 0 ? 0 : 1}},
                  {"PrintInfo", {{"Status", status}, {"CurrentLayer", 25}, {"TotalLayer", 100}, {"Filename", "pieza.gcode"},
                                 {"CurrentTicks", 60}, {"TotalTicks", 240}, {"ErrorNumber", 0}, {"TaskId", ""}}},
                  {"TempOfNozzle", 215.0}, {"TempTargetNozzle", 220}, {"TempOfHotbed", 59.5}, {"TempTargetHotbed", 60}}},
                {"MainboardID", kId},
                {"TimeStamp", 1700000000},
                {"Topic", std::string("sdcp/status/") + kId}};
    }

    static bool send_all(Socket c, const std::string& d) {
        std::size_t at = 0;
        while (at < d.size()) {
            const int k = int(send(c, d.data() + at, int(d.size() - at), 0));
            if (k <= 0)
                return false;
            at += std::size_t(k);
        }
        return true;
    }
    static bool send_text(Socket c, const std::string& t) {
        std::string f;
        f += char(0x81);
        if (t.size() < 126) {
            f += char(t.size());
        } else {
            f += char(126);
            f += char((t.size() >> 8) & 0xFF), f += char(t.size() & 0xFF);
        }
        return send_all(c, f + t);
    }
    // Reads exactly n bytes into out from pending and the socket.
    bool read_n(Socket c, std::string& pending, std::size_t n, std::string& out) {
        char buffer[65536];
        while (pending.size() < n) {
            if (stop_ || !readable(c, 200)) {
                if (stop_)
                    return false;
                continue;
            }
            const int k = int(recv(c, buffer, int(sizeof buffer), 0));
            if (k <= 0)
                return false;
            pending.append(buffer, std::size_t(k));
        }
        out = pending.substr(0, n);
        pending.erase(0, n);
        return true;
    }

    void websocket(Socket c, const std::string& head, std::string pending) {
        const std::string lower = [&] {
            std::string l = head;
            for (char& ch : l)
                ch = char(std::tolower(static_cast<unsigned char>(ch)));
            return l;
        }();
        const std::size_t at = lower.find("sec-websocket-key:");
        std::string key = head.substr(at + 18, head.find("\r\n", at) - at - 18);
        while (!key.empty() && key.front() == ' ')
            key.erase(key.begin());
        const std::string accept = tisma::net::base64(tisma::net::sha1(key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"));
        send_all(c, "HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: " +
                        accept + "\r\n\r\n");
        for (;;) {
            std::string h;
            if (!read_n(c, pending, 2, h))
                return;
            const int opcode = std::uint8_t(h[0]) & 0x0F;
            std::size_t n = std::uint8_t(h[1]) & 0x7F;
            std::string ext, mask, payload;
            if (n == 126) {
                if (!read_n(c, pending, 2, ext))
                    return;
                n = (std::size_t(std::uint8_t(ext[0])) << 8) | std::uint8_t(ext[1]);
            }
            if (!read_n(c, pending, 4, mask) || !read_n(c, pending, n, payload))
                return;
            for (std::size_t i = 0; i < payload.size(); ++i)
                payload[i] = char(payload[i] ^ mask[i % 4]);
            if (opcode == 0x8)
                return;
            if (payload == "ping") {
                send_text(c, "pong");
                continue;
            }
            const nlohmann::json j = nlohmann::json::parse(payload, nullptr, false);
            if (!j.is_object() || !j.contains("Data"))
                continue;
            const nlohmann::json& d = j["Data"];
            const int cmd = d.value("Cmd", -1);
            const std::string rid = d.value("RequestID", "");
            int ack = 0;
            {
                std::lock_guard lock(mutex);
                commands.push_back({cmd, rid});
                if (cmd == 128) {
                    ack = start_ack;
                    started_file = d.value("Data", nlohmann::json::object()).value("Filename", "");
                    if (ack == 0)
                        status = 1;
                } else if (cmd == 129) {
                    status = 6; // through 5
                } else if (cmd == 130) {
                    status = 8; // through 7
                } else if (cmd == 131) {
                    status = 1;
                }
                answered.push_back(rid);
            }
            const nlohmann::json answer = {{"Id", ""},
                                           {"Data", {{"Cmd", cmd}, {"Data", {{"Ack", ack}}}, {"RequestID", rid}, {"MainboardID", kId}}},
                                           {"Topic", std::string("sdcp/response/") + kId}};
            send_text(c, answer.dump());
            std::lock_guard lock(mutex);
            send_text(c, status_message().dump());
        }
    }

    // A multipart field's value (the fields are small, the file is «File»).
    static std::string field(const std::string& body, const std::string& name) {
        const std::string mark = "name=\"" + name + "\"";
        const std::size_t at = body.find(mark);
        if (at == std::string::npos)
            return {};
        const std::size_t from = body.find("\r\n\r\n", at) + 4;
        const std::size_t to = body.find("\r\n--", from);
        return body.substr(from, to - from);
    }

    void upload(Socket c, const std::string& head, std::string pending) {
        const std::size_t at = head.find("Content-Length:") != std::string::npos ? head.find("Content-Length:")
                                                                                 : head.find("content-length:");
        const std::size_t length = at == std::string::npos ? 0 : std::size_t(std::atoll(head.c_str() + at + 15));
        std::string body;
        if (!read_n(c, pending, length, body))
            return;
        Piece p;
        p.md5 = field(body, "S-File-MD5");
        p.offset = field(body, "Offset");
        p.uuid = field(body, "Uuid");
        p.total = field(body, "TotalSize");
        const std::string data = field(body, "File");
        p.bytes = data.size();
        std::string code = "000000";
        {
            std::lock_guard lock(mutex);
            if (int(pieces.size()) == fail_piece)
                code = "-2";
            else
                file += data;
            pieces.push_back(p);
        }
        const std::string answer = nlohmann::json{{"code", code}, {"success", code == "000000"}}.dump();
        send_all(c, "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: " + std::to_string(answer.size()) +
                        "\r\nConnection: close\r\n\r\n" + answer);
    }

    void serve_tcp() {
        while (!stop_) {
            if (!readable(tcp_, 100))
                continue;
            const Socket c = accept(tcp_, nullptr, nullptr);
            ++active_;
            std::thread([this, c] {
                std::string pending;
                char buffer[4096];
                while (pending.find("\r\n\r\n") == std::string::npos && !stop_) {
                    if (!readable(c, 200))
                        continue;
                    const int k = int(recv(c, buffer, int(sizeof buffer), 0));
                    if (k <= 0)
                        break;
                    pending.append(buffer, std::size_t(k));
                }
                const std::size_t end = pending.find("\r\n\r\n");
                if (end != std::string::npos) {
                    const std::string head = pending.substr(0, end + 4);
                    pending.erase(0, end + 4);
                    if (head.rfind("GET /websocket", 0) == 0) {
                        if (too_many)
                            send_all(c, "HTTP/1.1 500 Internal Server Error\r\nContent-Length: 15\r\n\r\ntoo many client");
                        else
                            websocket(c, head, pending);
                    } else if (head.rfind("POST /uploadFile/upload", 0) == 0) {
                        upload(c, head, pending);
                    }
                }
                close_socket(c);
                --active_;
            }).detach();
        }
    }

    void serve_udp() {
        char buffer[256];
        while (!stop_) {
            if (!readable(udp_, 100))
                continue;
            sockaddr_in from{};
            socklen_t size = sizeof from;
            const int n = int(recvfrom(udp_, buffer, int(sizeof buffer), 0, reinterpret_cast<sockaddr*>(&from), &size));
            if (n <= 0 || std::string(buffer, std::size_t(n)) != "M99999")
                continue;
            const std::string answer =
                nlohmann::json{{"Id", "f25273b12b094c5a8b9513a30ca60049"},
                               {"Data",
                                {{"Name", "Centauri Carbon"}, {"MachineName", "Centauri Carbon"}, {"BrandName", "ELEGOO"},
                                 {"MainboardIP", "127.0.0.1"}, {"MainboardID", kId}, {"ProtocolVersion", "V3.0.0"},
                                 {"FirmwareVersion", "V1.1.29"}}}}
                    .dump();
            for (int i = 0; i < answers_to_m99999; ++i) // EL1.1: two equal answers
                sendto(udp_, answer.data(), int(answer.size()), 0, reinterpret_cast<sockaddr*>(&from), size);
        }
    }

    Socket tcp_{}, udp_{};
    int port_ = 0, udp_port_ = 0;
    std::atomic<bool> stop_{false};
    std::atomic<int> active_{0};
    std::vector<std::thread> threads_;
};

} // namespace fake_sdcp
