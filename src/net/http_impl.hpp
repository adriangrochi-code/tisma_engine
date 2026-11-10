#pragma once

// Shared by the HTTP client's system parts (http_winhttp.cpp, http_httplib.cpp).

#include "net/http.hpp"

#include <string>

namespace tisma::net {

// The body to send: the request's own, or its form as multipart/form-data
// (then content_type is set). False with the reason when a file cannot be read.
bool prepare_body(const Request& request, std::string& body, std::string& content_type, std::string& error);

} // namespace tisma::net
