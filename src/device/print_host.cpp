#include "device/print_host.hpp"

#include "io/checksum.hpp"
#include "net/digest.hpp"
#include "net/http.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <fstream>
#include <iterator>

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

namespace {

// PrusaLink: the request as is, and once more with Digest when it answers
// 401 with a challenge (RFC 7616).
net::Response prusalink_send(const HostConfig& host, net::Request r) {
    if (host.user.empty() && !host.api_key.empty())
        return net::perform(r); // older firmware: the API key alone
    net::Response first = net::perform(r);
    net::DigestChallenge challenge;
    if (first.status != 401 || !net::parse_digest_challenge(first.www_authenticate, challenge))
        return first;
    net::Url url;
    if (!net::parse_url(r.url, url))
        return first;
    static unsigned count = 0;
    const std::string cnonce = net::md5_hex(r.url + std::to_string(++count)).substr(0, 16);
    r.headers.push_back(
        {"Authorization", net::digest_authorization(challenge, host.user, host.password, r.method, url.path, cnonce, 1)});
    return net::perform(r);
}

net::Request prusalink_request(const HostConfig& host, const std::string& method, const std::string& path) {
    net::Request r;
    r.method = method;
    r.url = base_of(host) + path;
    if (!host.api_key.empty())
        r.headers.push_back({"X-Api-Key", host.api_key});
    r.timeout_s = host.timeout_s;
    r.max_bytes = 4u << 20;
    return r;
}

HostVersion prusalink_version(const HostConfig& host) {
    HostVersion out;
    const net::Response r = prusalink_send(host, prusalink_request(host, "GET", "/api/version"));
    static_cast<HostResult&>(out) = result_of(r);
    if (!out.ok)
        return out;
    const nlohmann::json j = nlohmann::json::parse(r.body, nullptr, false);
    if (!j.is_object() || !j.contains("api")) {
        out.ok = false;
        out.code = "bad_answer";
        out.error = "the address answers, but not as PrusaLink";
        return out;
    }
    out.api = j.value("api", "");
    out.server = j.value("text", std::string("PrusaLink")) + " " + j.value("server", std::string());
    while (!out.server.empty() && out.server.back() == ' ')
        out.server.pop_back();
    return out;
}

HostResult prusalink_upload(const HostConfig& host, const std::string& path, const std::string& name, bool start) {
    net::Request r = prusalink_request(host, "PUT", "/api/v1/files/usb/" + name);
    r.timeout_s = std::max(host.timeout_s, 600);
    std::ifstream in(path, std::ios::binary);
    if (!in)
        return {false, "failed", "cannot read '" + path + "'"};
    r.body.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    r.headers.push_back({"Content-Type", "application/octet-stream"});
    r.headers.push_back({"Overwrite", "?1"});
    r.headers.push_back({"Print-After-Upload", start ? "?1" : "?0"});
    return result_of(prusalink_send(host, r));
}

HostStatus prusalink_status(const HostConfig& host) {
    HostStatus out;
    const net::Response r = prusalink_send(host, prusalink_request(host, "GET", "/api/v1/status"));
    static_cast<HostResult&>(out) = result_of(r);
    if (!out.ok)
        return out;
    const nlohmann::json j = nlohmann::json::parse(r.body, nullptr, false);
    if (!j.is_object() || !j.contains("printer") || !j["printer"].is_object()) {
        out.ok = false;
        out.code = "bad_answer";
        out.error = "the printer sent no state";
        return out;
    }
    const nlohmann::json& p = j["printer"];
    out.state = p.value("state", std::string());
    out.printing = out.state == "PRINTING" || out.state == "PAUSED";
    out.paused = out.state == "PAUSED";
    out.nozzle = number(p, "temp_nozzle");
    out.nozzle_target = number(p, "target_nozzle");
    out.bed = number(p, "temp_bed");
    out.bed_target = number(p, "target_bed");
    if (j.contains("job") && j["job"].is_object()) {
        out.progress = number(j["job"], "progress");
        out.seconds_left = number(j["job"], "time_remaining");
    }
    // The file's name is the job's, not the status' (the job id comes from there too).
    const net::Response job = prusalink_send(host, prusalink_request(host, "GET", "/api/v1/job"));
    if (job.ok()) {
        const nlohmann::json k = nlohmann::json::parse(job.body, nullptr, false);
        if (k.is_object() && k.contains("file") && k["file"].is_object())
            out.file = k["file"].value("display_name", k["file"].value("name", std::string()));
    }
    return out;
}

// The current job's id, which pause, resume and stop name (from /api/v1/job).
long prusalink_job(const HostConfig& host, HostResult& result) {
    const net::Response r = prusalink_send(host, prusalink_request(host, "GET", "/api/v1/job"));
    result = result_of(r);
    if (!result.ok)
        return -1;
    if (r.status == 204) {
        result = {false, "conflict", "the printer has no job"};
        return -1;
    }
    const nlohmann::json j = nlohmann::json::parse(r.body, nullptr, false);
    if (!j.is_object() || !j.contains("id") || !j["id"].is_number_integer()) {
        result = {false, "bad_answer", "the printer sent no job id"};
        return -1;
    }
    return j["id"].get<long>();
}

HostResult prusalink_job_call(const HostConfig& host, const std::string& method, const std::string& action) {
    HostResult result;
    const long id = prusalink_job(host, result);
    if (id < 0)
        return result;
    return result_of(prusalink_send(host, prusalink_request(host, method, "/api/v1/job/" + std::to_string(id) + action)));
}

} // namespace

namespace {

// Duet (RepRapFirmware, standalone): a session per call. rr_connect with the
// board's password; from RRF 3.5 the session key it gives goes in every
// request after it (X-Session-Key); rr_disconnect at the end.
std::string url_part(const std::string& s) {
    std::string out;
    for (const unsigned char c : s) {
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~' || c == '/' || c == ':')
            out += char(c);
        else {
            char hex[4];
            std::snprintf(hex, sizeof hex, "%%%02X", c);
            out += hex;
        }
    }
    return out;
}

class DuetSession {
public:
    explicit DuetSession(const HostConfig& host) : host_(host) {
        const std::string password = host.password.empty() ? "reprap" : host.password;
        const net::Response r = net::perform(make("GET", "/rr_connect?password=" + url_part(password) + "&sessionKey=yes"));
        result = result_of(r);
        if (!result.ok)
            return;
        const nlohmann::json j = nlohmann::json::parse(r.body, nullptr, false);
        if (!j.is_object() || !j.contains("err")) {
            result = {false, "bad_answer", "the address answers, but not as a Duet"};
            return;
        }
        const int err = j.value("err", -1);
        if (err == 1) {
            result = {false, "unauthorized", "the Duet refused the password"};
            return;
        }
        if (err != 0) {
            result = {false, "not_connected", "the Duet has no free session; close another browser tab or program"};
            return;
        }
        if (j.contains("sessionKey") && j["sessionKey"].is_number_integer())
            key_ = std::to_string(j["sessionKey"].get<long long>());
        open_ = true;
    }
    ~DuetSession() {
        if (open_)
            net::perform(make("GET", "/rr_disconnect"));
    }
    DuetSession(const DuetSession&) = delete;
    DuetSession& operator=(const DuetSession&) = delete;

    net::Request make(const std::string& method, const std::string& path) const {
        net::Request r;
        r.method = method;
        r.url = base_of(host_) + path;
        if (!key_.empty())
            r.headers.push_back({"X-Session-Key", key_});
        r.timeout_s = host_.timeout_s;
        r.max_bytes = 4u << 20;
        return r;
    }
    // An object model branch («state», «job»…): its «result», or null.
    nlohmann::json model(const std::string& key, const std::string& flags, HostResult& out) const {
        const net::Response r = net::perform(make("GET", "/rr_model?key=" + url_part(key) + "&flags=" + flags));
        out = result_of(r);
        if (!out.ok)
            return nullptr;
        const nlohmann::json j = nlohmann::json::parse(r.body, nullptr, false);
        if (!j.is_object() || !j.contains("result")) {
            out = {false, "bad_answer", "the Duet sent no object model"};
            return nullptr;
        }
        return j["result"];
    }
    // A G-code line, as the board runs it.
    HostResult gcode(const std::string& line) const { return result_of(net::perform(make("GET", "/rr_gcode?gcode=" + url_part(line)))); }

    HostResult result;

private:
    const HostConfig& host_;
    std::string key_;
    bool open_ = false;
};

HostVersion duet_version(const HostConfig& host) {
    HostVersion out;
    DuetSession s(host);
    static_cast<HostResult&>(out) = s.result;
    if (!out.ok)
        return out;
    const nlohmann::json b = s.model("boards[0]", "", out);
    if (!out.ok)
        return out;
    if (!b.is_object()) {
        out = {};
        out.code = "bad_answer";
        out.error = "the Duet sent no board";
        return out;
    }
    out.server = b.value("firmwareName", std::string("RepRapFirmware")) + " " + b.value("firmwareVersion", std::string());
    if (b.contains("name") && b["name"].is_string())
        out.server += " (" + b["name"].get<std::string>() + ")";
    out.api = "rr_";
    return out;
}

HostResult duet_upload(const HostConfig& host, const std::string& path, const std::string& name, bool start) {
    std::ifstream in(path, std::ios::binary);
    if (!in)
        return {false, "failed", "cannot read the G-code to send"};
    const std::string data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    DuetSession s(host);
    if (!s.result.ok)
        return s.result;
    const std::string target = "0:/gcodes/" + name;
    char crc[16];
    std::snprintf(crc, sizeof crc, "%08x",
                  unsigned(crc32(reinterpret_cast<const std::uint8_t*>(data.data()), data.size())));
    net::Request r = s.make("POST", "/rr_upload?name=" + url_part(target) + "&crc32=" + crc);
    r.timeout_s = std::max(host.timeout_s, 600);
    r.headers.push_back({"Content-Type", "application/octet-stream"});
    r.body = data;
    const net::Response answer = net::perform(r);
    HostResult out = result_of(answer);
    if (!out.ok)
        return out;
    const nlohmann::json j = nlohmann::json::parse(answer.body, nullptr, false);
    if (!j.is_object() || j.value("err", 1) != 0)
        return {false, "failed", "the Duet did not keep the file (a check of the upload failed)"};
    if (start)
        return s.gcode("M32 \"" + target + "\"");
    return out;
}

double heater_value(const nlohmann::json& heaters, const nlohmann::json& index, const char* key) {
    if (!index.is_number_integer() || !heaters.is_array())
        return -1.0;
    const long i = index.get<long>();
    if (i < 0 || std::size_t(i) >= heaters.size())
        return -1.0;
    return number(heaters[std::size_t(i)], key);
}

HostStatus duet_status(const HostConfig& host) {
    HostStatus out;
    DuetSession s(host);
    static_cast<HostResult&>(out) = s.result;
    if (!out.ok)
        return out;
    HostResult r;
    const nlohmann::json state = s.model("state", "f", r);
    if (!r.ok) {
        static_cast<HostResult&>(out) = r;
        return out;
    }
    out.state = state.is_object() ? state.value("status", std::string()) : std::string();
    out.paused = out.state == "paused" || out.state == "pausing";
    out.printing = out.paused || out.state == "processing" || out.state == "resuming" || out.state == "cancelling";
    const nlohmann::json job = s.model("job", "f", r);
    if (r.ok && job.is_object()) {
        if (job.contains("file") && job["file"].is_object()) {
            const nlohmann::json& f = job["file"];
            if (f.contains("fileName") && f["fileName"].is_string())
                out.file = f["fileName"].get<std::string>();
            const double size = number(f, "size"), at = number(job, "filePosition");
            if (size > 0.0 && at >= 0.0)
                out.progress = 100.0 * at / size;
        }
        if (job.contains("timesLeft") && job["timesLeft"].is_object()) {
            out.seconds_left = number(job["timesLeft"], "slicer");
            if (out.seconds_left < 0.0)
                out.seconds_left = number(job["timesLeft"], "file");
        }
    }
    // The heaters: the first tool's first heater, and the first bed's.
    const nlohmann::json heat = s.model("heat", "o", r);
    const nlohmann::json tools = s.model("tools", "", r);
    if (heat.is_object() && heat.contains("heaters")) {
        const nlohmann::json& heaters = heat["heaters"];
        if (tools.is_array() && !tools.empty() && tools[0].is_object() && tools[0].contains("heaters") &&
            tools[0]["heaters"].is_array() && !tools[0]["heaters"].empty()) {
            out.nozzle = heater_value(heaters, tools[0]["heaters"][0], "current");
            out.nozzle_target = heater_value(heaters, tools[0]["heaters"][0], "active");
        }
        nlohmann::json bed;
        if (heat.contains("bedHeaters") && heat["bedHeaters"].is_array() && !heat["bedHeaters"].empty())
            bed = heat["bedHeaters"][0];
        if (bed.is_array())
            bed = bed.empty() ? nlohmann::json() : bed[0];
        out.bed = heater_value(heaters, bed, "current");
        out.bed_target = heater_value(heaters, bed, "active");
    }
    return out;
}

// Pause M25, resume M24; cancel: a running job is paused first, and M0 on a
// paused job cancels it (runs the board's cancel.g).
HostResult duet_pause(const HostConfig& host, bool pause) {
    DuetSession s(host);
    if (!s.result.ok)
        return s.result;
    return s.gcode(pause ? "M25" : "M24");
}

HostResult duet_cancel(const HostConfig& host) {
    DuetSession s(host);
    if (!s.result.ok)
        return s.result;
    HostResult r;
    const nlohmann::json state = s.model("state", "f", r);
    if (!r.ok)
        return r;
    const std::string status = state.is_object() ? state.value("status", std::string()) : std::string();
    if (status != "paused") {
        r = s.gcode("M25");
        if (!r.ok)
            return r;
    }
    return s.gcode("M0");
}

} // namespace

bool host_kind_from(const std::string& word, HostKind& out) {
    if (word == "octoprint")
        out = HostKind::OctoPrint;
    else if (word == "moonraker")
        out = HostKind::Moonraker;
    else if (word == "prusalink")
        out = HostKind::PrusaLink;
    else if (word == "duet")
        out = HostKind::Duet;
    else
        return false;
    return true;
}

const char* host_kind_word(HostKind kind) {
    switch (kind) {
    case HostKind::Moonraker: return "moonraker";
    case HostKind::PrusaLink: return "prusalink";
    case HostKind::Duet: return "duet";
    default: return "octoprint";
    }
}

const char* host_kind_name(HostKind kind) {
    switch (kind) {
    case HostKind::Moonraker: return "Klipper (Moonraker)";
    case HostKind::PrusaLink: return "PrusaLink";
    case HostKind::Duet: return "Duet";
    default: return "OctoPrint";
    }
}

HostVersion host_version(const HostConfig& host) {
    if (host.kind == HostKind::Moonraker)
        return moonraker_version(host);
    if (host.kind == HostKind::PrusaLink)
        return prusalink_version(host);
    if (host.kind == HostKind::Duet)
        return duet_version(host);
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
    if (host.kind == HostKind::PrusaLink)
        return prusalink_upload(host, path, name, start);
    if (host.kind == HostKind::Duet)
        return duet_upload(host, path, name, start);
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
    if (host.kind == HostKind::PrusaLink)
        return prusalink_status(host);
    if (host.kind == HostKind::Duet)
        return duet_status(host);
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
    if (host.kind == HostKind::PrusaLink)
        return prusalink_job_call(host, "PUT", pause ? "/pause" : "/resume");
    if (host.kind == HostKind::Duet)
        return duet_pause(host, pause);
    net::Request r = request(host, "POST", "/api/job");
    r.headers.push_back({"Content-Type", "application/json"});
    r.body = nlohmann::json{{"command", "pause"}, {"action", pause ? "pause" : "resume"}}.dump();
    return result_of(net::perform(r));
}

HostResult host_cancel(const HostConfig& host) {
    if (host.kind == HostKind::Moonraker)
        return result_of(net::perform(request(host, "POST", "/printer/print/cancel")));
    if (host.kind == HostKind::PrusaLink)
        return prusalink_job_call(host, "DELETE", "");
    if (host.kind == HostKind::Duet)
        return duet_cancel(host);
    net::Request r = request(host, "POST", "/api/job");
    r.headers.push_back({"Content-Type", "application/json"});
    r.body = R"({"command": "cancel"})";
    return result_of(net::perform(r));
}

} // namespace tisma::device
