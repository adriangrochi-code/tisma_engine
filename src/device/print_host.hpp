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
// rr_disconnect). AstroBox has no published API: it is a fork of OctoPrint
// and answers OctoPrint's version and upload calls (devices.md, rule 24), so
// only those. Repetier-Server's Web API (repetier-server.com, «Web API»):
// /printer/info, /printer/api/<slug>?a=<command> with the key in x-api-key,
// and the upload of a file into the printer's models or jobs. Own design;
// see PROVENANCE.md.

#include <string>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

namespace tisma::device {

enum class HostKind { OctoPrint, Moonraker, PrusaLink, Duet, AstroBox, Repetier };

struct HostConfig {
    HostKind kind = HostKind::OctoPrint;
    std::string url;     // http(s)://host[:port], the program's address
    std::string api_key; // a secret: never logged
    std::string user, password; // PrusaLink's Digest login, Duet's board password (secrets too;
                                // Duet: «reprap» when empty, its default)
    std::string printer; // Repetier-Server: the printer's slug on the server; empty, its first printer
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
// Uploads the G-code to the program's storage and, with `start`, prints it;
// with `simulate` (Duet only: M37), the board runs it without moving, to
// time it (devices.md, rule 20).
HostResult host_upload(const HostConfig& host, const std::string& path, const std::string& name, bool start,
                       bool simulate = false);
HostStatus host_status(const HostConfig& host);
HostResult host_pause(const HostConfig& host, bool pause); // false: resume
HostResult host_cancel(const HostConfig& host);

// The printer's limits as the program knows them (printer_connections.md,
// «Leer los límites», rule 8): Moonraker's toolhead and configfile objects
// (GET /printer/objects/query?toolhead&configfile=settings), as JSON; null
// for the other programs or when the call fails.
struct HostLimits : HostResult {
    nlohmann::json toolhead, settings;
};
HostLimits host_limits(const HostConfig& host);

// Moonraker found on the network (printer_connections.md, «Búsqueda en la
// red»: its `_moonraker._tcp` services, each confirmed by GET /server/info):
// of the candidates (the service's instance name and address), the ones that
// answer, each address once, with what the server says it is.
struct FoundHost {
    std::string name, url, server;
};
std::vector<FoundHost> confirm_moonraker(const std::vector<std::pair<std::string, std::string>>& candidates,
                                         int timeout_s = 3);

// The engine's words for a host kind ("octoprint", "moonraker", "prusalink",
// "duet", "astrobox", "repetier"); false for another word.
bool host_kind_from(const std::string& word, HostKind& out);
// Back: the word host_kind_from reads, and the name a person knows it by
// («OctoPrint», «Klipper (Moonraker)», «PrusaLink», «Duet», «AstroBox»,
// «Repetier-Server»).
const char* host_kind_word(HostKind kind);
const char* host_kind_name(HostKind kind);

} // namespace tisma::device
