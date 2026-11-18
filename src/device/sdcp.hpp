#pragma once

// Elegoo printers on the local network by SDCP v3 (printer_connections.md,
// «Impresoras Elegoo por red local»; CBD-Tech's published SDCP V3.0.0):
// found by «M99999» on UDP port 3000, their JSON messages over a WebSocket on
// ws://<IP>:3030/websocket, files sent by HTTP in 1 MB pieces. Own design;
// see PROVENANCE.md.

#include <nlohmann/json.hpp>

#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace tisma::device {

struct SdcpPrinter {
    std::string name, machine, brand, address, mainboard_id, firmware;
};

// One answer to M99999 (its JSON: Data.Name, MachineName, BrandName,
// MainboardIP, MainboardID…); nothing for anything else.
std::optional<SdcpPrinter> parse_sdcp_discovery(const std::string& datagram, const std::string& sender);

// M99999 by broadcast to UDP 3000 (and to each of `extra`, host:port), the
// answers collected for `ms` milliseconds, each MainboardID once.
std::vector<SdcpPrinter> sdcp_discover(int ms, const std::vector<std::pair<std::string, int>>& extra = {});

// The words for an Ack of a command (§6 and §7 of the spec) and for
// PrintInfo.Status, in English.
std::string sdcp_ack_text(int command, int ack);
const char* sdcp_print_state(int status);

} // namespace tisma::device
