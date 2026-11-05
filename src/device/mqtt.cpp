// MQTT 3.1.1 packets (OASIS Standard, sections 2 and 3). Own code; see
// mqtt.hpp and PROVENANCE.md.

#include "device/mqtt.hpp"

#include <algorithm>
#include <chrono>

namespace tisma::device::mqtt {
namespace {

void put16(std::string& out, std::uint16_t v) {
    out += char(v >> 8);
    out += char(v & 0xFF);
}

void put_string(std::string& out, const std::string& s) {
    put16(out, std::uint16_t(s.size()));
    out += s;
}

// The fixed header: the type and flags, then the remaining length in 1 to 4
// bytes, 7 bits each, low first (section 2.2.3).
std::string packet(int type, int flags, const std::string& body) {
    std::string out;
    out += char((type << 4) | (flags & 0x0F));
    std::size_t n = body.size();
    do {
        unsigned char byte = n % 128;
        n /= 128;
        if (n > 0)
            byte |= 0x80;
        out += char(byte);
    } while (n > 0);
    return out + body;
}

struct Cursor {
    const std::string& s;
    std::size_t at = 0;
    std::uint16_t u16() {
        if (at + 2 > s.size())
            throw MqttError("bad_packet", "MQTT: a packet is cut short");
        const std::uint16_t v = std::uint16_t((std::uint8_t(s[at]) << 8) | std::uint8_t(s[at + 1]));
        at += 2;
        return v;
    }
    std::string str() {
        const std::size_t n = u16();
        if (at + n > s.size())
            throw MqttError("bad_packet", "MQTT: a string is cut short");
        std::string v = s.substr(at, n);
        at += n;
        return v;
    }
    std::uint8_t byte() {
        if (at >= s.size())
            throw MqttError("bad_packet", "MQTT: a packet is cut short");
        return std::uint8_t(s[at++]);
    }
};

} // namespace

std::string connect_packet(const Connect& c) {
    std::string body;
    put_string(body, "MQTT");
    body += char(4); // protocol level 4: 3.1.1
    int flags = 0x02; // clean session
    if (!c.user.empty())
        flags |= 0x80;
    if (!c.password.empty())
        flags |= 0x40;
    body += char(flags);
    put16(body, std::uint16_t(c.keepalive_s));
    put_string(body, c.client_id);
    if (!c.user.empty())
        put_string(body, c.user);
    if (!c.password.empty())
        put_string(body, c.password);
    return packet(kConnect, 0, body);
}

std::string subscribe_packet(std::uint16_t id, const std::string& topic, int qos) {
    std::string body;
    put16(body, id);
    put_string(body, topic);
    body += char(qos);
    return packet(kSubscribe, 0x02, body); // section 3.8.1: flags 0010
}

std::string publish_packet(const Publish& p) {
    std::string body;
    put_string(body, p.topic);
    if (p.qos > 0)
        put16(body, p.id);
    body += p.payload;
    return packet(kPublish, (p.qos & 3) << 1, body);
}

std::string puback_packet(std::uint16_t id) {
    std::string body;
    put16(body, id);
    return packet(kPuback, 0, body);
}

std::string pingreq_packet() { return packet(kPingreq, 0, {}); }
std::string disconnect_packet() { return packet(kDisconnect, 0, {}); }

std::string connack_packet(int return_code) {
    std::string body;
    body += char(0);
    body += char(return_code);
    return packet(kConnack, 0, body);
}

std::string suback_packet(std::uint16_t id, int granted_qos) {
    std::string body;
    put16(body, id);
    body += char(granted_qos);
    return packet(kSuback, 0, body);
}

std::string pingresp_packet() { return packet(kPingresp, 0, {}); }

std::optional<Packet> Reader::next() {
    if (buffer_.size() < 2)
        return std::nullopt;
    std::size_t length = 0, multiplier = 1, at = 1;
    for (;; ++at) {
        if (at > 4)
            throw MqttError("bad_packet", "MQTT: a remaining length longer than four bytes");
        if (at >= buffer_.size())
            return std::nullopt;
        const std::uint8_t b = std::uint8_t(buffer_[at]);
        length += (b & 0x7F) * multiplier;
        multiplier *= 128;
        if (!(b & 0x80))
            break;
    }
    const std::size_t start = at + 1;
    if (buffer_.size() < start + length)
        return std::nullopt;
    Packet p;
    p.type = std::uint8_t(buffer_[0]) >> 4;
    p.flags = buffer_[0] & 0x0F;
    p.body = buffer_.substr(start, length);
    buffer_.erase(0, start + length);
    return p;
}

Publish parse_publish(const Packet& p) {
    if (p.type != kPublish)
        throw MqttError("bad_packet", "MQTT: not a PUBLISH");
    Cursor c{p.body};
    Publish out;
    out.topic = c.str();
    out.qos = (p.flags >> 1) & 3;
    if (out.qos > 0)
        out.id = c.u16();
    out.payload = p.body.substr(c.at);
    return out;
}

Connect parse_connect(const Packet& p) {
    if (p.type != kConnect)
        throw MqttError("bad_packet", "MQTT: not a CONNECT");
    Cursor c{p.body};
    if (c.str() != "MQTT" || c.byte() != 4)
        throw MqttError("bad_packet", "MQTT: not version 3.1.1");
    const int flags = c.byte();
    Connect out;
    out.keepalive_s = c.u16();
    out.client_id = c.str();
    if (flags & 0x04) { // a will: its topic and message
        (void)c.str();
        (void)c.str();
    }
    if (flags & 0x80)
        out.user = c.str();
    if (flags & 0x40)
        out.password = c.str();
    return out;
}

std::vector<std::string> parse_subscribe(const Packet& p, std::uint16_t& id) {
    if (p.type != kSubscribe)
        throw MqttError("bad_packet", "MQTT: not a SUBSCRIBE");
    Cursor c{p.body};
    id = c.u16();
    std::vector<std::string> topics;
    while (c.at < p.body.size()) {
        topics.push_back(c.str());
        (void)c.byte(); // the QoS asked
    }
    return topics;
}

Client::Client(std::unique_ptr<net::TlsStream> stream) : stream_(std::move(stream)) {}

std::optional<Packet> Client::next_packet(int timeout_ms) {
    const auto end = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
    for (;;) {
        if (std::optional<Packet> p = reader_.next())
            return p;
        const auto left = std::chrono::duration_cast<std::chrono::milliseconds>(end - std::chrono::steady_clock::now()).count();
        if (left <= 0)
            return std::nullopt;
        try {
            reader_.feed(stream_->read(16384, int(left)));
        } catch (const net::TlsTimeout&) {
            return std::nullopt;
        } catch (const net::TlsError& e) {
            throw MqttError("closed", std::string("MQTT: ") + e.what());
        }
    }
}

void Client::connect(const Connect& c, int timeout_ms) {
    stream_->write(connect_packet(c));
    const std::optional<Packet> p = next_packet(timeout_ms);
    if (!p)
        throw MqttError("timeout", "MQTT: no answer to CONNECT");
    if (p->type != kConnack || p->body.size() < 2)
        throw MqttError("bad_packet", "MQTT: the answer to CONNECT is not a CONNACK");
    const int code = std::uint8_t(p->body[1]);
    if (code == 4 || code == 5)
        throw MqttError("unauthorized", "MQTT: the printer refused the access code");
    if (code != 0)
        throw MqttError("refused", "MQTT: the printer refused the connection (code " + std::to_string(code) + ")");
}

void Client::subscribe(const std::string& topic, int qos, int timeout_ms) {
    const std::uint16_t id = next_id_++;
    stream_->write(subscribe_packet(id, topic, qos));
    const auto end = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
    for (;;) {
        const auto left = std::chrono::duration_cast<std::chrono::milliseconds>(end - std::chrono::steady_clock::now()).count();
        const std::optional<Packet> p = next_packet(int(std::max<long long>(left, 0)));
        if (!p)
            throw MqttError("timeout", "MQTT: no answer to SUBSCRIBE");
        if (p->type == kSuback) {
            if (p->body.size() >= 3 && std::uint8_t(p->body[2]) == 0x80)
                throw MqttError("refused", "MQTT: the printer refused the subscription to " + topic);
            return;
        }
    }
}

void Client::publish(const std::string& topic, const std::string& payload, int qos) {
    Publish p{topic, payload, qos, qos > 0 ? next_id_++ : std::uint16_t(0)};
    if (next_id_ == 0)
        next_id_ = 1;
    stream_->write(publish_packet(p));
}

std::optional<Publish> Client::receive(int timeout_ms) {
    const auto end = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
    for (;;) {
        const auto left = std::chrono::duration_cast<std::chrono::milliseconds>(end - std::chrono::steady_clock::now()).count();
        const std::optional<Packet> p = next_packet(int(std::max<long long>(left, 0)));
        if (!p)
            return std::nullopt;
        if (p->type == kPublish) {
            Publish m = parse_publish(*p);
            if (m.qos == 1)
                stream_->write(puback_packet(m.id));
            return m;
        }
        // PINGRESP, PUBACK: nothing to do.
    }
}

void Client::ping() { stream_->write(pingreq_packet()); }

void Client::disconnect() {
    try {
        stream_->write(disconnect_packet());
        stream_->close();
    } catch (const net::TlsError&) {
    }
}

} // namespace tisma::device::mqtt
