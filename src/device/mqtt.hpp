#pragma once

// MQTT 3.1.1 (OASIS Standard, 29 October 2014), the part a Bambu Lab printer
// needs (printer_connections.md, «Bambu Lab en red local», rule 2): CONNECT
// with user and password, SUBSCRIBE, PUBLISH at QoS 0 and 1, PINGREQ and
// DISCONNECT, read and written as the standard's section 2 and 3 say; the
// broker side of the same packets for the tests' simulator. Over a TLS stream
// (net/tls.hpp). Own design; see PROVENANCE.md.

#include "net/tls.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace tisma::device::mqtt {

// Control packet types (section 2.2.1).
enum Type : int {
    kConnect = 1, kConnack = 2, kPublish = 3, kPuback = 4, kSubscribe = 8, kSuback = 9,
    kPingreq = 12, kPingresp = 13, kDisconnect = 14
};

class MqttError : public std::runtime_error {
public:
    MqttError(std::string what_code, const std::string& message) : std::runtime_error(message), code(std::move(what_code)) {}
    std::string code; // unauthorized, refused, bad_packet, closed, timeout
};

struct Packet {
    int type = 0;
    int flags = 0;     // the low four bits of the first byte
    std::string body;  // the variable header and the payload
};

struct Publish {
    std::string topic, payload;
    int qos = 0;
    std::uint16_t id = 0;
};

struct Connect {
    std::string client_id, user, password;
    int keepalive_s = 0;
};

// Packets to send.
std::string connect_packet(const Connect& c);
std::string subscribe_packet(std::uint16_t id, const std::string& topic, int qos);
std::string publish_packet(const Publish& p);
std::string puback_packet(std::uint16_t id);
std::string pingreq_packet();
std::string disconnect_packet();
// The broker's (the simulator's).
std::string connack_packet(int return_code);
std::string suback_packet(std::uint16_t id, int granted_qos);
std::string pingresp_packet();

// Packets out of a byte stream: feed what arrives, take whole packets.
class Reader {
public:
    void feed(const std::string& bytes) { buffer_ += bytes; }
    // The next whole packet, if one is there. Throws MqttError on a length
    // over four bytes (section 2.2.3).
    std::optional<Packet> next();

private:
    std::string buffer_;
};

Publish parse_publish(const Packet& p);
Connect parse_connect(const Packet& p);
// A SUBSCRIBE's packet id and topics.
std::vector<std::string> parse_subscribe(const Packet& p, std::uint16_t& id);

// A client on a TLS stream.
class Client {
public:
    explicit Client(std::unique_ptr<net::TlsStream> stream);
    // CONNECT and its CONNACK; throws MqttError("unauthorized") for return
    // codes 4 and 5 (a wrong user or password), "refused" for the others.
    void connect(const Connect& c, int timeout_ms);
    // SUBSCRIBE and its SUBACK.
    void subscribe(const std::string& topic, int qos, int timeout_ms);
    void publish(const std::string& topic, const std::string& payload, int qos);
    // The next PUBLISH, answering pings and acknowledging QoS 1; nothing
    // after timeout_ms. Throws MqttError("closed") when the stream ends.
    std::optional<Publish> receive(int timeout_ms);
    void ping();
    void disconnect();
    net::TlsStream& stream() { return *stream_; }

private:
    std::optional<Packet> next_packet(int timeout_ms);
    std::unique_ptr<net::TlsStream> stream_;
    Reader reader_;
    std::uint16_t next_id_ = 1;
};

} // namespace tisma::device::mqtt
