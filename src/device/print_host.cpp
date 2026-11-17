#include "device/print_host.hpp"

#include "device/flashforge.hpp"
#include "device/sdcp.hpp"
#include "net/websocket.hpp"

#include "io/checksum.hpp"
#include "net/digest.hpp"
#include "net/http.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <memory>
#include <random>
#include <thread>
#include <cctype>
#include <cstdlib>
#include <cstring>
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

std::string url_part(const std::string& s);

HostResult prusalink_upload(const HostConfig& host, const std::string& path, const std::string& name, bool start) {
    // The name in the path, percent-encoded: «Mi pieza #2.gcode» (BH53).
    net::Request r = prusalink_request(host, "PUT", "/api/v1/files/usb/" + url_part(name));
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

HostResult duet_upload(const HostConfig& host, const std::string& path, const std::string& name, bool start,
                       bool simulate) {
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
    if (simulate)
        return s.gcode("M37 P\"" + target + "\"");
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

namespace {

// AstroBox: OctoPrint's version and upload calls only (no published API for
// the rest).
HostResult unsupported(const char* what) {
    HostResult out;
    out.code = "unsupported";
    out.error = what;
    return out;
}

// Repetier-Server (its Web API): the commands go to /printer/api/<slug> as
// ?a=<command>&data=<URL-encoded JSON>, the key in x-api-key.
nlohmann::json repetier_json(const net::Response& r) {
    return nlohmann::json::parse(r.body, nullptr, false);
}

// The printer's slug: the one asked for, or the server's first printer.
std::string repetier_slug(const HostConfig& host, HostResult& result, std::string* server = nullptr) {
    const net::Response r = net::perform(request(host, "GET", "/printer/info"));
    result = result_of(r);
    if (!result.ok)
        return {};
    const nlohmann::json j = repetier_json(r);
    if (!j.is_object() || !j.contains("printers") || !j["printers"].is_array()) {
        result.ok = false;
        result.code = "bad_answer";
        result.error = "the address answers, but not as Repetier-Server";
        return {};
    }
    if (server)
        *server = j.value("name", std::string("Repetier-Server")) + " " + j.value("version", std::string());
    for (const nlohmann::json& p : j["printers"]) {
        const std::string slug = p.is_object() ? p.value("slug", std::string()) : std::string();
        if (!slug.empty() && (host.printer.empty() || slug == host.printer))
            return slug;
    }
    result.ok = false;
    result.code = "failed";
    result.error = host.printer.empty() ? "Repetier-Server has no printer"
                                        : "Repetier-Server has no printer named \"" + host.printer + "\"";
    return {};
}

net::Response repetier_command(const HostConfig& host, const std::string& slug, const std::string& command,
                               const nlohmann::json& data = nlohmann::json::object()) {
    return net::perform(request(host, "POST", "/printer/api/" + url_part(slug) + "?a=" + command +
                                                  "&data=" + url_part(data.dump())));
}

HostVersion repetier_version(const HostConfig& host) {
    HostVersion out;
    std::string server;
    const std::string slug = repetier_slug(host, out, &server);
    if (!out.ok)
        return out;
    // /printer/info needs no key: a command checks it.
    static_cast<HostResult&>(out) = result_of(repetier_command(host, slug, "listPrinter"));
    if (out.ok)
        out.server = server;
    return out;
}

HostResult repetier_upload(const HostConfig& host, const std::string& path, const std::string& name, bool start) {
    HostResult out;
    const std::string slug = repetier_slug(host, out);
    if (!out.ok)
        return out;
    // Into the jobs to print it now, else into the models (the file field is «filename»).
    net::Request r = request(host, "POST", std::string(start ? "/printer/job/" : "/printer/model/") + url_part(slug) +
                                               "?a=upload&name=" + url_part(name));
    r.timeout_s = std::max(host.timeout_s, 600);
    r.form.push_back({"filename", "", path, name, "application/octet-stream"});
    return result_of(net::perform(r));
}

double repetier_temp(const nlohmann::json& heater, const char* key) {
    return heater.is_object() ? number(heater, key) : -1.0;
}

HostStatus repetier_status(const HostConfig& host) {
    HostStatus out;
    const std::string slug = repetier_slug(host, out);
    if (!out.ok)
        return out;
    const net::Response list = repetier_command(host, slug, "listPrinter");
    static_cast<HostResult&>(out) = result_of(list);
    if (!out.ok)
        return out;
    const nlohmann::json printers = repetier_json(list);
    bool found = false;
    if (printers.is_array())
        for (const nlohmann::json& p : printers) {
            if (!p.is_object() || p.value("slug", std::string()) != slug)
                continue;
            found = true;
            const std::string job = p.value("job", std::string("none"));
            out.printing = !job.empty() && job != "none";
            out.file = out.printing ? job : std::string();
            out.state = p.value("online", 0) == 0 ? "Offline" : out.printing ? "Printing" : "Idle";
            if (p.contains("done"))
                out.progress = number(p, "done");
        }
    if (!found) {
        out.ok = false;
        out.code = "bad_answer";
        out.error = "Repetier-Server sent no state for the printer";
        return out;
    }
    const net::Response state = repetier_command(host, slug, "stateList");
    if (state.ok()) {
        nlohmann::json j = repetier_json(state);
        if (j.is_object() && j.contains(slug))
            j = j[slug];
        if (j.is_object()) {
            if (j.contains("extruder") && j["extruder"].is_array() && !j["extruder"].empty()) {
                out.nozzle = repetier_temp(j["extruder"][0], "tempRead");
                out.nozzle_target = repetier_temp(j["extruder"][0], "tempSet");
            }
            if (j.contains("heatedBed")) {
                out.bed = repetier_temp(j["heatedBed"], "tempRead");
                out.bed_target = repetier_temp(j["heatedBed"], "tempSet");
            }
        }
    }
    return out;
}

HostResult repetier_job(const HostConfig& host, const char* command) {
    HostResult out;
    const std::string slug = repetier_slug(host, out);
    if (!out.ok)
        return out;
    return result_of(repetier_command(host, slug, command));
}

} // namespace


namespace {

// Flashforge (printer_connections.md, «Flashforge en red local»; the
// community's flashforge-api-docs wiki, «HTTP REST API»).
// The address as a person types it: the IP alone gets http:// and port 8898.
std::string flashforge_base(const HostConfig& host) {
    std::string b = base_of(host);
    if (b.find("://") == std::string::npos)
        b = "http://" + b;
    const std::size_t host_at = b.find("://") + 3;
    if (b.find(':', host_at) == std::string::npos)
        b += ":8898";
    return b;
}

net::Request flashforge_request(const HostConfig& host, const std::string& path, const nlohmann::json& body) {
    net::Request r;
    r.method = "POST";
    r.url = flashforge_base(host) + path;
    nlohmann::json b = body;
    b["serialNumber"] = host.printer;
    b["checkCode"] = host.api_key;
    r.body = b.dump();
    r.headers.push_back({"Content-Type", "application/json"});
    r.timeout_s = host.timeout_s;
    r.connect_timeout_s = std::min(host.timeout_s, 10);
    r.max_bytes = 1u << 20;
    return r;
}

// The answer's «code» as a result, with the words of the spec.
HostResult flashforge_result(const HostConfig& host, const net::Response& r, nlohmann::json* answer = nullptr) {
    HostResult out = result_of(r);
    if (!out.ok) {
        if (out.code == "unreachable")
            out.error = "cannot reach the printer at " + flashforge_base(host) +
                        "; check that it is on, on the same network and in LAN-only mode";
        return out;
    }
    const nlohmann::json j = nlohmann::json::parse(r.body, nullptr, false);
    if (!j.is_object() || !j.contains("code") || !j["code"].is_number()) {
        out.ok = false;
        out.code = "bad_answer";
        out.error = "the address answers, but not as a Flashforge printer";
        return out;
    }
    const int code = j["code"].get<int>();
    out.ok = code == 0;
    if (code == 1) {
        out.code = "unauthorized";
        out.error = "wrong serial number or check code; see the printer's network mode screen";
    } else if (code == 2) {
        out.code = "conflict";
        out.error = "the printer is busy";
    } else if (code == 3) {
        out.code = "failed";
        out.error = "the file is not on the printer";
    } else if (code != 0) {
        out.code = "failed";
        out.error = "the printer refused the request";
    }
    if (answer)
        *answer = j;
    return out;
}

HostStatus flashforge_status(const HostConfig& host) {
    HostStatus out;
    nlohmann::json j;
    static_cast<HostResult&>(out) = flashforge_result(host, net::perform(flashforge_request(host, "/detail", nlohmann::json::object())), &j);
    if (!out.ok)
        return out;
    const nlohmann::json d = j.value("detail", nlohmann::json::object());
    out.state = d.value("status", std::string());
    // Its words (ready, busy, heating, printing, pausing, pause, canceling,
    // cancel, completed…); an unknown one counts as busy.
    out.printing = out.state == "printing" || out.state == "heating" || out.state == "pausing";
    out.paused = out.state == "pause";
    out.file = d.value("printFileName", std::string());
    const double progress = number(d, "printProgress");
    out.progress = progress >= 0.0 ? progress * 100.0 : -1.0; // 0 to 1
    out.seconds_left = number(d, "estimatedTime");
    out.nozzle = number(d, "rightTemp");
    out.nozzle_target = number(d, "rightTargetTemp");
    out.bed = number(d, "platTemp");
    out.bed_target = number(d, "platTargetTemp");
    return out;
}

HostVersion flashforge_version(const HostConfig& host) {
    HostVersion out;
    nlohmann::json j;
    static_cast<HostResult&>(out) = flashforge_result(host, net::perform(flashforge_request(host, "/detail", nlohmann::json::object())), &j);
    if (out.ok) {
        const nlohmann::json d = j.value("detail", nlohmann::json::object());
        out.server = "Flashforge " + d.value("name", std::string());
    }
    return out;
}

HostResult flashforge_upload(const HostConfig& host, const std::string& path, const std::string& name, bool start) {
    net::Request r;
    r.method = "POST";
    r.url = flashforge_base(host) + "/uploadGcode";
    std::error_code ec;
    const auto size = std::filesystem::file_size(path, ec);
    // The credentials go in headers here, the booleans as "true" or "false".
    r.headers = {{"serialNumber", host.printer},
                 {"checkCode", host.api_key},
                 {"fileSize", std::to_string(ec ? 0 : size)},
                 {"printNow", start ? "true" : "false"},
                 {"levelingBeforePrint", host.level_before_print ? "true" : "false"},
                 {"flowCalibration", "false"},
                 {"firstLayerInspection", "false"},
                 {"timeLapseVideo", "false"}};
    r.timeout_s = std::max(host.timeout_s, 600);
    r.form.push_back({"gcodeFile", "", path, name, "application/octet-stream"});
    return flashforge_result(host, net::perform(r));
}

// /control answers success even for an unknown command: the state read
// after it says whether it took (the spec).
HostResult flashforge_job(const HostConfig& host, const char* action) {
    const nlohmann::json body = {{"payload", {{"cmd", "jobCtl_cmd"}, {"args", {{"jobID", ""}, {"action", action}}}}}};
    HostResult out = flashforge_result(host, net::perform(flashforge_request(host, "/control", body)));
    if (!out.ok)
        return out;
    const std::string want = std::string(action) == "pause" ? "pause" : std::string(action) == "cancel" ? "cancel" : "";
    for (int i = 0; i < 10; ++i) {
        const HostStatus st = flashforge_status(host);
        if (!st.ok)
            return st;
        if (want.empty() ? st.state != "pause" && st.state != "pausing"
                         : st.state == want || st.state == want + "ing" || (want == "cancel" && st.state == "ready"))
            return out;
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
    }
    out.ok = false;
    out.code = "failed";
    out.error = std::string("the printer did not ") + action;
    return out;
}

} // namespace


namespace {

// The older Flashforge printers, Adventurer 3 and 4 (TCP 8899, «~» commands;
// device/flashforge.hpp). The address is the IP, with or without http://.
std::string legacy_host(const HostConfig& host) {
    std::string h = base_of(host);
    if (const std::size_t at = h.find("://"); at != std::string::npos)
        h = h.substr(at + 3);
    if (const std::size_t colon = h.find(':'); colon != std::string::npos)
        h = h.substr(0, colon);
    return h;
}
int legacy_port(const HostConfig& host) {
    const std::string b = base_of(host);
    const std::size_t at = b.find("://") == std::string::npos ? 0 : b.find("://") + 3;
    const std::size_t colon = b.find(':', at);
    return colon == std::string::npos ? 8899 : std::atoi(b.c_str() + colon + 1);
}

HostResult legacy_result(const FlashforgeSession& s) {
    HostResult out;
    out.ok = s.ok;
    out.code = s.code;
    out.error = s.error;
    return out;
}

// A labelled value of an answer: «MachineStatus: READY» → READY.
std::string legacy_field(const std::string& answer, const std::string& label) {
    const std::size_t at = answer.find(label);
    if (at == std::string::npos)
        return {};
    std::size_t from = at + label.size();
    while (from < answer.size() && answer[from] == ' ')
        ++from;
    std::string v = answer.substr(from, answer.find('\n', from) - from);
    while (!v.empty() && (v.back() == ' ' || v.back() == '\r'))
        v.pop_back();
    return v;
}

// «T0:210/210 B:55/55» (the older ones space them differently).
void legacy_temperature(const std::string& answer, const char* label, double& now, double& target) {
    const std::size_t at = answer.find(label);
    if (at == std::string::npos)
        return;
    const char* p = answer.c_str() + at + std::strlen(label);
    char* end = nullptr;
    now = std::strtod(p, &end);
    while (*end == ' ')
        ++end;
    if (*end == '/')
        target = std::strtod(end + 1, nullptr);
}

HostVersion legacy_version(const HostConfig& host) {
    HostVersion out;
    const FlashforgeSession s = flashforge_tcp(legacy_host(host), legacy_port(host), {"M115"}, host.timeout_s * 1000 / 4);
    static_cast<HostResult&>(out) = legacy_result(s);
    if (out.ok)
        out.server = "Flashforge " + legacy_field(s.answers.at(0), "Machine Type:") + " (" +
                     legacy_field(s.answers.at(0), "SN:") + ")";
    return out;
}

HostStatus legacy_status(const HostConfig& host) {
    HostStatus out;
    const FlashforgeSession s = flashforge_tcp(legacy_host(host), legacy_port(host), {"M119", "M105"}, host.timeout_s * 1000 / 4);
    static_cast<HostResult&>(out) = legacy_result(s);
    if (!out.ok)
        return out;
    out.state = legacy_field(s.answers.at(0), "MachineStatus:");
    out.printing = out.state == "BUILDING_FROM_SD";
    out.paused = out.state == "PAUSED";
    out.file = legacy_field(s.answers.at(0), "CurrentFile:");
    legacy_temperature(s.answers.at(1), "T0:", out.nozzle, out.nozzle_target);
    legacy_temperature(s.answers.at(1), "B:", out.bed, out.bed_target);
    return out;
}

HostResult legacy_command(const HostConfig& host, const char* command) {
    return legacy_result(flashforge_tcp(legacy_host(host), legacy_port(host), {command}, host.timeout_s * 1000 / 4));
}

} // namespace


namespace {

// Elegoo by SDCP v3 (printer_connections.md, «Impresoras Elegoo por red
// local»): a WebSocket on 3030 for each call, the request with its RequestID
// answered on sdcp/response/<MainboardID>; files by HTTP in 1 MB pieces.
struct Sdcp {
    std::unique_ptr<net::WebSocket> ws;
    std::string id; // MainboardID
    std::string address;
    nlohmann::json status; // the last sdcp/status message's Status
    HostResult result;
};

std::string elegoo_address(const HostConfig& host) { return legacy_host(host); }

// The printer's port for discovery and messages: 3000 for M99999 and 3030 for
// the WebSocket and the uploads, as the spec has them; a test's own
// «host:port» moves the 3030 ones.
int elegoo_port(const HostConfig& host) {
    const int p = legacy_port(host);
    return p == 8899 ? 3030 : p; // legacy_port gives 8899 when none was written
}

Sdcp sdcp_open(const HostConfig& host) {
    Sdcp c;
    c.address = elegoo_address(host);
    c.id = host.printer;
    if (c.id.empty()) {
        // Its MainboardID from M99999 asked of it directly (UDP 3000).
        const int udp = std::getenv("TISMA_TEST_SDCP_UDP") ? std::atoi(std::getenv("TISMA_TEST_SDCP_UDP")) : 3000;
        for (const SdcpPrinter& p : sdcp_discover(1500, {{c.address, udp}}))
            if (p.address == c.address || c.id.empty())
                c.id = p.mainboard_id;
    }
    int status = 0;
    std::string error;
    c.ws = net::WebSocket::connect(c.address, elegoo_port(host), "/websocket", std::min(host.timeout_s, 10) * 1000, status, error);
    if (!c.ws) {
        c.result.code = status == 500 ? "busy" : status == 0 ? "unreachable" : "bad_answer";
        c.result.error = status == 500 ? "the printer has too many connections open"
                         : status == 0 ? "cannot reach the printer at " + c.address +
                                             "; check that it is on and on the same network"
                                       : "the address answers, but not as an Elegoo printer: " + error;
        return c;
    }
    if (c.id.empty()) {
        c.result.code = "bad_answer";
        c.result.error = "the printer at " + c.address + " did not say its MainboardID";
        c.ws.reset();
        return c;
    }
    c.result.ok = true;
    return c;
}

std::string sdcp_request_id() {
    static std::mt19937_64 random(std::random_device{}());
    char id[33];
    std::snprintf(id, sizeof id, "%016llx%016llx", static_cast<unsigned long long>(random()),
                  static_cast<unsigned long long>(random()));
    return id;
}

// Sends one command and waits for its answer (matched by RequestID); the
// status messages that come meanwhile are kept. Its Data.Data, or null with
// result set when none came.
nlohmann::json sdcp_call(Sdcp& c, int cmd, const nlohmann::json& data, int timeout_ms) {
    const std::string rid = sdcp_request_id();
    const nlohmann::json request = {
        {"Id", ""},
        {"Data",
         {{"Cmd", cmd}, {"Data", data}, {"RequestID", rid}, {"MainboardID", c.id},
          {"TimeStamp", std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count()},
          {"From", 0}}},
        {"Topic", "sdcp/request/" + c.id}};
    if (!c.ws->send_text(request.dump())) {
        c.result = {false, "unreachable", "the connection to the printer closed"};
        return nullptr;
    }
    const auto end = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
    for (;;) {
        const int left = int(std::chrono::duration_cast<std::chrono::milliseconds>(end - std::chrono::steady_clock::now()).count());
        if (left <= 0)
            break;
        const std::optional<std::string> m = c.ws->receive(left);
        if (!m)
            break;
        const nlohmann::json j = nlohmann::json::parse(*m, nullptr, false);
        if (!j.is_object())
            continue;
        const std::string topic = j.value("Topic", "");
        if (topic.rfind("sdcp/status", 0) == 0 && j.contains("Status"))
            c.status = j["Status"];
        if (topic.rfind("sdcp/response", 0) == 0 && j.contains("Data") && j["Data"].value("RequestID", "") == rid)
            return j["Data"].value("Data", nlohmann::json::object());
    }
    c.result = {false, "timeout", "the printer did not answer"};
    return nullptr;
}

// A value of the status wherever it is nested (the community's FDM fields
// sit at different depths).
const nlohmann::json* sdcp_find(const nlohmann::json& j, const char* key) {
    if (j.is_object()) {
        if (j.contains(key))
            return &j[key];
        for (const auto& [k, v] : j.items())
            if (const nlohmann::json* f = sdcp_find(v, key))
                return f;
    }
    return nullptr;
}
double sdcp_number(const nlohmann::json& j, const char* key) {
    const nlohmann::json* f = sdcp_find(j, key);
    return f && f->is_number() ? f->get<double>() : -1.0;
}

HostResult sdcp_ack(Sdcp& c, int cmd, const nlohmann::json& answer) {
    if (!c.result.ok)
        return c.result;
    const int ack = answer.is_object() && answer.contains("Ack") && answer["Ack"].is_number() ? answer["Ack"].get<int>() : 0;
    HostResult out;
    out.ok = ack == 0;
    if (!out.ok) {
        out.code = cmd == 128 && ack == 1 ? "conflict" : "failed";
        out.error = sdcp_ack_text(cmd, ack);
    }
    return out;
}

HostStatus elegoo_status(const HostConfig& host) {
    HostStatus out;
    Sdcp c = sdcp_open(host);
    if (!c.result.ok) {
        static_cast<HostResult&>(out) = c.result;
        return out;
    }
    const nlohmann::json answer = sdcp_call(c, 0, nlohmann::json::object(), host.timeout_s * 1000 / 2);
    // The state comes on sdcp/status; its answer may come first.
    for (int i = 0; i < 20 && c.status.is_null() && c.result.ok; ++i)
        if (const auto m = c.ws->receive(100)) {
            const nlohmann::json j = nlohmann::json::parse(*m, nullptr, false);
            if (j.is_object() && j.value("Topic", "").rfind("sdcp/status", 0) == 0 && j.contains("Status"))
                c.status = j["Status"];
        }
    if (!c.result.ok) {
        static_cast<HostResult&>(out) = c.result;
        return out;
    }
    (void)answer;
    if (c.status.is_null()) {
        out.code = "bad_answer";
        out.error = "the printer sent no state";
        return out;
    }
    out.ok = true;
    const nlohmann::json* info = sdcp_find(c.status, "PrintInfo");
    const int st = info && info->contains("Status") && (*info)["Status"].is_number() ? (*info)["Status"].get<int>() : 0;
    out.state = sdcp_print_state(st);
    out.printing = st >= 1 && st <= 5;
    out.paused = st == 6;
    if (info) {
        out.file = info->value("Filename", std::string());
        const double layer = sdcp_number(*info, "CurrentLayer"), layers = sdcp_number(*info, "TotalLayer");
        if (layers > 0.0 && layer >= 0.0)
            out.progress = 100.0 * layer / layers;
    }
    out.nozzle = sdcp_number(c.status, "TempOfNozzle");
    out.nozzle_target = sdcp_number(c.status, "TempTargetNozzle");
    out.bed = sdcp_number(c.status, "TempOfHotbed");
    out.bed_target = sdcp_number(c.status, "TempTargetHotbed");
    return out;
}

HostVersion elegoo_version(const HostConfig& host) {
    HostVersion out;
    const HostStatus st = elegoo_status(host);
    static_cast<HostResult&>(out) = st;
    if (out.ok)
        out.server = "Elegoo (SDCP)";
    return out;
}

HostResult elegoo_upload(const HostConfig& host, const std::string& path, const std::string& name, bool start) {
    std::ifstream in(path, std::ios::binary);
    const std::string data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    const std::string md5 = net::md5_hex(data);
    const std::string uuid = sdcp_request_id();
    const std::string url = "http://" + elegoo_address(host) + ":" + std::to_string(elegoo_port(host)) + "/uploadFile/upload";
    constexpr std::size_t kPiece = 1u << 20; // 1 MB, in order
    for (std::size_t at = 0; at < data.size() || at == 0; at += kPiece) {
        net::Request r;
        r.method = "POST";
        r.url = url;
        r.timeout_s = std::max(host.timeout_s, 120);
        r.form = {{"S-File-MD5", md5, "", "", "text/plain"},
                  {"Check", "1", "", "", "text/plain"},
                  {"Offset", std::to_string(at), "", "", "text/plain"},
                  {"Uuid", uuid, "", "", "text/plain"},
                  {"TotalSize", std::to_string(data.size()), "", "", "text/plain"},
                  {"File", data.substr(at, kPiece), "", name, "application/octet-stream"}};
        const net::Response resp = net::perform(r);
        HostResult res = result_of(resp);
        if (!res.ok) {
            if (res.code == "unreachable")
                res.error = "cannot reach the printer at " + elegoo_address(host);
            return res;
        }
        const nlohmann::json j = nlohmann::json::parse(resp.body, nullptr, false);
        std::string code = j.is_object() && j.contains("code") ? (j["code"].is_string() ? j["code"].get<std::string>()
                                                                                        : j["code"].dump())
                                                               : resp.body;
        if (code.find("000000") == std::string::npos) {
            res.ok = false;
            res.code = "failed";
            res.error = code.find("-2") != std::string::npos ? "the upload failed (the offset did not match)"
                        : code.find("-1") != std::string::npos ? "the upload failed (negative offset)"
                        : code.find("-3") != std::string::npos ? "the upload failed (the printer could not open the file)"
                                                              : "the upload failed";
            return res;
        }
        if (data.empty())
            break;
    }
    if (!start)
        return {true, "", ""};
    Sdcp c = sdcp_open(host);
    if (!c.result.ok)
        return c.result;
    const nlohmann::json answer = sdcp_call(c, 128, {{"Filename", name}, {"StartLayer", 0}}, host.timeout_s * 1000 / 2);
    return sdcp_ack(c, 128, answer);
}

HostResult elegoo_command(const HostConfig& host, int cmd) {
    Sdcp c = sdcp_open(host);
    if (!c.result.ok)
        return c.result;
    const nlohmann::json answer = sdcp_call(c, cmd, nlohmann::json::object(), host.timeout_s * 1000 / 2);
    return sdcp_ack(c, cmd, answer);
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
    else if (word == "astrobox")
        out = HostKind::AstroBox;
    else if (word == "repetier")
        out = HostKind::Repetier;
    else if (word == "flashforge")
        out = HostKind::Flashforge;
    else if (word == "flashforge_legacy")
        out = HostKind::FlashforgeLegacy;
    else if (word == "elegoo")
        out = HostKind::Elegoo;
    else
        return false;
    return true;
}

HostLimits host_limits(const HostConfig& host) {
    HostLimits out;
    if (host.kind != HostKind::Moonraker) {
        out.code = "failed";
        out.error = "this program does not report the printer's limits";
        return out;
    }
    const net::Response r = net::perform(request(host, "GET", "/printer/objects/query?toolhead&configfile=settings"));
    static_cast<HostResult&>(out) = result_of(r);
    if (!out.ok)
        return out;
    const nlohmann::json j = moonraker_result(r);
    if (!j.is_object() || !j.contains("status") || !j["status"].is_object()) {
        out.ok = false;
        out.code = "bad_answer";
        out.error = "the printer's program sent no printer objects";
        return out;
    }
    const nlohmann::json& st = j["status"];
    if (st.contains("toolhead"))
        out.toolhead = st["toolhead"];
    if (st.contains("configfile") && st["configfile"].is_object() && st["configfile"].contains("settings"))
        out.settings = st["configfile"]["settings"];
    return out;
}

const char* host_kind_word(HostKind kind) {
    switch (kind) {
    case HostKind::Moonraker: return "moonraker";
    case HostKind::PrusaLink: return "prusalink";
    case HostKind::Duet: return "duet";
    case HostKind::AstroBox: return "astrobox";
    case HostKind::Repetier: return "repetier";
    case HostKind::Flashforge: return "flashforge";
    case HostKind::FlashforgeLegacy: return "flashforge_legacy";
    case HostKind::Elegoo: return "elegoo";
    default: return "octoprint";
    }
}

const char* host_kind_name(HostKind kind) {
    switch (kind) {
    case HostKind::Moonraker: return "Klipper (Moonraker)";
    case HostKind::PrusaLink: return "PrusaLink";
    case HostKind::Duet: return "Duet";
    case HostKind::AstroBox: return "AstroBox";
    case HostKind::Repetier: return "Repetier-Server";
    case HostKind::Flashforge: return "Flashforge (5M, AD5X, Creator 5)";
    case HostKind::FlashforgeLegacy: return "Flashforge (Adventurer 3, 4)";
    case HostKind::Elegoo: return "Elegoo (Centauri Carbon, resina)";
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
    if (host.kind == HostKind::Repetier)
        return repetier_version(host);
    if (host.kind == HostKind::Flashforge)
        return flashforge_version(host);
    if (host.kind == HostKind::FlashforgeLegacy)
        return legacy_version(host);
    if (host.kind == HostKind::Elegoo)
        return elegoo_version(host);
    HostVersion out;
    const net::Response r = net::perform(request(host, "GET", "/api/version"));
    static_cast<HostResult&>(out) = result_of(r);
    if (!out.ok)
        return out;
    const nlohmann::json j = nlohmann::json::parse(r.body, nullptr, false);
    if (!j.is_object() || !j.contains("api")) {
        out.ok = false;
        out.code = "bad_answer";
        out.error = host.kind == HostKind::AstroBox ? "the address answers, but not as AstroBox"
                                                    : "the address answers, but not as OctoPrint";
        return out;
    }
    out.api = j.value("api", "");
    out.server = j.value("text", std::string(host.kind == HostKind::AstroBox ? "AstroBox " : "OctoPrint ") +
                                     j.value("server", std::string()));
    return out;
}

HostResult host_upload(const HostConfig& host, const std::string& path, const std::string& name, bool start,
                       bool simulate) {
    if (host.kind == HostKind::Moonraker)
        return moonraker_upload(host, path, name, start);
    if (host.kind == HostKind::PrusaLink)
        return prusalink_upload(host, path, name, start);
    if (host.kind == HostKind::Duet)
        return duet_upload(host, path, name, start, simulate);
    if (host.kind == HostKind::Repetier)
        return repetier_upload(host, path, name, start);
    if (host.kind == HostKind::Flashforge)
        return flashforge_upload(host, path, name, start);
    if (host.kind == HostKind::Elegoo)
        return elegoo_upload(host, path, name, start);
    if (host.kind == HostKind::FlashforgeLegacy) // M28 and M29: not confirmed (the spec)
        return unsupported("sending files to an Adventurer 3 or 4 over the network is not confirmed yet; "
                           "copy the file to a USB stick");
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
    if (host.kind == HostKind::Repetier)
        return repetier_status(host);
    if (host.kind == HostKind::Flashforge)
        return flashforge_status(host);
    if (host.kind == HostKind::FlashforgeLegacy)
        return legacy_status(host);
    if (host.kind == HostKind::Elegoo)
        return elegoo_status(host);
    if (host.kind == HostKind::AstroBox) {
        HostStatus out;
        static_cast<HostResult&>(out) = unsupported("AstroBox has no published API for the printer's state");
        return out;
    }
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
    if (host.kind == HostKind::Repetier)
        return pause ? unsupported("Repetier-Server's API has no pause command")
                     : repetier_job(host, "continueJob");
    if (host.kind == HostKind::AstroBox)
        return unsupported("AstroBox has no published API to pause or resume");
    if (host.kind == HostKind::Flashforge)
        return flashforge_job(host, pause ? "pause" : "continue");
    if (host.kind == HostKind::FlashforgeLegacy)
        return legacy_command(host, pause ? "M25" : "M24");
    if (host.kind == HostKind::Elegoo)
        return elegoo_command(host, pause ? 129 : 131);
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
    if (host.kind == HostKind::Repetier)
        return repetier_job(host, "stopJob");
    if (host.kind == HostKind::AstroBox)
        return unsupported("AstroBox has no published API to cancel");
    if (host.kind == HostKind::Flashforge)
        return flashforge_job(host, "cancel");
    if (host.kind == HostKind::FlashforgeLegacy)
        return legacy_command(host, "M26");
    if (host.kind == HostKind::Elegoo)
        return elegoo_command(host, 130);
    net::Request r = request(host, "POST", "/api/job");
    r.headers.push_back({"Content-Type", "application/json"});
    r.body = R"({"command": "cancel"})";
    return result_of(net::perform(r));
}

std::vector<FoundHost> confirm_moonraker(const std::vector<std::pair<std::string, std::string>>& candidates,
                                         int timeout_s) {
    std::vector<FoundHost> out;
    for (const auto& [name, url] : candidates) {
        std::string base = url;
        while (base.size() > 8 && base.back() == '/')
            base.pop_back();
        if (std::any_of(out.begin(), out.end(), [&](const FoundHost& f) { return f.url == base; }))
            continue; // one printer once (rule 2)
        HostConfig host;
        host.kind = HostKind::Moonraker;
        host.url = base;
        host.timeout_s = timeout_s;
        if (const HostVersion v = host_version(host); v.ok)
            out.push_back({name, base, v.server});
    }
    return out;
}

} // namespace tisma::device

