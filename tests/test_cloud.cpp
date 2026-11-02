// The cloud client against a mock server built from docs/spec/features/cloud.md
// (rules 2 to 11 and 5a; tests CC1 to CC6 and CC13 to CC15 as far as the
// client goes), on 127.0.0.1 with cpp-httplib's server.

#include "cloud/client.hpp"
#include "cloud/report.hpp"
#include "cloud/secret_store.hpp"
#include "net/http.hpp"
#include "mock_cloud.hpp"

#include <catch2/catch_test_macros.hpp>
#include <httplib.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <iterator>
#include <filesystem>
#include <fstream>
#include <map>
#include <mutex>
#include <random>
#include <string>
#include <thread>

using namespace tisma;
using nlohmann::json;
using tisma::test::MockCloud;

namespace {

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

TEST_CASE("cloud: the account's jobs are listed (rule 10)", "[cloud]") {
    MockCloud mock;
    cloud::Client client(mock.url(), "T1");
    std::vector<cloud::Job> jobs;
    cloud::Failure error;
    REQUIRE(client.jobs(jobs, error));
    REQUIRE(jobs.size() == 1);
    CHECK(jobs[0].id == "j0");
    CHECK(jobs[0].kind == "fea");
    CHECK(jobs[0].status == "failed");
    CHECK(jobs[0].error == "The part has no supports");
    CHECK(jobs[0].seconds == 61.0);
    CHECK(jobs[0].created_at == "2026-11-01T18:20:00Z");
    // One sent: it comes first.
    const std::filesystem::path project = temp_file("tisma_cloud_list.3mf", "PK fake 3mf");
    cloud::Job sent;
    REQUIRE(client.submit("slice", "{}", project.string(), sent, error));
    REQUIRE(client.jobs(jobs, error));
    REQUIRE(jobs.size() == 2);
    CHECK(jobs[0].id == "j1");
    CHECK(jobs[0].kind == "slice");
    CHECK(jobs[0].status == "queued");
    // Without a session: the server's 401.
    cloud::Client stranger(mock.url(), "");
    CHECK_FALSE(stranger.jobs(jobs, error));
    CHECK(error.expired());
    std::filesystem::remove(project);
}

TEST_CASE("cloud: the account's own reports, the newest first (problem_reports.md, rule 11; IP10)", "[cloud]") {
    MockCloud mock;
    cloud::Client client(mock.url(), "T1");
    std::vector<cloud::SentReport> reports;
    cloud::Failure error;
    REQUIRE(client.my_reports(reports, error));
    REQUIRE(reports.size() == 2);
    CHECK(reports[0].id == "9");
    CHECK(reports[0].title == "Tisma closed while slicing");
    CHECK(reports[0].status == "confirmed");
    CHECK(reports[1].status == "fixed");
    cloud::Client stranger(mock.url(), "");
    CHECK_FALSE(stranger.my_reports(reports, error));
    CHECK(error.expired());
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
    const std::filesystem::path project = temp_file("tisma_cloud_big.3mf", std::string(1500000, 'x'));
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

TEST_CASE("cloud: the session's token is kept in the system's store, not in a file of ours (rule 3, CC1)", "[cloud]") {
#ifndef _WIN32
    // Linux and macOS in the tests: the stand-in for secret-tool.
    const std::filesystem::path store = std::filesystem::temp_directory_path() / "tisma_fake_secrets.txt";
    std::filesystem::remove(store);
    setenv("TISMA_SECRET_TOOL", TISMA_FAKE_SECRET_TOOL, 1);
    setenv("TISMA_FAKE_SECRETS", store.string().c_str(), 1);
#ifdef __APPLE__
    // macOS uses its keychain through security(1): not touched by the tests.
    unsetenv("TISMA_SECRET_TOOL");
    unsetenv("TISMA_FAKE_SECRETS");
    SKIP("the macOS keychain is not used by the tests");
#endif
    CHECK(cloud::secret_store() == cloud::SecretStore::SecretTool);
#else
    CHECK(cloud::secret_store() == cloud::SecretStore::WindowsCredentials);
#endif
    const std::string server = "http://127.0.0.1:1/tisma-test-" + std::to_string(std::random_device{}());
    std::string error;
    CHECK_FALSE(cloud::load_secret(server).has_value());
    REQUIRE(cloud::store_secret(server, "token with spaces & ñ", error));
    REQUIRE(cloud::load_secret(server).has_value());
    CHECK(*cloud::load_secret(server) == "token with spaces & ñ");
    REQUIRE(cloud::store_secret(server, "T2", error)); // replaces it
    CHECK(*cloud::load_secret(server) == "T2");
    CHECK(cloud::forget_secret(server));
    CHECK_FALSE(cloud::load_secret(server).has_value());
#ifndef _WIN32
    // Without a store the client keeps the token only while it runs.
    const char* old_path = std::getenv("PATH");
    const std::string path = old_path ? old_path : "";
    setenv("TISMA_SECRET_TOOL", "", 1);
    setenv("PATH", "/nonexistent", 1);
    CHECK(cloud::secret_store() == cloud::SecretStore::None);
    CHECK_FALSE(cloud::store_secret(server, "T3", error));
    CHECK_FALSE(error.empty());
    setenv("PATH", path.c_str(), 1);
    unsetenv("TISMA_SECRET_TOOL");
    unsetenv("TISMA_FAKE_SECRETS");
    std::filesystem::remove(store);
#endif
}

TEST_CASE("cloud: a problem report sends what was accepted and the log's end (IP3, IP4, IP8)", "[cloud]") {
    cloud::ReportInput in;
    in.title = "The supports are missing";
    in.description = "Under the overhang";
    in.app_version = "0.11.0";
    in.os = "Windows 11 Pro 24H2 (x64), a description far longer than eighty characters so it gets cut";
    in.printer = "Creality CR-5 Pro H";
    in.print_profile = "0,20 mm";
    in.material_profile = "PLA";
    in.session_log = "line 1\nline 2\n";
    std::map<std::string, std::string> f = cloud::report_fields(in);
    CHECK(f.at("app") == "pc");
    CHECK(f.at("kind") == "bug");
    CHECK(f.at("os").size() == cloud::kReportOsMax);
    // IP3: the log starts with the program, the system and the printer.
    CHECK(f.at("log").rfind("Tisma 0.11.0\nSystem: Windows 11", 0) == 0);
    CHECK(f.at("log").find("Printer: Creality CR-5 Pro H\n") != std::string::npos);
    CHECK(f.at("log").find("line 2\n") != std::string::npos);
    CHECK(f.at("log") == cloud::report_log(in)); // IP5: what is shown is what goes
    // IP4: without the box, none of them.
    in.attach = false;
    f = cloud::report_fields(in);
    CHECK_FALSE(f.contains("app_version"));
    CHECK_FALSE(f.contains("os"));
    CHECK_FALSE(f.contains("log"));
    // IP8: a long log sends its end, the total within the limit, whole characters.
    in.attach = true;
    in.session_log = "START" + std::string(300000, 'x') + "ñ" + "THE END";
    f = cloud::report_fields(in);
    CHECK(f.at("log").size() <= cloud::kReportLogMax);
    CHECK(f.at("log").find("START") == std::string::npos);
    CHECK(f.at("log").ends_with("ñTHE END"));
    // The texts within their limits, cut between characters.
    in.description = std::string(9999, 'a') + "ñ";
    f = cloud::report_fields(in);
    CHECK(f.at("description") == std::string(9999, 'a'));
}
