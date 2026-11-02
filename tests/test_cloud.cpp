// The cloud client against a mock server built from docs/spec/features/cloud.md
// (rules 2 to 11 and 5a; tests CC1 to CC6 and CC13 to CC15 as far as the
// client goes), on 127.0.0.1 with cpp-httplib's server.

#include "cloud/client.hpp"
#include "net/http.hpp"

#include <catch2/catch_test_macros.hpp>
#include <httplib.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <atomic>
#include <iterator>
#include <filesystem>
#include <fstream>
#include <map>
#include <mutex>
#include <string>
#include <thread>

using namespace tisma;
using nlohmann::json;

namespace {

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
                                 "currency": "USD", "remote_printers": 1},
                                {"name": "Maker", "monthly_hours": 10, "max_concurrent": 2, "price_cents": 900,
                                 "currency": "USD", "remote_printers": 3}])",
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
            if (file.content.size() > 4096) {
                res.status = 413;
                res.set_content(R"({"detail": "The project is larger than the plan allows"})", "application/json");
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
    std::string job_json() const {
        json j = {{"id", "j1"},        {"kind", kind_},       {"status", state_},
                  {"progress", state_ == "done" ? 100 : state_ == "running" ? 50 : 0},
                  {"seconds", 12.5},   {"error", nullptr},    {"result_filename", "project.gcode"}};
        if (state_ == "done")
            j["summary"] = {{"print_time", "1h 2m 3s"}, {"filament_g", 12.3}, {"result", {{"verdict", "ok"}}}};
        return j.dump();
    }

    httplib::Server server_;
    std::thread thread_;
    int port_ = 0;
};

std::filesystem::path temp_file(const std::string& name, const std::string& content) {
    const std::filesystem::path p = std::filesystem::temp_directory_path() / name;
    std::ofstream(p, std::ios::binary) << content;
    return p;
}

} // namespace

TEST_CASE("http: addresses are read with their scheme, host, port and path", "[cloud]") {
    net::Url u;
    REQUIRE(net::parse_url("https://cloud.tisma.example/v1/me?x=1", u));
    CHECK(u.https);
    CHECK(u.host == "cloud.tisma.example");
    CHECK(u.port == 443);
    CHECK(u.path == "/v1/me?x=1");
    REQUIRE(net::parse_url("http://127.0.0.1:8080", u));
    CHECK_FALSE(u.https);
    CHECK(u.port == 8080);
    CHECK(u.path == "/");
    REQUIRE(net::parse_url("http://[::1]:9000/a", u));
    CHECK(u.host == "::1");
    CHECK(u.port == 9000);
    CHECK_FALSE(net::parse_url("ftp://x/", u));
    CHECK_FALSE(net::parse_url("http://", u));
    CHECK_FALSE(net::parse_url("http://host:99999/", u));
    CHECK_FALSE(net::parse_url("http://user@host/", u)); // no credentials in addresses
}

TEST_CASE("http: a multipart body has each field and file between its boundaries", "[cloud]") {
    const std::filesystem::path file = temp_file("tisma_cloud_part.bin", std::string("AB\0C", 4));
    std::string body, error;
    REQUIRE(net::form_body({{"kind", "slice", "", "", "text/plain"}, {"file", "", file.string(), "p.3mf", "model/3mf"}},
                           "XYZ", body, error));
    std::string expected = "--XYZ\r\nContent-Disposition: form-data; name=\"kind\"\r\n\r\nslice\r\n"
                           "--XYZ\r\nContent-Disposition: form-data; name=\"file\"; filename=\"p.3mf\"\r\n"
                           "Content-Type: model/3mf\r\n\r\n";
    expected += std::string("AB\0C", 4);
    expected += "\r\n--XYZ--\r\n";
    CHECK(body == expected);
    CHECK_FALSE(net::form_body({{"file", "", "/no/such/file", "x", "a/b"}}, "XYZ", body, error));
    CHECK(error.find("/no/such/file") != std::string::npos);
    std::filesystem::remove(file);
}

TEST_CASE("cloud: the server's error text is what the person reads (rule 4)", "[cloud]") {
    CHECK(cloud::api_error(R"({"detail": "Session expired"})", "", 401) == "Session expired");
    CHECK(cloud::api_error(R"({"detail": [{"msg": "title: at least 3 characters"}]})", "", 422) ==
          "title: at least 3 characters");
    CHECK(cloud::api_error("<html>bad gateway</html>", "", 502) == "HTTP 502");
    CHECK(cloud::api_error("", "Could not establish connection", 0) == "Could not establish connection");
    cloud::Job job;
    cloud::parse_job(R"({"id": "j", "status": "done", "progress": 100, "seconds": 3,
                         "summary": {"print_time": "1h", "result": {"verdict": "ok", "safety": 2.5}, "x": null}})",
                     job);
    CHECK(job.finished());
    CHECK(job.summary.at("print_time") == "1h");
    CHECK(job.summary.at("result.verdict") == "ok");
    CHECK(job.summary.at("result.safety") == "2.5");
    CHECK_FALSE(job.summary.contains("x"));
}

TEST_CASE("cloud: log in, read the account and the plans, log out (CC1, CC2, CC13, CC14, CC15)", "[cloud]") {
    MockCloud mock;
    cloud::Client client(mock.url() + "/", "");
    cloud::Failure error;
    std::string billing;
    REQUIRE(client.health(billing, error));
    CHECK(billing == "https://cloud.example/account");

    // CC13: the plans without a session; the hidden one is not there.
    std::vector<cloud::Plan> plans;
    REQUIRE(client.plans(plans, error));
    REQUIRE(plans.size() == 2);
    CHECK(plans[1].name == "Maker");
    CHECK(plans[1].monthly_hours == 10.0);
    CHECK(plans[1].price_cents == 900);
    CHECK(plans[1].currency == "USD");
    CHECK(plans[1].max_concurrent == 2);

    std::string token;
    CHECK_FALSE(client.login("ana@example.com", "wrong", "pc", token, error));
    CHECK(error.status == 401);
    CHECK(error.message == "Wrong email or password");
    REQUIRE(client.login("ana@example.com", "secret123", "pc", token, error));
    CHECK(token == "T1");
    CHECK(client.token() == "T1");

    // CC14: the hours as the server sends them, with what the administrator gave.
    cloud::Account account;
    REQUIRE(client.account(account, error));
    CHECK(account.plan == "Maker");
    CHECK(account.paid);
    CHECK(account.hours_included == 11.5);
    CHECK(account.hours_left == 9.25);
    CHECK(account.max_concurrent == 2);

    REQUIRE(client.logout(error));
    CHECK(client.token().empty());

    // CC2 and CC15: an expired session and a disabled account both ask to log in.
    cloud::Client expired(mock.url(), "old");
    CHECK_FALSE(expired.account(account, error));
    CHECK(error.expired());
    CHECK(error.message == "Session expired");
    cloud::Client disabled(mock.url(), "disabled");
    CHECK_FALSE(disabled.account(account, error));
    CHECK(error.expired());
    CHECK(error.message == "The account is disabled");
    // Too many requests: its own status (rule 4).
    cloud::Client busy(mock.url(), "busy");
    CHECK_FALSE(busy.account(account, error));
    CHECK(error.status == 429);
}

TEST_CASE("cloud: a problem report goes with or without an account (CC6)", "[cloud]") {
    MockCloud mock;
    cloud::Failure error;
    int similar = -1;
    cloud::Client anonymous(mock.url(), "");
    REQUIRE(anonymous.report({{"app", "pc"}, {"kind", "bug"}, {"title", "The plate jumps"}, {"description", "ñandú"}},
                             similar, error));
    CHECK(similar == 2);
    {
        std::lock_guard lock(mock.mutex_);
        CHECK(mock.report_token_.empty());
        CHECK(mock.last_report_["description"] == "ñandú");
        CHECK_FALSE(mock.last_report_.contains("log")); // nothing the person did not accept
    }
    cloud::Client signed_in(mock.url(), "T1");
    REQUIRE(signed_in.report({{"app", "pc"}, {"kind", "crash"}, {"title", "Crash on open"}}, similar, error));
    {
        std::lock_guard lock(mock.mutex_);
        CHECK(mock.report_token_ == "Bearer T1");
    }
    CHECK_FALSE(anonymous.report({{"app", "pc"}, {"kind", "bug"}, {"title", "x"}}, similar, error));
    CHECK(error.status == 422);
    CHECK(error.message == "title: at least 3 characters");
}

TEST_CASE("cloud: a job goes from queued to done and its result comes down (CC3)", "[cloud]") {
    MockCloud mock;
    const std::filesystem::path project = temp_file("tisma_cloud_project.3mf", "PK fake 3mf");
    const std::filesystem::path result = std::filesystem::temp_directory_path() / "tisma_cloud_result.gcode";
    std::filesystem::remove(result);
    cloud::Client client(mock.url(), "T1");
    client.poll_ms = 5;
    cloud::Job job;
    cloud::Failure error;
    std::vector<std::string> seen;
    std::atomic<bool> cancel{false};
    REQUIRE(client.run("slice", "{}", project.string(), result.string(), job, error,
                       [&](const cloud::Job& j) { seen.push_back(j.status); }, cancel));
    CHECK(seen.front() == "queued");
    CHECK(std::find(seen.begin(), seen.end(), "running") != seen.end());
    CHECK(seen.back() == "done");
    CHECK(job.summary.at("print_time") == "1h 2m 3s");
    CHECK(job.summary.at("result.verdict") == "ok");
    {
        std::lock_guard lock(mock.mutex_);
        CHECK(mock.kind_ == "slice");
        CHECK(mock.params_ == "{}");
        CHECK(mock.filename_ == "project.3mf");
        CHECK(mock.project_ == "PK fake 3mf");
    }
    std::ifstream in(result, std::ios::binary);
    const std::string gcode((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    CHECK(gcode == "; sliced on the server\nG1 X10 Y10 E1\n");
    in.close();
    std::filesystem::remove(result);
    std::filesystem::remove(project);
}

TEST_CASE("cloud: cancelling a running job cancels it on the server too (CC4)", "[cloud]") {
    MockCloud mock;
    const std::filesystem::path project = temp_file("tisma_cloud_cancel.3mf", "PK");
    cloud::Client client(mock.url(), "T1");
    client.poll_ms = 5;
    cloud::Job job;
    cloud::Failure error;
    std::atomic<bool> cancel{false};
    CHECK_FALSE(client.run("fea", R"({"mode": "analyze"})", project.string(), "unused.tea", job, error,
                           [&](const cloud::Job& j) {
                               if (j.status == "running")
                                   cancel = true;
                           },
                           cancel));
    CHECK(error.message == "Cancelled");
    std::lock_guard lock(mock.mutex_);
    CHECK(mock.cancels_ == 1);
    CHECK(mock.state_ == "cancelled");
    std::filesystem::remove(project);
}

TEST_CASE("cloud: a project too large is refused with the server's words (CC5), and no server is an error", "[cloud]") {
    MockCloud mock;
    const std::filesystem::path project = temp_file("tisma_cloud_big.3mf", std::string(5000, 'x'));
    cloud::Client client(mock.url(), "T1");
    cloud::Job job;
    cloud::Failure error;
    CHECK_FALSE(client.submit("slice", "{}", project.string(), job, error));
    CHECK(error.status == 413);
    CHECK(error.message == "The project is larger than the plan allows");
    std::filesystem::remove(project);

    // Nothing listens on port 9 here: an error with words, no status.
    cloud::Client nowhere("http://127.0.0.1:9", "T1");
    std::string billing;
    CHECK_FALSE(nowhere.health(billing, error));
    CHECK(error.status == 0);
    CHECK_FALSE(error.message.empty());
}
