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
    HostConfig host{HostKind::OctoPrint, fake.url() + "/", "KEY"};
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
