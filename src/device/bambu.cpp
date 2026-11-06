// Bambu Lab printers on the local network. Own code; see bambu.hpp and
// PROVENANCE.md.

#include "device/bambu.hpp"

#include "device/ftps.hpp"
#include "device/mqtt.hpp"
#include "io/zip.hpp"
#include "net/digest.hpp"
#include "net/tls.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstring>
#include <memory>
#include <random>

namespace tisma::device {
namespace {

using nlohmann::json;

double number(const json& o, const char* key, double fallback = -1.0) {
    if (!o.contains(key))
        return fallback;
    const json& v = o.at(key);
    if (v.is_number())
        return v.get<double>();
    if (v.is_string()) {
        try {
            return std::stod(v.get<std::string>());
        } catch (const std::exception&) {
        }
    }
    return fallback;
}

std::string text(const json& o, const char* key) {
    if (!o.contains(key))
        return {};
    const json& v = o.at(key);
    if (v.is_string())
        return v.get<std::string>();
    if (v.is_number())
        return v.dump();
    return {};
}

// An open session: TLS checked against the pinned certificate, MQTT
// connected and subscribed to the report topic.
struct Session {
    std::unique_ptr<mqtt::Client> client;
    std::string serial;
};

std::atomic<int> sequence{1};

template <class R>
bool open(const BambuConfig& c, Session& s, R& result) {
    std::unique_ptr<net::TlsStream> tls;
    try {
        tls = net::TlsStream::connect(c.host, c.mqtt_port, c.timeout_ms);
    } catch (const net::TlsError& e) {
        result.code = "unreachable";
        result.error = std::string("cannot reach the printer at ") + c.host + ": " + e.what();
        return false;
    }
    result.sha256 = tls->peer_sha256();
    result.serial = c.serial.empty() ? tls->peer_cn() : c.serial;
    // Rule 4: the certificate the person accepted, or nothing is sent (not
    // even the access code) until they accept this one.
    if (c.pinned_sha256.empty()) {
        result.code = "confirm_certificate";
        result.error = "the printer's certificate is new: confirm it is " + result.serial;
        return false;
    }
    if (tls->peer_sha256() != c.pinned_sha256) {
        result.code = "certificate_changed";
        result.error = "the printer's certificate is not the one accepted";
        return false;
    }
    if (result.serial.empty()) {
        result.code = "bad_answer";
        result.error = "the printer's certificate has no serial number";
        return false;
    }
    s.serial = result.serial;
    s.client = std::make_unique<mqtt::Client>(std::move(tls));
    std::mt19937 random(std::random_device{}());
    mqtt::Connect connect{"tisma-" + std::to_string(random() % 1000000000u), "bblp", c.access_code, 60};
    try {
        s.client->connect(connect, c.timeout_ms);
        s.client->subscribe("device/" + s.serial + "/report", 0, c.timeout_ms);
    } catch (const mqtt::MqttError& e) {
        result.code = e.code == "closed" ? "unreachable" : e.code;
        result.error = e.what();
        return false;
    }
    return true;
}

std::string request_topic(const Session& s) { return "device/" + s.serial + "/request"; }

} // namespace

void bambu_merge(json& kept, const json& report_print) {
    if (!kept.is_object())
        kept = json::object();
    for (const auto& [key, value] : report_print.items()) {
        if (value.is_object() && kept.contains(key) && kept.at(key).is_object())
            bambu_merge(kept[key], value);
        else
            kept[key] = value;
    }
}

BambuStatus bambu_status_from(const json& p) {
    BambuStatus s;
    s.ok = true;
    s.state = text(p, "gcode_state");
    // The documentation shows IDLE only; the others are read as their words
    // say (not tested on hardware).
    s.printing = s.state == "RUNNING" || s.state == "PREPARE" || s.state == "SLICING";
    s.paused = s.state == "PAUSE";
    s.finished = s.state == "FINISH";
    s.failed = s.state == "FAILED";
    s.progress = number(p, "mc_percent");
    s.minutes_left = number(p, "mc_remaining_time");
    s.nozzle = number(p, "nozzle_temper");
    s.nozzle_target = number(p, "nozzle_target_temper");
    s.bed = number(p, "bed_temper");
    s.bed_target = number(p, "bed_target_temper");
    s.chamber = number(p, "chamber_temper");
    s.layer = int(number(p, "layer_num"));
    s.layers = int(number(p, "total_layer_num"));
    s.job = text(p, "subtask_name");
    s.error_code = (long long)number(p, "print_error", 0.0);
    s.nozzle_diameter = text(p, "nozzle_diameter");
    if (p.contains("ams") && p.at("ams").is_object()) {
        const json& a = p.at("ams");
        s.tray_now = int(number(a, "tray_now"));
        if (a.contains("ams") && a.at("ams").is_array())
            for (const json& unit : a.at("ams")) {
                const int ams = int(number(unit, "id"));
                if (!unit.contains("tray") || !unit.at("tray").is_array())
                    continue;
                for (const json& t : unit.at("tray")) {
                    BambuTray tray;
                    tray.ams = ams;
                    tray.slot = int(number(t, "id"));
                    tray.type = text(t, "tray_type");
                    tray.color = text(t, "tray_color").substr(0, 6);
                    tray.remain = int(number(t, "remain"));
                    s.trays.push_back(tray);
                }
            }
    }
    // The external spool (vt_tray), when the report has it.
    if (p.contains("vt_tray") && p.at("vt_tray").is_object()) {
        const json& t = p.at("vt_tray");
        BambuTray tray;
        tray.ams = 255;
        tray.slot = int(number(t, "id"));
        tray.type = text(t, "tray_type");
        tray.color = text(t, "tray_color").substr(0, 6);
        tray.remain = int(number(t, "remain"));
        s.trays.push_back(tray);
    }
    return s;
}

BambuStatus bambu_status(const BambuConfig& config) {
    BambuStatus out;
    Session s;
    if (!open(config, s, out))
        return out;
    const std::string serial = out.serial, sha = out.sha256;
    json kept = json::object();
    try {
        s.client->publish(request_topic(s),
                          json{{"pushing", {{"sequence_id", std::to_string(sequence++)}, {"command", "pushall"},
                                            {"version", 1}, {"push_target", 1}}}}
                              .dump(),
                          0);
        const auto end = std::chrono::steady_clock::now() + std::chrono::milliseconds(config.timeout_ms);
        for (;;) {
            const auto left = std::chrono::duration_cast<std::chrono::milliseconds>(end - std::chrono::steady_clock::now()).count();
            if (left <= 0) {
                out.code = "timeout";
                out.error = "the printer did not send its state in time";
                return out;
            }
            const std::optional<mqtt::Publish> m = s.client->receive(int(left));
            if (!m)
                continue;
            const json report = json::parse(m->payload, nullptr, false);
            if (!report.is_object() || !report.contains("print") || !report.at("print").is_object())
                continue;
            bambu_merge(kept, report.at("print"));
            if (text(report.at("print"), "command") == "push_status")
                break;
        }
    } catch (const mqtt::MqttError& e) {
        out.code = e.code == "closed" ? "unreachable" : e.code;
        out.error = e.what();
        return out;
    } catch (const net::TlsError& e) {
        out.code = "unreachable";
        out.error = e.what();
        return out;
    }
    s.client->disconnect();
    out = bambu_status_from(kept);
    out.serial = serial;
    out.sha256 = sha;
    return out;
}

BambuResult bambu_command(const BambuConfig& config, const std::string& command) {
    BambuResult out;
    if (command != "pause" && command != "resume" && command != "stop") {
        out.code = "failed";
        out.error = "unknown command " + command;
        return out;
    }
    Session s;
    if (!open(config, s, out))
        return out;
    try {
        // QoS 1, as the documentation says for these three.
        s.client->publish(request_topic(s),
                          json{{"print", {{"sequence_id", std::to_string(sequence++)}, {"command", command}, {"param", ""}}}}.dump(),
                          1);
        s.client->disconnect();
    } catch (const net::TlsError& e) {
        out.code = "unreachable";
        out.error = e.what();
        return out;
    }
    out.ok = true;
    return out;
}

std::string bambu_package(const std::string& gcode) {
    const std::string content_types =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\">"
        "<Default Extension=\"rels\" ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/>"
        "<Default Extension=\"model\" ContentType=\"application/vnd.ms-package.3dmanufacturing-3dmodel+xml\"/>"
        "<Default Extension=\"gcode\" ContentType=\"text/x.gcode\"/>"
        "</Types>\n";
    const std::string rels =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
        "<Relationship Target=\"/3D/3dmodel.model\" Id=\"rel0\" "
        "Type=\"http://schemas.microsoft.com/3dmanufacturing/2013/01/3dmodel\"/></Relationships>\n";
    const std::string model =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<model unit=\"millimeter\" xml:lang=\"en-US\" xmlns=\"http://schemas.microsoft.com/3dmanufacturing/core/2015/02\">"
        "<metadata name=\"Application\">Tisma</metadata><resources/><build/></model>\n";
    return write_zip_stored({{"[Content_Types].xml", content_types},
                             {"_rels/.rels", rels},
                             {"3D/3dmodel.model", model},
                             {"Metadata/plate_1.gcode", gcode},
                             {"Metadata/plate_1.gcode.md5", net::md5_hex(gcode)}});
}

json bambu_project_file(const BambuPrint& job, const std::string& file, const std::string& package_md5, int seq) {
    json mapping = json::array();
    for (const int slot : job.ams_mapping)
        mapping.push_back(slot);
    return json{{"print",
                 {{"sequence_id", std::to_string(seq)},
                  {"command", "project_file"},
                  {"param", "Metadata/plate_1.gcode"},
                  {"project_id", "0"},
                  {"profile_id", "0"},
                  {"task_id", "0"},
                  {"subtask_id", "0"},
                  {"subtask_name", job.name},
                  {"file", file},
                  // The card's root, as OpenBambuAPI shows it for a file
                  // there (not tested on hardware).
                  {"url", "ftp:///" + file},
                  {"md5", package_md5},
                  {"timelapse", job.timelapse},
                  {"bed_type", job.bed_type.empty() ? std::string("auto") : job.bed_type},
                  {"bed_levelling", job.bed_leveling},
                  {"flow_cali", job.flow_calibration},
                  {"vibration_cali", job.vibration_calibration},
                  {"layer_inspect", false},
                  {"ams_mapping", mapping},
                  {"use_ams", job.use_ams}}}};
}

BambuResult bambu_print(const BambuConfig& config, const BambuPrint& job,
                        const std::function<void(std::size_t, std::size_t)>& progress) {
    BambuResult out;
    if (job.gcode.empty()) {
        out.code = "failed";
        out.error = "no G-code to send";
        return out;
    }
    // Check the certificate and the access code before uploading anything.
    Session s;
    if (!open(config, s, out))
        return out;
    const std::string package = bambu_package(job.gcode);
    std::string file;
    for (const char c : job.name.empty() ? std::string("tisma") : job.name)
        file += (c == '/' || c == '\\' || c == ':' || static_cast<unsigned char>(c) < 32) ? '_' : c;
    file += ".gcode.3mf";
    FtpsConfig ftp;
    ftp.host = config.host;
    ftp.port = config.ftps_port;
    ftp.user = "bblp";
    ftp.password = config.access_code;
    ftp.timeout_ms = std::max(config.timeout_ms, 15000);
    ftp.pinned_sha256 = config.pinned_sha256;
    ftp.expected_cn = out.serial;
    try {
        ftps_upload(ftp, file, package, [&](std::size_t sent) {
            if (progress)
                progress(sent, package.size());
        });
    } catch (const FtpsError& e) {
        out.code = e.code;
        out.error = e.what();
        return out;
    }
    try {
        s.client->publish(request_topic(s), bambu_project_file(job, file, net::md5_hex(package), sequence++).dump(), 1);
        s.client->disconnect();
    } catch (const net::TlsError& e) {
        out.code = "unreachable";
        out.error = e.what();
        return out;
    }
    out.ok = true;
    return out;
}

std::string bambu_camera_auth(const std::string& access_code) {
    // Little-endian: payload size 0x40, type 0x3000, flags 0, zero; then the
    // user and the password, 32 bytes each, padded with zeros (video.md).
    std::string p(80, '\0');
    const auto put32 = [&](std::size_t at, std::uint32_t v) {
        for (int i = 0; i < 4; ++i)
            p[at + std::size_t(i)] = char((v >> (8 * i)) & 0xFF);
    };
    put32(0, 0x40);
    put32(4, 0x3000);
    std::memcpy(p.data() + 16, "bblp", 4);
    std::memcpy(p.data() + 48, access_code.data(), std::min<std::size_t>(access_code.size(), 32));
    return p;
}

BambuResult bambu_camera_stream(const BambuConfig& config, const std::function<bool(std::string)>& on_frame,
                                int idle_timeout_ms, const std::atomic<bool>* stop) {
    BambuResult out;
    std::unique_ptr<net::TlsStream> tls;
    try {
        tls = net::TlsStream::connect(config.host, config.camera_port, config.timeout_ms);
    } catch (const net::TlsError& e) {
        out.code = "unreachable";
        out.error = e.what();
        return out;
    }
    out.sha256 = tls->peer_sha256();
    out.serial = tls->peer_cn();
    if (config.pinned_sha256.empty() || (tls->peer_sha256() != config.pinned_sha256 &&
                                         (config.serial.empty() || tls->peer_cn() != config.serial))) {
        out.code = config.pinned_sha256.empty() ? "confirm_certificate" : "certificate_changed";
        out.error = "the camera's certificate is not the one accepted";
        return out;
    }
    int got = 0;
    try {
        tls->write(bambu_camera_auth(config.access_code));
        for (;;) {
            if (stop && stop->load())
                break;
            // Short waits, so a stop is seen soon; idle_timeout_ms in all.
            std::string header;
            for (int waited = 0; header.size() < 16;) {
                try {
                    header += tls->read(16 - header.size(), 250);
                } catch (const net::TlsTimeout&) {
                    waited += 250;
                    if ((stop && stop->load()) || waited >= idle_timeout_ms)
                        throw;
                }
            }
            std::uint32_t size = 0;
            for (int i = 3; i >= 0; --i)
                size = (size << 8) | std::uint8_t(header[std::size_t(i)]);
            if (size == 0 || size > 16u * 1024 * 1024) {
                out.code = "bad_answer";
                out.error = "the camera sent a frame of an impossible size";
                return out;
            }
            std::string jpeg = tls->read_exact(size, std::max(idle_timeout_ms, 1000));
            if (jpeg.size() < 4 || std::uint8_t(jpeg[0]) != 0xFF || std::uint8_t(jpeg[1]) != 0xD8) {
                out.code = "bad_answer";
                out.error = "the camera sent something that is not a JPEG";
                return out;
            }
            ++got;
            if (!on_frame(std::move(jpeg)))
                break;
        }
    } catch (const net::TlsTimeout&) {
    } catch (const net::TlsError& e) {
        if (got == 0) {
            out.code = "unauthorized";
            out.error = std::string("the camera closed the connection (wrong access code?): ") + e.what();
            return out;
        }
    }
    out.ok = got > 0;
    if (!out.ok && out.code.empty()) {
        out.code = "timeout";
        out.error = "the camera sent no image in time";
    }
    return out;
}

BambuFrames bambu_camera(const BambuConfig& config, int frames, int timeout_ms) {
    BambuFrames out;
    const BambuResult r = bambu_camera_stream(
        config,
        [&](std::string jpeg) {
            out.jpegs.push_back(std::move(jpeg));
            return int(out.jpegs.size()) < frames;
        },
        timeout_ms);
    static_cast<BambuResult&>(out) = r;
    return out;
}

const char* bambu_help() {
    return "Bambu Lab: on the printer turn on LAN only mode and Developer mode (Settings > LAN only). Hostname = IP "
           "address of the printer, API key = access code shown there. The serial number is read from the printer.";
}

} // namespace tisma::device
