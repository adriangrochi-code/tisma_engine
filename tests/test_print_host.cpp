// The network printer programs' client against a simulator built from
// OctoPrint's published REST API (printer_connections.md: «se prueban contra
// un simulador propio»).
#include "device/print_host.hpp"
#include "device/printer_limits.hpp"
#include "fake_print_hosts.hpp"

#include <catch2/catch_test_macros.hpp>
#include <httplib.h>
#include <nlohmann/json.hpp>

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
