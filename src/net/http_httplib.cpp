// The HTTP client on Linux and macOS: cpp-httplib (MIT), over OpenSSL 3
// (Apache-2.0) for https:// when the build found it (TISMA_HTTPS). Own
// design on that library's client; see PROVENANCE.md.

#include "net/http.hpp"
#include "net/http_impl.hpp"

#include <httplib.h>

#include <memory>

namespace tisma::net {

bool https_available() {
#ifdef CPPHTTPLIB_SSL_ENABLED
    return true;
#else
    return false;
#endif
}

Response perform(const Request& request) {
    Response out;
    Url url;
    if (!parse_url(request.url, url)) {
        out.error = "not an http:// or https:// address: " + request.url;
        return out;
    }
    std::string body, form_type;
    if (!prepare_body(request, body, form_type, out.error))
        return out;

    std::unique_ptr<httplib::ClientImpl> client;
    if (url.https) {
#ifdef CPPHTTPLIB_SSL_ENABLED
        auto secure = std::make_unique<httplib::SSLClient>(url.host, url.port);
        secure->enable_server_certificate_verification(true);
        client = std::move(secure);
#else
        out.error = "this build has no HTTPS (it was built without OpenSSL)";
        return out;
#endif
    } else {
        client = std::make_unique<httplib::ClientImpl>(url.host, url.port);
    }
    client->set_connection_timeout(request.connect_timeout_s, 0);
    client->set_read_timeout(request.timeout_s, 0);
    client->set_write_timeout(request.timeout_s, 0);
    client->set_follow_location(false);
    client->set_keep_alive(false);

    httplib::Request req;
    req.method = request.method;
    req.path = url.path;
    std::string content_type = form_type;
    for (const auto& [name, value] : request.headers) {
        if (httplib::detail::case_ignore::equal(name, "Content-Type") && content_type.empty())
            content_type = value;
        else
            req.set_header(name, value);
    }
    if (!body.empty() || request.method == "POST" || request.method == "PUT") {
        req.body = std::move(body);
        req.set_header("Content-Type", content_type.empty() ? std::string("application/octet-stream") : content_type);
    }
    // The answer, kept until it passes the limit.
    bool too_large = false;
    std::string received;
    req.content_receiver = [&](const char* data, std::size_t size, std::size_t, std::size_t) {
        if (request.max_bytes > 0 && received.size() + size > request.max_bytes) {
            too_large = true;
            return false;
        }
        received.append(data, size);
        return true;
    };
    httplib::Response res;
    httplib::Error error = httplib::Error::Success;
    const bool sent = client->send(req, res, error);
    if (too_large) {
        out.error = "the answer is larger than " + std::to_string(request.max_bytes) + " bytes";
        return out;
    }
    if (!sent) {
        out.error = httplib::to_string(error);
#ifdef CPPHTTPLIB_SSL_ENABLED
        if (error == httplib::Error::SSLServerVerification)
            out.error += " (the server's certificate is not trusted)";
#endif
        return out;
    }
    out.status = res.status;
    out.body = std::move(received);
    return out;
}

} // namespace tisma::net
