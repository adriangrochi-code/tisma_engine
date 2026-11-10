#pragma once

// The engine's HTTP client (cloud.md, rule 1: everything over HTTPS, JSON in
// and out, files as they are). One blocking call, run outside the interface's
// thread. On Windows it uses WinHTTP (the system's certificates and proxy);
// elsewhere cpp-httplib over OpenSSL 3. Own design; see PROVENANCE.md.

#include <cstddef>
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace tisma::net {

// A part of a multipart/form-data body (RFC 7578): a field, or a file read
// from `path` and sent with `filename`.
struct FormPart {
    std::string name;
    std::string value;
    std::string path;     // a file's contents instead of value
    std::string filename; // the name the server sees; empty for a field
    std::string content_type = "application/octet-stream";
};

struct Request {
    std::string method = "GET";
    std::string url; // http:// or https://, with an optional port
    std::vector<std::pair<std::string, std::string>> headers;
    std::string body; // with its Content-Type among the headers
    std::vector<FormPart> form; // when not empty: the body, as multipart/form-data
    int connect_timeout_s = 10;
    int timeout_s = 60;
    std::size_t max_bytes = 0; // the largest answer accepted; 0: no limit
};

struct Response {
    int status = 0;    // the HTTP status; 0 when no answer came
    std::string body;
    std::string error; // why there is no answer (connection, certificate, timeout, too large), in English
    std::string www_authenticate; // the server's WWW-Authenticate header, if any (a Digest challenge)
    std::string location;         // a redirect's Location header, if any; redirects are never followed here
    bool ok() const { return error.empty() && status >= 200 && status < 300; }
};

// Sends the request and waits for the whole answer. Never throws.
Response perform(const Request& request);

// Whether this build talks HTTPS (always on Windows; with OpenSSL elsewhere).
bool https_available();

// The parts of a URL; false when it is not http:// or https://host[:port][/path].
struct Url {
    bool https = false;
    std::string host;
    int port = 0;
    std::string path = "/"; // with its query
};
bool parse_url(const std::string& text, Url& out);

// A multipart/form-data body with this boundary, and its Content-Type. False
// with the reason when a file cannot be read.
bool form_body(const std::vector<FormPart>& parts, const std::string& boundary, std::string& body,
               std::string& error);

} // namespace tisma::net
