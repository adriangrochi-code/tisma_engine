// Bambu Lab printers on the local network (printer_connections.md, «Bambu Lab
// en red local»; devices.md, rules 11 and 16) against the tests' own
// simulator (bambu_sim.hpp). Not tested on hardware.

#include "device/bambu_simulator.hpp"
#include "device/bambu.hpp"
#include "device/ftps.hpp"
#include "device/mqtt.hpp"
#include "device/mdns.hpp"
#include "device/ssdp.hpp"
#include "io/zip.hpp"
#include "net/digest.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <nlohmann/json.hpp>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include <future>
#include <thread>

using namespace tisma;
using namespace tisma::device;
using Catch::Matchers::ContainsSubstring;

namespace {

constexpr const char* kSerial = "01P00A123456789";
constexpr const char* kCode = "12345678";

BambuConfig config_for(device::BambuSimulator& sim) {
    BambuConfig c;
    c.host = "127.0.0.1";
    c.access_code = kCode;
    c.pinned_sha256 = sim.sha256();
    c.mqtt_port = sim.mqtt_port();
    c.ftps_port = sim.ftps_port();
    c.camera_port = sim.camera_port();
    c.timeout_ms = 5000;
    return c;
}

} // namespace

TEST_CASE("MQTT 3.1.1 packets: written, read back in pieces, and a long length", "[bambu][mqtt]") {
    mqtt::Reader r;
    const std::string connect = mqtt::connect_packet({"tisma-1", "bblp", "secret", 60});
    const std::string big(300, 'x'); // a remaining length of two bytes
    const std::string publish = mqtt::publish_packet({"device/x/request", big, 1, 7});
    const std::string all = connect + publish + mqtt::subscribe_packet(3, "device/x/report", 0);
    for (const char c : all) // one byte at a time
        r.feed(std::string(1, c));
    std::optional<mqtt::Packet> p = r.next();
    REQUIRE(p);
    const mqtt::Connect c = mqtt::parse_connect(*p);
    CHECK(c.client_id == "tisma-1");
    CHECK(c.user == "bblp");
    CHECK(c.password == "secret");
    CHECK(c.keepalive_s == 60);
    p = r.next();
    REQUIRE(p);
    const mqtt::Publish m = mqtt::parse_publish(*p);
    CHECK(m.topic == "device/x/request");
    CHECK(m.payload == big);
    CHECK(m.qos == 1);
    CHECK(m.id == 7);
    p = r.next();
    REQUIRE(p);
    CHECK(p->flags == 2); // SUBSCRIBE's fixed flags (section 3.8.1)
    std::uint16_t id = 0;
    CHECK(mqtt::parse_subscribe(*p, id) == std::vector<std::string>{"device/x/report"});
    CHECK(id == 3);
    CHECK_FALSE(r.next());
    mqtt::Reader bad;
    bad.feed(std::string("\x30\xFF\xFF\xFF\xFF\x01", 6));
    CHECK_THROWS_AS(bad.next(), mqtt::MqttError);
}

TEST_CASE("FTPS: the passive port of a 227 reply", "[bambu][ftps]") {
    CHECK(pasv_port("227 Entering Passive Mode (192,168,1,20,195,80)") == 195 * 256 + 80);
    CHECK(pasv_port("227 Entering Passive Mode (0,0,0,0,4,1).") == 1025);
    CHECK(pasv_port("227 nothing") == -1);
    CHECK(pasv_port("200 (1,2,3,4,5,6)") == -1);
    CHECK(pasv_port("227 (1,2,3,4,5,600)") == -1);
}

TEST_CASE("SSDP: a printer's announcement read, and heard on a UDP port", "[bambu][ssdp]") {
    const std::string notify = "NOTIFY * HTTP/1.1\r\nHOST: 239.255.255.250:1990\r\nServer: UPnP/1.0\r\n"
                               "Location: 192.168.1.20\r\nNT: urn:bambulab-com:device:3dprinter:1\r\n"
                               "USN: 01P00A123456789\r\nDevModel.bambu.com: C11\r\nDevName.bambu.com: Taller\r\n\r\n";
    const std::optional<SsdpPrinter> p = parse_ssdp(notify, "192.168.1.20");
    REQUIRE(p);
    CHECK(p->id == "01P00A123456789");
    CHECK(p->address == "192.168.1.20");
    CHECK(p->model == "C11");
    CHECK(p->name == "Taller");
    CHECK_FALSE(parse_ssdp("GET / HTTP/1.1\r\n\r\n", "1.2.3.4"));
    CHECK_FALSE(parse_ssdp("NOTIFY * HTTP/1.1\r\nHOST: x\r\n\r\n", "1.2.3.4")); // no USN

    // Heard on a port of the test's: sent twice, listed once.
    constexpr int kPort = 42021;
    auto heard = std::async(std::launch::async, [] { return ssdp_listen(1500, {kPort}); });
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
#ifdef _WIN32
    const SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
#else
    const int s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
#endif
    sockaddr_in to{};
    to.sin_family = AF_INET;
    to.sin_port = htons(kPort);
    to.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    for (int i = 0; i < 2; ++i)
        sendto(s, notify.data(), int(notify.size()), 0, reinterpret_cast<sockaddr*>(&to), int(sizeof to));
#ifdef _WIN32
    closesocket(s);
#else
    close(s);
#endif
    const SsdpListen found = heard.get();
    REQUIRE(found.printers.size() == 1);
    CHECK(found.printers[0].id == "01P00A123456789");
    CHECK(found.printers[0].address == "127.0.0.1"); // the sender's
}

TEST_CASE("Bambu Lab: the certificate is confirmed once, then the state with the AMS (rules 2 to 4)", "[bambu]") {
    device::BambuSimulator sim(kSerial, kCode);
    BambuConfig c = config_for(sim);
    // First time: nothing is sent, the person sees the serial and the
    // certificate's fingerprint.
    c.pinned_sha256.clear();
    BambuStatus s = bambu_status(c);
    CHECK_FALSE(s.ok);
    CHECK(s.code == "confirm_certificate");
    CHECK(s.serial == kSerial); // the certificate's name (rule 3)
    CHECK(s.sha256 == sim.sha256());
    // Accepted: the state.
    c.pinned_sha256 = s.sha256;
    s = bambu_status(c);
    INFO(s.error);
    REQUIRE(s.ok);
    CHECK(s.state == "IDLE");
    CHECK_FALSE(s.printing);
    CHECK(s.nozzle == 24.5);
    CHECK(s.bed == 23.0);
    CHECK(s.nozzle_diameter == "0.4");
    REQUIRE(s.trays.size() == 4);
    CHECK(s.trays[0].type == "PLA");
    CHECK(s.trays[0].color == "F95959");
    CHECK(s.trays[0].remain == 80);
    CHECK(s.trays[1].type.empty()); // an empty slot
    CHECK(s.trays[2].type == "PETG");
    CHECK(s.tray_now == 255);
    // Another certificate, or a wrong access code.
    c.pinned_sha256 = std::string(64, '0');
    CHECK(bambu_status(c).code == "certificate_changed");
    c.pinned_sha256 = sim.sha256();
    c.access_code = "00000000";
    CHECK(bambu_status(c).code == "unauthorized");
    // Nobody there.
    c.mqtt_port = 1;
    CHECK(bambu_status(c).code == "unreachable");
}

TEST_CASE("Bambu Lab: a P1's partial reports are merged into the whole state", "[bambu]") {
    nlohmann::json kept;
    bambu_merge(kept, {{"gcode_state", "RUNNING"}, {"mc_percent", 10}, {"ams", {{"tray_now", "1"}}}});
    bambu_merge(kept, {{"mc_percent", 42}, {"ams", {{"tray_pre", "0"}}}});
    const BambuStatus s = bambu_status_from(kept);
    CHECK(s.printing);
    CHECK(s.progress == 42.0);
    CHECK(s.tray_now == 1);
    CHECK(kept["ams"]["tray_pre"] == "0");

    device::BambuSimulator sim(kSerial, kCode);
    sim.partial_first = true;
    const BambuStatus whole = bambu_status(config_for(sim));
    REQUIRE(whole.ok);
    CHECK(whole.state == "IDLE");
    CHECK(whole.trays.size() == 4);
}

TEST_CASE("Bambu Lab: print from the AMS, uploaded by FTPS with the TLS session resumed (rule 5)", "[bambu]") {
    device::BambuSimulator sim(kSerial, kCode);
    BambuPrint job;
    job.name = "pieza: 1";
    job.gcode = "; generated by Tisma engine\nG28\nG1 X10 Y10 E1\n";
    job.use_ams = true;
    job.ams_mapping = {-1, -1, -1, -1, 2};
    std::size_t last = 0, total = 0;
    const BambuResult r = bambu_print(config_for(sim), job, [&](std::size_t sent, std::size_t all) {
        last = sent;
        total = all;
    });
    INFO(r.error);
    REQUIRE(r.ok);
    CHECK(last == total);
    // The package on the card: a 3MF with the G-code and its MD5.
    const auto files = sim.files();
    REQUIRE(files.count("pieza_ 1.gcode.3mf") == 1);
    const std::string& package = files.at("pieza_ 1.gcode.3mf");
    const ZipArchive zip(package);
    const ZipArchive::Entry* gcode = zip.find("Metadata/plate_1.gcode");
    REQUIRE(gcode);
    CHECK(zip.read(*gcode) == job.gcode);
    const ZipArchive::Entry* md5 = zip.find("Metadata/plate_1.gcode.md5");
    REQUIRE(md5);
    CHECK(zip.read(*md5) == net::md5_hex(job.gcode));
    // And the command that starts it.
    std::vector<nlohmann::json> commands;
    for (int i = 0; i < 50 && commands.empty(); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        commands = sim.commands();
    }
    REQUIRE(commands.size() == 1);
    const nlohmann::json& p = commands[0];
    CHECK(p["command"] == "project_file");
    CHECK(p["param"] == "Metadata/plate_1.gcode");
    CHECK(p["subtask_name"] == "pieza: 1");
    CHECK(p["file"] == "pieza_ 1.gcode.3mf");
    CHECK(p["md5"] == net::md5_hex(package));
    CHECK(p["use_ams"] == true);
    CHECK(p["ams_mapping"] == nlohmann::json::array({-1, -1, -1, -1, 2}));
    CHECK(sim.state()["gcode_state"] == "PREPARE");
    // The data connection resumed the control connection's session.
    FtpsConfig f;
    f.host = "127.0.0.1";
    f.port = sim.ftps_port();
    f.user = "bblp";
    f.password = kCode;
    CHECK(ftps_upload(f, "x.gcode.3mf", "abc").resumed);
    f.password = "wrong";
    try {
        ftps_upload(f, "y.gcode.3mf", "abc");
        FAIL("a wrong access code was taken");
    } catch (const FtpsError& e) {
        CHECK(e.code == "unauthorized");
        CHECK_THAT(std::string(e.what()), !ContainsSubstring("wrong")); // never the code itself (rule 8)
    }
}

TEST_CASE("Bambu Lab: pause, resume and stop", "[bambu]") {
    device::BambuSimulator sim(kSerial, kCode);
    const BambuConfig c = config_for(sim);
    const auto settle = [&](const char* state) {
        for (int i = 0; i < 50 && sim.state()["gcode_state"] != state; ++i)
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        return sim.state()["gcode_state"] == state;
    };
    REQUIRE(bambu_command(c, "pause").ok);
    CHECK(settle("PAUSE"));
    CHECK(bambu_status(c).paused);
    REQUIRE(bambu_command(c, "resume").ok);
    CHECK(settle("RUNNING"));
    REQUIRE(bambu_command(c, "stop").ok);
    CHECK(settle("FAILED"));
    CHECK(bambu_command(c, "explode").code == "failed");
    CHECK(sim.commands().at(0)["param"] == "");
}

TEST_CASE("Bambu Lab: the A1 and P1 camera (rule 6)", "[bambu]") {
    const std::string auth = bambu_camera_auth("12345678");
    REQUIRE(auth.size() == 80);
    CHECK(auth[0] == 0x40);
    CHECK(auth[4] == 0x00);
    CHECK(auth[5] == 0x30); // 0x3000, little-endian
    CHECK(auth.substr(16, 4) == "bblp");
    CHECK(auth[20] == '\0');
    CHECK(auth.substr(48, 8) == "12345678");

    device::BambuSimulator sim(kSerial, kCode);
    BambuConfig c = config_for(sim);
    c.serial = kSerial;
    const BambuFrames f = bambu_camera(c, 3, 5000);
    INFO(f.error);
    REQUIRE(f.ok);
    REQUIRE(f.jpegs.size() == 3);
    CHECK(f.jpegs[2].starts_with("\xFF\xD8"));
    c.access_code = "00000000";
    CHECK_FALSE(bambu_camera(c, 1, 3000).ok);
}

namespace {

// A DNS name as labels (RFC 1035 4.1.2).
void dns_name(std::vector<std::uint8_t>& out, const std::string& name) {
    std::size_t start = 0;
    while (start < name.size()) {
        std::size_t dot = name.find('.', start);
        if (dot == std::string::npos)
            dot = name.size();
        out.push_back(std::uint8_t(dot - start));
        out.insert(out.end(), name.begin() + std::ptrdiff_t(start), name.begin() + std::ptrdiff_t(dot));
        start = dot + 1;
    }
    out.push_back(0);
}

void dns_u16(std::vector<std::uint8_t>& out, unsigned v) {
    out.push_back(std::uint8_t(v >> 8));
    out.push_back(std::uint8_t(v));
}

// A record: name (as given bytes), type, class IN, TTL 120, and its data.
void dns_rr(std::vector<std::uint8_t>& out, const std::vector<std::uint8_t>& name, unsigned type,
            const std::vector<std::uint8_t>& data) {
    out.insert(out.end(), name.begin(), name.end());
    dns_u16(out, type);
    dns_u16(out, 0x8001); // IN, cache flush
    out.insert(out.end(), {0, 0, 0, 120});
    dns_u16(out, unsigned(data.size()));
    out.insert(out.end(), data.begin(), data.end());
}

} // namespace

TEST_CASE("mDNS: the question and an OctoPrint answer (RFC 6762, RFC 6763, RFC 1035)", "[ssdp]") {
    const std::vector<std::uint8_t> q = mdns_query("_octoprint._tcp.local");
    // Header: one question; then the name, type PTR (12) and class IN with the QU bit.
    REQUIRE(q.size() == 12 + 22 + 1 + 4);
    CHECK(q[5] == 1);
    CHECK(q[12] == 10);
    CHECK(std::string(q.begin() + 13, q.begin() + 23) == "_octoprint");
    CHECK(q[q.size() - 3] == 12);
    CHECK(q[q.size() - 2] == 0x80);

    // The answer: PTR to the instance, then SRV, TXT and A, with names
    // compressed by pointers back to the first one.
    std::vector<std::uint8_t> p = {0, 0, 0x84, 0, 0, 0, 0, 4, 0, 0, 0, 0};
    const std::size_t service_at = p.size();
    std::vector<std::uint8_t> service;
    dns_name(service, "_octoprint._tcp.local");
    std::vector<std::uint8_t> instance_data = {9, 'O', 'c', 't', 'o', 'P', 'r', 'i', 'n', 't',
                                               std::uint8_t(0xC0), std::uint8_t(service_at)};
    dns_rr(p, service, 12, instance_data);
    const std::size_t instance_at = p.size() - instance_data.size();
    const std::vector<std::uint8_t> instance_ptr = {std::uint8_t(0xC0), std::uint8_t(instance_at)};
    std::vector<std::uint8_t> srv = {0, 0, 0, 0, 0x13, 0x88}; // priority, weight, port 5000
    dns_name(srv, "octopi.local");
    dns_rr(p, instance_ptr, 33, srv);
    std::vector<std::uint8_t> txt;
    for (const std::string e : {"path=/octo", "version=1.10.2", "api=0.1", "Model=Prusa MK3S"}) {
        txt.push_back(std::uint8_t(e.size()));
        txt.insert(txt.end(), e.begin(), e.end());
    }
    dns_rr(p, instance_ptr, 16, txt);
    std::vector<std::uint8_t> host;
    dns_name(host, "octopi.local");
    dns_rr(p, host, 1, {192, 168, 1, 42});

    const std::vector<MdnsService> found = parse_mdns(p, "_octoprint._tcp.local");
    REQUIRE(found.size() == 1);
    CHECK(found[0].instance == "OctoPrint._octoprint._tcp.local");
    CHECK(found[0].host == "octopi.local");
    CHECK(found[0].port == 5000);
    CHECK(found[0].address == "192.168.1.42");
    CHECK(found[0].txt.at("version") == "1.10.2");
    CHECK(found[0].txt.at("model") == "Prusa MK3S"); // keys in lower case
    CHECK(found[0].url() == "http://192.168.1.42:5000/octo");
    // Another service type: nothing; a cut or looping packet: nothing, no throw.
    CHECK(parse_mdns(p, "_http._tcp.local").empty());
    CHECK(parse_mdns(std::vector<std::uint8_t>(p.begin(), p.begin() + 40), "_octoprint._tcp.local").empty());
    std::vector<std::uint8_t> loop = {0, 0, 0x84, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0xC0, 12};
    CHECK(parse_mdns(loop, "_octoprint._tcp.local").empty());
    CHECK(parse_mdns({}, "_octoprint._tcp.local").empty());
    // Anyone on the network can send a packet: damaged copies of the answer
    // (bytes changed, cut short) never read past its end (ASan) nor throw.
    unsigned seed = 11;
    for (int i = 0; i < 5000; ++i) {
        std::vector<std::uint8_t> bad = p;
        for (int k = 0; k < 1 + int(seed % 5); ++k) {
            seed = seed * 1103515245u + 12345u;
            bad[(seed >> 8) % bad.size()] = std::uint8_t(seed >> 16);
        }
        seed = seed * 1103515245u + 12345u;
        if (i % 4 == 0)
            bad.resize(bad.size() - (seed >> 4) % bad.size());
        for (const MdnsService& s : parse_mdns(bad, "_octoprint._tcp.local"))
            CHECK(s.url().size() < 4096);
    }
}
