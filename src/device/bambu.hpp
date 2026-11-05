#pragma once

// Bambu Lab printers on the local network (printer_connections.md, «Bambu Lab
// en red local», rules 1 to 8; devices.md, rules 11 and 16). The printer in
// «LAN only» and developer mode; its access code is the password.
// - State: MQTT 3.1.1 over TLS (port 8883, user «bblp»), «pushall» on
//   device/<serial>/request, the answer on device/<serial>/report.
// - The serial number is the certificate's name (CN); the certificate's
//   SHA-256 is shown once for the person to accept, and then required.
// - Printing: the G-code in a «.gcode.3mf» package (Metadata/plate_1.gcode
//   and its MD5), sent by FTPS (port 990) and started with «project_file».
// - Pause, resume and stop; the A1 and P1 camera (TLS, port 6000).
// The protocol as the community documents it (OpenBambuAPI: mqtt.md, ftp.md,
// video.md): Bambu Lab does not publish it. What that documentation leaves
// open (the states other than IDLE, the «url» and «md5» of project_file,
// ams_mapping) is marked «not tested on hardware» where it is used. Own
// design; see PROVENANCE.md.

#include <functional>
#include <string>
#include <vector>

#include <nlohmann/json_fwd.hpp>

namespace tisma::device {

struct BambuConfig {
    std::string host;        // the printer's IP address
    std::string access_code; // a secret (rule 8): never logged nor in a message
    std::string serial;      // empty: taken from the certificate (rule 3)
    std::string pinned_sha256; // the certificate the person accepted; empty: none yet
    int mqtt_port = 8883, ftps_port = 990, camera_port = 6000;
    int timeout_ms = 10000;
};

// What a call gave: ok, or a stable code with an English message:
// unreachable, confirm_certificate (first time: show sha256 and serial and
// ask), certificate_changed, unauthorized, timeout, refused, bad_answer,
// failed.
struct BambuResult {
    bool ok = false;
    std::string code, error;
    std::string serial, sha256; // the printer's, as its certificate says
};

struct BambuTray {
    int ams = -1, slot = -1; // AMS unit and its slot; the external spool is ams 255
    std::string type;        // PLA, PETG…; empty: an empty slot
    std::string color;       // RRGGBB
    int remain = -1;         // percent; -1 unknown
};

struct BambuStatus : BambuResult {
    std::string state;  // gcode_state as the printer says it (IDLE…)
    bool printing = false, paused = false, finished = false, failed = false;
    double progress = -1.0;     // mc_percent
    double minutes_left = -1.0; // mc_remaining_time
    double nozzle = -1.0, nozzle_target = -1.0, bed = -1.0, bed_target = -1.0, chamber = -1.0;
    int layer = -1, layers = -1;
    std::string job;     // subtask_name
    long long error_code = 0; // print_error
    std::string nozzle_diameter;
    std::vector<BambuTray> trays;
    int tray_now = -1;   // the tray in use: ams * 4 + slot, 254 the external spool, 255 none
};

// The state from the printer's «print» object (a whole report or the P1's
// partial ones merged by bambu_merge).
BambuStatus bambu_status_from(const nlohmann::json& print);
// Lays a report's «print» object over the state kept (the P1 series sends
// only what changed).
void bambu_merge(nlohmann::json& kept, const nlohmann::json& report_print);

// Connects, asks for the whole state and reads it.
BambuStatus bambu_status(const BambuConfig& config);
// «pause», «resume» or «stop».
BambuResult bambu_command(const BambuConfig& config, const std::string& command);

struct BambuPrint {
    std::string name;     // the job's name; the file is <name>.gcode.3mf
    std::string gcode;    // text G-code (rule 7: never binary)
    bool use_ams = false; // «Print from the AMS»
    // The AMS slot of each filament of the G-code, as OpenBambuAPI's
    // ams_mapping (not tested on hardware); empty: the printer decides.
    std::vector<int> ams_mapping;
    bool timelapse = false, bed_leveling = true;
};

// The package the printer prints: a 3MF with Metadata/plate_1.gcode and its
// MD5 in Metadata/plate_1.gcode.md5.
std::string bambu_package(const std::string& gcode);
// The «project_file» command for a package uploaded as `file`.
nlohmann::json bambu_project_file(const BambuPrint& job, const std::string& file, const std::string& package_md5,
                                  int sequence);
// Uploads the package by FTPS and starts it. `progress` gets the bytes sent
// and the total.
BambuResult bambu_print(const BambuConfig& config, const BambuPrint& job,
                        const std::function<void(std::size_t, std::size_t)>& progress = {});

// The A1 and P1 camera: its authentication packet (80 bytes) and the next
// JPEG frames, at most `frames` of them in `timeout_ms`.
std::string bambu_camera_auth(const std::string& access_code);
struct BambuFrames : BambuResult {
    std::vector<std::string> jpegs;
};
BambuFrames bambu_camera(const BambuConfig& config, int frames, int timeout_ms);

// The printer's words to the engine's: «Bambu Lab: on the printer turn on LAN
// only mode…» (devices.md, rule 11).
const char* bambu_help();

} // namespace tisma::device
