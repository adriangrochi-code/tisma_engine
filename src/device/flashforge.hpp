#pragma once

// Flashforge printers on the local network (printer_connections.md,
// «Flashforge en red local»), the parts that are not HTTP: finding them by
// UDP and the older Adventurer 3 and 4 over TCP port 8899 («~» commands,
// answers ending in «ok»), as the community's flashforge-api-docs wiki
// describes them («Discovery Protocol», «M-Code Reference»). The newer ones'
// HTTP is in print_host. Own design; see PROVENANCE.md.

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace tisma::device {

struct FlashforgePrinter {
    std::string name, address, serial; // the serial only from a modern answer (or M115)
    bool modern = false;                // 276 bytes or more: HTTP on http_port
    int tcp_port = 8899, http_port = 8898;
    int status = 0; // the older ones: 0 ready, 1 busy, 2 error
};

// One answer to the discovery datagram from `sender`: a modern one (196
// bytes or more, big endian: name at 0, command port at 0x84, HTTP port at
// 0x8E, serial at 0x92) or an older one (140 bytes: name, port, VID, PID,
// status). Nothing for anything else.
std::optional<FlashforgePrinter> parse_flashforge_discovery(const std::string& datagram, const std::string& sender);

// Sends the discovery datagram (225.0.0.9:19000 and 255.255.255.255:48899
// for the newer ones, 225.0.0.9:8899 for the older) and collects the answers
// for `ms` milliseconds, each printer once (by address and kind). `extra` also asks those
// host:port pairs directly (the tests' simulator on this machine).
std::vector<FlashforgePrinter> flashforge_discover(int ms, const std::vector<std::pair<std::string, int>>& extra = {});

// One TCP command session with an older printer (port 8899): opens it with
// M601 S1 («Control Success»; «Control Failed» is another program holding
// it), runs `commands`, releases it with M602. Each answer's text, in order;
// `code` "conflict" when another program holds it, "unreachable" when
// nothing answers, "bad_answer" when the answers are not the printer's.
struct FlashforgeSession {
    bool ok = false;
    std::string code, error;
    std::vector<std::string> answers;
};
FlashforgeSession flashforge_tcp(const std::string& host, int port, const std::vector<std::string>& commands,
                                 int timeout_ms = 5000);

} // namespace tisma::device
