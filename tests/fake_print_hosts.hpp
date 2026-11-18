#pragma once
// Fake print host servers, local, for the engine's tests (test_print_host.cpp)
// and the interface's (fake_print_host_main.cpp): OctoPrint, Moonraker,
// PrusaLink, Duet and Repetier-Server, each as the engine's client uses them
// (printer_connections.md, devices.md). Own design; moved here unchanged from
// test_print_host.cpp so both can use them.
#include "net/digest.hpp"

#include <httplib.h>
#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

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
            body = file.content;
            started = req.form.has_field("print") && req.form.get_field("print") == "true";
            if (started)
                state = "printing";
            res.status = 201;
            res.set_content(R"({"result": {"item": {"path": "x.gcode", "root": "gcodes"}, "print_started": true}})",
                            "application/json");
        });
        server_.Get("/printer/objects/query", [this](const httplib::Request& req, httplib::Response& res) {
            std::lock_guard lock(mutex_);
            if (req.has_param("toolhead")) { // the limits (printer_connections.md, rule 8)
                res.set_content(R"({"result": {"status": {"toolhead": {"max_velocity": 300, "max_accel": 3000,
                    "square_corner_velocity": 5, "axis_minimum": [0, 0, -2, 0], "axis_maximum": [235, 235, 250, 0]},
                    "configfile": {"settings": {"printer": {"max_z_velocity": 15}}}}}})",
                                "application/json");
                return;
            }
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
    std::string state = "standby", name, body;
    bool started = false, asked = false;
    std::vector<std::string> calls;

private:
    httplib::Server server_;
    std::thread thread_;
    std::mutex mutex_;
    int port_ = 0;
};

} // namespace

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
                field("uri") == req.target) // as it was sent, before decoding
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

namespace {

// Repetier-Server as its Web API describes it: /printer/info without a key,
// the commands under /printer/api/<slug>?a=…, the uploads into models or jobs.
class FakeRepetier {
public:
    FakeRepetier() {
        server_.Get("/printer/info", [](const httplib::Request&, httplib::Response& res) {
            res.set_content(R"({"name": "Repetier-Server", "version": "1.4.10", "apikey": "",
                                "printers": [{"slug": "Ender_3", "name": "Ender 3", "active": true, "online": 1}]})",
                            "application/json");
        });
        server_.Post(R"(/printer/api/([^/]+))", [this](const httplib::Request& req, httplib::Response& res) {
            if (req.get_header_value("x-api-key") != "KEY") {
                res.status = 401;
                return;
            }
            std::lock_guard lock(mutex_);
            const std::string a = req.get_param_value("a");
            commands.push_back(req.matches[1].str() + ":" + a);
            if (a == "listPrinter")
                res.set_content(nlohmann::json::array({{{"slug", "Ender_3"}, {"online", 1}, {"job", job}, {"done", 12.5}}}).dump(),
                                "application/json");
            else if (a == "stateList")
                res.set_content(R"({"Ender_3": {"extruder": [{"tempRead": 199.5, "tempSet": 200}],
                                                "heatedBed": {"tempRead": 54.0, "tempSet": 55}}})",
                                "application/json");
            else if (a == "stopJob")
                job = "none";
            else
                res.set_content("{}", "application/json");
        });
        const auto upload = [this](bool queue) {
            return [this, queue](const httplib::Request& req, httplib::Response& res) {
                if (req.get_header_value("x-api-key") != "KEY") {
                    res.status = 401;
                    return;
                }
                std::lock_guard lock(mutex_);
                if (req.get_param_value("a") != "upload" || !req.form.has_file("filename")) {
                    res.status = 400;
                    return;
                }
                uploaded = req.form.get_file("filename").content;
                uploaded_name = req.get_param_value("name");
                if (queue)
                    job = uploaded_name;
                to_jobs = queue;
                res.set_content("{}", "application/json");
            };
        };
        server_.Post(R"(/printer/job/Ender_3)", upload(true));
        server_.Post(R"(/printer/model/Ender_3)", upload(false));
        port_ = server_.bind_to_any_port("127.0.0.1");
        thread_ = std::thread([this] { server_.listen_after_bind(); });
        server_.wait_until_ready();
    }
    ~FakeRepetier() {
        server_.stop();
        thread_.join();
    }
    std::string url() const { return "http://127.0.0.1:" + std::to_string(port_); }

    std::string job = "none", uploaded, uploaded_name;
    bool to_jobs = false;
    std::vector<std::string> commands;

private:
    httplib::Server server_;
    std::thread thread_;
    std::mutex mutex_;
    int port_ = 0;
};

} // namespace

// A Flashforge Adventurer 5M in LAN mode (printer_connections.md, «Flashforge
// en red local», FF1): HTTP POST /detail, /uploadGcode and /control, with the
// serial number SNFAKE123 and the check code 0a1b2c3d; every answer has «code».
class FakeFlashforge {
public:
    FakeFlashforge() {
        const auto authorized = [](const nlohmann::json& b) {
            return b.value("serialNumber", "") == "SNFAKE123" && b.value("checkCode", "") == "0a1b2c3d";
        };
        server_.Post("/detail", [this, authorized](const httplib::Request& req, httplib::Response& res) {
            const nlohmann::json b = nlohmann::json::parse(req.body, nullptr, false);
            std::lock_guard lock(mutex_);
            ++details;
            if (!b.is_object() || !authorized(b)) {
                res.set_content(R"({"code": 1, "message": "Access code is different"})", "application/json");
                return;
            }
            const nlohmann::json d = {{"status", state},     {"name", "Adventurer 5M"}, {"rightTemp", 210.5},
                                      {"rightTargetTemp", 210}, {"platTemp", 54.5},    {"platTargetTemp", 55},
                                      {"printProgress", state == "ready" ? 0.0 : 0.25}, {"printDuration", 60},
                                      {"estimatedTime", 1800.0}, {"printFileName", uploaded_name}};
            res.set_content(nlohmann::json{{"code", 0}, {"message", "Success"}, {"detail", d}}.dump(), "application/json");
        });
        server_.Post("/control", [this, authorized](const httplib::Request& req, httplib::Response& res) {
            const nlohmann::json b = nlohmann::json::parse(req.body, nullptr, false);
            std::lock_guard lock(mutex_);
            if (!b.is_object() || !authorized(b)) {
                res.set_content(R"({"code": 1, "message": "Access code is different"})", "application/json");
                return;
            }
            const nlohmann::json p = b.value("payload", nlohmann::json::object());
            const std::string action = p.value("args", nlohmann::json::object()).value("action", "");
            controls.push_back(p.value("cmd", "") + ":" + action);
            if (p.value("cmd", "") == "jobCtl_cmd")
                state = action == "pause" ? "pause" : action == "continue" ? "printing" : action == "cancel" ? "cancel" : state;
            res.set_content(R"({"code": 0, "message": "Success"})", "application/json"); // success for anything, as the real one
        });
        server_.Post("/uploadGcode", [this](const httplib::Request& req, httplib::Response& res) {
            std::lock_guard lock(mutex_);
            if (req.get_header_value("serialNumber") != "SNFAKE123" || req.get_header_value("checkCode") != "0a1b2c3d") {
                res.set_content(R"({"code": 1, "message": "Access code is different"})", "application/json");
                return;
            }
            if (!req.form.has_file("gcodeFile")) {
                res.set_content(R"({"code": -1, "message": "Parameters is error"})", "application/json");
                return;
            }
            const auto file = req.form.get_file("gcodeFile");
            uploaded = file.content;
            uploaded_name = file.filename;
            file_size = req.get_header_value("fileSize");
            print_now = req.get_header_value("printNow");
            leveling = req.get_header_value("levelingBeforePrint");
            if (print_now == "true")
                state = "printing";
            res.set_content(R"({"code": 0, "message": "Success"})", "application/json");
        });
        port_ = server_.bind_to_any_port("127.0.0.1");
        thread_ = std::thread([this] { server_.listen_after_bind(); });
        server_.wait_until_ready();
    }
    ~FakeFlashforge() {
        server_.stop();
        thread_.join();
    }
    std::string url() const { return "http://127.0.0.1:" + std::to_string(port_); }

    std::string state = "ready", uploaded, uploaded_name, file_size, print_now, leveling;
    std::vector<std::string> controls;
    int details = 0;

private:
    httplib::Server server_;
    std::thread thread_;
    std::mutex mutex_;
    int port_ = 0;
};
