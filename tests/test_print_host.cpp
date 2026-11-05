// The network printer programs' client against a simulator built from
// OctoPrint's published REST API (printer_connections.md: «se prueban contra
// un simulador propio»).
#include "device/print_host.hpp"

#include <catch2/catch_test_macros.hpp>
#include <httplib.h>
#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

using namespace tisma::device;

namespace {

class FakeOctoPrint {
public:
    FakeOctoPrint() {
        const auto keyed = [this](const httplib::Request& req, httplib::Response& res) {
            if (req.get_header_value("X-Api-Key") != "KEY") {
                res.status = 403;
                return false;
            }
            return true;
        };
        server_.Get("/api/version", [keyed](const httplib::Request& req, httplib::Response& res) {
            if (keyed(req, res))
                res.set_content(R"({"api": "0.1", "server": "1.10.2", "text": "OctoPrint 1.10.2"})", "application/json");
        });
        server_.Post("/api/files/local", [this, keyed](const httplib::Request& req, httplib::Response& res) {
            if (!keyed(req, res))
                return;
            std::lock_guard lock(mutex_);
            if (!req.form.has_file("file")) {
                res.status = 400;
                return;
            }
            const auto file = req.form.get_file("file");
            uploaded_name = file.filename;
            uploaded = file.content;
            started = req.form.get_field("print") == "true";
            if (started)
                state = "Printing";
            res.status = 201;
            res.set_content(R"({"done": true})", "application/json");
        });
        server_.Get("/api/job", [this, keyed](const httplib::Request& req, httplib::Response& res) {
            if (!keyed(req, res))
                return;
            std::lock_guard lock(mutex_);
            nlohmann::json j{{"state", state},
                             {"job", {{"file", {{"name", uploaded_name}}}}},
                             {"progress", {{"completion", started ? nlohmann::json(42.5) : nlohmann::json(nullptr)}, {"printTimeLeft", started ? nlohmann::json(600) : nlohmann::json(nullptr)}}}};
            res.set_content(j.dump(), "application/json");
        });
        server_.Get("/api/printer", [this, keyed](const httplib::Request& req, httplib::Response& res) {
            if (!keyed(req, res))
                return;
            if (!board) {
                res.status = 409;
                return;
            }
            res.set_content(R"({"temperature": {"tool0": {"actual": 214.8, "target": 215.0},
                                                "bed": {"actual": 59.9, "target": 60.0}},
                                "state": {"text": "Printing"}})",
                            "application/json");
        });
        server_.Post("/api/job", [this, keyed](const httplib::Request& req, httplib::Response& res) {
            if (!keyed(req, res))
                return;
            std::lock_guard lock(mutex_);
            const nlohmann::json j = nlohmann::json::parse(req.body);
            commands.push_back(j.value("command", "") + (j.contains("action") ? ":" + j.value("action", "") : ""));
            if (j.value("command", "") == "pause")
                state = j.value("action", "") == "pause" ? "Paused" : "Printing";
            else if (j.value("command", "") == "cancel")
                state = "Operational";
            res.status = 204;
        });
        port_ = server_.bind_to_any_port("127.0.0.1");
        thread_ = std::thread([this] { server_.listen_after_bind(); });
        server_.wait_until_ready();
    }
    ~FakeOctoPrint() {
        server_.stop();
        thread_.join();
    }
    std::string url() const { return "http://127.0.0.1:" + std::to_string(port_); }

    std::string state = "Operational", uploaded_name, uploaded;
    bool started = false, board = true;
    std::vector<std::string> commands;

private:
    httplib::Server server_;
    std::thread thread_;
    std::mutex mutex_;
    int port_ = 0;
};

} // namespace

TEST_CASE("print host: OctoPrint's REST API, test, upload and print, state, pause and cancel (printer_connections.md)",
          "[device][net]") {
    FakeOctoPrint fake;
    HostConfig host;
    host.kind = HostKind::OctoPrint;
    host.url = fake.url() + "/";
    host.api_key = "KEY";
    // The connection and the key.
    const HostVersion v = host_version(host);
    REQUIRE(v.ok);
    CHECK(v.server == "OctoPrint 1.10.2");
    HostConfig wrong = host;
    wrong.api_key = "nope";
    const HostVersion refused = host_version(wrong);
    CHECK_FALSE(refused.ok);
    CHECK(refused.code == "unauthorized");
    HostConfig nowhere = host;
    nowhere.url = "http://127.0.0.1:1";
    nowhere.timeout_s = 2;
    CHECK(host_version(nowhere).code == "unreachable");
    // Upload and print.
    const std::filesystem::path g = std::filesystem::temp_directory_path() / "tisma_print_host.gcode";
    std::ofstream(g, std::ios::binary) << "G28\nG1 X10 Y10 F3000\n";
    const HostResult up = host_upload(host, g.string(), "cube.gcode", true);
    REQUIRE(up.ok);
    CHECK(fake.uploaded_name == "cube.gcode");
    CHECK(fake.uploaded == "G28\nG1 X10 Y10 F3000\n");
    CHECK(fake.started);
    // The state with the temperatures.
    HostStatus s = host_status(host);
    REQUIRE(s.ok);
    CHECK(s.printing);
    CHECK(s.file == "cube.gcode");
    CHECK(s.progress == 42.5);
    CHECK(s.seconds_left == 600.0);
    CHECK(s.nozzle == 214.8);
    CHECK(s.bed_target == 60.0);
    // Pause, resume, cancel.
    REQUIRE(host_pause(host, true).ok);
    CHECK(host_status(host).paused);
    REQUIRE(host_pause(host, false).ok);
    REQUIRE(host_cancel(host).ok);
    CHECK(fake.commands == std::vector<std::string>{"pause:pause", "pause:resume", "cancel"});
    CHECK_FALSE(host_status(host).printing);
    // No printer on the program: the state still reads, without temperatures.
    fake.board = false;
    s = host_status(host);
    CHECK(s.ok);
    CHECK(s.nozzle == -1.0);
    std::filesystem::remove(g);
}

namespace {

class FakeMoonraker {
public:
    FakeMoonraker() {
        server_.Get("/server/info", [](const httplib::Request&, httplib::Response& res) {
            res.set_content(R"({"result": {"klippy_connected": true, "klippy_state": "ready", "moonraker_version": "v0.9.3",
                                           "api_version_string": "1.5.0"}})",
                            "application/json");
        });
        server_.Post("/server/files/upload", [this](const httplib::Request& req, httplib::Response& res) {
            std::lock_guard lock(mutex_);
            const auto file = req.form.get_file("file");
            name = file.filename;
            started = req.form.has_field("print") && req.form.get_field("print") == "true";
            if (started)
                state = "printing";
            res.status = 201;
            res.set_content(R"({"result": {"item": {"path": "x.gcode", "root": "gcodes"}, "print_started": true}})",
                            "application/json");
        });
        server_.Get("/printer/objects/query", [this](const httplib::Request& req, httplib::Response& res) {
            std::lock_guard lock(mutex_);
            asked = req.has_param("print_stats") && req.has_param("virtual_sdcard") && req.has_param("extruder") &&
                    req.has_param("heater_bed");
            nlohmann::json j{{"result",
                              {{"status",
                                {{"print_stats", {{"state", state}, {"filename", name}}},
                                 {"virtual_sdcard", {{"progress", 0.25}}},
                                 {"extruder", {{"temperature", 209.5}, {"target", 210.0}}},
                                 {"heater_bed", {{"temperature", 64.0}, {"target", 65.0}}}}}}}};
            res.set_content(j.dump(), "application/json");
        });
        for (const char* what : {"pause", "resume", "cancel"})
            server_.Post(std::string("/printer/print/") + what, [this, what](const httplib::Request&, httplib::Response& res) {
                std::lock_guard lock(mutex_);
                calls.push_back(what);
                state = std::string(what) == "pause" ? "paused" : std::string(what) == "resume" ? "printing" : "cancelled";
                res.set_content(R"({"result": "ok"})", "application/json");
            });
        port_ = server_.bind_to_any_port("127.0.0.1");
        thread_ = std::thread([this] { server_.listen_after_bind(); });
        server_.wait_until_ready();
    }
    ~FakeMoonraker() {
        server_.stop();
        thread_.join();
    }
    std::string url() const { return "http://127.0.0.1:" + std::to_string(port_); }
    std::string state = "standby", name;
    bool started = false, asked = false;
    std::vector<std::string> calls;

private:
    httplib::Server server_;
    std::thread thread_;
    std::mutex mutex_;
    int port_ = 0;
};

} // namespace

TEST_CASE("print host: Moonraker's API (Klipper), the same calls (printer_connections.md)", "[device][net]") {
    FakeMoonraker fake;
    HostConfig host;
    host.kind = HostKind::Moonraker;
    host.url = fake.url();
    const HostVersion v = host_version(host);
    REQUIRE(v.ok);
    CHECK(v.server == "Moonraker v0.9.3 (Klipper ready)");
    const std::filesystem::path g = std::filesystem::temp_directory_path() / "tisma_print_host_k.gcode";
    std::ofstream(g, std::ios::binary) << "G28\n";
    REQUIRE(host_upload(host, g.string(), "part.gcode", true).ok);
    CHECK(fake.name == "part.gcode");
    CHECK(fake.started);
    const HostStatus s = host_status(host);
    REQUIRE(s.ok);
    CHECK(fake.asked);
    CHECK(s.printing);
    CHECK(s.progress == 25.0);
    CHECK(s.nozzle == 209.5);
    CHECK(s.bed_target == 65.0);
    REQUIRE(host_pause(host, true).ok);
    CHECK(host_status(host).paused);
    REQUIRE(host_pause(host, false).ok);
    REQUIRE(host_cancel(host).ok);
    CHECK(fake.calls == std::vector<std::string>{"pause", "resume", "cancel"});
    CHECK_FALSE(host_status(host).printing);
    std::filesystem::remove(g);
}

#include "net/digest.hpp"

TEST_CASE("digest: MD5 (RFC 1321's test suite) and the Digest response (RFC 2617's example)", "[device][net]") {
    using namespace tisma::net;
    CHECK(md5_hex("") == "d41d8cd98f00b204e9800998ecf8427e");
    CHECK(md5_hex("abc") == "900150983cd24fb0d6963f7d28e17f72");
    CHECK(md5_hex("message digest") == "f96b697d7cb7938d525a2f31aaf161d0");
    CHECK(md5_hex("12345678901234567890123456789012345678901234567890123456789012345678901234567890") ==
          "57edf4a22be3c955ac49da2e2107b67a");
    DigestChallenge ch;
    REQUIRE(parse_digest_challenge(R"(Digest realm="testrealm@host.com", qop="auth,auth-int", nonce="dcd98b7102dd2f0e8b11d0f600bfb0c093", opaque="5ccc069c403ebaf9f0171e9517f40e41")",
                                   ch));
    CHECK(ch.realm == "testrealm@host.com");
    CHECK(ch.opaque == "5ccc069c403ebaf9f0171e9517f40e41");
    const std::string h = digest_authorization(ch, "Mufasa", "Circle Of Life", "GET", "/dir/index.html", "0a4f113b", 1);
    CHECK(h.find("response=\"6629fae49393a05397450978507c4ef1\"") != std::string::npos);
    CHECK(h.find("nc=00000001") != std::string::npos);
    CHECK_FALSE(parse_digest_challenge("Basic realm=\"x\"", ch));
}

namespace {

// A PrusaLink that asks for Digest (user "maker"), from its published OpenAPI.
class FakePrusaLink {
public:
    FakePrusaLink() {
        const auto authorized = [](const httplib::Request& req, httplib::Response& res) {
            const std::string a = req.get_header_value("Authorization");
            const auto field = [&](const std::string& key) {
                const std::size_t at = a.find(key + "=\"");
                if (at == std::string::npos)
                    return std::string();
                const std::size_t from = at + key.size() + 2;
                return a.substr(from, a.find('"', from) - from);
            };
            const std::size_t nc_at = a.find("nc=");
            const std::string nc = nc_at == std::string::npos ? "" : a.substr(nc_at + 3, 8);
            const std::string ha1 = tisma::net::md5_hex("maker:Printer:secret");
            const std::string ha2 = tisma::net::md5_hex(req.method + ":" + field("uri"));
            const std::string expected = tisma::net::md5_hex(ha1 + ":N0NCE:" + nc + ":" + field("cnonce") + ":auth:" + ha2);
            if (a.rfind("Digest ", 0) == 0 && field("username") == "maker" && field("response") == expected &&
                field("uri") == req.path)
                return true;
            res.status = 401;
            res.set_header("WWW-Authenticate", R"(Digest realm="Printer", nonce="N0NCE", qop="auth", algorithm=MD5)");
            return false;
        };
        server_.Get("/api/version", [authorized](const httplib::Request& req, httplib::Response& res) {
            if (authorized(req, res))
                res.set_content(R"({"api": "2.0.0", "server": "2.1.2", "text": "PrusaLink"})", "application/json");
        });
        server_.Put(R"(/api/v1/files/usb/(.+))", [this, authorized](const httplib::Request& req, httplib::Response& res) {
            if (!authorized(req, res))
                return;
            std::lock_guard lock(mutex_);
            name = req.matches[1];
            body = req.body;
            started = req.get_header_value("Print-After-Upload") == "?1";
            if (started)
                state = "PRINTING";
            res.status = 201;
        });
        server_.Get("/api/v1/status", [this, authorized](const httplib::Request& req, httplib::Response& res) {
            if (!authorized(req, res))
                return;
            std::lock_guard lock(mutex_);
            nlohmann::json j{{"printer", {{"state", state}, {"temp_nozzle", 214.0}, {"target_nozzle", 215.0},
                                          {"temp_bed", 59.0}, {"target_bed", 60.0}}},
                             {"job", {{"id", 7}, {"progress", 12.0}, {"time_remaining", 1800}}}};
            res.set_content(j.dump(), "application/json");
        });
        server_.Get("/api/v1/job", [this, authorized](const httplib::Request& req, httplib::Response& res) {
            if (!authorized(req, res))
                return;
            std::lock_guard lock(mutex_);
            if (state == "IDLE") {
                res.status = 204;
                return;
            }
            res.set_content(nlohmann::json{{"id", 7}, {"state", state}, {"file", {{"name", "CUBE~1.GCO"}, {"display_name", name}}}}.dump(),
                            "application/json");
        });
        server_.Put(R"(/api/v1/job/(\d+)/(pause|resume))", [this, authorized](const httplib::Request& req, httplib::Response& res) {
            if (!authorized(req, res))
                return;
            std::lock_guard lock(mutex_);
            calls.push_back(std::string(req.matches[2]) + " " + std::string(req.matches[1]));
            state = req.matches[2] == "pause" ? "PAUSED" : "PRINTING";
            res.status = 204;
        });
        server_.Delete(R"(/api/v1/job/(\d+))", [this, authorized](const httplib::Request& req, httplib::Response& res) {
            if (!authorized(req, res))
                return;
            std::lock_guard lock(mutex_);
            calls.push_back("stop " + std::string(req.matches[1]));
            state = "IDLE";
            res.status = 204;
        });
        port_ = server_.bind_to_any_port("127.0.0.1");
        thread_ = std::thread([this] { server_.listen_after_bind(); });
        server_.wait_until_ready();
    }
    ~FakePrusaLink() {
        server_.stop();
        thread_.join();
    }
    std::string url() const { return "http://127.0.0.1:" + std::to_string(port_); }
    std::string state = "IDLE", name, body;
    bool started = false;
    std::vector<std::string> calls;

private:
    httplib::Server server_;
    std::thread thread_;
    std::mutex mutex_;
    int port_ = 0;
};

} // namespace

TEST_CASE("print host: PrusaLink's API with Digest, upload and print, state, pause, resume and stop (printer_connections.md)",
          "[device][net]") {
    FakePrusaLink fake;
    HostConfig host;
    host.kind = HostKind::PrusaLink;
    host.url = fake.url();
    host.user = "maker";
    host.password = "secret";
    const HostVersion v = host_version(host);
    INFO(v.error);
    REQUIRE(v.ok);
    CHECK(v.server == "PrusaLink 2.1.2");
    HostConfig wrong = host;
    wrong.password = "nope";
    CHECK(host_version(wrong).code == "unauthorized");
    CHECK(host_cancel(host).code == "conflict"); // no job yet
    const std::filesystem::path g = std::filesystem::temp_directory_path() / "tisma_print_host_p.gcode";
    std::ofstream(g, std::ios::binary) << "G28\nM104 S215\n";
    REQUIRE(host_upload(host, g.string(), "cube.gcode", true).ok);
    CHECK(fake.name == "cube.gcode");
    CHECK(fake.body == "G28\nM104 S215\n");
    CHECK(fake.started);
    const HostStatus s = host_status(host);
    REQUIRE(s.ok);
    CHECK(s.printing);
    CHECK(s.file == "cube.gcode"); // the long name, from the job
    CHECK(s.progress == 12.0);
    CHECK(s.seconds_left == 1800.0);
    CHECK(s.nozzle_target == 215.0);
    REQUIRE(host_pause(host, true).ok);
    CHECK(host_status(host).paused);
    REQUIRE(host_pause(host, false).ok);
    REQUIRE(host_cancel(host).ok);
    CHECK(fake.calls == std::vector<std::string>{"pause 7", "resume 7", "stop 7"});
    HostKind k;
    CHECK(host_kind_from("prusalink", k));
    CHECK(k == HostKind::PrusaLink);
    CHECK(host_kind_from("duet", k));
    CHECK(k == HostKind::Duet);
    CHECK_FALSE(host_kind_from("repetier", k));
    std::filesystem::remove(g);
}

namespace {

// A Duet in standalone mode, from the RepRapFirmware wiki («HTTP requests»,
// «Object Model Documentation»): a password, a session key, rr_upload with
// a CRC32, rr_gcode, and the object model's state, job, heat and tools.
class FakeDuet {
public:
    FakeDuet() {
        server_.Get("/rr_connect", [this](const httplib::Request& req, httplib::Response& res) {
            std::lock_guard lock(mutex_);
            if (req.get_param_value("password") != "secret") {
                res.set_content(R"({"err": 1})", "application/json");
                return;
            }
            ++sessions;
            res.set_content(R"({"err": 0, "sessionTimeout": 8000, "boardType": "duet3mini5plus", "sessionKey": 4711})",
                            "application/json");
        });
        server_.Get("/rr_disconnect", [this](const httplib::Request&, httplib::Response& res) {
            std::lock_guard lock(mutex_);
            --sessions;
            res.set_content(R"({"err": 0})", "application/json");
        });
        const auto keyed = [](const httplib::Request& req, httplib::Response& res) {
            if (req.get_header_value("X-Session-Key") != "4711") {
                res.status = 401;
                return false;
            }
            return true;
        };
        server_.Post("/rr_upload", [this, keyed](const httplib::Request& req, httplib::Response& res) {
            if (!keyed(req, res))
                return;
            std::lock_guard lock(mutex_);
            name = req.get_param_value("name");
            crc = req.get_param_value("crc32");
            body = req.body;
            res.set_content(R"({"err": 0})", "application/json");
        });
        server_.Get("/rr_gcode", [this, keyed](const httplib::Request& req, httplib::Response& res) {
            if (!keyed(req, res))
                return;
            std::lock_guard lock(mutex_);
            const std::string g = req.get_param_value("gcode");
            codes.push_back(g);
            if (g.rfind("M32", 0) == 0)
                status = "processing";
            else if (g == "M25")
                status = "paused";
            else if (g == "M24")
                status = "processing";
            else if (g == "M0")
                status = "idle";
            res.set_content(R"({"buff": 255})", "application/json");
        });
        server_.Get("/rr_model", [this, keyed](const httplib::Request& req, httplib::Response& res) {
            if (!keyed(req, res))
                return;
            std::lock_guard lock(mutex_);
            const std::string key = req.get_param_value("key");
            nlohmann::json result;
            if (key == "boards[0]")
                result = {{"firmwareName", "RepRapFirmware"}, {"firmwareVersion", "3.5.2"}, {"name", "Duet 3 Mini 5+"}};
            else if (key == "state")
                result = {{"status", status}};
            else if (key == "job")
                result = {{"file", {{"fileName", "0:/gcodes/cube.gcode"}, {"size", 2000}}},
                          {"filePosition", 500},
                          {"timesLeft", {{"file", 900}, {"slicer", nullptr}}}};
            else if (key == "heat")
                result = {{"bedHeaters", {0, -1}},
                          {"heaters", {{{"current", 59.5}, {"active", 60}}, {{"current", 210.2}, {"active", 215}}}}};
            else if (key == "tools")
                result = {{{"number", 0}, {"heaters", {1}}}};
            res.set_content(nlohmann::json{{"key", key}, {"flags", req.get_param_value("flags")}, {"result", result}}.dump(),
                            "application/json");
        });
        port_ = server_.bind_to_any_port("127.0.0.1");
        thread_ = std::thread([this] { server_.listen_after_bind(); });
        server_.wait_until_ready();
    }
    ~FakeDuet() {
        server_.stop();
        thread_.join();
    }
    std::string url() const { return "http://127.0.0.1:" + std::to_string(port_); }

    std::mutex mutex_;
    std::string name, crc, body, status = "idle";
    std::vector<std::string> codes;
    int sessions = 0;

private:
    httplib::Server server_;
    std::thread thread_;
    int port_ = 0;
};

} // namespace

TEST_CASE("print host: Duet (RepRapFirmware standalone), session, upload and print, state, pause and cancel "
          "(printer_connections.md)",
          "[device][net]") {
    FakeDuet fake;
    HostConfig host;
    host.kind = HostKind::Duet;
    host.url = fake.url();
    host.password = "secret";
    const HostVersion v = host_version(host);
    INFO(v.error);
    REQUIRE(v.ok);
    CHECK(v.server == "RepRapFirmware 3.5.2 (Duet 3 Mini 5+)");
    HostConfig wrong = host;
    wrong.password = "nope";
    CHECK(host_version(wrong).code == "unauthorized");

    const std::filesystem::path g = std::filesystem::temp_directory_path() / "tisma_print_host_d.gcode";
    std::ofstream(g, std::ios::binary) << "G28\n";
    REQUIRE(host_upload(host, g.string(), "cube.gcode", true).ok);
    CHECK(fake.name == "0:/gcodes/cube.gcode");
    CHECK(fake.body == "G28\n");
    CHECK(fake.crc == "f722db83"); // CRC-32 of «G28\n»
    REQUIRE_FALSE(fake.codes.empty());
    CHECK(fake.codes.back() == "M32 \"0:/gcodes/cube.gcode\"");

    const HostStatus s = host_status(host);
    INFO(s.error);
    REQUIRE(s.ok);
    CHECK(s.printing);
    CHECK_FALSE(s.paused);
    CHECK(s.file == "0:/gcodes/cube.gcode");
    CHECK(s.progress == 25.0);
    CHECK(s.seconds_left == 900.0); // no slicer estimate: the file's
    CHECK(s.nozzle == 210.2);
    CHECK(s.nozzle_target == 215.0);
    CHECK(s.bed == 59.5);
    CHECK(s.bed_target == 60.0);

    REQUIRE(host_pause(host, true).ok);
    CHECK(host_status(host).paused);
    REQUIRE(host_pause(host, false).ok);
    // Cancel while printing: paused first, then M0.
    REQUIRE(host_cancel(host).ok);
    const std::vector<std::string> tail(fake.codes.end() - 4, fake.codes.end());
    CHECK(tail == std::vector<std::string>{"M25", "M24", "M25", "M0"});
    CHECK(fake.sessions == 0); // every session closed
    std::filesystem::remove(g);
}
