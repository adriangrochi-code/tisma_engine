#pragma once

// Programs that run printers over the network (printer_connections.md,
// «Programas que manejan impresoras»): test the connection and the
// credentials, upload a file, start it, read the state, and pause, resume or
// cancel. Each call as the program's public API documentation describes it:
// OctoPrint's REST API (docs.octoprint.org, «REST API») and Moonraker's
// (moonraker.readthedocs.io, «External API»: server, files, printer) and
// PrusaLink's published OpenAPI (v1: version, status, files, job), with
// HTTP Digest (RFC 7616) for its user and password, and Duet's RepRapFirmware
// in standalone mode (the RepRapFirmware wiki, «HTTP requests» and «Object
// Model Documentation»: rr_connect, rr_upload, rr_gcode, rr_model,
// rr_disconnect). Own design; see PROVENANCE.md.

#include <string>

namespace tisma::device {

enum class HostKind { OctoPrint, Moonraker, PrusaLink, Duet };

struct HostConfig {
    HostKind kind = HostKind::OctoPrint;
    std::string url;     // http(s)://host[:port], the program's address
    std::string api_key; // a secret: never logged
    std::string user, password; // PrusaLink's Digest login, Duet's board password (secrets too;
                                // Duet: «reprap» when empty, its default)
    int timeout_s = 20;
};

// What a call gave: ok, or why not in English with a stable code
// (unreachable, unauthorized, not_connected, conflict, bad_answer, failed).
struct HostResult {
    bool ok = false;
    std::string code;
    std::string error;
};

struct HostVersion : HostResult {
    std::string server; // «OctoPrint 1.10.2»
    std::string api;
};

struct HostStatus : HostResult {
    std::string state;       // the program's own words: Operational, Printing, Paused…
    bool printing = false, paused = false;
    std::string file;        // the job's file
    double progress = -1.0;  // percent; -1 unknown
    double seconds_left = -1.0;
    double nozzle = -1.0, nozzle_target = -1.0, bed = -1.0, bed_target = -1.0; // °C; -1 unknown
};

HostVersion host_version(const HostConfig& host);
// Uploads the G-code to the program's storage and, with `start`, prints it.
HostResult host_upload(const HostConfig& host, const std::string& path, const std::string& name, bool start);
HostStatus host_status(const HostConfig& host);
HostResult host_pause(const HostConfig& host, bool pause); // false: resume
HostResult host_cancel(const HostConfig& host);

// The engine's words for a host kind ("octoprint", "moonraker", "prusalink",
// "duet"); false for another word.
bool host_kind_from(const std::string& word, HostKind& out);
// Back: the word host_kind_from reads, and the name a person knows it by
// («OctoPrint», «Klipper (Moonraker)», «PrusaLink», «Duet»).
const char* host_kind_word(HostKind kind);
const char* host_kind_name(HostKind kind);

} // namespace tisma::device
