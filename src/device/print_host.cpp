#include "device/print_host.hpp"

#include "net/http.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>

namespace tisma::device {

namespace {

std::string base_of(const HostConfig& host) {
    std::string b = host.url;
    while (!b.empty() && b.back() == '/')
        b.pop_back();
    return b;
}

net::Request request(const HostConfig& host, const std::string& method, const std::string& path) {
    net::Request r;
    r.method = method;
    r.url = base_of(host) + path;
    r.headers.push_back({"X-Api-Key", host.api_key});
    r.timeout_s = host.timeout_s;
    r.max_bytes = 4u << 20;
    return r;
}

// The answer's status as a result: 401/403 the key, 409 the printer not
// connected to the program (OctoPrint: «the printer is not operational»).
HostResult result_of(const net::Response& r) {
    HostResult out;
    if (!r.error.empty() || r.status == 0) {
        out.code = "unreachable";
        out.error = "cannot reach the printer's program: " + (r.error.empty() ? std::string("no answer") : r.error);
    } else if (r.status == 401 || r.status == 403) {
        out.code = "unauthorized";
        out.error = "the printer's program refused the API key";
    } else if (r.status == 409) {
        out.code = "not_connected";
        out.error = "the printer's program has no printer connected, or it is busy";
    } else if (r.status < 200 || r.status >= 300) {
        out.code = "failed";
        out.error = "the printer's program answered " + std::to_string(r.status);
    } else {
        out.ok = true;
    }
    return out;
}

double number(const nlohmann::json& j, const char* key) {
    return j.is_object() && j.contains(key) && j.at(key).is_number() ? j.at(key).get<double>() : -1.0;
}

} // namespace

namespace {

// Moonraker (Klipper): JSON answers wrapped in {"result": …}.
nlohmann::json moonraker_result(const net::Response& r) {
    const nlohmann::json j = nlohmann::json::parse(r.body, nullptr, false);
    return j.is_object() && j.contains("result") ? j["result"] : nlohmann::json();
}

HostVersion moonraker_version(const HostConfig& host) {
    HostVersion out;
    const net::Response r = net::perform(request(host, "GET", "/server/info"));
    static_cast<HostResult&>(out) = result_of(r);
    if (!out.ok)
        return out;
    const nlohmann::json j = moonraker_result(r);
    if (!j.is_object() || !j.contains("klippy_state")) {
        out.ok = false;
        out.code = "bad_answer";
        out.error = "the address answers, but not as Moonraker";
        return out;
    }
    out.api = j.value("api_version_string", std::string());
    out.server = "Moonraker " + j.value("moonraker_version", std::string()) + " (Klipper " + j.value("klippy_state", "") + ")";
    return out;
}

HostResult moonraker_upload(const HostConfig& host, const std::string& path, const std::string& name, bool start) {
    net::Request r = request(host, "POST", "/server/files/upload");
    r.timeout_s = std::max(host.timeout_s, 600);
    r.form.push_back({"file", "", path, name, "application/octet-stream"});
    r.form.push_back({"root", "gcodes", "", "", "text/plain"});
    if (start)
        r.form.push_back({"print", "true", "", "", "text/plain"});
    return result_of(net::perform(r));
}

HostStatus moonraker_status(const HostConfig& host) {
    HostStatus out;
    const net::Response r =
        net::perform(request(host, "GET", "/printer/objects/query?print_stats&virtual_sdcard&extruder&heater_bed"));
    static_cast<HostResult&>(out) = result_of(r);
    if (!out.ok)
        return out;
    const nlohmann::json j = moonraker_result(r);
    if (!j.is_object() || !j.contains("status") || !j["status"].is_object()) {
        out.ok = false;
        out.code = "bad_answer";
        out.error = "the printer's program sent no printer state";
        return out;
    }
    const nlohmann::json& st = j["status"];
    if (st.contains("print_stats") && st["print_stats"].is_object()) {
        out.state = st["print_stats"].value("state", std::string());
        out.file = st["print_stats"].value("filename", std::string());
    }
    out.printing = out.state == "printing" || out.state == "paused";
    out.paused = out.state == "paused";
    if (st.contains("virtual_sdcard")) {
        const double p = number(st["virtual_sdcard"], "progress");
        out.progress = p >= 0.0 ? 100.0 * p : -1.0;
    }
    if (st.contains("extruder")) {
        out.nozzle = number(st["extruder"], "temperature");
        out.nozzle_target = number(st["extruder"], "target");
    }
    if (st.contains("heater_bed")) {
        out.bed = number(st["heater_bed"], "temperature");
        out.bed_target = number(st["heater_bed"], "target");
    }
    return out;
}

} // namespace

HostVersion host_version(const HostConfig& host) {
    if (host.kind == HostKind::Moonraker)
        return moonraker_version(host);
    HostVersion out;
    const net::Response r = net::perform(request(host, "GET", "/api/version"));
    static_cast<HostResult&>(out) = result_of(r);
    if (!out.ok)
        return out;
    const nlohmann::json j = nlohmann::json::parse(r.body, nullptr, false);
    if (!j.is_object() || !j.contains("api")) {
        out.ok = false;
        out.code = "bad_answer";
        out.error = "the address answers, but not as OctoPrint";
        return out;
    }
    out.api = j.value("api", "");
    out.server = j.value("text", "OctoPrint " + j.value("server", std::string()));
    return out;
}

HostResult host_upload(const HostConfig& host, const std::string& path, const std::string& name, bool start) {
    if (host.kind == HostKind::Moonraker)
        return moonraker_upload(host, path, name, start);
    net::Request r = request(host, "POST", "/api/files/local");
    r.timeout_s = std::max(host.timeout_s, 600); // a large file on a slow network
    r.form.push_back({"file", "", path, name, "application/octet-stream"});
    r.form.push_back({"select", start ? "true" : "false", "", "", "text/plain"});
    r.form.push_back({"print", start ? "true" : "false", "", "", "text/plain"});
    return result_of(net::perform(r));
}

HostStatus host_status(const HostConfig& host) {
    if (host.kind == HostKind::Moonraker)
        return moonraker_status(host);
    HostStatus out;
    const net::Response job = net::perform(request(host, "GET", "/api/job"));
    static_cast<HostResult&>(out) = result_of(job);
    if (!out.ok)
        return out;
    const nlohmann::json j = nlohmann::json::parse(job.body, nullptr, false);
    if (!j.is_object()) {
        out.ok = false;
        out.code = "bad_answer";
        out.error = "the printer's program sent no job state";
        return out;
    }
    out.state = j.value("state", "");
    out.printing = out.state.rfind("Printing", 0) == 0 || out.state == "Pausing" || out.state == "Cancelling";
    out.paused = out.state == "Paused" || out.state == "Pausing";
    if (j.contains("job") && j["job"].is_object() && j["job"].contains("file") && j["job"]["file"].is_object())
        out.file = j["job"]["file"].value("name", std::string());
    if (j.contains("progress")) {
        out.progress = number(j["progress"], "completion");
        out.seconds_left = number(j["progress"], "printTimeLeft");
    }
    // The temperatures: 409 when no printer is connected, which is not an error here.
    const net::Response printer = net::perform(request(host, "GET", "/api/printer"));
    if (printer.ok()) {
        const nlohmann::json p = nlohmann::json::parse(printer.body, nullptr, false);
        if (p.is_object() && p.contains("temperature") && p["temperature"].is_object()) {
            const nlohmann::json& t = p["temperature"];
            if (t.contains("tool0")) {
                out.nozzle = number(t["tool0"], "actual");
                out.nozzle_target = number(t["tool0"], "target");
            }
            if (t.contains("bed")) {
                out.bed = number(t["bed"], "actual");
                out.bed_target = number(t["bed"], "target");
            }
        }
    }
    return out;
}

HostResult host_pause(const HostConfig& host, bool pause) {
    if (host.kind == HostKind::Moonraker)
        return result_of(net::perform(request(host, "POST", pause ? "/printer/print/pause" : "/printer/print/resume")));
    net::Request r = request(host, "POST", "/api/job");
    r.headers.push_back({"Content-Type", "application/json"});
    r.body = nlohmann::json{{"command", "pause"}, {"action", pause ? "pause" : "resume"}}.dump();
    return result_of(net::perform(r));
}

HostResult host_cancel(const HostConfig& host) {
    if (host.kind == HostKind::Moonraker)
        return result_of(net::perform(request(host, "POST", "/printer/print/cancel")));
    net::Request r = request(host, "POST", "/api/job");
    r.headers.push_back({"Content-Type", "application/json"});
    r.body = R"({"command": "cancel"})";
    return result_of(net::perform(r));
}

} // namespace tisma::device
