#pragma once

// A mock Tisma Cloud server for the tests, built from docs/spec/features/cloud.md
// with cpp-httplib's server, on 127.0.0.1 and a free port.

#include "io/zip.hpp"

#include <httplib.h>
#include <nlohmann/json.hpp>

#include <mutex>
#include <string>
#include <thread>

namespace tisma::test {

using nlohmann::json;

// The server's side of cloud.md, as small as the tests need.
class MockCloud {
public:
    MockCloud() {
        server_.Get("/v1/health", [](const httplib::Request&, httplib::Response& res) {
            res.set_content(R"({"ok": true, "billing_url": "https://cloud.example/account"})", "application/json");
        });
        server_.Post("/v1/auth/login", [](const httplib::Request& req, httplib::Response& res) {
            const json body = json::parse(req.body);
            if (body.value("email", "") == "ana@example.com" && body.value("password", "") == "secret123" &&
                !body.value("device", "").empty()) {
                res.set_content(R"({"token": "T1", "email": "ana@example.com"})", "application/json");
            } else {
                res.status = 401;
                res.set_content(R"({"detail": "Wrong email or password"})", "application/json");
            }
        });
        server_.Post("/v1/auth/logout", [this](const httplib::Request& req, httplib::Response& res) {
            if (!authorized(req, res))
                return;
            res.set_content("{}", "application/json");
        });
        server_.Get("/v1/me", [this](const httplib::Request& req, httplib::Response& res) {
            if (!authorized(req, res))
                return;
            // 10 h of the plan, 1.5 h given by an administrator, 2.25 h used.
            res.set_content(R"({"email": "ana@example.com", "plan": "Maker", "paid_until": "2026-12-01T00:00:00Z",
                                "paid": true, "hours_included": 11.5, "hours_used": 2.25, "hours_left": 9.25,
                                "active_jobs": 1, "max_concurrent": 2})",
                            "application/json");
        });
        // The public plans, with or without a session; a hidden plan is not sent.
        server_.Get("/v1/plans", [](const httplib::Request&, httplib::Response& res) {
            res.set_content(R"([{"name": "Free", "monthly_hours": 1, "max_concurrent": 1, "price_cents": 0,
                                 "currency": "USD", "remote_printers": 1, "classes": ["slice"]},
                                {"name": "Maker", "monthly_hours": 10, "max_concurrent": 2, "price_cents": 900,
                                 "currency": "USD", "remote_printers": 3,
                                 "classes": ["slice", "slice_large", "fea", "fea_optimize", "aero", "orient"]}])",
                            "application/json");
        });
        server_.Post("/v1/reports", [this](const httplib::Request& req, httplib::Response& res) {
            const json body = json::parse(req.body);
            if (body.value("title", "").size() < 3) {
                res.status = 422;
                res.set_content(R"({"detail": [{"loc": ["body", "title"], "msg": "title: at least 3 characters"}]})",
                                "application/json");
                return;
            }
            if (body.value("title", "").rfind("flood", 0) == 0) {
                res.status = 429;
                res.set_content(R"({"detail": "Too many reports"})", "application/json");
                return;
            }
            std::lock_guard lock(mutex_);
            last_report_ = body;
            report_token_ = req.get_header_value("Authorization");
            res.set_content(R"({"id": 7, "similar": 2})", "application/json");
        });
        server_.Post("/v1/jobs", [this](const httplib::Request& req, httplib::Response& res) {
            if (!authorized(req, res))
                return;
            if (!req.form.has_field("kind") || !req.form.has_field("params") || !req.form.has_file("file")) {
                res.status = 422;
                res.set_content(R"({"detail": "kind, params and file are needed"})", "application/json");
                return;
            }
            const httplib::FormData file = req.form.get_file("file");
            if (file.content.size() > (std::size_t(1) << 20)) {
                res.status = 413;
                res.set_content(R"({"detail": "The project is larger than the plan allows"})", "application/json");
                return;
            }
            // As the real server: a ZIP with a 3D/*.model that has triangles.
            if (const char* problem = not_3mf(file.content)) {
                res.status = 422;
                res.set_content(json{{"detail", problem}}.dump(), "application/json");
                return;
            }
            std::lock_guard lock(mutex_);
            kind_ = req.form.get_field("kind");
            params_ = req.form.get_field("params");
            filename_ = file.filename;
            project_ = file.content;
            polls_ = 0;
            state_ = "queued";
            res.set_content(job_json(), "application/json");
        });
        // The account's own reports, oldest first here: the client sorts them anyway (IP10).
        server_.Get("/v1/me/reports", [](const httplib::Request& req, httplib::Response& res) {
            if (!authorized(req, res))
                return;
            res.set_content(R"({"reports": [{"id": 3, "created_at": "2026-10-20T10:00:00Z", "title": "The supports are missing",
                                             "status": "fixed"},
                                            {"id": 9, "created_at": "2026-11-01T09:30:00Z", "title": "Tisma closed while slicing",
                                             "status": "confirmed"}]})",
                            "application/json");
        });
        // The account's jobs: the one sent, if any, and an older analysis that failed.
        server_.Get("/v1/jobs", [this](const httplib::Request& req, httplib::Response& res) {
            if (!authorized(req, res))
                return;
            if (req.get_param_value("limit") != "50") {
                res.status = 422;
                res.set_content(R"({"detail": "limit expected"})", "application/json");
                return;
            }
            std::lock_guard lock(mutex_);
            json list = json::array();
            if (!state_.empty())
                list.push_back(json::parse(job_json()));
            list.push_back({{"id", "j0"}, {"kind", "fea"}, {"status", "failed"}, {"progress", 40}, {"seconds", 61.0},
                            {"error", "The part has no supports"}, {"result_filename", nullptr},
                            {"created_at", "2026-11-01T18:20:00Z"}});
            res.set_content(json{{"jobs", list}}.dump(), "application/json");
        });
        server_.Get(R"(/v1/jobs/([a-z0-9]+))", [this](const httplib::Request& req, httplib::Response& res) {
            if (!authorized(req, res))
                return;
            std::lock_guard lock(mutex_);
            if (req.matches[1] != "j1") {
                res.status = 404;
                res.set_content(R"({"detail": "No such job"})", "application/json");
                return;
            }
            // queued, running at 50 %, done.
            ++polls_;
            if (state_ != "cancelled")
                state_ = polls_ == 1 ? "running" : polls_ >= 3 ? "done" : "running";
            res.set_content(job_json(), "application/json");
        });
        server_.Post(R"(/v1/jobs/([a-z0-9]+)/cancel)", [this](const httplib::Request& req, httplib::Response& res) {
            if (!authorized(req, res))
                return;
            std::lock_guard lock(mutex_);
            state_ = "cancelled";
            ++cancels_;
            res.set_content(job_json(), "application/json");
        });
        server_.Get(R"(/v1/jobs/([a-z0-9]+)/result)", [this](const httplib::Request& req, httplib::Response& res) {
            if (!authorized(req, res))
                return;
            std::lock_guard lock(mutex_);
            if (kind_ == "aero") { // the engine's .taero (cloud.md, rule 10c)
                res.set_content(json{{"drag_n", 0.42}, {"cd", 1.05}, {"simulated", true}, {"object", 0}, {"instance", 0}}.dump(),
                                "application/json");
                return;
            }
            if (kind_ == "fea") { // an analysis: the engine's .tea (cloud.md, rule 9)
                const json tea = {{"format", "tisma-engineering-result"}, {"version", 1}, {"kind", "structural"},
                                  {"object", 0}, {"fingerprint", "mock"},
                                  {"report", {{"verdict", {{"name", "holds"}, {"min_safety", 3.5}, {"required_safety", 2.0}}},
                                              {"valid", true}}}};
                res.set_content(tea.dump(), "application/json");
                return;
            }
            res.set_content("; sliced on the server\nG1 X10 Y10 E1\n", "text/plain");
        });
        port_ = server_.bind_to_any_port("127.0.0.1");
        thread_ = std::thread([this] { server_.listen_after_bind(); });
        server_.wait_until_ready();
    }
    ~MockCloud() {
        server_.stop();
        thread_.join();
    }
    std::string url() const { return "http://127.0.0.1:" + std::to_string(port_); }

    std::mutex mutex_;
    json last_report_;
    std::string report_token_, kind_, params_, filename_, project_, state_;
    int polls_ = 0, cancels_ = 0;

private:
    // T1 is a good session; "disabled" an account the administrator turned
    // off (CC15); "busy" one that asks too often; anything else is over.
    static bool authorized(const httplib::Request& req, httplib::Response& res) {
        const std::string auth = req.get_header_value("Authorization");
        if (auth == "Bearer T1")
            return true;
        if (auth == "Bearer busy") {
            res.status = 429;
            res.set_content(R"({"detail": "Too many requests: wait a minute"})", "application/json");
            return false;
        }
        res.status = 401;
        res.set_content(auth == "Bearer disabled" ? R"({"detail": "The account is disabled"})"
                                                  : R"({"detail": "Session expired"})",
                        "application/json");
        return false;
    }
    // Why an upload is not a project the server takes, or null.
    static const char* not_3mf(const std::string& bytes) {
        if (!tisma::looks_like_zip(bytes))
            return "The project is not a 3MF file";
        try {
            const tisma::ZipArchive zip(bytes);
            for (const tisma::ZipArchive::Entry& e : zip.entries())
                if (e.name.starts_with("3D/") && e.name.ends_with(".model") &&
                    zip.read(e).find("<triangle ") != std::string::npos)
                    return nullptr;
        } catch (const std::exception&) {
            return "The project is not a 3MF file";
        }
        return "The 3MF file has no model";
    }
    std::string job_json() const {
        json j = {{"id", "j1"},        {"kind", kind_},       {"status", state_},
                  {"progress", state_ == "done" ? 100 : state_ == "running" ? 50 : 0},
                  {"seconds", 12.5},   {"error", nullptr},    {"result_filename", "project.gcode"},
                  {"created_at", "2026-11-02T20:00:00Z"}};
        if (state_ == "done")
            j["summary"] = {{"print_time", "1h 2m 3s"}, {"filament_g", 12.3}, {"result", {{"verdict", "ok"}}}};
        return j.dump();
    }

    httplib::Server server_;
    std::thread thread_;
    int port_ = 0;
};

} // namespace tisma::test
