#include "net/http.hpp"
#include "net/http_impl.hpp"

#include <cctype>
#include <fstream>
#include <iterator>
#include <random>

// What is the same on every system: URLs and multipart bodies. Own design;
// the multipart layout is RFC 7578's (and RFC 2046's boundaries).

namespace tisma::net {

bool parse_url(const std::string& text, Url& out) {
    Url u;
    std::string rest;
    if (text.rfind("https://", 0) == 0) {
        u.https = true;
        rest = text.substr(8);
    } else if (text.rfind("http://", 0) == 0) {
        rest = text.substr(7);
    } else {
        return false;
    }
    const std::size_t slash = rest.find('/');
    std::string authority = rest.substr(0, slash);
    u.path = slash == std::string::npos ? "/" : rest.substr(slash);
    if (authority.empty() || authority.find('@') != std::string::npos)
        return false;
    u.port = u.https ? 443 : 80;
    // An IPv6 address goes in brackets: [::1]:8080.
    std::size_t colon = std::string::npos;
    if (authority.front() == '[') {
        const std::size_t close = authority.find(']');
        if (close == std::string::npos)
            return false;
        u.host = authority.substr(1, close - 1);
        if (close + 1 < authority.size()) {
            if (authority[close + 1] != ':')
                return false;
            colon = close + 1;
        }
    } else {
        colon = authority.rfind(':');
        u.host = authority.substr(0, colon);
    }
    if (colon != std::string::npos) {
        const std::string port = authority.substr(colon + 1);
        if (port.empty() || port.size() > 5)
            return false;
        for (const char c : port)
            if (!std::isdigit(static_cast<unsigned char>(c)))
                return false;
        u.port = std::stoi(port);
        if (u.port <= 0 || u.port > 65535)
            return false;
    }
    if (u.host.empty())
        return false;
    out = u;
    return true;
}

bool form_body(const std::vector<FormPart>& parts, const std::string& boundary, std::string& body,
               std::string& error) {
    body.clear();
    for (const FormPart& p : parts) {
        body += "--" + boundary + "\r\n";
        body += "Content-Disposition: form-data; name=\"" + p.name + "\"";
        if (!p.filename.empty())
            body += "; filename=\"" + p.filename + "\"";
        body += "\r\n";
        if (!p.path.empty() || !p.filename.empty())
            body += "Content-Type: " + p.content_type + "\r\n";
        body += "\r\n";
        if (!p.path.empty()) {
            std::ifstream in(p.path, std::ios::binary);
            if (!in) {
                error = "cannot read " + p.path;
                return false;
            }
            body.append(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
        } else {
            body += p.value;
        }
        body += "\r\n";
    }
    body += "--" + boundary + "--\r\n";
    return true;
}

namespace {

// A boundary of 24 random letters and digits (a file with it inside is too
// unlikely to matter: 62^24 choices).
std::string make_boundary() {
    static thread_local std::mt19937_64 random{std::random_device{}()};
    static const char letters[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    std::string b = "tisma-";
    for (int i = 0; i < 24; ++i)
        b += letters[random() % (sizeof(letters) - 1)];
    return b;
}

} // namespace

bool prepare_body(const Request& request, std::string& body, std::string& content_type, std::string& error) {
    content_type.clear();
    if (request.form.empty()) {
        body = request.body;
        return true;
    }
    const std::string boundary = make_boundary();
    content_type = "multipart/form-data; boundary=" + boundary;
    return form_body(request.form, boundary, body, error);
}

} // namespace tisma::net
