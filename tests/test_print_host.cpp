// The network printer programs' client against a simulator built from
// OctoPrint's published REST API (printer_connections.md: «se prueban contra
// un simulador propio»).
#include "device/flashforge.hpp"
#include "device/print_host.hpp"
#include "device/sdcp.hpp"
#include "net/digest.hpp"
#include "net/websocket.hpp"
#include "device/printer_limits.hpp"
#include "fake_flashforge_net.hpp"
#include "fake_print_hosts.hpp"
#include "fake_sdcp.hpp"

#include <cmath>
#include <catch2/catch_test_macros.hpp>
#include <httplib.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

using namespace tisma::device;


TEST_CASE("print host: OctoPrint's REST API, test, upload and print, state, pause and cancel (printer_connections.md)",
          "[device][net]") {
    FakeOctoPrint fake;
    HostConfig host;
    host.kind = HostKind::OctoPrint;
    host.url = fake.url() + "/";
    host.api_key = "KEY";
    // The connection and the key.
    const HostVersion v = host_version(host);
    REQUIRE(v.ok);
    CHECK(v.server == "OctoPrint 1.10.2");
    HostConfig wrong = host;
    wrong.api_key = "nope";
    const HostVersion refused = host_version(wrong);
    CHECK_FALSE(refused.ok);
    CHECK(refused.code == "unauthorized");
    HostConfig nowhere = host;
    nowhere.url = "http://127.0.0.1:1";
    nowhere.timeout_s = 2;
    CHECK(host_version(nowhere).code == "unreachable");
    // Upload and print.
    const std::filesystem::path g = std::filesystem::temp_directory_path() / "tisma_print_host.gcode";
    std::ofstream(g, std::ios::binary) << "G28\nG1 X10 Y10 F3000\n";
    const HostResult up = host_upload(host, g.string(), "cube.gcode", true);
    REQUIRE(up.ok);
    CHECK(fake.uploaded_name == "cube.gcode");
    CHECK(fake.uploaded == "G28\nG1 X10 Y10 F3000\n");
    // A quote in the name does not end the form's header (BH53).
    REQUIRE(host_upload(host, g.string(), "big \"one\".gcode", false).ok);
    CHECK(fake.uploaded_name == "big _one_.gcode");
    REQUIRE(host_upload(host, g.string(), "cube.gcode", true).ok);
    CHECK(fake.started);
    // The state with the temperatures.
    HostStatus s = host_status(host);
    REQUIRE(s.ok);
    CHECK(s.printing);
    CHECK(s.file == "cube.gcode");
    CHECK(s.progress == 42.5);
    CHECK(s.seconds_left == 600.0);
    CHECK(s.nozzle == 214.8);
    CHECK(s.bed_target == 60.0);
    // Pause, resume, cancel.
    REQUIRE(host_pause(host, true).ok);
    CHECK(host_status(host).paused);
    REQUIRE(host_pause(host, false).ok);
    REQUIRE(host_cancel(host).ok);
    CHECK(fake.commands == std::vector<std::string>{"pause:pause", "pause:resume", "cancel"});
    CHECK_FALSE(host_status(host).printing);
    // No printer on the program: the state still reads, without temperatures.
    fake.board = false;
    s = host_status(host);
    CHECK(s.ok);
    CHECK(s.nozzle == -1.0);
    std::filesystem::remove(g);
}


TEST_CASE("print host: Moonraker's API (Klipper), the same calls (printer_connections.md)", "[device][net]") {
    FakeMoonraker fake;
    HostConfig host;
    host.kind = HostKind::Moonraker;
    host.url = fake.url();
    const HostVersion v = host_version(host);
    REQUIRE(v.ok);
    CHECK(v.server == "Moonraker v0.9.3 (Klipper ready)");
    const std::filesystem::path g = std::filesystem::temp_directory_path() / "tisma_print_host_k.gcode";
    std::ofstream(g, std::ios::binary) << "G28\n";
    REQUIRE(host_upload(host, g.string(), "part.gcode", true).ok);
    CHECK(fake.name == "part.gcode");
    CHECK(fake.started);
    const HostStatus s = host_status(host);
    REQUIRE(s.ok);
    CHECK(fake.asked);
    CHECK(s.printing);
    CHECK(s.progress == 25.0);
    CHECK(s.nozzle == 209.5);
    CHECK(s.bed_target == 65.0);
    REQUIRE(host_pause(host, true).ok);
    CHECK(host_status(host).paused);
    REQUIRE(host_pause(host, false).ok);
    REQUIRE(host_cancel(host).ok);
    CHECK(fake.calls == std::vector<std::string>{"pause", "resume", "cancel"});
    CHECK_FALSE(host_status(host).printing);
    // The printer's limits, for the profile's empty values.
    const HostLimits limits = host_limits(host);
    REQUIRE(limits.ok);
    const tisma::device::PrinterLimits read = parse_klipper_limits(limits.toolhead, limits.settings);
    CHECK(read.values.at("max_feed_x") == 300.0);
    CHECK(read.values.at("max_feed_z") == 15.0);
    CHECK(read.max_x == 235.0);
    std::filesystem::remove(g);
}

#include "net/digest.hpp"

TEST_CASE("digest: MD5 (RFC 1321's test suite) and the Digest response (RFC 2617's example)", "[device][net]") {
    using namespace tisma::net;
    CHECK(md5_hex("") == "d41d8cd98f00b204e9800998ecf8427e");
    CHECK(md5_hex("abc") == "900150983cd24fb0d6963f7d28e17f72");
    CHECK(md5_hex("message digest") == "f96b697d7cb7938d525a2f31aaf161d0");
    CHECK(md5_hex("12345678901234567890123456789012345678901234567890123456789012345678901234567890") ==
          "57edf4a22be3c955ac49da2e2107b67a");
    DigestChallenge ch;
    REQUIRE(parse_digest_challenge(R"(Digest realm="testrealm@host.com", qop="auth,auth-int", nonce="dcd98b7102dd2f0e8b11d0f600bfb0c093", opaque="5ccc069c403ebaf9f0171e9517f40e41")",
                                   ch));
    CHECK(ch.realm == "testrealm@host.com");
    CHECK(ch.opaque == "5ccc069c403ebaf9f0171e9517f40e41");
    const std::string h = digest_authorization(ch, "Mufasa", "Circle Of Life", "GET", "/dir/index.html", "0a4f113b", 1);
    CHECK(h.find("response=\"6629fae49393a05397450978507c4ef1\"") != std::string::npos);
    CHECK(h.find("nc=00000001") != std::string::npos);
    CHECK_FALSE(parse_digest_challenge("Basic realm=\"x\"", ch));
}


TEST_CASE("print host: PrusaLink's API with Digest, upload and print, state, pause, resume and stop (printer_connections.md)",
          "[device][net]") {
    FakePrusaLink fake;
    HostConfig host;
    host.kind = HostKind::PrusaLink;
    host.url = fake.url();
    host.user = "maker";
    host.password = "secret";
    const HostVersion v = host_version(host);
    INFO(v.error);
    REQUIRE(v.ok);
    CHECK(v.server == "PrusaLink 2.1.2");
    HostConfig wrong = host;
    wrong.password = "nope";
    CHECK(host_version(wrong).code == "unauthorized");
    CHECK(host_cancel(host).code == "conflict"); // no job yet
    const std::filesystem::path g = std::filesystem::temp_directory_path() / "tisma_print_host_p.gcode";
    std::ofstream(g, std::ios::binary) << "G28\nM104 S215\n";
    // A name with spaces, # and an accent, as typed (BH53): percent-encoded
    // in the path, the same name on the printer.
    REQUIRE(host_upload(host, g.string(), "Mi pieza #2 ñ.gcode", false).ok);
    CHECK(fake.name == "Mi pieza #2 ñ.gcode");
    REQUIRE(host_upload(host, g.string(), "cube.gcode", true).ok);
    CHECK(fake.name == "cube.gcode");
    CHECK(fake.body == "G28\nM104 S215\n");
    CHECK(fake.started);
    const HostStatus s = host_status(host);
    REQUIRE(s.ok);
    CHECK(s.printing);
    CHECK(s.file == "cube.gcode"); // the long name, from the job
    CHECK(s.progress == 12.0);
    CHECK(s.seconds_left == 1800.0);
    CHECK(s.nozzle_target == 215.0);
    REQUIRE(host_pause(host, true).ok);
    CHECK(host_status(host).paused);
    REQUIRE(host_pause(host, false).ok);
    REQUIRE(host_cancel(host).ok);
    CHECK(fake.calls == std::vector<std::string>{"pause 7", "resume 7", "stop 7"});
    HostKind k;
    CHECK(host_kind_from("prusalink", k));
    CHECK(k == HostKind::PrusaLink);
    CHECK(host_kind_from("duet", k));
    CHECK(k == HostKind::Duet);
    CHECK_FALSE(host_kind_from("flashair", k));
    std::filesystem::remove(g);
}


TEST_CASE("print host: Duet (RepRapFirmware standalone), session, upload and print, state, pause and cancel "
          "(printer_connections.md)",
          "[device][net]") {
    FakeDuet fake;
    HostConfig host;
    host.kind = HostKind::Duet;
    host.url = fake.url();
    host.password = "secret";
    const HostVersion v = host_version(host);
    INFO(v.error);
    REQUIRE(v.ok);
    CHECK(v.server == "RepRapFirmware 3.5.2 (Duet 3 Mini 5+)");
    HostConfig wrong = host;
    wrong.password = "nope";
    CHECK(host_version(wrong).code == "unauthorized");

    const std::filesystem::path g = std::filesystem::temp_directory_path() / "tisma_print_host_d.gcode";
    std::ofstream(g, std::ios::binary) << "G28\n";
    REQUIRE(host_upload(host, g.string(), "cube.gcode", true).ok);
    CHECK(fake.name == "0:/gcodes/cube.gcode");
    CHECK(fake.body == "G28\n");
    CHECK(fake.crc == "f722db83"); // CRC-32 of «G28\n»
    REQUIRE_FALSE(fake.codes.empty());
    CHECK(fake.codes.back() == "M32 \"0:/gcodes/cube.gcode\"");

    const HostStatus s = host_status(host);
    INFO(s.error);
    REQUIRE(s.ok);
    CHECK(s.printing);
    CHECK_FALSE(s.paused);
    CHECK(s.file == "0:/gcodes/cube.gcode");
    CHECK(s.progress == 25.0);
    CHECK(s.seconds_left == 900.0); // no slicer estimate: the file's
    CHECK(s.nozzle == 210.2);
    CHECK(s.nozzle_target == 215.0);
    CHECK(s.bed == 59.5);
    CHECK(s.bed_target == 60.0);

    REQUIRE(host_pause(host, true).ok);
    CHECK(host_status(host).paused);
    REQUIRE(host_pause(host, false).ok);
    // Upload and simulate (devices.md, rule 20): M37 with the file.
    REQUIRE(host_upload(host, g.string(), "sim.gcode", false, true).ok);
    CHECK(fake.codes.back() == "M37 P\"0:/gcodes/sim.gcode\"");
    fake.status = "processing";
    // Cancel while printing: paused first, then M0.
    REQUIRE(host_cancel(host).ok);
    const std::vector<std::string> tail(fake.codes.end() - 5, fake.codes.end());
    CHECK(tail == std::vector<std::string>{"M25", "M24", "M37 P\"0:/gcodes/sim.gcode\"", "M25", "M0"});
    CHECK(fake.sessions == 0); // every session closed
    std::filesystem::remove(g);
}

#include "device/mdns.hpp"

TEST_CASE("print host: Moonraker found on the network, confirmed by its server info (printer_connections.md)",
          "[device][net]") {
    FakeMoonraker fake;
    // The service's address with its route prefix.
    MdnsService service;
    service.instance = "Moonraker @ voron._moonraker._tcp.local";
    service.address = "127.0.0.1";
    service.port = 7125;
    service.txt = {{"uuid", "1234abcd"}, {"route_prefix", ""}, {"https_port", ""}, {"version", "v0.9.3"}};
    CHECK(service.url() == "http://127.0.0.1:7125/");
    service.txt["route_prefix"] = "/klipper";
    CHECK(service.url() == "http://127.0.0.1:7125/klipper");
    // The one that answers, once; one where nothing listens is left out.
    const std::vector<FoundHost> found = confirm_moonraker(
        {{"Moonraker @ voron", fake.url() + "/"}, {"Moonraker @ voron", fake.url()}, {"Moonraker @ gone", "http://127.0.0.1:1"}},
        2);
    REQUIRE(found.size() == 1);
    CHECK(found[0].name == "Moonraker @ voron");
    CHECK(found[0].url == fake.url());
    CHECK(found[0].server == "Moonraker v0.9.3 (Klipper ready)");
}


TEST_CASE("print host: Repetier-Server's Web API, test, upload and print, state, continue and stop (devices.md)",
          "[device][net]") {
    FakeRepetier fake;
    HostConfig host;
    host.kind = HostKind::Repetier;
    host.url = fake.url();
    host.api_key = "KEY";
    const HostVersion v = host_version(host);
    INFO(v.error);
    REQUIRE(v.ok);
    CHECK(v.server == "Repetier-Server 1.4.10");
    HostConfig wrong = host;
    wrong.api_key = "nope";
    CHECK(host_version(wrong).code == "unauthorized");
    HostConfig other = host;
    other.printer = "Prusa";
    CHECK_FALSE(host_version(other).ok);

    const std::filesystem::path g = std::filesystem::temp_directory_path() / "tisma_print_host_r.gcode";
    std::ofstream(g, std::ios::binary) << "G28\n";
    REQUIRE(host_upload(host, g.string(), "cube 1.gcode", false).ok);
    CHECK_FALSE(fake.to_jobs);
    CHECK(fake.uploaded_name == "cube 1.gcode");
    CHECK(fake.uploaded == "G28\n");
    CHECK_FALSE(host_status(host).printing);
    REQUIRE(host_upload(host, g.string(), "cube.gcode", true).ok);
    CHECK(fake.to_jobs);

    const HostStatus s = host_status(host);
    INFO(s.error);
    REQUIRE(s.ok);
    CHECK(s.printing);
    CHECK(s.file == "cube.gcode");
    CHECK(s.progress == 12.5);
    CHECK(s.nozzle == 199.5);
    CHECK(s.nozzle_target == 200.0);
    CHECK(s.bed == 54.0);
    CHECK(s.bed_target == 55.0);

    CHECK(host_pause(host, true).code == "unsupported");
    REQUIRE(host_pause(host, false).ok);
    REQUIRE(host_cancel(host).ok);
    CHECK(fake.commands.back() == "Ender_3:stopJob");
    CHECK_FALSE(host_status(host).printing);
    std::filesystem::remove(g);
}

TEST_CASE("print host: AstroBox answers OctoPrint's version and upload calls (devices.md, rule 24)", "[device][net]") {
    FakeOctoPrint fake;
    HostConfig host;
    host.kind = HostKind::AstroBox;
    host.url = fake.url();
    host.api_key = "KEY";
    REQUIRE(host_version(host).ok);
    const std::filesystem::path g = std::filesystem::temp_directory_path() / "tisma_print_host_a.gcode";
    std::ofstream(g, std::ios::binary) << "G28\n";
    REQUIRE(host_upload(host, g.string(), "cube.gcode", true).ok);
    CHECK(fake.started);
    CHECK(host_status(host).code == "unsupported");
    CHECK(host_cancel(host).code == "unsupported");
    HostKind k;
    REQUIRE(host_kind_from("repetier", k));
    CHECK(k == HostKind::Repetier);
    CHECK(std::string(host_kind_name(HostKind::Repetier)) == "Repetier-Server");
    std::filesystem::remove(g);
}

TEST_CASE("print host: Flashforge in LAN mode, test, upload and print, state, pause and cancel (FF1)", "[device][net]") {
    FakeFlashforge fake;
    HostConfig host;
    host.kind = HostKind::Flashforge;
    host.url = fake.url();
    host.printer = "SNFAKE123";
    host.api_key = "0a1b2c3d";
    const HostVersion v = host_version(host);
    INFO(v.error);
    REQUIRE(v.ok);
    CHECK(v.server == "Flashforge Adventurer 5M");
    // FF1.2: a wrong check code is said once, with what to check.
    HostConfig wrong = host;
    wrong.api_key = "ffffffff";
    const int before = fake.details;
    const HostVersion w = host_version(wrong);
    CHECK(w.code == "unauthorized");
    CHECK(w.error.find("check code") != std::string::npos);
    CHECK(fake.details == before + 1);

    // FF1.3: the upload, printing and levelling, its real size in fileSize.
    const std::filesystem::path g = std::filesystem::temp_directory_path() / "tisma_print_host_ff.gcode";
    const std::string gcode = "G28\nG1 X10 Y10 E1\n";
    std::ofstream(g, std::ios::binary) << gcode;
    host.level_before_print = true;
    const HostResult up = host_upload(host, g.string(), "pieza.gcode", true);
    INFO(up.error);
    REQUIRE(up.ok);
    CHECK(fake.uploaded == gcode);
    CHECK(fake.uploaded_name == "pieza.gcode");
    CHECK(fake.file_size == std::to_string(gcode.size()));
    CHECK(fake.print_now == "true");
    CHECK(fake.leveling == "true");
    const HostStatus st = host_status(host);
    REQUIRE(st.ok);
    CHECK(st.printing);
    CHECK(st.progress == 25.0); // 0 to 1 on the printer
    CHECK(st.nozzle == 210.5);
    CHECK(st.bed_target == 55.0);
    CHECK(st.seconds_left == 1800.0);
    CHECK(st.file == "pieza.gcode");

    // FF1.4: pause and cancel by jobCtl_cmd, the state read from /detail.
    REQUIRE(host_pause(host, true).ok);
    CHECK(host_status(host).paused);
    REQUIRE(host_pause(host, false).ok);
    REQUIRE(host_cancel(host).ok);
    CHECK(fake.controls == std::vector<std::string>{"jobCtl_cmd:pause", "jobCtl_cmd:continue", "jobCtl_cmd:cancel"});
    CHECK(host_status(host).state == "cancel");
    std::filesystem::remove(g);

    // FF1.6: nothing listens: «cannot reach», with what to check, in time.
    HostConfig closed = host;
    closed.url = "http://127.0.0.1:9";
    closed.timeout_s = 3;
    const HostVersion c = host_version(closed);
    CHECK(c.code == "unreachable");
    CHECK(c.error.find("LAN-only") != std::string::npos);
}

TEST_CASE("print host: Flashforge found by UDP, newer and older (FF1.1)", "[device][net]") {
    const auto modern = parse_flashforge_discovery(fake_flashforge::Discovery::modern(), "192.168.1.30");
    REQUIRE(modern);
    CHECK(modern->modern);
    CHECK(modern->name == "Adventurer 5M");
    CHECK(modern->serial == "SNFAKE123");
    CHECK(modern->http_port == 8898);
    CHECK(modern->tcp_port == 8899);
    const auto legacy = parse_flashforge_discovery(fake_flashforge::Discovery::legacy(), "192.168.1.31");
    REQUIRE(legacy);
    CHECK_FALSE(legacy->modern);
    CHECK(legacy->name == "Adventurer 4");
    CHECK(legacy->serial.empty()); // until M115
    CHECK_FALSE(parse_flashforge_discovery(std::string(100, 'x'), "1.2.3.4"));

    fake_flashforge::Discovery fake;
    const auto found = flashforge_discover(1500, {{"127.0.0.1", fake.port()}});
    REQUIRE(found.size() == 2);
    CHECK(std::count_if(found.begin(), found.end(), [](const FlashforgePrinter& p) { return p.modern && p.serial == "SNFAKE123"; }) == 1);
    CHECK(std::count_if(found.begin(), found.end(), [](const FlashforgePrinter& p) { return !p.modern && p.serial.empty(); }) == 1);
}

TEST_CASE("print host: an older Flashforge over TCP 8899, its session, state and job (FF1.5)", "[device][net]") {
    fake_flashforge::Legacy fake;
    HostConfig host;
    host.kind = HostKind::FlashforgeLegacy;
    host.url = "127.0.0.1:" + std::to_string(fake.port());
    host.timeout_s = 8;
    const HostVersion v = host_version(host);
    INFO(v.error);
    REQUIRE(v.ok);
    CHECK(v.server == "Flashforge Flashforge Adventurer 4 (SNOLD456)");
    const HostStatus st = host_status(host);
    REQUIRE(st.ok);
    CHECK(st.printing);
    CHECK(st.file == "cubo.gx");
    CHECK(st.nozzle == 205.0);
    CHECK(st.nozzle_target == 210.0);
    CHECK(st.bed == 58.0);
    REQUIRE(host_pause(host, true).ok);
    CHECK(host_status(host).paused);
    REQUIRE(host_cancel(host).ok);
    const std::vector<std::string> c = fake.commands();
    CHECK(std::count(c.begin(), c.end(), "M601 S1") == std::count(c.begin(), c.end(), "M602")); // each session released
    CHECK(std::find(c.begin(), c.end(), "M25") != c.end());
    CHECK(std::find(c.begin(), c.end(), "M26") != c.end());
    // Sending a file is not offered: not confirmed (the spec).
    CHECK(host_upload(host, "x.gcode", "x.gcode", true).code == "unsupported");

    // FF1.5: another program holds it: said so, and nothing else sent.
    fake_flashforge::Legacy busy(true);
    HostConfig other = host;
    other.url = "127.0.0.1:" + std::to_string(busy.port());
    const HostVersion b = host_version(other);
    CHECK(b.code == "conflict");
    CHECK(b.error.find("another program") != std::string::npos);
    CHECK(busy.commands() == std::vector<std::string>{"M601 S1"});
}

TEST_CASE("websocket: SHA-1 and Base64 of the handshake (FIPS 180-4, RFC 4648, RFC 6455)", "[net]") {
    const auto hex = [](const std::string& b) {
        std::string out;
        char two[3];
        for (const char c : b) {
            std::snprintf(two, sizeof two, "%02x", std::uint8_t(c));
            out += two;
        }
        return out;
    };
    CHECK(hex(tisma::net::sha1("abc")) == "a9993e364706816aba3e25717850c26c9cd0d89d");
    CHECK(hex(tisma::net::sha1("")) == "da39a3ee5e6b4b0d3255bfef95601890afd80709");
    CHECK(tisma::net::base64("foobar") == "Zm9vYmFy");
    CHECK(tisma::net::base64("fooba") == "Zm9vYmE=");
    CHECK(tisma::net::base64("foob") == "Zm9vYg==");
    // RFC 6455, 1.3: the sample key and its accept.
    CHECK(tisma::net::base64(tisma::net::sha1("dGhlIHNhbXBsZSBub25jZQ==258EAFA5-E914-47DA-95CA-C5AB0DC85B11")) ==
          "s3pPLMBiTxaQ9kYGzzhZRbK+xOo=");
}

TEST_CASE("print host: Elegoo by SDCP, found, state, upload in pieces, print, pause and stop (EL1)", "[device][net]") {
    fake_sdcp::Printer fake;
    // EL1.1: two equal answers to M99999, one printer.
    const std::vector<SdcpPrinter> found = sdcp_discover(1000, {{"127.0.0.1", fake.udp_port()}});
    REQUIRE(found.size() == 1);
    CHECK(found[0].name == "Centauri Carbon");
    CHECK(found[0].address == "127.0.0.1");
    CHECK(found[0].mainboard_id == fake_sdcp::Printer::kId);

    HostConfig host;
    host.kind = HostKind::Elegoo;
    host.url = "127.0.0.1:" + std::to_string(fake.port());
    host.printer = fake_sdcp::Printer::kId;
    host.timeout_s = 10;
    const HostVersion v = host_version(host);
    INFO(v.error);
    REQUIRE(v.ok);
    const HostStatus idle = host_status(host);
    REQUIRE(idle.ok);
    CHECK(idle.state == "idle");
    CHECK(idle.nozzle == 215.0);
    CHECK(idle.bed_target == 60.0);

    // EL1.2: 2.5 MB in three pieces, one Uuid, the whole size and MD5.
    std::string data(2621440, 'G');
    for (std::size_t i = 0; i < data.size(); i += 97)
        data[i] = char('0' + i % 10);
    const std::filesystem::path g = std::filesystem::temp_directory_path() / "tisma_print_host_sdcp.gcode";
    std::ofstream(g, std::ios::binary) << data;
    const HostResult up = host_upload(host, g.string(), "pieza.gcode", true);
    INFO(up.error);
    REQUIRE(up.ok);
    {
        std::lock_guard lock(fake.mutex);
        REQUIRE(fake.pieces.size() == 3);
        CHECK(fake.pieces[0].offset == "0");
        CHECK(fake.pieces[1].offset == "1048576");
        CHECK(fake.pieces[2].offset == "2097152");
        CHECK(fake.pieces[0].uuid == fake.pieces[2].uuid);
        CHECK(fake.pieces[1].total == std::to_string(data.size()));
        CHECK(fake.pieces[2].md5 == tisma::net::md5_hex(data));
        CHECK(fake.file == data);
        REQUIRE(!fake.commands.empty());
        CHECK(fake.commands.back().first == 128);
    }
    const HostStatus printing = host_status(host);
    CHECK(printing.printing);
    CHECK(printing.progress == 25.0);
    CHECK(printing.file == "pieza.gcode");

    // EL1.5: pause and stop, each answer matched by its RequestID.
    REQUIRE(host_pause(host, true).ok);
    CHECK(host_status(host).state == "paused");
    REQUIRE(host_cancel(host).ok);
    CHECK(host_status(host).state == "stopped");
    {
        std::lock_guard lock(fake.mutex);
        REQUIRE(fake.answered.size() == fake.commands.size());
        for (std::size_t i = 0; i < fake.commands.size(); ++i)
            CHECK(fake.answered[i] == fake.commands[i].second);
    }

    // EL1.3: a piece refused: the upload stops and says why.
    {
        std::lock_guard lock(fake.mutex);
        fake.pieces.clear();
        fake.fail_piece = 1;
    }
    const HostResult bad = host_upload(host, g.string(), "otra.gcode", true);
    CHECK_FALSE(bad.ok);
    CHECK(bad.error == "the upload failed (the offset did not match)");
    {
        std::lock_guard lock(fake.mutex);
        CHECK(fake.pieces.size() == 2); // nothing after the refused one
        fake.fail_piece = -1;
        fake.status = 0;
        fake.start_ack = 1;
    }
    // EL1.4: Ack 1 to start: busy, and not printing.
    const HostResult busy = host_upload(host, g.string(), "otra.gcode", true);
    CHECK(busy.code == "conflict");
    CHECK(busy.error == "the printer is busy");
    CHECK_FALSE(host_status(host).printing);
    std::filesystem::remove(g);

    // EL1.6: the sixth connection refused with a 500.
    fake.too_many = true;
    const HostStatus many = host_status(host);
    CHECK(many.code == "busy");
    CHECK(many.error == "the printer has too many connections open");
}

TEST_CASE("print host: a stock Creality, its state by 9999 or Moonraker when it answers (CR1)", "[device][net]") {
    fake_sdcp::Creality fake;
    HostConfig host;
    host.kind = HostKind::Creality;
    host.url = "127.0.0.1:" + std::to_string(fake.port());
    host.timeout_s = 5;
    // Moonraker answers nowhere: the printer's own WebSocket.
#ifdef _WIN32
    _putenv_s("TISMA_TEST_CREALITY_MOONRAKER_PORT", "9");
#else
    setenv("TISMA_TEST_CREALITY_MOONRAKER_PORT", "9", 1);
#endif
    // CR1-3: model empty, the board's code.
    const HostVersion v = host_version(host);
    INFO(v.error);
    REQUIRE(v.ok);
    CHECK(v.server == "Creality (board F012) K1C-ABCD");
    // CR1-2: the times pushed, in seconds.
    const HostStatus st = host_status(host);
    REQUIRE(st.ok);
    CHECK(st.seconds_done == 600.0);
    CHECK(st.seconds_left == 1200.0);
    CHECK(std::abs(st.progress - 100.0 / 3.0) < 1e-9);
    CHECK(st.printing);
    // Sending over 9999 is not tried: the message says what to turn on.
    const HostResult up = host_upload(host, "x.gcode", "x.gcode", true);
    CHECK(up.code == "unsupported");
    CHECK(up.error.find("Moonraker/Fluidd") != std::string::npos);
    // CR1-5: the link cut mid-print: still printing, reconnecting.
    fake.down = true;
    const HostStatus cut = host_status(host);
    CHECK(cut.printing);
    CHECK(cut.state == "reconnecting");

    // CR1-1: Moonraker answers on its port: that connection, not 9999's.
    FakeMoonraker moon;
    const std::string port = moon.url().substr(moon.url().rfind(':') + 1);
#ifdef _WIN32
    _putenv_s("TISMA_TEST_CREALITY_MOONRAKER_PORT", port.c_str());
#else
    setenv("TISMA_TEST_CREALITY_MOONRAKER_PORT", port.c_str(), 1);
#endif
    const HostVersion mv = host_version(host);
    INFO(mv.error);
    REQUIRE(mv.ok);
    CHECK(mv.server.find("Moonraker") != std::string::npos);
#ifdef _WIN32
    _putenv_s("TISMA_TEST_CREALITY_MOONRAKER_PORT", "");
#else
    unsetenv("TISMA_TEST_CREALITY_MOONRAKER_PORT");
#endif
}
