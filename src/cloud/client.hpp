#pragma once

// The client of Tisma Cloud, the computing service (docs/spec/features/cloud.md):
// the account, the public plans, problem reports, and slicing or analysing on
// the servers. The owner's code: the Tisma Cloud client of Tisma Slicer,
// relicensed by the owner on 02-11-2026, without PrusaSlicer's HTTP layer
// (see PROVENANCE.md); its requests go through net/http.hpp.
//
// The calls block: run them outside the interface's thread.

#include <atomic>
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace tisma::cloud {

// The account (GET /v1/me). The hours already count what an administrator
// gave or took: they are shown as they come (cloud.md, rule 5a).
struct Account {
    std::string email;
    std::string plan;
    std::string paid_until; // ISO 8601; empty: never paid
    bool paid = false;
    double hours_included = 0.0;
    double hours_used = 0.0;
    double hours_left = 0.0;
    int active_jobs = 0;
    int max_concurrent = 0;
};

// A public plan (GET /v1/plans, no session needed; cloud.md, rule 5a).
struct Plan {
    std::string name;
    double monthly_hours = 0.0;
    int max_concurrent = 0;
    long long price_cents = 0;
    std::string currency;
    int remote_printers = 0;
};

// A report the account sent (GET /v1/me/reports; problem_reports.md, rule 11).
struct SentReport {
    std::string id, created_at, title;
    std::string status; // new, confirmed, fixed, wontfix or duplicate
};

struct Job {
    std::string id;
    std::string kind;   // slice or fea
    std::string status; // queued, running, done, failed, cancelled
    std::string created_at; // as the server writes it (ISO 8601)
    int progress = 0;
    std::string error;
    std::string result_filename;
    double seconds = 0.0;
    // The summary, flattened: "print_time", "result.verdict", ...
    std::map<std::string, std::string> summary;

    bool finished() const { return status == "done" || status == "failed" || status == "cancelled"; }
};

// The text of a failed job for a person (cloud.md, rule 19a): in Spanish,
// the summary's error_es, else a text by its code (bad_mode, bad_quality,
// not_in_cloud, needs_conditions, no_valid_result); otherwise, and in
// English, the job's error.
std::string job_error(const Job& job, bool spanish);

// What a failed call tells: the server's text (its "detail") or the
// connection's, and the HTTP status (0 without an answer). The interface has
// its own notice for 401, 413, 422 and 429 (cloud.md, rules 2 and 4).
// Tisma Cloud's own server (cloud.md, rule 1; the owner confirmed it on
// 2026-11-02): the default of the server field. The address a person saved
// wins over it, and --cloud-server over both for one run.
inline constexpr const char* kDefaultServer = "https://tismaservidores.net";

struct Failure {
    int status = 0;
    std::string message;
    bool expired() const { return status == 401; } // the session is over: log in again
};

class Client {
public:
    Client(std::string url, std::string token);

    const std::string& url() const { return url_; }
    const std::string& token() const { return token_; }

    // GET /v1/health: the server answers; billing_url is the web page to make
    // an account and pay the plan (empty when the server has none).
    bool health(std::string& billing_url, Failure& error);
    // POST /v1/auth/login: this device's token, kept by the client too.
    bool login(const std::string& email, const std::string& password, const std::string& device, std::string& token,
               Failure& error);
    bool logout(Failure& error);
    bool account(Account& account, Failure& error);
    // GET /v1/plans, with or without a session.
    bool plans(std::vector<Plan>& plans, Failure& error);

    // POST /v1/reports (cloud.md, rule 11): app, kind, title, description,
    // steps, expected, actual, app_version, os, log; with the account's token
    // when there is one. similar: how many reports of the same problem the
    // server has besides this one.
    bool report(const std::map<std::string, std::string>& fields, int& similar, Failure& error);
    // GET /v1/me/reports: the account's own reports (up to 100), the newest first.
    bool my_reports(std::vector<SentReport>& reports, Failure& error);

    // A job: kind "slice" or "fea", params a JSON object, the project (3MF).
    bool submit(const std::string& kind, const std::string& params_json, const std::string& project_path, Job& job,
                Failure& error);
    bool status(const std::string& job_id, Job& job, Failure& error);
    // GET /v1/jobs?limit=50 (cloud.md, rule 10): the account's latest jobs,
    // newest first.
    bool jobs(std::vector<Job>& jobs, Failure& error);
    bool cancel(const std::string& job_id, Failure& error);
    bool download(const std::string& job_id, const std::string& path, Failure& error);

    // Sends the job, waits for it (update gets its state: queued or running
    // and the progress) and downloads the result to result_path. cancel is
    // polled: the job is cancelled on the server too. On failure, error tells
    // why (the server's message when there is one).
    bool run(const std::string& kind, const std::string& params_json, const std::string& project_path,
             const std::string& result_path, Job& job, Failure& error, const std::function<void(const Job&)>& update,
             const std::atomic<bool>& cancel);

    // How often run() asks for the job's state (cloud.md, rule 8: every 2 s).
    int poll_ms = 2000;

private:
    std::string url_;
    std::string token_;
};

// For the tests: the text of an API error ("detail" as text, or the first
// validation error's "msg"), or the connection's error, or "HTTP <status>".
std::string api_error(const std::string& body, const std::string& error, int status);
// The job's fields from its JSON; the summary flattened with dots.
void parse_job(const std::string& json, Job& job);

} // namespace tisma::cloud
