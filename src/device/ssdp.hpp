#pragma once

// Finding Bambu Lab printers on the local network (printer_connections.md,
// «Búsqueda en la red»): they announce themselves by SSDP (UPnP Device
// Architecture 1.1, section 1: NOTIFY messages over UDP, HTTP-like header
// lines) to UDP ports 1990 and 2021 (Bambu Lab's wiki, «Device
// discovering»). The exact fields are to be measured on a real printer
// (rule in the spec's table), so the reader is lenient: the printer's id is
// the USN, its address the sender's (or the Location), and its model and
// name any header whose name ends in «DevModel…» or «DevName…». Not tested
// on hardware. Own design; see PROVENANCE.md.

#include <map>
#include <optional>
#include <string>
#include <vector>

namespace tisma::device {

struct SsdpPrinter {
    std::string id;      // the announcement's USN: the printer's serial number
    std::string address; // its IP address
    std::string model, name;
    std::map<std::string, std::string> headers; // all of them, names in lower case
};

// One datagram from `sender` (an IP address); nothing when it is not an
// announcement with a USN.
std::optional<SsdpPrinter> parse_ssdp(const std::string& datagram, const std::string& sender);

struct SsdpListen {
    std::vector<SsdpPrinter> printers; // each once, by id
    // The ports another program holds (rule 5: Bambu Studio keeps 2021).
    std::vector<int> busy_ports;
};

// Listens on the given UDP ports (1990 and 2021 for the printers; the tests
// give their own) for `ms` milliseconds.
SsdpListen ssdp_listen(int ms, const std::vector<int>& ports = {1990, 2021});

} // namespace tisma::device
