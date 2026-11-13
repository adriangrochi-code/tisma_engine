#pragma once

// Finding OctoPrint on the local network (printer_connections.md, «Búsqueda
// en la red»): Zeroconf, that is multicast DNS (RFC 6762) with DNS-based
// service discovery (RFC 6763), over the DNS message format (RFC 1035,
// section 4, with its name compression). A question for the PTR records of
// a service type («_octoprint._tcp.local») goes to 224.0.0.251, port 5353,
// asking for unicast answers (the QU bit, RFC 6762 section 5.4); each answer
// gives the service's instance (PTR), host and port (SRV), its TXT strings
// (OctoPrint's documentation: path, version, api, model, vendor) and the
// host's IPv4 address (A). Own design; see PROVENANCE.md. Not tested with a
// real printer.

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace tisma::device {

struct MdnsService {
    std::string instance;                   // «OctoPrint on octopi._octoprint._tcp.local»
    std::string host;                       // «octopi.local»
    std::string address;                    // its IPv4 address, when the answer gave it
    int port = 0;
    std::map<std::string, std::string> txt; // «path» → «/», «model» → …
    // The program's address: http://address:port + path (OctoPrint's
    // Discovery documentation: the server is the service's plus «path»;
    // Moonraker's `_moonraker._tcp` service: plus «route_prefix»).
    std::string url() const;
};

// The question for a service type's PTR records, with the QU bit.
std::vector<std::uint8_t> mdns_query(const std::string& service);
// The services of that type in one answer, with what its records say;
// records of other names are left out. Never throws on a damaged packet.
std::vector<MdnsService> parse_mdns(const std::vector<std::uint8_t>& packet, const std::string& service);

// Asks the network for `service` and listens for `ms` milliseconds; each
// instance once. Empty when the network does not let it (the port taken, no
// multicast).
std::vector<MdnsService> mdns_browse(const std::string& service, int ms);

} // namespace tisma::device
