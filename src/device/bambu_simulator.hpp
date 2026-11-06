#pragma once

// A simulated Bambu Lab printer (the tests and the interface's «Simulated» choice, as the USB page has): the MQTT broker side of
// device/<serial>/request and report, an FTPS server that keeps the files
// sent to it, and the A1/P1 camera, each on its own TLS port of 127.0.0.1,
// as printer_connections.md (rules 2 to 6) and OpenBambuAPI describe them.
// Own design; see PROVENANCE.md.

#include "device/mqtt.hpp"
#include "net/tls.hpp"

#include <nlohmann/json.hpp>

#include <atomic>
#include <chrono>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace tisma::device {

class BambuSimulator {
public:
    BambuSimulator(std::string serial, std::string access_code)
        : serial_(std::move(serial)), code_(std::move(access_code)), mqtt_(serial_), ftps_(serial_), camera_(serial_) {
        state_ = {{"command", "push_status"}, {"gcode_state", "IDLE"}, {"mc_percent", 0}, {"mc_remaining_time", 0},
                  {"nozzle_temper", 24.5}, {"nozzle_target_temper", 0.0}, {"bed_temper", 23.0},
                  {"bed_target_temper", 0.0}, {"chamber_temper", 22.0}, {"layer_num", 0}, {"total_layer_num", 0},
                  {"subtask_name", ""}, {"print_error", 0}, {"nozzle_diameter", "0.4"},
                  {"ams",
                   {{"ams",
                     {{{"id", "0"}, {"humidity", "4"}, {"temp", "22.7"},
                       {"tray",
                        {{{"id", "0"}, {"tray_type", "PLA"}, {"tray_color", "F95959FF"}, {"remain", 80}},
                         {{"id", "1"}},
                         {{"id", "2"}, {"tray_type", "PETG"}, {"tray_color", "00AE42FF"}, {"remain", 35}},
                         {{"id", "3"}}}}}}},
                    {"tray_now", "255"}}}};
        threads_.emplace_back([this] { serve_mqtt(); });
        threads_.emplace_back([this] { serve_ftps(); });
        threads_.emplace_back([this] { serve_camera(); });
    }
    ~BambuSimulator() {
        stop_ = true;
        for (std::thread& t : threads_)
            t.join();
    }

    int mqtt_port() const { return mqtt_.port(); }
    int ftps_port() const { return ftps_.port(); }
    int camera_port() const { return camera_.port(); }
    std::string sha256() const { return mqtt_.cert_sha256(); }

    // Like a P1, the first answer to pushall is a partial report.
    bool partial_first = false;

    nlohmann::json state() {
        std::lock_guard lock(mutex_);
        return state_;
    }
    std::vector<nlohmann::json> commands() {
        std::lock_guard lock(mutex_);
        return commands_;
    }
    std::map<std::string, std::string> files() {
        std::lock_guard lock(mutex_);
        return files_;
    }

public:
    // The camera's picture: a 32 x 24 JPEG made for these tests.
    static std::string camera_jpeg() {
        static const char* hex =
            "ffd8ffe000104a46494600010100000100010000ffdb0043000d090a0b0a080d0b0a0b0e0e0d0f13201513121213271c1e17"
            "202e2931302e292d2c333a4a3e333646372c2d405741464c4e525352323e5a615a50604a51524fffdb0043010e0e0e131113"
            "261515264f352d354f4f4f4f4f4f4f4f4f4f4f4f4f4f4f4f4f4f4f4f4f4f4f4f4f4f4f4f4f4f4f4f4f4f4f4f4f4f4f4f4f4f"
            "4f4f4f4f4f4f4f4fffc00011080018002003012200021101031101ffc4001f00000105010101010101000000000000000001"
            "02030405060708090a0bffc400b5100002010303020403050504040000017d01020300041105122131410613516107227114"
            "328191a1082342b1c11552d1f02433627282090a161718191a25262728292a3435363738393a434445464748494a53545556"
            "5758595a636465666768696a737475767778797a838485868788898a92939495969798999aa2a3a4a5a6a7a8a9aab2b3b4b5"
            "b6b7b8b9bac2c3c4c5c6c7c8c9cad2d3d4d5d6d7d8d9dae1e2e3e4e5e6e7e8e9eaf1f2f3f4f5f6f7f8f9faffc4001f010003"
            "0101010101010101010000000000000102030405060708090a0bffc400b51100020102040403040705040400010277000102"
            "031104052131061241510761711322328108144291a1b1c109233352f0156272d10a162434e125f11718191a262728292a35"
            "363738393a434445464748494a535455565758595a636465666768696a737475767778797a82838485868788898a92939495"
            "969798999aa2a3a4a5a6a7a8a9aab2b3b4b5b6b7b8b9bac2c3c4c5c6c7c8c9cad2d3d4d5d6d7d8d9dae2e3e4e5e6e7e8e9ea"
            "f2f3f4f5f6f7f8f9faffda000c03010002110311003f00e2a2b4f6ab915a7b568c569ed5722b4f6aeea98931c2e2cce8ad3d"
            "aae4569ed5a315a7b55c8ad3dab8ea624fa4c2e2c822b4f6ab915a7b5145714e723f3ac2d491722b4f6ab915a7b514571549"
            "c8fa5c2d491fffd9";
        std::string out;
        for (const char* p = hex; p[0] && p[1]; p += 2)
            out += char(std::stoi(std::string(p, 2), nullptr, 16));
        return out;
    }

private:
    void serve_mqtt() {
        while (!stop_) {
            std::unique_ptr<net::TlsStream> s;
            try {
                s = mqtt_.accept(100);
            } catch (const net::TlsError&) {
                continue;
            }
            if (!s)
                continue;
            try {
                mqtt_session(*s);
            } catch (const std::exception&) {
            }
        }
    }

    void mqtt_session(net::TlsStream& s) {
        mqtt::Reader reader;
        bool connected = false;
        while (!stop_) {
            std::optional<mqtt::Packet> p = reader.next();
            if (!p) {
                try {
                    reader.feed(s.read(16384, 100));
                } catch (const net::TlsTimeout&) {
                }
                continue;
            }
            using namespace device::mqtt;
            if (p->type == kConnect) {
                const Connect c = parse_connect(*p);
                connected = c.user == "bblp" && c.password == code_;
                s.write(connack_packet(connected ? 0 : 5));
                if (!connected)
                    return;
            } else if (!connected) {
                return;
            } else if (p->type == kSubscribe) {
                std::uint16_t id = 0;
                parse_subscribe(*p, id);
                s.write(suback_packet(id, 0));
            } else if (p->type == kPingreq) {
                s.write(pingresp_packet());
            } else if (p->type == kDisconnect) {
                return;
            } else if (p->type == kPublish) {
                const Publish m = parse_publish(*p);
                if (m.qos == 1)
                    s.write(puback_packet(m.id));
                if (m.topic != "device/" + serial_ + "/request")
                    continue;
                const nlohmann::json j = nlohmann::json::parse(m.payload, nullptr, false);
                if (j.contains("pushing") && j["pushing"].value("command", "") == "pushall") {
                    const std::string topic = "device/" + serial_ + "/report";
                    nlohmann::json now = state();
                    if (partial_first)
                        s.write(publish_packet({topic, nlohmann::json{{"print", {{"command", "push_partial"},
                                                                                 {"nozzle_temper", now["nozzle_temper"]}}}}
                                                           .dump(),
                                                0, 0}));
                    s.write(publish_packet({topic, nlohmann::json{{"print", now}}.dump(), 0, 0}));
                } else if (j.contains("print")) {
                    std::lock_guard lock(mutex_);
                    commands_.push_back(j["print"]);
                    const std::string command = j["print"].value("command", "");
                    if (command == "pause")
                        state_["gcode_state"] = "PAUSE";
                    else if (command == "resume")
                        state_["gcode_state"] = "RUNNING";
                    else if (command == "stop")
                        state_["gcode_state"] = "FAILED";
                    else if (command == "project_file") {
                        state_["gcode_state"] = "PREPARE";
                        state_["subtask_name"] = j["print"].value("subtask_name", "");
                    }
                }
            }
        }
    }

    void serve_ftps() {
        while (!stop_) {
            std::unique_ptr<net::TlsStream> s;
            try {
                s = ftps_.accept(100);
            } catch (const net::TlsError&) {
                continue;
            }
            if (!s)
                continue;
            try {
                ftps_session(*s);
            } catch (const std::exception&) {
            }
        }
    }

    void ftps_session(net::TlsStream& s) {
        std::string buffer;
        const auto line = [&]() -> std::string {
            for (;;) {
                if (const auto end = buffer.find("\r\n"); end != std::string::npos) {
                    std::string l = buffer.substr(0, end);
                    buffer.erase(0, end + 2);
                    return l;
                }
                buffer += s.read(4096, 5000);
            }
        };
        s.write("220-Simulated printer\r\n220 ready\r\n");
        bool user = false, logged = false, prot = false;
        while (!stop_) {
            const std::string l = line();
            const std::string verb = l.substr(0, l.find(' '));
            const std::string arg = l.find(' ') == std::string::npos ? "" : l.substr(l.find(' ') + 1);
            if (verb == "USER") {
                user = arg == "bblp";
                s.write("331 password please\r\n");
            } else if (verb == "PASS") {
                logged = user && arg == code_;
                s.write(logged ? "230 logged in\r\n" : "530 login incorrect\r\n");
            } else if (!logged) {
                s.write("530 not logged in\r\n");
            } else if (verb == "PBSZ" || verb == "TYPE") {
                s.write("200 ok\r\n");
            } else if (verb == "PROT") {
                prot = arg == "P";
                s.write("200 ok\r\n");
            } else if (verb == "PASV") {
                const int p = ftps_.port();
                s.write("227 Entering Passive Mode (127,0,0,1," + std::to_string(p / 256) + "," + std::to_string(p % 256) +
                        ")\r\n");
            } else if (verb == "STOR") {
                if (!prot) {
                    s.write("521 data must be protected\r\n");
                    continue;
                }
                // The data connection comes to the same listener.
                std::unique_ptr<net::TlsStream> data = ftps_.accept(5000);
                if (!data) {
                    s.write("425 no data connection\r\n");
                    continue;
                }
                s.write("150 sending\r\n");
                std::string bytes;
                try {
                    for (;;)
                        bytes += data->read(65536, 5000);
                } catch (const net::TlsError&) {
                }
                {
                    std::lock_guard lock(mutex_);
                    files_[arg] = bytes;
                }
                s.write("226 transfer complete\r\n");
            } else if (verb == "QUIT") {
                s.write("221 bye\r\n");
                return;
            } else {
                s.write("502 not implemented\r\n");
            }
        }
    }

    void serve_camera() {
        while (!stop_) {
            std::unique_ptr<net::TlsStream> s;
            try {
                s = camera_.accept(100);
            } catch (const net::TlsError&) {
                continue;
            }
            if (!s)
                continue;
            try {
                const std::string auth = s->read_exact(80, 5000);
                if (auth.compare(48, code_.size(), code_) != 0 || auth.compare(16, 4, "bblp") != 0) {
                    s->close();
                    continue;
                }
                // A real JPEG (32 x 24, made for these tests), until the
                // client closes or the simulator stops.
                const std::string jpeg = camera_jpeg();
                std::string header(16, '\0');
                for (int b = 0; b < 4; ++b)
                    header[std::size_t(b)] = char((jpeg.size() >> (8 * b)) & 0xFF);
                header[8] = 1;
                while (!stop_) {
                    s->write(header + jpeg);
                    std::this_thread::sleep_for(std::chrono::milliseconds(50));
                }
                s->close();
            } catch (const std::exception&) {
            }
        }
    }

    std::string serial_, code_;
    net::TlsListener mqtt_, ftps_, camera_;
    std::atomic<bool> stop_{false};
    std::mutex mutex_;
    nlohmann::json state_;
    std::vector<nlohmann::json> commands_;
    std::map<std::string, std::string> files_;
    std::vector<std::thread> threads_;
};

} // namespace tisma::device
