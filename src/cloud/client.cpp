#include "cloud/client.hpp"

#include "net/http.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <thread>

// The owner's code (see client.hpp and PROVENANCE.md), with the engine's HTTP
// layer in place of PrusaSlicer's.

namespace tisma::cloud {

namespace {

// The largest result downloaded (the server keeps them up to 1 GB).
constexpr std::size_t kMaxResultBytes = std::size_t(1) << 30;

std::string json_text(const nlohmann::json& v) { return v.is_string() ? v.get<std::string>() : v.dump(); }

void flatten(const nlohmann::json& j, const std::string& prefix, std::map<std::string, std::string>& out) {
    for (auto it = j.begin(); it != j.end(); ++it) {
        if (it->is_object())
            flatten(*it, prefix + it.key() + ".", out);
        else if (!it->is_null())
            out[prefix + it.key()] = json_text(*it);
    }
}

void parse_job_json(const nlohmann::json& j, Job& job) {
    const auto text = [&](const char* key) {
        return j.contains(key) && j[key].is_string() ? j[key].get<std::string>() : std::string();
    };
    // The server's ids are text; a number is taken as its digits.
    job.id = j.contains("id") && j["id"].is_number_integer() ? std::to_string(j["id"].get<long long>()) : text("id");
    job.kind = text("kind");
    job.status = text("status");
    job.created_at = text("created_at");
    job.progress = j.contains("progress") && j["progress"].is_number() ? int(j["progress"].get<double>()) : 0;
    job.error = j.contains("error") && j["error"].is_string() ? j["error"].get<std::string>() : std::string();
    job.result_filename =
        j.contains("result_filename") && j["result_filename"].is_string() ? j["result_filename"].get<std::string>() : "";
    job.seconds = j.contains("seconds") && j["seconds"].is_number() ? j["seconds"].get<double>() : 0.0;
    job.summary.clear();
    if (j.contains("summary") && j["summary"].is_object())
        flatten(j["summary"], std::string(), job.summary);
}

// A failure from an answer that did not come or was not 2xx.
Failure failure(const net::Response& r) { return {r.status, api_error(r.body, r.error, r.status)}; }

Failure unexpected() { return {0, "Unexpected answer of the server"}; }

} // namespace

std::string job_error(const Job& job, bool spanish) {
    if (!spanish)
        return job.error;
    if (const auto it = job.summary.find("error_es"); it != job.summary.end() && !it->second.empty())
        return it->second;
    static const std::pair<const char*, const char*> kByCode[] = {
        {"bad_mode", "el modo de análisis no es válido"},
        {"bad_quality", "la calidad no es válida"},
        {"not_in_cloud", "este análisis todavía no corre en la nube"},
        {"needs_conditions", "el análisis necesita un punto o zona fija y una fuerza (marcalos en Ingeniería)"},
        {"no_valid_result", "el análisis no dio un resultado válido"}};
    if (const auto it = job.summary.find("code"); it != job.summary.end())
        for (const auto& [code, text] : kByCode)
            if (it->second == code)
                return text;
    return job.error;
}

std::string api_error(const std::string& body, const std::string& error, int status) {
    try {
        const nlohmann::json j = nlohmann::json::parse(body);
        if (j.is_object() && j.contains("detail")) {
            const nlohmann::json& d = j["detail"];
            if (d.is_string())
                return d.get<std::string>();
            // Validation errors: a list of {msg, ...}.
            if (d.is_array() && !d.empty() && d[0].is_object() && d[0].contains("msg") && d[0]["msg"].is_string())
                return d[0]["msg"].get<std::string>();
        }
    } catch (const std::exception&) {
    }
    if (!error.empty())
        return error;
    return "HTTP " + std::to_string(status);
}

void parse_job(const std::string& json, Job& job) { parse_job_json(nlohmann::json::parse(json), job); }

Client::Client(std::string url, std::string token) : url_(std::move(url)), token_(std::move(token)) {
    while (!url_.empty() && url_.back() == '/')
        url_.pop_back();
}

namespace {

// A request to the API, with the token when there is one.
net::Request api(const std::string& method, const std::string& url, const std::string& token, int timeout_s) {
    net::Request r;
    r.method = method;
    r.url = url;
    r.connect_timeout_s = 10;
    r.timeout_s = timeout_s;
    if (!token.empty())
        r.headers.emplace_back("Authorization", "Bearer " + token);
    return r;
}

net::Request json_post(const std::string& url, const std::string& token, const nlohmann::json& body, int timeout_s) {
    net::Request r = api("POST", url, token, timeout_s);
    r.headers.emplace_back("Content-Type", "application/json");
    r.body = body.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace);
    return r;
}

} // namespace

bool Client::health(std::string& billing_url, Failure& error) {
    const net::Response r = net::perform(api("GET", url_ + "/v1/health", "", 20));
    if (!r.ok()) {
        error = failure(r);
        return false;
    }
    try {
        const nlohmann::json j = nlohmann::json::parse(r.body);
        billing_url = j.contains("billing_url") && j["billing_url"].is_string() ? j["billing_url"].get<std::string>() : "";
        if (j.value("ok", false))
            return true;
    } catch (const std::exception&) {
    }
    error = unexpected();
    return false;
}

bool Client::login(const std::string& email, const std::string& password, const std::string& device,
                   std::string& token, Failure& error) {
    const nlohmann::json body = {{"email", email}, {"password", password}, {"device", device}};
    const net::Response r = net::perform(json_post(url_ + "/v1/auth/login", "", body, 60));
    if (!r.ok()) {
        error = failure(r);
        return false;
    }
    try {
        token = nlohmann::json::parse(r.body).at("token").get<std::string>();
    } catch (const std::exception&) {
        error = unexpected();
        return false;
    }
    token_ = token;
    return true;
}

bool Client::logout(Failure& error) {
    const net::Response r = net::perform(json_post(url_ + "/v1/auth/logout", token_, nlohmann::json::object(), 30));
    if (!r.ok()) {
        error = failure(r);
        return false;
    }
    token_.clear();
    return true;
}

bool Client::account(Account& account, Failure& error) {
    const net::Response r = net::perform(api("GET", url_ + "/v1/me", token_, 30));
    if (!r.ok()) {
        error = failure(r);
        return false;
    }
    try {
        const nlohmann::json j = nlohmann::json::parse(r.body);
        account.email = j.value("email", std::string());
        account.plan = j.value("plan", std::string());
        account.paid_until = j.contains("paid_until") && j["paid_until"].is_string() ? j["paid_until"].get<std::string>()
                                                                                    : std::string();
        account.paid = j.value("paid", false);
        account.hours_included = j.value("hours_included", 0.0);
        account.hours_used = j.value("hours_used", 0.0);
        account.hours_left = j.value("hours_left", 0.0);
        account.active_jobs = j.value("active_jobs", 0);
        account.max_concurrent = j.value("max_concurrent", 0);
        return true;
    } catch (const std::exception&) {
        error = unexpected();
        return false;
    }
}

bool Client::plans(std::vector<Plan>& plans, Failure& error) {
    const net::Response r = net::perform(api("GET", url_ + "/v1/plans", token_, 30));
    if (!r.ok()) {
        error = failure(r);
        return false;
    }
    try {
        const nlohmann::json j = nlohmann::json::parse(r.body);
        // A list, or {"plans": [...]}.
        const nlohmann::json& list = j.is_object() && j.contains("plans") ? j["plans"] : j;
        if (!list.is_array()) {
            error = unexpected();
            return false;
        }
        plans.clear();
        for (const nlohmann::json& p : list) {
            if (!p.is_object())
                continue;
            Plan plan;
            plan.name = p.value("name", std::string());
            plan.monthly_hours = p.contains("monthly_hours") && p["monthly_hours"].is_number() ? p["monthly_hours"].get<double>() : 0.0;
            plan.max_concurrent = p.value("max_concurrent", 0);
            plan.price_cents = p.contains("price_cents") && p["price_cents"].is_number() ? p["price_cents"].get<long long>() : 0;
            plan.currency = p.value("currency", std::string());
            plan.remote_printers = p.value("remote_printers", 0);
            plans.push_back(std::move(plan));
        }
        return true;
    } catch (const std::exception&) {
        error = unexpected();
        return false;
    }
}

bool Client::report(const std::map<std::string, std::string>& fields, int& similar, Failure& error) {
    nlohmann::json body = nlohmann::json::object();
    for (const auto& [key, value] : fields)
        body[key] = value;
    const net::Response r = net::perform(json_post(url_ + "/v1/reports", token_, body, 60));
    if (!r.ok()) {
        error = failure(r);
        return false;
    }
    try {
        similar = nlohmann::json::parse(r.body).value("similar", 0);
        return true;
    } catch (const std::exception&) {
        error = unexpected();
        return false;
    }
}

bool Client::submit(const std::string& kind, const std::string& params_json, const std::string& project_path, Job& job,
                    Failure& error) {
    net::Request req = api("POST", url_ + "/v1/jobs", token_, 1800);
    req.form.push_back({"kind", kind, "", "", "text/plain"});
    req.form.push_back({"params", params_json, "", "", "application/json"});
    req.form.push_back({"file", "", project_path, "project.3mf", "application/vnd.ms-package.3dmanufacturing-3dmodel+xml"});
    const net::Response r = net::perform(req);
    if (!r.ok()) {
        error = failure(r);
        return false;
    }
    try {
        parse_job(r.body, job);
        if (!job.id.empty())
            return true;
    } catch (const std::exception&) {
    }
    error = unexpected();
    return false;
}

bool Client::my_reports(std::vector<SentReport>& reports, Failure& error) {
    const net::Response r = net::perform(api("GET", url_ + "/v1/me/reports", token_, 30));
    if (!r.ok()) {
        error = failure(r);
        return false;
    }
    try {
        const nlohmann::json j = nlohmann::json::parse(r.body);
        // {"reports": [...]}, newest first, at most 100; a bare list is read too.
        const nlohmann::json& list = j.is_object() && j.contains("reports") ? j["reports"] : j;
        if (!list.is_array()) {
            error = unexpected();
            return false;
        }
        const auto text = [](const nlohmann::json& o, const char* key) {
            if (!o.contains(key))
                return std::string();
            const nlohmann::json& v = o[key];
            return v.is_string() ? v.get<std::string>() : v.is_number_integer() ? std::to_string(v.get<long long>()) : "";
        };
        reports.clear();
        for (const nlohmann::json& item : list) {
            if (!item.is_object())
                continue;
            reports.push_back({text(item, "id"), text(item, "created_at"), text(item, "title"), text(item, "status")});
        }
        // The newest first (rule 11, IP10), as the server sends them; kept so
        // if it ever does not (ISO dates sort as text).
        std::stable_sort(reports.begin(), reports.end(),
                         [](const SentReport& a, const SentReport& b) { return a.created_at > b.created_at; });
        return true;
    } catch (const std::exception&) {
        error = unexpected();
        return false;
    }
}

bool Client::jobs(std::vector<Job>& jobs, Failure& error) {
    // The latest 50 (the server's default; up to 200), newest first.
    const net::Response r = net::perform(api("GET", url_ + "/v1/jobs?limit=50", token_, 30));
    if (!r.ok()) {
        error = failure(r);
        return false;
    }
    try {
        const nlohmann::json j = nlohmann::json::parse(r.body);
        // {"jobs": [...]}; a bare list is read too.
        const nlohmann::json& list = j.is_object() && j.contains("jobs") ? j["jobs"] : j;
        if (!list.is_array()) {
            error = unexpected();
            return false;
        }
        jobs.clear();
        for (const nlohmann::json& item : list) {
            if (!item.is_object())
                continue;
            Job job;
            parse_job_json(item, job);
            jobs.push_back(std::move(job));
        }
        return true;
    } catch (const std::exception&) {
        error = unexpected();
        return false;
    }
}

bool Client::status(const std::string& job_id, Job& job, Failure& error) {
    const net::Response r = net::perform(api("GET", url_ + "/v1/jobs/" + job_id, token_, 30));
    if (!r.ok()) {
        error = failure(r);
        return false;
    }
    try {
        parse_job(r.body, job);
        return true;
    } catch (const std::exception&) {
        error = unexpected();
        return false;
    }
}

bool Client::cancel(const std::string& job_id, Failure& error) {
    const net::Response r =
        net::perform(json_post(url_ + "/v1/jobs/" + job_id + "/cancel", token_, nlohmann::json::object(), 30));
    if (!r.ok()) {
        error = failure(r);
        return false;
    }
    return true;
}

bool Client::download(const std::string& job_id, const std::string& path, Failure& error) {
    net::Request req = api("GET", url_ + "/v1/jobs/" + job_id + "/result", token_, 1800);
    req.max_bytes = kMaxResultBytes;
    const net::Response r = net::perform(req);
    if (!r.ok()) {
        error = failure(r);
        return false;
    }
    std::ofstream out(std::filesystem::path(std::u8string(path.begin(), path.end())), std::ios::binary | std::ios::trunc);
    out.write(r.body.data(), std::streamsize(r.body.size()));
    if (!out.flush()) {
        error = {0, "Cannot write " + path};
        return false;
    }
    return true;
}

bool Client::run(const std::string& kind, const std::string& params_json, const std::string& project_path,
                 const std::string& result_path, Job& job, Failure& error, const std::function<void(const Job&)>& update,
                 const std::atomic<bool>& cancelled) {
    if (!submit(kind, params_json, project_path, job, error))
        return false;
    if (update)
        update(job);
    int failures = 0;
    while (!job.finished()) {
        // Wait, looking at cancel every 50 ms.
        for (int waited = 0; waited < poll_ms && !cancelled; waited += 50)
            std::this_thread::sleep_for(std::chrono::milliseconds(std::min(50, std::max(1, poll_ms - waited))));
        if (cancelled) {
            Failure ignored;
            cancel(job.id, ignored);
            error = {0, "Cancelled"};
            return false;
        }
        Failure err;
        if (!status(job.id, job, err)) {
            // A lost answer or two (network) is borne; a closed session is not.
            if (err.expired() || ++failures > 10) {
                error = err;
                return false;
            }
            continue;
        }
        failures = 0;
        if (update)
            update(job);
    }
    if (job.status != "done") {
        error = {0, job.status == "cancelled" ? std::string("Cancelled") : job.error};
        return false;
    }
    return download(job.id, result_path, error);
}

} // namespace tisma::cloud
