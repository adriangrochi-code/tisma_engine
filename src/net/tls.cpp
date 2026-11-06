// TLS streams over Mbed TLS (THIRD_PARTY.md). Own code; see tls.hpp and
// PROVENANCE.md.

#include "net/tls.hpp"

#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>
#include <mbedtls/error.h>
#include <mbedtls/ecp.h>
#include <mbedtls/net_sockets.h>
#include <mbedtls/oid.h>
#include <mbedtls/pk.h>
#include <mbedtls/sha256.h>
#include <mbedtls/ssl.h>
#include <mbedtls/ssl_cache.h>
#include <mbedtls/x509_crt.h>
#include <psa/crypto.h>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <netinet/in.h>
#include <sys/socket.h>
#endif

#include <array>
#include <chrono>
#include <cstring>
#include <mutex>
#include <vector>

namespace tisma::net {
namespace {

std::string hex(const unsigned char* p, std::size_t n) {
    static const char* digits = "0123456789abcdef";
    std::string out;
    out.reserve(2 * n);
    for (std::size_t i = 0; i < n; ++i) {
        out += digits[p[i] >> 4];
        out += digits[p[i] & 15];
    }
    return out;
}

std::string why(const char* what, int code) {
    char text[160];
    mbedtls_strerror(code, text, sizeof text);
    return std::string(what) + ": " + text;
}

void psa_once() {
    static std::once_flag once;
    std::call_once(once, [] {
        if (psa_crypto_init() != PSA_SUCCESS)
            throw TlsError("TLS: the crypto library does not start");
    });
}

// A random generator seeded from the system's entropy: one per stream, as
// Mbed TLS's generator is not shared between threads here.
struct Random {
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context drbg;
    Random() {
        mbedtls_entropy_init(&entropy);
        mbedtls_ctr_drbg_init(&drbg);
        static const char personal[] = "tisma-tls";
        if (const int r = mbedtls_ctr_drbg_seed(&drbg, mbedtls_entropy_func, &entropy,
                                                reinterpret_cast<const unsigned char*>(personal), sizeof personal - 1)) {
            mbedtls_ctr_drbg_free(&drbg);
            mbedtls_entropy_free(&entropy);
            throw TlsError(why("TLS: random seed", r));
        }
    }
    ~Random() {
        mbedtls_ctr_drbg_free(&drbg);
        mbedtls_entropy_free(&entropy);
    }
    Random(const Random&) = delete;
    Random& operator=(const Random&) = delete;
};

std::string common_name(const mbedtls_x509_crt& crt) {
    for (const mbedtls_x509_name* n = &crt.subject; n; n = n->next)
        if (n->oid.p && MBEDTLS_OID_CMP(MBEDTLS_OID_AT_CN, &n->oid) == 0)
            return std::string(reinterpret_cast<const char*>(n->val.p), n->val.len);
    return {};
}

} // namespace

struct TlsStream::Impl {
    Random random;
    mbedtls_net_context net;
    mbedtls_ssl_config conf;
    mbedtls_ssl_context ssl;
    std::string sha256, cn;
    bool resumed = false;
    bool open = false;
    Impl() {
        mbedtls_net_init(&net);
        mbedtls_ssl_config_init(&conf);
        mbedtls_ssl_init(&ssl);
    }
    ~Impl() {
        if (open)
            mbedtls_ssl_close_notify(&ssl);
        mbedtls_ssl_free(&ssl);
        mbedtls_ssl_config_free(&conf);
        mbedtls_net_free(&net);
    }
    void handshake(int timeout_ms) {
        mbedtls_ssl_conf_read_timeout(&conf, std::uint32_t(timeout_ms));
        for (;;) {
            const int r = mbedtls_ssl_handshake(&ssl);
            if (r == 0)
                break;
            if (r != MBEDTLS_ERR_SSL_WANT_READ && r != MBEDTLS_ERR_SSL_WANT_WRITE)
                throw TlsError(why("TLS handshake", r));
        }
        open = true;
        if (const mbedtls_x509_crt* peer = mbedtls_ssl_get_peer_cert(&ssl)) {
            unsigned char sum[32];
            mbedtls_sha256(peer->raw.p, peer->raw.len, sum, 0);
            sha256 = hex(sum, 32);
            cn = common_name(*peer);
        }
    }
};

TlsStream::TlsStream(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}
TlsStream::~TlsStream() = default;

std::unique_ptr<TlsStream> TlsStream::connect(const std::string& host, int port, int timeout_ms, const TlsStream* resume_from) {
    psa_once();
    auto impl = std::make_unique<Impl>();
    if (const int r = mbedtls_net_connect(&impl->net, host.c_str(), std::to_string(port).c_str(), MBEDTLS_NET_PROTO_TCP))
        throw TlsError(why(("cannot reach " + host + ":" + std::to_string(port)).c_str(), r));
    if (const int r = mbedtls_ssl_config_defaults(&impl->conf, MBEDTLS_SSL_IS_CLIENT, MBEDTLS_SSL_TRANSPORT_STREAM,
                                                  MBEDTLS_SSL_PRESET_DEFAULT))
        throw TlsError(why("TLS setup", r));
    // No authority is trusted: the caller pins the certificate (rule 4).
    mbedtls_ssl_conf_authmode(&impl->conf, MBEDTLS_SSL_VERIFY_NONE);
    mbedtls_ssl_conf_rng(&impl->conf, mbedtls_ctr_drbg_random, &impl->random.drbg);
    mbedtls_ssl_conf_min_tls_version(&impl->conf, MBEDTLS_SSL_VERSION_TLS1_2);
    mbedtls_ssl_conf_max_tls_version(&impl->conf, MBEDTLS_SSL_VERSION_TLS1_2);
    if (const int r = mbedtls_ssl_setup(&impl->ssl, &impl->conf))
        throw TlsError(why("TLS setup", r));
    mbedtls_ssl_set_bio(&impl->ssl, &impl->net, mbedtls_net_send, nullptr, mbedtls_net_recv_timeout);
    std::vector<unsigned char> offered;
    if (resume_from) {
        mbedtls_ssl_session session;
        mbedtls_ssl_session_init(&session);
        if (mbedtls_ssl_get_session(&resume_from->impl_->ssl, &session) == 0) {
            const unsigned char* id = *mbedtls_ssl_session_get_id(&session);
            offered.assign(id, id + mbedtls_ssl_session_get_id_len(&session));
            if (const int r = mbedtls_ssl_set_session(&impl->ssl, &session)) {
                mbedtls_ssl_session_free(&session);
                throw TlsError(why("TLS resume", r));
            }
        }
        mbedtls_ssl_session_free(&session);
    }
    impl->handshake(timeout_ms);
    if (!offered.empty()) {
        mbedtls_ssl_session now;
        mbedtls_ssl_session_init(&now);
        if (mbedtls_ssl_get_session(&impl->ssl, &now) == 0) {
            const unsigned char* id = *mbedtls_ssl_session_get_id(&now);
            impl->resumed = mbedtls_ssl_session_get_id_len(&now) == offered.size() &&
                            std::memcmp(id, offered.data(), offered.size()) == 0;
        }
        mbedtls_ssl_session_free(&now);
    }
    return std::unique_ptr<TlsStream>(new TlsStream(std::move(impl)));
}

void TlsStream::write(std::string_view bytes) {
    if (!impl_->open)
        throw TlsError("TLS: the connection is closed");
    const auto* p = reinterpret_cast<const unsigned char*>(bytes.data());
    std::size_t left = bytes.size();
    while (left > 0) {
        const int r = mbedtls_ssl_write(&impl_->ssl, p, left);
        if (r == MBEDTLS_ERR_SSL_WANT_READ || r == MBEDTLS_ERR_SSL_WANT_WRITE)
            continue;
        if (r < 0)
            throw TlsError(why("TLS write", r));
        p += r;
        left -= std::size_t(r);
    }
}

std::string TlsStream::read(std::size_t max, int timeout_ms) {
    if (!impl_->open)
        throw TlsError("TLS: the connection is closed");
    mbedtls_ssl_conf_read_timeout(&impl_->conf, std::uint32_t(timeout_ms > 0 ? timeout_ms : 1));
    std::string out(max, '\0');
    for (;;) {
        const int r = mbedtls_ssl_read(&impl_->ssl, reinterpret_cast<unsigned char*>(out.data()), max);
        if (r > 0) {
            out.resize(std::size_t(r));
            return out;
        }
        if (r == MBEDTLS_ERR_SSL_WANT_READ || r == MBEDTLS_ERR_SSL_WANT_WRITE)
            continue;
        if (r == MBEDTLS_ERR_SSL_TIMEOUT)
            throw TlsTimeout("TLS: nothing arrived in time");
        impl_->open = false;
        if (r == 0 || r == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY)
            throw TlsError("TLS: the other side closed the connection");
        throw TlsError(why("TLS read", r));
    }
}

std::string TlsStream::read_exact(std::size_t n, int timeout_ms) {
    std::string out;
    const auto end = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
    while (out.size() < n) {
        const auto left = std::chrono::duration_cast<std::chrono::milliseconds>(end - std::chrono::steady_clock::now()).count();
        if (left <= 0)
            throw TlsTimeout("TLS: the rest did not arrive in time");
        out += read(n - out.size(), int(left));
    }
    return out;
}

void TlsStream::close() {
    if (impl_->open) {
        mbedtls_ssl_close_notify(&impl_->ssl);
        impl_->open = false;
    }
    mbedtls_net_free(&impl_->net);
}

const std::string& TlsStream::peer_sha256() const { return impl_->sha256; }
const std::string& TlsStream::peer_cn() const { return impl_->cn; }
bool TlsStream::resumed() const { return impl_->resumed; }

struct TlsListener::Impl {
    Random random;
    mbedtls_net_context listen;
    mbedtls_pk_context key;
    mbedtls_x509_crt cert;
    mbedtls_ssl_cache_context cache;
    mbedtls_ssl_config conf;
    int port = 0;
    std::string sha256;
    Impl() {
        mbedtls_net_init(&listen);
        mbedtls_pk_init(&key);
        mbedtls_x509_crt_init(&cert);
        mbedtls_ssl_cache_init(&cache);
        mbedtls_ssl_config_init(&conf);
    }
    ~Impl() {
        mbedtls_ssl_config_free(&conf);
        mbedtls_ssl_cache_free(&cache);
        mbedtls_x509_crt_free(&cert);
        mbedtls_pk_free(&key);
        mbedtls_net_free(&listen);
    }
};

TlsListener::TlsListener(const std::string& cn) : impl_(std::make_unique<Impl>()) {
    psa_once();
    Impl& m = *impl_;
    // A P-256 key and a certificate for it, signed by itself.
    if (int r = mbedtls_pk_setup(&m.key, mbedtls_pk_info_from_type(MBEDTLS_PK_ECKEY)); r ||
        (r = mbedtls_ecp_gen_key(MBEDTLS_ECP_DP_SECP256R1, mbedtls_pk_ec(m.key), mbedtls_ctr_drbg_random, &m.random.drbg)))
        throw TlsError(why("TLS key", r));
    mbedtls_x509write_cert w;
    mbedtls_x509write_crt_init(&w);
    const std::string name = "CN=" + cn;
    const unsigned char serial[] = {1};
    std::array<unsigned char, 2048> der{};
    int written = 0;
    if (mbedtls_x509write_crt_set_subject_name(&w, name.c_str()) || mbedtls_x509write_crt_set_issuer_name(&w, name.c_str()) ||
        mbedtls_x509write_crt_set_serial_raw(&w, const_cast<unsigned char*>(serial), sizeof serial) ||
        mbedtls_x509write_crt_set_validity(&w, "20250101000000", "20450101000000")) {
        mbedtls_x509write_crt_free(&w);
        throw TlsError("TLS: the test certificate cannot be made");
    }
    mbedtls_x509write_crt_set_subject_key(&w, &m.key);
    mbedtls_x509write_crt_set_issuer_key(&w, &m.key);
    mbedtls_x509write_crt_set_md_alg(&w, MBEDTLS_MD_SHA256);
    written = mbedtls_x509write_crt_der(&w, der.data(), der.size(), mbedtls_ctr_drbg_random, &m.random.drbg);
    mbedtls_x509write_crt_free(&w);
    if (written <= 0)
        throw TlsError(why("TLS certificate", written));
    const unsigned char* start = der.data() + der.size() - std::size_t(written);
    if (const int r = mbedtls_x509_crt_parse_der(&m.cert, start, std::size_t(written)))
        throw TlsError(why("TLS certificate", r));
    unsigned char sum[32];
    mbedtls_sha256(start, std::size_t(written), sum, 0);
    m.sha256 = hex(sum, 32);

    if (const int r = mbedtls_ssl_config_defaults(&m.conf, MBEDTLS_SSL_IS_SERVER, MBEDTLS_SSL_TRANSPORT_STREAM,
                                                  MBEDTLS_SSL_PRESET_DEFAULT))
        throw TlsError(why("TLS setup", r));
    mbedtls_ssl_conf_rng(&m.conf, mbedtls_ctr_drbg_random, &m.random.drbg);
    mbedtls_ssl_conf_min_tls_version(&m.conf, MBEDTLS_SSL_VERSION_TLS1_2);
    mbedtls_ssl_conf_max_tls_version(&m.conf, MBEDTLS_SSL_VERSION_TLS1_2);
    mbedtls_ssl_conf_session_cache(&m.conf, &m.cache, mbedtls_ssl_cache_get, mbedtls_ssl_cache_set);
    if (const int r = mbedtls_ssl_conf_own_cert(&m.conf, &m.cert, &m.key))
        throw TlsError(why("TLS certificate", r));

    if (const int r = mbedtls_net_bind(&m.listen, "127.0.0.1", "0", MBEDTLS_NET_PROTO_TCP))
        throw TlsError(why("TLS listen", r));
    sockaddr_in address{};
    socklen_t length = socklen_t(sizeof address);
#ifdef _WIN32
    if (getsockname(static_cast<SOCKET>(m.listen.fd), reinterpret_cast<sockaddr*>(&address), &length) != 0)
#else
    if (getsockname(m.listen.fd, reinterpret_cast<sockaddr*>(&address), &length) != 0)
#endif
        throw TlsError("TLS: the listening port is unknown");
    m.port = ntohs(address.sin_port);
}

TlsListener::~TlsListener() = default;

int TlsListener::port() const { return impl_->port; }
std::string TlsListener::cert_sha256() const { return impl_->sha256; }

std::unique_ptr<TlsStream> TlsListener::accept(int timeout_ms) {
    if (mbedtls_net_poll(&impl_->listen, MBEDTLS_NET_POLL_READ, std::uint32_t(timeout_ms)) <= 0)
        return nullptr;
    auto s = std::make_unique<TlsStream::Impl>();
    if (const int r = mbedtls_net_accept(&impl_->listen, &s->net, nullptr, 0, nullptr))
        throw TlsError(why("TLS accept", r));
    // The stream's own configuration, sharing the listener's certificate and
    // session cache (the stream's reads change its timeout).
    if (const int r = mbedtls_ssl_config_defaults(&s->conf, MBEDTLS_SSL_IS_SERVER, MBEDTLS_SSL_TRANSPORT_STREAM,
                                                  MBEDTLS_SSL_PRESET_DEFAULT))
        throw TlsError(why("TLS setup", r));
    mbedtls_ssl_conf_rng(&s->conf, mbedtls_ctr_drbg_random, &s->random.drbg);
    mbedtls_ssl_conf_min_tls_version(&s->conf, MBEDTLS_SSL_VERSION_TLS1_2);
    mbedtls_ssl_conf_max_tls_version(&s->conf, MBEDTLS_SSL_VERSION_TLS1_2);
    mbedtls_ssl_conf_session_cache(&s->conf, &impl_->cache, mbedtls_ssl_cache_get, mbedtls_ssl_cache_set);
    if (const int r = mbedtls_ssl_conf_own_cert(&s->conf, &impl_->cert, &impl_->key))
        throw TlsError(why("TLS certificate", r));
    if (const int r = mbedtls_ssl_setup(&s->ssl, &s->conf))
        throw TlsError(why("TLS setup", r));
    mbedtls_ssl_set_bio(&s->ssl, &s->net, mbedtls_net_send, nullptr, mbedtls_net_recv_timeout);
    s->handshake(timeout_ms);
    return std::unique_ptr<TlsStream>(new TlsStream(std::move(s)));
}


} // namespace tisma::net
